#pragma once

#include <nanobind/nanobind.h>
#include <nanobind_pyarrow/table.h>

#include <cstdint>

#include "descriptor/parameter_descriptor.h"
#include "descriptor/type_descriptor.h"
#include "fixed_size_parameter.h"

class ParameterSet {
   private:
	std::shared_ptr<ParameterMapDescriptor> parameter_map;
	std::vector<std::shared_ptr<FixedSizeParameter>> parameters;
	uint16_t parameter_set_identifier;
	uint32_t definition_identifier;
	uint32_t data_size = 0;

   public:
	ParameterSet(uint16_t identifier, uint32_t definition_identifier, std::shared_ptr<ParameterMapDescriptor> parameter_map);
	~ParameterSet();

	void add(std::shared_ptr<FixedSizeParameter> param);
	uint16_t get_hash() const { return parameter_set_identifier; }

	void parse_data_packet(nanobind::bytes data);
	static std::shared_ptr<ParameterSet> from_identifier_data(nanobind::bytes data,std::shared_ptr<ParameterMapDescriptor> parameter_map);
	std::shared_ptr<arrow::Table> flush_to_arrow_table();
};

void init_parameterset(nanobind::module_& m);
