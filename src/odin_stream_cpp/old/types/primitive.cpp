#include "./primitive.h"

#include <nanobind/stl/string.h>

#include <map>
#include <stdexcept>

namespace nb = nanobind;

const std::map<PrimitiveType, size_t> primitive_sizes = {{PrimitiveType::INT8, 1},   {PrimitiveType::UINT8, 1},   {PrimitiveType::BOOL, 1},  // Common practice
                                                         {PrimitiveType::INT16, 2},  {PrimitiveType::UINT16, 2},  {PrimitiveType::INT32, 4},
                                                         {PrimitiveType::UINT32, 4}, {PrimitiveType::FLOAT32, 4}, {PrimitiveType::INT64, 8},
                                                         {PrimitiveType::UINT64, 8}, {PrimitiveType::FLOAT64, 8}, {PrimitiveType::CHAR, 1},};


PrimitiveTypeDescriptor::PrimitiveTypeDescriptor(PrimitiveType t) : type(t) {
	auto it = primitive_sizes.find(t);
	if (it == primitive_sizes.end()) {
		throw std::runtime_error("Unknown primitive type in C++.");
	}
	size = it->second;
}

void init_primitive(nb::module_& m) {
	using namespace nb::literals;

	nb::class_<PrimitiveTypeDescriptor>(m, "PrimitiveTypeDescriptor", "Descriptor for primitive types")
		.def(nb::init<PrimitiveType>(), "type"_a)
		.def("get_size", &PrimitiveTypeDescriptor::get_size, "Get size of the type in bytes")
		.def("get_primitive_type", &PrimitiveTypeDescriptor::get_primitive_type, "Get the primitive type")
		.def("__repr__", &PrimitiveTypeDescriptor::repr, "Get string representation of the type descriptor");

	// Enum
	nb::enum_<PrimitiveType>(m, "PrimitiveType", "Enum for primitive types")
		.value("INT8", PrimitiveType::INT8)
		.value("UINT8", PrimitiveType::UINT8)
		.value("BOOL", PrimitiveType::BOOL)
		.value("INT16", PrimitiveType::INT16)
		.value("UINT16", PrimitiveType::UINT16)
		.value("INT32", PrimitiveType::INT32)
		.value("UINT32", PrimitiveType::UINT32)
		.value("FLOAT32", PrimitiveType::FLOAT32)
		.value("INT64", PrimitiveType::INT64)
		.value("UINT64", PrimitiveType::UINT64)
		.value("FLOAT64", PrimitiveType::FLOAT64)
		.value("CHAR", PrimitiveType::CHAR)
        .export_values();

}