#include "./struct.h"

namespace nb = nanobind;

void StructDescriptor::add_member(const std::string& name, std::shared_ptr<PrimitiveTypeDescriptor> descriptor) {
	members.emplace_back(name, std::move(descriptor));
}
void StructDescriptor::add_member(const std::string& name, std::shared_ptr<StructDescriptor> descriptor) { members.emplace_back(name, std::move(descriptor)); }

size_t StructDescriptor::get_size() const {
	size_t total_size = 0;
	for (const auto& member : members) {
		total_size += member.second->get_size();
	}
	return total_size;
}

void init_struct(nb::module_& m) {
	using namespace nb::literals;

	nb::class_<StructDescriptor>(m, "StructDescriptor", "Descriptor for structure types")
		.def(nb::init<>(), "Create a new StructDescriptor.")

        // Bind the first overload using nb::overload_cast
        .def("add_member",
            nb::overload_cast<const std::string&, std::shared_ptr<PrimitiveTypeDescriptor>>(&StructDescriptor::add_member),
            "name"_a, "descriptor"_a, // Use .none() if None should be allowed: "descriptor"_a.none()
            "Add a primitive member to the struct descriptor.")

       // Bind the second overload using nb::overload_cast
       .def("add_member",
            nb::overload_cast<const std::string&, std::shared_ptr<StructDescriptor>>(&StructDescriptor::add_member),
            "name"_a, "descriptor"_a, // Use .none() if None should be allowed: "descriptor"_a.none()
            "Add a struct member to the struct descriptor (can be recursive).")

		.def("get_size", &StructDescriptor::get_size, "Get size of the struct in bytes.")
		// .def("decode", &StructDescriptor::decode, "Decode the struct from a byte buffer.")
		.def("__repr__", &StructDescriptor::repr, "Get string representation of the struct descriptor.");
		// .def_static("FromPythonList", &StructDescriptor::from_list, "py_definition_list"_a,
	                // "Create a StructDescriptor from a Python list of (name, type) tuples.");
}
