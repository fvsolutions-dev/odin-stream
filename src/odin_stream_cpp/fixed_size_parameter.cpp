// FixedSizeParameter.cpp

#include "fixed_size_parameter.h"  // Include the header file declaring the class

#include <arrow/api.h>          // Include main Arrow header (includes builders, types, etc.)
#include <arrow/memory_pool.h>  // Specifically for default_memory_pool
#include <nanobind/nanobind.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/shared_ptr.h>  // Make sure to include this for shared_ptr support
#include <nanobind/stl/string.h>
#include <nanobind/stl/unordered_map.h>  // May be needed for binding map access if desired
#include <nanobind/stl/vector.h>
#include <stdint.h>  // For std::string

#include <sstream>    // For std::ostringstream in repr()
#include <stdexcept>  // For throwing exceptions (e.g., std::runtime_error)
#include <string>     // For std::string

namespace nb = nanobind;

void init_fixed_size_parameter(nb::module_& m) {
	using namespace nanobind::literals;  // Bring the _a literal into scope

	nb::class_<FixedSizeParameter>(m, "FixedSizeParameter")
		.def(nb::init<uint32_t, uint16_t, std::shared_ptr<ParameterDescriptor>>(), "index"_a, "size"_a, "parameter"_a,
		     "Create a new FixedSizeParameter with the given index, size, and parmameter.")
		.def("get_index", &FixedSizeParameter::get_index, "Get the index of the parameter.")
		.def("get_size", &FixedSizeParameter::get_size, "Get the size of the parameter.")
		.def("get_parameter", &FixedSizeParameter::get_parameter, "Get the parameter descriptor.")
		.def("__repr__", &FixedSizeParameter::repr);
}