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

class StreamProcessor {
   private:
	std::unordered_map<uint16_t, std::shared_ptr<ParameterSet>> parameter_sets_map;
	std::shared_ptr<ParameterMapDescriptor> parameter_map;

   public:
	StreamProcessor(std::shared_ptr<ParameterMapDescriptor> parameter_map);

	void process_bytes_list(nanobind::list bytes_list);

	size_t get_parameter_set_count() const;
	std::shared_ptr<ParameterSet> get_parameter_set(uint16_t identifier);
	std::vector<uint16_t> get_parameter_set_identifiers() const;

	void clear_parameter_sets();
};

void init_stream_processor(nanobind::module_& m);
