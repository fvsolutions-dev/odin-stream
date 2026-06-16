#include "stream_processor.h"

#include <cstring>
#include <string>

extern "C" {
#include "embedded_odin_stream/stream_packet.h"
}

namespace nb = nanobind;

OdinStreamDecoder::OdinStreamDecoder(std::shared_ptr<ParameterMapDescriptor> parameter_map, bool silent_errors)
	: parameter_map(parameter_map), silent_errors(silent_errors) {}

// Map an ODIN_element_type_t value (as carried on the wire by a 0x03 item) to
// the matching primitive in the registry. Index-aligned with the enum:
// 0=BOOL,1=HEX,2=UINT8,3=UINT16,4=UINT32,5=UINT64,6=INT8,7=INT16,8=INT32,
// 9=INT64,10=FLOAT32,11=FLOAT64,12=CHAR,13=CUSTOM.
static std::shared_ptr<PrimitiveTypeDescriptor> primitive_for_element_type(uint8_t element_type) {
	static const char* const names[] = {
		"bool",  // 0  BOOL
		"u8",    // 1  HEX (raw bytes, treated as u8)
		"u8",    // 2  UINT8
		"u16",   // 3  UINT16
		"u32",   // 4  UINT32
		"u64",   // 5  UINT64
		"i8",    // 6  INT8
		"i16",   // 7  INT16
		"i32",   // 8  INT32
		"i64",   // 9  INT64
		"f32",   // 10 FLOAT32
		"f64",   // 11 FLOAT64
		"char",  // 12 CHAR
		"unknown",  // 13 CUSTOM
	};
	const char* name = (element_type < (sizeof(names) / sizeof(names[0]))) ? names[element_type] : "unknown";
	return PrimitiveTypeDescriptor::get_by_name(name);
}

void OdinStreamDecoder::process_identifier_ext(nb::bytes data) {
	uint32_t definition_identifier = 0;
	uint16_t total_count = 0;
	uint16_t start_ordinal = 0;

	const uint8_t* buffer = reinterpret_cast<const uint8_t*>(data.c_str());
	const size_t buffer_size = data.size();

	if (buffer_size < sizeof(streaming_identifier_ext_packet_header_t)) {
		throw nb::value_error("Input data too small to contain extended identifier packet header.");
	}

	const streaming_identifier_ext_packet_header_t* hdr = reinterpret_cast<const streaming_identifier_ext_packet_header_t*>(buffer);
	if (hdr->header.type != STREAM_STREAM_PACKET_TYPE_IDENTIFIER_EXT) {
		throw nb::value_error("Incorrect packet type for extended identifier.");
	}
	const uint16_t identifier = hdr->header.identifier;
	definition_identifier = hdr->definition_identifier;
	total_count = hdr->total_count;
	start_ordinal = hdr->start_ordinal;

	// Record the completeness signal for the deferred path keyed by identifier.
	auto& p = pending[identifier];
	p.total_count = total_count;
	p.have_total_count = true;
	p.definition_identifier = definition_identifier;

	// Walk the items inline (mirrors stream_packet_parse_identifier_ext): parse
	// only fully-present items, tolerating a trailing item truncated by a capped
	// transport. Each item merges into learned_descriptors, overwriting on
	// re-seeing an index, and is recorded at its ordinal so a ParameterSet can be
	// built straight from the self-describing stream.
	const uint8_t* ptr = buffer + sizeof(streaming_identifier_ext_packet_header_t);
	const uint8_t* end = buffer + buffer_size;
	uint16_t ordinal = start_ordinal;
	while (ptr + sizeof(streaming_identifier_ext_item_header_t) <= end) {
		streaming_identifier_ext_item_header_t item;
		std::memcpy(&item, ptr, sizeof(item));
		const uint8_t* name = ptr + sizeof(item);
		if (name + item.name_len > end) {
			break;  // Trailing item truncated; stop.
		}
		std::string dotted_name(reinterpret_cast<const char*>(name), item.name_len);
		learned_descriptors[item.index] = descriptor_for_ext_item(item.index, dotted_name, item.element_type, item.type_id);
		p.ext_items[ordinal] = ExtItem{item.index, item.size};
		ordinal++;
		ptr = name + item.name_len;
	}

	// Try to build directly from the self-describing schema (no 0x01 needed); if
	// a 0x01 IDENTIFIER was buffered, that late-arriving schema may now make it
	// resolvable too.
	if (!try_build_from_ext(identifier)) {
		try_resolve_pending(identifier);
	}
}

