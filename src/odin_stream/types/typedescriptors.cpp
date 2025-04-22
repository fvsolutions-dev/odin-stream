
#include "typedescriptors.h"
#include "./struct.h"
namespace nb = nanobind;

ParameterDefinition::ParameterDefinition(nanobind::dict type_descriptors) {
	for (const auto& item : type_descriptors) {
		uint32_t key = nanobind::cast<uint32_t>(item.first);

		if (nb::isinstance<StructDescriptor>(item.second)) {
			auto struct_descriptor = nb::cast<std::shared_ptr<StructDescriptor>>(item.second);
			type_descriptors_map[key] = struct_descriptor;
			continue;
		} else if (nb::isinstance<PrimitiveTypeDescriptor>(item.second)) {
			auto primitive_descriptor = nb::cast<std::shared_ptr<PrimitiveTypeDescriptor>>(item.second);
			type_descriptors_map[key] = primitive_descriptor;
			continue;
		}
	}
}

std::optional<std::shared_ptr<TypeDescriptor>> ParameterDefinition::find_by_id(uint32_t key) {
	auto it = type_descriptors_map.find(key);
	if (it != type_descriptors_map.end()) {
		return it->second;
	} else {
		return std::nullopt;  // or throw an exception if desired
	}
}

void init_type_desciptors(nb::module_& m) {
	using namespace nb::literals;

	nb::class_<ParameterDefinition>(m, "ParameterDefinition")
		.def(nb::init<nb::dict>(), "type_descriptors"_a, "Constructor for the ParameterDefinition. Initializes with type descriptors.")
		.def("find_by_id", &ParameterDefinition::find_by_id, "key"_a, "Get a TypeDescriptor by its key.");
}