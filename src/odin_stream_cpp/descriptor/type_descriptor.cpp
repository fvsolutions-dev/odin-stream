#include "type_descriptor.h"

std::shared_ptr<PrimitiveTypeDescriptor> PrimitiveTypeDescriptor::get_by_name(const std::string& name) {
	// Create a map of primitive types to their descriptors
	static std::unordered_map<std::string, std::shared_ptr<PrimitiveTypeDescriptor>> primitive_types = {
		{"i8", std::make_shared<PrimitiveTypeDescriptor>("i8", 1, arrow::int8())},
		{"u8", std::make_shared<PrimitiveTypeDescriptor>("u8", 1, arrow::uint8())},
		{"i16", std::make_shared<PrimitiveTypeDescriptor>("i16", 2, arrow::int16())},
		{"u16", std::make_shared<PrimitiveTypeDescriptor>("u16", 2, arrow::uint16())},
		{"i32", std::make_shared<PrimitiveTypeDescriptor>("i32", 4, arrow::int32())},
		{"u32", std::make_shared<PrimitiveTypeDescriptor>("u32", 4, arrow::uint32())},
		{"f32", std::make_shared<PrimitiveTypeDescriptor>("f32", 4, arrow::float32())},
		{"i64", std::make_shared<PrimitiveTypeDescriptor>("i64", 8, arrow::int64())},
		{"u64", std::make_shared<PrimitiveTypeDescriptor>("u64", 8, arrow::uint64())},
		{"f64", std::make_shared<PrimitiveTypeDescriptor>("f64", 8, arrow::float64())},
		{"bool", std::make_shared<PrimitiveTypeDescriptor>("bool", 1, arrow::boolean())},
		{"char", std::make_shared<PrimitiveTypeDescriptor>("char", 1, arrow::utf8())},
		{"unknown", std::make_shared<PrimitiveTypeDescriptor>("null", 0, arrow::null())}};
		
	auto it = primitive_types.find(name);
	if (it != primitive_types.end()) {
		return it->second;
	} else {
		throw std::runtime_error("Unknown type name: " + name);
	}
}

void init_type_descriptor(nanobind::module_& m) {
	using namespace nanobind::literals;
	namespace nb = nanobind;

	nb::class_<PrimitiveTypeDescriptor>(m, "PrimitiveTypeDescriptor")
		.def(nb::init<std::string, size_t, std::shared_ptr<arrow::DataType>>(), "name"_a, "size"_a, "arrow_data_type"_a)
		.def("get_size", &PrimitiveTypeDescriptor::get_size, "Get the size of the primitive type descriptor")
		.def("get_arrow_type", &PrimitiveTypeDescriptor::get_arrow_type, "Get the Arrow data type")
		.def("get_name", &PrimitiveTypeDescriptor::get_name, "Get the name of the primitive type descriptor")
		.def_static("get_by_name", &PrimitiveTypeDescriptor::get_by_name, "name"_a, "Get the primitive type descriptor by name")
		.def("__repr__", &PrimitiveTypeDescriptor::repr, "Get string representation of the primitive type descriptor");

	nb::class_<CompositeTypeDescriptor>(m, "CompositeTypeDescriptor")
		.def(nb::init<>())
		.def("add_member",
	         (void (CompositeTypeDescriptor::*)(const std::string&, std::shared_ptr<PrimitiveTypeDescriptor>))&CompositeTypeDescriptor::add_member, "name"_a,
	         "descriptor"_a)
		.def("add_member",
	         (void (CompositeTypeDescriptor::*)(const std::string&, std::shared_ptr<CompositeTypeDescriptor>))&CompositeTypeDescriptor::add_member, "name"_a,
	         "descriptor"_a)
		.def("get_size", &CompositeTypeDescriptor::get_size, "Get the size of the composite type descriptor")
		.def("__repr__", &CompositeTypeDescriptor::repr, "Get string representation of the composite type descriptor");

	};