// ODIN_element_type_t value for a CUSTOM (composite) type on the wire.
static constexpr uint8_t ELEMENT_TYPE_CUSTOM = 13;

std::shared_ptr<ParameterDescriptor> OdinStreamDecoder::descriptor_for_ext_item(uint32_t index, const std::string& name, uint8_t element_type,
                                                                                uint16_t type_id) {
	if (element_type == ELEMENT_TYPE_CUSTOM) {
		// Remember the type_id so the descriptor can be refreshed if the matching
		// 0x04 TYPE_DESCRIPTOR arrives later.
		index_type_id[index] = type_id;
		auto it = learned_types.find(type_id);
		if (it != learned_types.end()) {
			return std::make_shared<ParameterDescriptor>(index, name, it->second);
		}
		// Type not learned yet: fall back to an unknown-typed column. It will be
		// rebuilt once the 0x04 arrives (process_type_descriptor refreshes it).
		return ParameterDescriptor::unknown_type(index, name);
	}
	auto primitive = primitive_for_element_type(element_type);
	return std::make_shared<ParameterDescriptor>(index, name, primitive);
}

void OdinStreamDecoder::process_type_descriptor(nb::bytes data) {
	const uint8_t* buffer = reinterpret_cast<const uint8_t*>(data.c_str());
	const size_t buffer_size = data.size();

	// Accumulator shared between the type and field callbacks. The C parser invokes
	// type_cb once per type, then field_cb for each of that type's fields, so the
	// "current" type is always the last one seen.
	struct Ctx {
		std::unordered_map<uint16_t, std::shared_ptr<CompositeTypeDescriptor>>* learned_types;
		uint16_t current_type_id = 0;
		std::shared_ptr<CompositeTypeDescriptor> current;
	} ctx{&learned_types};

	auto type_cb = [](void* c, uint16_t type_id, uint16_t /*size*/, uint8_t /*field_count*/, const char* name, uint8_t name_len) {
		Ctx* ctx = static_cast<Ctx*>(c);
		ctx->current_type_id = type_id;
		ctx->current = std::make_shared<CompositeTypeDescriptor>();
		(void)name;
		(void)name_len;
		// A type is atomic within a chunk, so overwrite/insert as soon as it starts;
		// fields append into the same descriptor before the next type begins.
		(*ctx->learned_types)[type_id] = ctx->current;
	};
	auto field_cb = [](void* c, uint8_t element_type, uint16_t /*size*/, uint16_t /*offset*/, const char* name, uint8_t name_len) {
		Ctx* ctx = static_cast<Ctx*>(c);
		if (!ctx->current) {
			return;
		}
		std::string field_name(name, name_len);
		// Fields are wire-described as primitives; map element_type to a primitive.
		// (Nested CUSTOM fields aren't expressible on the field wire today.)
		auto primitive = primitive_for_element_type(element_type);
		ctx->current->add_member(field_name, primitive);
	};

	stream_packet_status_t st = stream_packet_parse_type_descriptor(buffer, buffer_size, nullptr, nullptr, nullptr, type_cb, field_cb, &ctx);
	if (st != STREAM_PACKET_SUCCESS) {
		throw nb::value_error("Failed to parse type-descriptor packet.");
	}

	// Refresh any CUSTOM learned_descriptors that referenced a type_id we just
	// learned, so a parameter set built from those descriptors picks up the real
	// struct layout instead of the unknown fallback.
	for (const auto& [index, type_id] : index_type_id) {
		auto tit = learned_types.find(type_id);
		if (tit == learned_types.end()) {
			continue;
		}
		auto dit = learned_descriptors.find(index);
		std::string name = dit != learned_descriptors.end() ? dit->second->get_name() : ("param_" + std::to_string(index));
		learned_descriptors[index] = std::make_shared<ParameterDescriptor>(index, name, tit->second);
	}
}

