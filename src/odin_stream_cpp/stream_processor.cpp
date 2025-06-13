#include "stream_processor.h"

extern "C" {
#include "embedded_odin_stream/stream_packet.h"
}

namespace nb = nanobind;

OdinStreamDecoder::OdinStreamDecoder(std::shared_ptr<ParameterMapDescriptor> parameter_map, bool silent_errors)
	: parameter_map(parameter_map), silent_errors(silent_errors) {}

void OdinStreamDecoder::process_packets(nb::list bytes_list) {
	for (const auto& handle : bytes_list) {
		nb::bytes item = nb::cast<nb::bytes>(handle);

		if (item.size() < sizeof(streaming_data_packet_header_t)) {
			statistics.other_errors++;
			if (!silent_errors) {
				throw nb::value_error("Input data too small to contain data packet header.");
			}
			continue;
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

					// If it exists, do nothing
					if (it != parameter_sets_map.end()) {
						ParameterSet& param_set = *it->second;
						param_set.parse_identifier_data(item);
					} else {
						// If it doesn't exist, create a new one
						std::shared_ptr<ParameterSet> new_set = ParameterSet::from_identifier_data(item, parameter_map);

						// Insert the new parameter set into the map
						parameter_sets_map.insert_or_assign(new_set->get_hash(), new_set);
					}
					break;

				// --- Process Data Packet ---
				case STREAM_STREAM_PACKET_TYPE_DATA:

					if (it == parameter_sets_map.end()) {
						statistics.received_unresolved_data_packets++;
					} else {
						statistics.received_data_packets++;
						ParameterSet& param_set = *it->second;
						param_set.parse_data_packet(item);
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
	printf("Cleared all stored ParameterSets.\n");
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
		.def_ro("statistics", &OdinStreamDecoder::statistics, "Get the statistics of the stream processor.");

	nb::class_<OdinStreamStatistics>(m, "OdinStreamStatistics")
		.def(nb::init<>())
		.def_ro("identifier_decoding_errors", &OdinStreamStatistics::identifier_decoding_errors)
		.def_ro("data_decoding_errors", &OdinStreamStatistics::data_decoding_errors)
		.def_ro("event_decoding_errors", &OdinStreamStatistics::event_decoding_errors)
		.def_ro("other_errors", &OdinStreamStatistics::other_errors)
		.def_ro("received_events_packets", &OdinStreamStatistics::received_events_packets)
		.def_ro("received_identifier_packets", &OdinStreamStatistics::received_identifier_packets)
		.def_ro("received_data_packets", &OdinStreamStatistics::received_data_packets)
		.def_ro("received_unresolved_data_packets", &OdinStreamStatistics::received_unresolved_data_packets)
		.def_ro("received_other_packets", &OdinStreamStatistics::received_other_packets)
		.def("__repr__", &OdinStreamStatistics::repr);


	}