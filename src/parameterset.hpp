// odin_stream_wrapper.hpp
#pragma once

#include <nanobind/nanobind.h>

#include <cstdint>
// #include <nanobind/stl/vector.h>

// #include <cstdint>
// #include <string>
// #include <unordered_map>
// #include <vector>
#include <nanobind_pyarrow/table.h>

// #include "enums.hpp"
#include "fixed_size_parameter.hpp"

class ParameterSet {
   private:
	std::vector<FixedSizeParameter> parameters;  // Vector of parameters
	uint16_t parameter_set_identifier;           // Identifier of the parameter set
	uint32_t definition_identifier;              // Identifier for the definition of the parameter set
	uint32_t data_size;                          // Expected size of the data
   public:
	ParameterSet(uint16_t identifier, uint32_t definition_identifier);
	~ParameterSet();

	ParameterSet(const ParameterSet&) = default;
	ParameterSet& operator=(const ParameterSet&) = default;

	ParameterSet(ParameterSet&& other) = default;  // Default move constructor
	ParameterSet& operator=(ParameterSet&& other) = default;

	void add(FixedSizeParameter param);

	uint16_t get_hash() const { return parameter_set_identifier; }

	static ParameterSet from_identifier_data(nanobind::bytes data);
	void parse_data_packet(nanobind::bytes data);
	std::shared_ptr<arrow::Table> flush_to_arrow_table();

	// 	std::string repr() const;
};

void init_parameterset(nanobind::module_& m);