std::shared_ptr<ParameterDescriptor> OdinStreamDecoder::resolve_descriptor(uint32_t index) {
	if (auto db = parameter_map->find_by_id(index)) {
		return db.value();
	}
	auto it = learned_descriptors.find(index);
	if (it != learned_descriptors.end()) {
		return it->second;
	}
	return nullptr;
}

std::shared_ptr<ParameterSet> OdinStreamDecoder::build_set_from_identifier(nb::bytes data) {
	return ParameterSet::from_identifier_data(data, parameter_map, &learned_descriptors);
}

void OdinStreamDecoder::try_resolve_pending(uint16_t identifier) {
	auto pit = pending.find(identifier);
	if (pit == pending.end() || !pit->second.have_identifier) {
		return;  // No buffered IDENTIFIER to (re)build from.
	}
	PendingIdentifier& p = pit->second;

	// Parse the buffered IDENTIFIER to know exactly which indices the schema
	// needs, and how many. Only proceed once every index is resolvable from the
	// OdinDB or the wire-learned schema; total_count is the completeness gate
	// (we don't build a partial schema that would freeze stale columns).
	stream_parameter_set_t* pset = stream_packet_parse_identifier(reinterpret_cast<const uint8_t*>(p.identifier_bytes.c_str()), p.identifier_bytes.size());
	if (!pset) {
		return;  // Can't parse yet; leave buffered.
	}

	bool all_resolvable = true;
	size_t count = pset->parameter_count;
	for (size_t i = 0; i < count; ++i) {
		if (!resolve_descriptor(pset->parameters[i].index)) {
			all_resolvable = false;
			break;
		}
	}
	stream_parameter_set_destroy(pset);

	// If we know the expected total and haven't learned that many yet, wait — the
	// schema is still arriving in chunks.
	if (p.have_total_count && p.total_count > 0 && learned_descriptors.size() < p.total_count) {
		// We may still build if the OdinDB resolves everything; otherwise defer.
		if (!all_resolvable) {
			return;
		}
	}
	if (!all_resolvable) {
		return;
	}

	// Build (or rebuild) the ParameterSet now that the schema is fully resolvable.
	std::shared_ptr<ParameterSet> set = build_set_from_identifier(p.identifier_bytes);
	parameter_sets_map.insert_or_assign(set->get_hash(), set);

	// Replay any buffered DATA packets against the freshly-built set.
	for (auto& data_bytes : p.data_packets) {
		set->parse_data_packet(data_bytes);
		statistics.received_data_packets++;
	}
	p.data_packets.clear();
}

