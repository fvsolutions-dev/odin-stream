#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind_pyarrow/pyarrow_import.h>
#include <nanobind_pyarrow/table.h>

#include <map>
#include <unordered_map>
#include <vector>

#include "descriptor/parameter_descriptor.h"
#include "descriptor/type_descriptor.h"
#include "parameterset.h"

extern "C" {
#include "embedded_odin_stream/statistics.h"
}

class OdinStreamStatistics {
   public:
	uint32_t identifier_decoding_errors = 0;
	uint32_t data_decoding_errors = 0;
	uint32_t event_decoding_errors = 0;
	uint32_t other_errors = 0;

	uint32_t received_events_packets = 0;            // Total valid events received
	uint32_t received_identifier_packets = 0;        // Total valid identifiers received
	uint32_t received_identifier_ext_packets = 0;    // Total valid extended (self-describing) identifiers received
	uint32_t received_data_packets = 0;              // Total data packets received
	uint32_t received_unresolved_data_packets = 0;   // Total unresolved packets received
	uint32_t received_type_descriptor_packets = 0;   // Total type-descriptor (0x04) packets received
	uint32_t received_other_packets = 0;             // Total other packets received

	// repr
	std::string repr() const {
		return "OdinStreamStatistics(received{events=" + std::to_string(received_events_packets) +
		       ", identifiers=" + std::to_string(received_identifier_packets) +
		       ", identifiers_ext=" + std::to_string(received_identifier_ext_packets) +
		       ", type_descriptors=" + std::to_string(received_type_descriptor_packets) +
		       ", data=" + std::to_string(received_data_packets) +
		       ", unresolved_data=" + std::to_string(received_unresolved_data_packets) + ", other=" + std::to_string(received_other_packets) +
		       "}, errors{identifier=" + std::to_string(identifier_decoding_errors) + ", data=" + std::to_string(data_decoding_errors) +
		       ", event=" + std::to_string(event_decoding_errors) + ", other=" + std::to_string(other_errors) + "})";
	}
};

class OdinStreamDecoder {
   private:
	std::unordered_map<uint16_t, std::shared_ptr<ParameterSet>> parameter_sets_map;
	std::shared_ptr<ParameterMapDescriptor> parameter_map;
	bool silent_errors = false;

	// Schema learned from self-describing (0x03) IDENTIFIER_EXT chunks, keyed by
	// parameter index. Chunks accumulate across packets; re-seeing an index
	// overwrites. These provide wire-sourced names + types when no OdinDB is
	// available (or when the OdinDB lacks a parameter).
	std::unordered_map<uint32_t, std::shared_ptr<ParameterDescriptor>> learned_descriptors;

	// CUSTOM struct layouts learned from TYPE_DESCRIPTOR (0x04) packets, keyed by
	// type_id. Built straight from the wire fields (each field's name +
	// PrimitiveTypeDescriptor + offset), so composites decode into per-field Arrow
	// columns with no OdinDB. Accumulates across chunks; re-seeing a type_id
	// overwrites.
	std::unordered_map<uint16_t, std::shared_ptr<CompositeTypeDescriptor>> learned_types;

	// For each 0x03 ext-item index whose element_type is CUSTOM (13), the type_id
	// it referenced. Lets a CUSTOM parameter resolve its struct layout from
	// `learned_types` once the matching 0x04 has arrived, regardless of arrival
	// order. 0 = not a CUSTOM index.
	std::unordered_map<uint32_t, uint16_t> index_type_id;

	// Per-identifier state for the deferred path: when a 0x01/0x02 packet arrives
	// but the schema isn't yet resolvable, we buffer the raw bytes here and only
	// build the ParameterSet once the schema is resolvable. total_count (from the
	// ext header) is the completeness signal. The 0x03 chunks themselves also
	// page in an ordinal->item ordering so a ParameterSet can be built straight
	// from the self-describing stream with no 0x01 IDENTIFIER at all.
	struct ExtItem {
		uint32_t index = 0;
		uint16_t size = 0;
	};
	struct PendingIdentifier {
		nanobind::bytes identifier_bytes;        // last-seen raw 0x01 IDENTIFIER packet (if any)
		bool have_identifier = false;
		std::vector<nanobind::bytes> data_packets;  // buffered raw 0x02 DATA packets awaiting a schema
		uint16_t total_count = 0;                    // expected parameter count (0 = unknown)
		bool have_total_count = false;
		uint32_t definition_identifier = 0;
		std::map<uint16_t, ExtItem> ext_items;       // ordinal -> (index,size) learned from 0x03 chunks
	};
	std::unordered_map<uint16_t, PendingIdentifier> pending;

