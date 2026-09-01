#include "parameter_descriptor.h"

#include <arrow/type.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/unordered_map.h>

#include <string>

// `parameters` maps parameter id -> ParameterDescriptor.
//
// The previous version of this loop assigned into `parameter_map`, which named the
// nanobind::dict argument rather than the member of the same name, so it mutated its
// own input while iterating it and left the member map empty. It also stored bare
// type descriptors, which are not ParameterDescriptors. It compiled only because
// nanobind <= 2.6 still had `dict::operator[]` for integral keys; newer nanobind
// dropped that overload, which is what finally surfaced the mistake.
ParameterMapDescriptor::ParameterMapDescriptor(nanobind::dict parameters) {
	for (const auto& item : parameters) {
		uint32_t key = nanobind::cast<uint32_t>(item.first);

		if (!nanobind::isinstance<ParameterDescriptor>(item.second)) {
			throw std::runtime_error("ParameterMapDescriptor expects a dict of {id: ParameterDescriptor}");
		}

		parameter_map[key] = nanobind::cast<std::shared_ptr<ParameterDescriptor>>(item.second);
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
		.def(nb::init<nb::dict>(), "parameters"_a, "Construct from a dict of {parameter id: ParameterDescriptor}.")
		.def(nb::init<>(), "Construct an empty map; fill it with add_parameter().")
		.def("find_by_id", &ParameterMapDescriptor::find_by_id, "key"_a, "Get a ParameterDescriptor by its key.")
		.def("find_by_name", &ParameterMapDescriptor::find_by_name, "name"_a, "Get a ParameterDescriptor by its name.")
		.def("add_parameter", &ParameterMapDescriptor::add_parameter, "parameter"_a, "Add a parameter to the map.")
		.def("__repr__", &ParameterMapDescriptor::repr, "Get string representation of the parameter map descriptor.");
		
}