bool OdinStreamDecoder::try_build_from_ext(uint16_t identifier) {
	// Already built from a prior packet? Then we're done (replay still handled by
	// the DATA path).
	if (parameter_sets_map.find(identifier) != parameter_sets_map.end()) {
		return true;
	}

	auto pit = pending.find(identifier);
	if (pit == pending.end()) {
		return false;
	}
	PendingIdentifier& p = pit->second;

	// Need the completeness signal and a contiguous ordinal run [0, total_count).
	if (!p.have_total_count || p.total_count == 0) {
		return false;
	}
	if (p.ext_items.size() < p.total_count) {
		return false;  // Still paging in chunks; wait for the full picture.
	}
	for (uint16_t ord = 0; ord < p.total_count; ++ord) {
		if (p.ext_items.find(ord) == p.ext_items.end()) {
			return false;  // A gap in the ordinal coverage; not complete yet.
		}
	}

	// If any item is a CUSTOM type whose 0x04 TYPE_DESCRIPTOR hasn't arrived yet,
	// defer: building now would freeze an unknown-typed (null) column for the
	// struct. The DATA stays buffered and we retry once the 0x04 lands.
	for (uint16_t ord = 0; ord < p.total_count; ++ord) {
		const ExtItem& ei = p.ext_items.at(ord);
		auto tit = index_type_id.find(ei.index);
		if (tit != index_type_id.end() && learned_types.find(tit->second) == learned_types.end()) {
			return false;
		}
	}

	// Build the ordered ParameterSet straight from the learned schema.
	auto set = std::make_shared<ParameterSet>(identifier, p.definition_identifier, parameter_map);
	for (uint16_t ord = 0; ord < p.total_count; ++ord) {
		const ExtItem& ei = p.ext_items.at(ord);
		std::shared_ptr<ParameterDescriptor> descriptor = resolve_descriptor(ei.index);
		if (!descriptor) {
			descriptor = ParameterDescriptor::unknown_type(ei.index, "param_" + std::to_string(ei.index));
		}
		set->add(std::make_shared<FixedSizeParameter>(ei.index, ei.size, descriptor));
	}
	parameter_sets_map.insert_or_assign(set->get_hash(), set);

	// Replay any buffered DATA against the freshly-built set.
	for (auto& data_bytes : p.data_packets) {
		set->parse_data_packet(data_bytes);
		statistics.received_data_packets++;
	}
	p.data_packets.clear();
	return true;
}

