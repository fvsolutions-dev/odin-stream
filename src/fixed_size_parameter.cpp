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


// get_index() Implementation
uint32_t FixedSizeParameter::get_index() const { return index; }

// set_index() Implementation
void FixedSizeParameter::set_index(uint32_t new_index) { index = new_index; }

// add_data() Implementation
// void FixedSizeParameter::add_data(const uint8_t* data, size_t data_size) {
// 	// 1. Validate input data pointer
// 	if (data == nullptr) {
// 		throw std::invalid_argument("Input data pointer cannot be null.");
// 	}

// 	// Check if the size is valid
// 	if (size != data_size) {
// 		throw std::length_error("Invalid data size provided. Expected " + std::to_string(size) + " bytes, but got " + std::to_string(data_size) + " bytes.");
// 	}
// 	// Append the data to the end of the buffer
// 	packets.insert(packets.end(), data, data + size);
// 	num_packets++;
// }

void init_fixed_size_parameter(nb::module_& m) {
	using namespace nanobind::literals;  // Bring the _a literal into scope

	// Bind the FixedSizeParameter class to Python
	nb::class_<FixedSizeParameter>(m, "FixedSizeParameter")
		.def(nb::init<uint32_t, uint16_t, std::shared_ptr<TypeDescriptor>>(), "index"_a, "size"_a, "type_descriptor"_a,
		     "Create a new FixedSizeParameter with the given index, size, and type descriptor.")
		.def("get_index", &FixedSizeParameter::get_index, "Get the index of the parameter.")
		.def("set_index", &FixedSizeParameter::set_index, "Set the index of the parameter.")

		// .def(
		// 	"add_data",
		// 	[](FixedSizeParameter& self, nb::bytes data) {
		// 		// Convert nb::bytes to uint8_t* and size_t
		// 		const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(data.c_str());
		// 		size_t size = data.size();
		// 		self.add_data(data_ptr, size);
		// 	},
		// 	"data"_a, "Add data to the parameter.")
		.def("__repr__", &FixedSizeParameter::repr);
}