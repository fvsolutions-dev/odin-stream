#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind_pyarrow/pyarrow_import.h>
#include <nanobind_pyarrow/table.h>

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

	uint32_t received_events_packets = 0;           // Total valid events received
	uint32_t received_identifier_packets = 0;       // Total valid identifiers received
	uint32_t received_data_packets = 0;             // Total data packets received
	uint32_t received_unresolved_data_packets = 0;  // Total unresolved packets received
	uint32_t received_other_packets = 0;            // Total other packets received

	// repr
	std::string repr() const {
		return "OdinStreamStatistics(received{events=" + std::to_string(received_events_packets) +
		       ", identifiers=" + std::to_string(received_identifier_packets) + ", data=" + std::to_string(received_data_packets) +
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

   public:
	OdinStreamStatistics statistics = {0};

	OdinStreamDecoder(std::shared_ptr<ParameterMapDescriptor> parameter_map, bool silent_errors);

	void process_packets(nanobind::list packets);
	std::shared_ptr<ParameterSet> get_parameter_set(uint16_t identifier);
	std::vector<uint16_t> get_parameter_set_identifiers() const;

	void clear_parameter_sets();

	std::string stats_str() const {
		return "OdinStreamStatistics(identifier_decoding_errors=" + std::to_string(statistics.identifier_decoding_errors) +
		       ", data_decoding_errors=" + std::to_string(statistics.data_decoding_errors) +
		       ", event_decoding_errors=" + std::to_string(statistics.event_decoding_errors) + ", other_errors=" + std::to_string(statistics.other_errors) +
		       ", received_events_packets=" + std::to_string(statistics.received_events_packets) +
		       ", received_identifier_packets=" + std::to_string(statistics.received_identifier_packets) +
		       ", received_data_packets=" + std::to_string(statistics.received_data_packets) +
		       ", received_unresolved_data_packets=" + std::to_string(statistics.received_unresolved_data_packets) +
		       ", received_other_packets=" + std::to_string(statistics.received_other_packets) + ")";
	}
};

void init_stream_processor(nanobind::module_& m);
