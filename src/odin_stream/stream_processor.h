#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind_pyarrow/pyarrow_import.h>
#include <nanobind_pyarrow/table.h>

#include "parameterset.h"
#include "types/primitive.h"
#include "types/struct.h"
#include "types/typedescriptors.h"


class StreamProcessor {
   private:
	std::unordered_map<uint16_t, std::shared_ptr<ParameterSet>> parameter_sets_map;
	std::shared_ptr<TypeDescriptors> type_descriptors_map;

   public:
	StreamProcessor(std::shared_ptr<TypeDescriptors> type_descriptors);
    
	void process_bytes_list(nanobind::list bytes_list);

	size_t get_parameter_set_count() const;
	std::shared_ptr<ParameterSet> get_parameter_set(uint16_t identifier);
	void clear_parameter_sets();
};

void init_stream_processor(nanobind::module_& m);
