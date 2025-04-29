#pragma once

#include <nanobind/nanobind.h>
#include <nanobind_pyarrow/table.h>

#include <cstdint>

#include "descriptor/parameter_descriptor.h"
#include "descriptor/type_descriptor.h"
#include "fixed_size_parameter.h"

class ParametersetStatistics {
   public:
	uint32_t decoding_errors;     // Number of decoding errors
	uint32_t identifier_packets;  // Number of validated data packets
	uint32_t data_packets;        // Number of validated data packets

	// uint32_t identifier_decoding_errors;
	// uint32_t data_decoding_errors;
	// uint32_t event_decoding_errors;
	// uint32_t other_errors;

	// uint32_t received_events_packets;           // Total valid events received
	// uint32_t received_identifier_packets;       // Total valid identifiers received
	// uint32_t received_data_packets;             // Total data packets received
	// uint32_t received_unresolved_data_packets;  // Total unresolved packets received
	// uint32_t received_other_packets;            // Total other packets received

	// repr
	// std::string repr() const {
	//	 return "OdinStreamStatistics(received{events=" + std::to_string(received_events_packets) +
	//			", identifiers=" + std::to_string(received_identifier_packets) +
	//			", data=" + std::to_string(received_data_packets) +
	//			", unresolved_data=" + std::to_string(received_unresolved_data_packets) +
	//			", other=" + std::to_string(received_other_packets) +
	//			"}, errors{identifier=" + std::to_string(identifier_decoding_errors) +
	//			", data=" + std::to_string(data_decoding_errors) +
	//			", event=" + std::to_string(event_decoding_errors) +
	//			", other=" + std::to_string(other_errors) + "})";
	// }
};

class ParameterSet {
   private:
	std::shared_ptr<ParameterMapDescriptor> parameter_map;
	std::vector<std::shared_ptr<FixedSizeParameter>> parameters;

	// Vector to store metadata
	std::vector<uint32_t> timestamp;
	std::vector<uint16_t> sequence_number;

	uint16_t parameter_set_identifier;
	uint32_t definition_identifier;
	uint32_t data_size = 0;

   public:
	ParametersetStatistics statistics;

	ParameterSet(uint16_t identifier, uint32_t definition_identifier, std::shared_ptr<ParameterMapDescriptor> parameter_map);
	~ParameterSet();

	void parse_data_packet(nanobind::bytes data);
	void parse_identifier_data(nanobind::bytes data);

	void add(std::shared_ptr<FixedSizeParameter> param);
	uint16_t get_hash() const { return parameter_set_identifier; }

	static std::shared_ptr<ParameterSet> from_identifier_data(nanobind::bytes data, std::shared_ptr<ParameterMapDescriptor> parameter_map);
	
	std::shared_ptr<arrow::Table> flush_to_arrow_table();
};

void init_parameterset(nanobind::module_& m);
