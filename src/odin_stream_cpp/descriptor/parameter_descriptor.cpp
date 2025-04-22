#include "parameter_descriptor.h"

#include <arrow/type.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/unordered_map.h>

#include <string>

ParameterMapDescriptor::ParameterMapDescriptor(nanobind::dict parameter_map) {
	for (const auto& item : parameter_map) {
		uint32_t key = nanobind::cast<uint32_t>(item.first);

		if (nanobind::isinstance<CompositeTypeDescriptor>(item.second)) {
			parameter_map[key] = nanobind::cast<std::shared_ptr<CompositeTypeDescriptor>>(item.second);
			continue;

		} else if (nanobind::isinstance<PrimitiveTypeDescriptor>(item.second)) {
			parameter_map[key] = nanobind::cast<std::shared_ptr<PrimitiveTypeDescriptor>>(item.second);
			continue;
		
		} else {
			// Handle the case where the type is not recognized
			throw std::runtime_error("Unknown type descriptor in ParameterDefinition");
		}
	}
}

std::optional<std::shared_ptr<ParameterDescriptor>> ParameterMapDescriptor::find_by_id(uint32_t key) {
	auto it = parameter_map.find(key);
	if (it != parameter_map.end()) {
		return it->second;
	} else {
		return std::nullopt;  // or throw an exception if desired
	}
}

std::optional<std::shared_ptr<ParameterDescriptor>> ParameterMapDescriptor::find_by_name(std::string name) {
	for (const auto& pair : parameter_map) {
		if (pair.second->get_name() == name) {
			return pair.second;
		}
	}
	return std::nullopt;  // or throw an exception if desired
}


void init_parameter_descriptor(nanobind::module_& m) {
	using namespace nanobind::literals;
	namespace nb = nanobind;

	nb::class_<ParameterDescriptor>(m, "ParameterDescriptor")
		.def(nb::init<uint32_t, std::string, std::shared_ptr<PrimitiveTypeDescriptor>>(), "id"_a, "name"_a, "type_descriptor"_a)
		.def(nb::init<uint32_t, std::string, std::shared_ptr<CompositeTypeDescriptor>>(), "id"_a, "name"_a, "type_descriptor"_a)
		.def("get_id", &ParameterDescriptor::get_id, "Get the ID of the parameter")
		.def("get_name", &ParameterDescriptor::get_name, "Get the name of the parameter")
		.def("get_type_descriptor", &ParameterDescriptor::get_type_descriptor, "Get the type descriptor of the parameter")
		.def("get_size", &ParameterDescriptor::get_size, "Get the size of the parameter")
		.def_static("unknown_type", &ParameterDescriptor::unknown_type, "id"_a, "name"_a, "Create an unknown type parameter descriptor")
		.def("__repr__", &ParameterDescriptor::repr, "Get string representation of the parameter descriptor");
	
	nb::class_<ParameterMapDescriptor>(m, "ParameterMapDescriptor")
		.def(nb::init<nb::dict>(), "parameter_map"_a, "Constructor for the ParameterMapDescriptor. Initializes with parameter map.")
		.def(nb::init<>(), "Constructor for the ParameterMapDescriptor. Initializes with parameter map.")
		.def("find_by_id", &ParameterMapDescriptor::find_by_id, "key"_a, "Get a ParameterDescriptor by its key.")
		.def("find_by_name", &ParameterMapDescriptor::find_by_name, "name"_a, "Get a ParameterDescriptor by its name.")
		.def("add_parameter", &ParameterMapDescriptor::add_parameter, "parameter"_a, "Add a parameter to the map.")
		.def("__repr__", &ParameterMapDescriptor::repr, "Get string representation of the parameter map descriptor.");
		
}