void OdinStreamDecoder::process_packets(nb::list bytes_list) {
	for (const auto& handle : bytes_list) {
		nb::bytes item = nb::cast<nb::bytes>(handle);

		if (item.size() < sizeof(streaming_data_packet_header_t)) {
			// Note: ext-identifier headers are larger than this minimum, so a
			// short-but-valid header would only be a tiny plain header; treat
			// undersized input uniformly as an error like before.
			if (item.size() < sizeof(stream_packet_header_t)) {
				statistics.other_errors++;
				if (!silent_errors) {
					throw nb::value_error("Input data too small to contain packet header.");
				}
				continue;
			}
		}

		// Check header type
		const streaming_data_packet_header_t* header = reinterpret_cast<const streaming_data_packet_header_t*>(item.c_str());

		// Check if we have a associated parameter set
		auto it = parameter_sets_map.find(header->header.identifier);

		try {
			switch (header->header.type) {
				// --- Process Identifier Packet ---
				case STREAM_STREAM_PACKET_TYPE_IDENTIFIER:
					statistics.received_identifier_packets++;

					// If it exists, do nothing (already-built schema).
					if (it != parameter_sets_map.end()) {
						ParameterSet& param_set = *it->second;
						param_set.parse_identifier_data(item);
					} else {
						// Decide whether the schema is resolvable up front. With an
						// OdinDB that knows every parameter (the original path) this
						// is true and we build immediately, behaving exactly as
						// before. Otherwise we DEFER: buffer the IDENTIFIER and only
						// build once the wire schema (0x03 chunks) has filled in.
						const uint16_t identifier = header->header.identifier;
						bool resolvable_now = false;
						stream_parameter_set_t* pset =
							stream_packet_parse_identifier(reinterpret_cast<const uint8_t*>(item.c_str()), item.size());
						if (pset) {
							resolvable_now = true;
							for (size_t i = 0; i < pset->parameter_count; ++i) {
								if (!resolve_descriptor(pset->parameters[i].index)) {
									resolvable_now = false;
									break;
								}
							}
							stream_parameter_set_destroy(pset);
						}

						// If we know the expected total from a prior 0x03 header and
						// haven't learned all of it yet, defer even if currently
						// resolvable from learned — the picture isn't complete.
						auto pend_it = pending.find(identifier);
						bool waiting_for_full_schema = pend_it != pending.end() && pend_it->second.have_total_count &&
						                               pend_it->second.total_count > 0 && learned_descriptors.size() < pend_it->second.total_count;

						if (resolvable_now && !waiting_for_full_schema) {
							std::shared_ptr<ParameterSet> new_set = build_set_from_identifier(item);
							parameter_sets_map.insert_or_assign(new_set->get_hash(), new_set);
						} else {
							// Buffer the IDENTIFIER for later (re)build.
							PendingIdentifier& p = pending[identifier];
							p.identifier_bytes = item;
							p.have_identifier = true;
							try_resolve_pending(identifier);
						}
					}
					break;

				// --- Process Extended (self-describing) Identifier Packet ---
				case STREAM_STREAM_PACKET_TYPE_IDENTIFIER_EXT:
					statistics.received_identifier_ext_packets++;
					process_identifier_ext(item);
					break;

				// --- Process Type-Descriptor Packet (CUSTOM struct layouts) ---
				case STREAM_STREAM_PACKET_TYPE_TYPE_DESCRIPTOR:
					statistics.received_type_descriptor_packets++;
					process_type_descriptor(item);
					// A newly-learned struct layout may now make a deferred (pending)
					// identifier fully resolvable — retry each one. Snapshot the keys
					// because try_build_from_ext may mutate `pending`.
					{
						std::vector<uint16_t> ids;
						ids.reserve(pending.size());
						for (const auto& kv : pending) {
							ids.push_back(kv.first);
						}
						for (uint16_t id : ids) {
							if (!try_build_from_ext(id)) {
								try_resolve_pending(id);
							}
						}
					}
					break;

				// --- Process Data Packet ---
				case STREAM_STREAM_PACKET_TYPE_DATA:

					if (it != parameter_sets_map.end()) {
						statistics.received_data_packets++;
						ParameterSet& param_set = *it->second;
						param_set.parse_data_packet(item);
					} else {
						// No built schema yet. If we have (or expect) one pending,
						// buffer this DATA so it can be replayed once the schema
						// resolves; otherwise it's genuinely unresolved.
						const uint16_t identifier = header->header.identifier;
						auto pend_it = pending.find(identifier);
						if (pend_it != pending.end() && (pend_it->second.have_identifier || pend_it->second.have_total_count)) {
							pend_it->second.data_packets.push_back(item);
							// Attempt resolution in case this DATA followed the
							// completing schema chunk in the same batch — either via
							// the self-describing 0x03 ordering or a buffered 0x01.
							if (!try_build_from_ext(identifier)) {
								try_resolve_pending(identifier);
							}
						} else {
							statistics.received_unresolved_data_packets++;
						}
					}

					break;

				// --- Process Event Packet ---
				case STREAM_STREAM_PACKET_TYPE_EVENT:
					statistics.received_events_packets++;
					break;

				default:
					statistics.received_other_packets++;
					break;
			}
		} catch (const std::exception& e) {
			switch (header->header.type) {
				case STREAM_STREAM_PACKET_TYPE_IDENTIFIER:
				case STREAM_STREAM_PACKET_TYPE_IDENTIFIER_EXT:
				case STREAM_STREAM_PACKET_TYPE_TYPE_DESCRIPTOR:
					statistics.identifier_decoding_errors++;
					break;
				case STREAM_STREAM_PACKET_TYPE_DATA:
					statistics.data_decoding_errors++;
					break;
				case STREAM_STREAM_PACKET_TYPE_EVENT:
					statistics.event_decoding_errors++;
					break;
				default:
					statistics.other_errors++;
					break;
			}

			if (!silent_errors) {
				// Reraise the exception
				// This will be caught by the Python layer
				// and can be handled there
				throw nb::value_error((std::string("Error processing packet: ") + e.what()).c_str());
			}
		}
	}
}