	// Build a ParameterSet directly from the ordinal-ordered 0x03 schema once all
	// total_count items have been learned for `identifier`, then replay buffered
	// DATA. Returns true if it built (or had already built) the set.
	bool try_build_from_ext(uint16_t identifier);

	// Try to (re)build the ParameterSet for `identifier` from a buffered 0x01
	// IDENTIFIER once its schema is resolvable, then replay any buffered DATA.
	void try_resolve_pending(uint16_t identifier);

	// Resolve a parameter index to a descriptor, preferring the OdinDB map, then
	// wire-learned descriptors. Returns nullptr when neither knows it.
	std::shared_ptr<ParameterDescriptor> resolve_descriptor(uint32_t index);

	// Build a ParameterSet from a raw 0x01 IDENTIFIER, using OdinDB + learned
	// descriptors, falling back to unknown_type for still-missing indices.
	std::shared_ptr<ParameterSet> build_set_from_identifier(nanobind::bytes data);

	// Process a 0x03 IDENTIFIER_EXT chunk: merge its items into learned_descriptors.
	void process_identifier_ext(nanobind::bytes data);

	// Process a 0x04 TYPE_DESCRIPTOR chunk: build CompositeTypeDescriptors from the
	// wire fields and merge them into learned_types, then refresh any CUSTOM
	// learned_descriptors that referenced the newly-learned type_ids.
	void process_type_descriptor(nanobind::bytes data);

	// Build the ParameterDescriptor for `index` from a wire-learned ext item:
	// resolves a CUSTOM element_type against learned_types[type_id] (falling back
	// to unknown_type until the 0x04 arrives), else a primitive by element_type.
	std::shared_ptr<ParameterDescriptor> descriptor_for_ext_item(uint32_t index, const std::string& name, uint8_t element_type,
	                                                             uint16_t type_id);

   public:
	OdinStreamStatistics statistics = {0};

	OdinStreamDecoder(std::shared_ptr<ParameterMapDescriptor> parameter_map, bool silent_errors);

	void process_packets(nanobind::list packets);
	std::shared_ptr<ParameterSet> get_parameter_set(uint16_t identifier);
	std::vector<uint16_t> get_parameter_set_identifiers() const;

	// Expose the wire-learned schema so callers can inspect names/types decoded
	// purely from the stream (no OdinDB).
	std::shared_ptr<ParameterMapDescriptor> get_learned_descriptors() const;

	void clear_parameter_sets();

	std::string stats_str() const {
		return "OdinStreamStatistics(identifier_decoding_errors=" + std::to_string(statistics.identifier_decoding_errors) +
		       ", data_decoding_errors=" + std::to_string(statistics.data_decoding_errors) +
		       ", event_decoding_errors=" + std::to_string(statistics.event_decoding_errors) + ", other_errors=" + std::to_string(statistics.other_errors) +
		       ", received_events_packets=" + std::to_string(statistics.received_events_packets) +
		       ", received_identifier_packets=" + std::to_string(statistics.received_identifier_packets) +
		       ", received_identifier_ext_packets=" + std::to_string(statistics.received_identifier_ext_packets) +
		       ", received_data_packets=" + std::to_string(statistics.received_data_packets) +
		       ", received_unresolved_data_packets=" + std::to_string(statistics.received_unresolved_data_packets) +
		       ", received_type_descriptor_packets=" + std::to_string(statistics.received_type_descriptor_packets) +
		       ", received_other_packets=" + std::to_string(statistics.received_other_packets) + ")";
	}
};

void init_stream_processor(nanobind::module_& m);