void OdinStreamDecoder::clear_parameter_sets() {
	parameter_sets_map.clear();
	pending.clear();
	printf("Cleared all stored ParameterSets.\n");
}

// Note: learned_descriptors / learned_types / index_type_id are intentionally
// NOT cleared here — the wire-learned schema is meant to persist so a re-sent
// IDENTIFIER can rebuild against it without re-receiving the 0x03/0x04 stream.

std::shared_ptr<ParameterMapDescriptor> OdinStreamDecoder::get_learned_descriptors() const {
	auto map = std::make_shared<ParameterMapDescriptor>();
	for (const auto& pair : learned_descriptors) {
		map->add_parameter(pair.second);
	}
	return map;
}

std::shared_ptr<ParameterSet> OdinStreamDecoder::get_parameter_set(uint16_t identifier) {
	auto it = parameter_sets_map.find(identifier);
	if (it != parameter_sets_map.end()) {
		return it->second;
	} else {
		throw nb::key_error("ParameterSet with the given identifier not found.");
	}
}

std::vector<uint16_t> OdinStreamDecoder::get_parameter_set_identifiers() const {
	std::vector<uint16_t> identifiers;
	identifiers.reserve(parameter_sets_map.size());
	for (const auto& pair : parameter_sets_map) {
		identifiers.push_back(pair.first);
	}
	return identifiers;
}

void init_stream_processor(nb::module_& m) {
	using namespace nb::literals;

	nb::class_<OdinStreamDecoder>(m, "StreamProcessor")
		.def(nb::init<std::shared_ptr<ParameterMapDescriptor>, bool>(), "parameter_map"_a, "silent_errors"_a = false,
	         "Constructor for the StreamProcessor. Initializes with the parameter map.")
		.def("process_packets", &OdinStreamDecoder::process_packets, "bytes_list"_a, "Process a list of bytes.")
		.def("get_parameter_set", &OdinStreamDecoder::get_parameter_set, "identifier"_a, "Get a ParameterSet by its identifier.")
		.def("clear_parameter_sets", &OdinStreamDecoder::clear_parameter_sets, "Clear all stored ParameterSets.")
		.def("get_parameter_set_identifiers", &OdinStreamDecoder::get_parameter_set_identifiers, "Get a list of all stored ParameterSet identifiers.")
		.def("get_learned_descriptors", &OdinStreamDecoder::get_learned_descriptors,
	         "Get the schema learned from self-describing (0x03) IDENTIFIER_EXT packets as a ParameterMapDescriptor.")
		.def_ro("statistics", &OdinStreamDecoder::statistics, "Get the statistics of the stream processor.");

	nb::class_<OdinStreamStatistics>(m, "OdinStreamStatistics")
		.def(nb::init<>())
		.def_ro("identifier_decoding_errors", &OdinStreamStatistics::identifier_decoding_errors)
		.def_ro("data_decoding_errors", &OdinStreamStatistics::data_decoding_errors)
		.def_ro("event_decoding_errors", &OdinStreamStatistics::event_decoding_errors)
		.def_ro("other_errors", &OdinStreamStatistics::other_errors)
		.def_ro("received_events_packets", &OdinStreamStatistics::received_events_packets)
		.def_ro("received_identifier_packets", &OdinStreamStatistics::received_identifier_packets)
		.def_ro("received_identifier_ext_packets", &OdinStreamStatistics::received_identifier_ext_packets)
		.def_ro("received_data_packets", &OdinStreamStatistics::received_data_packets)
		.def_ro("received_unresolved_data_packets", &OdinStreamStatistics::received_unresolved_data_packets)
		.def_ro("received_type_descriptor_packets", &OdinStreamStatistics::received_type_descriptor_packets)
		.def_ro("received_other_packets", &OdinStreamStatistics::received_other_packets)
		.def("__repr__", &OdinStreamStatistics::repr);
}
