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

// // **Static factory method to create from Python list**
// std::shared_ptr<StructDescriptor> StructDescriptor::from_list(nb::list py_definition_list) {
// 	// Create the StructDescriptor object that will be populated
// 	auto struct_desc = std::make_shared<StructDescriptor>();

// 	// Reserve space for efficiency if list size is known (it is)
// 	struct_desc->members.reserve(nb::len(py_definition_list));

// 	// Iterate over the Python list
// 	for (nb::handle item : py_definition_list) {
// 		// Ensure each item is a tuple of size 2
// 		if (!nb::isinstance<nb::tuple>(item) || nb::len(item) != 2) {
// 			throw nb::type_error("Struct definition must be a list of (name: str, type) tuples.");
// 		}
// 		nb::tuple pair = nb::cast<nb::tuple>(item);

// 		// Ensure the first element is a string (the name)
// 		if (!nb::isinstance<nb::str>(pair[0])) {
// 			throw nb::type_error("Struct member name must be a string.");
// 		}
     
// 		std::string name = nb::cast<std::string>(pair[0]);

// 		// The second element is the Python type definition (PrimitiveType or Struct)
// 		nb::handle py_type_def = pair[1];

// 		// Check if it's a PrimitiveType
// 		if (nb::isinstance<PrimitiveTypeDescriptor>(py_type_def)) {
// 			auto primitive_descriptor = nb::cast<std::shared_ptr<PrimitiveTypeDescriptor>>(py_type_def);
// 			struct_desc->add_member(name, primitive_descriptor);
// 			continue;
// 		} else if (nb::isinstance<StructDescriptor>(py_type_def)) {
// 			auto struct_descriptor = nb::cast<std::shared_ptr<StructDescriptor>>(py_type_def);
// 			struct_desc->add_member(name, struct_descriptor);
// 			continue;
// 		}

// 		throw nb::type_error("Unsupported type for struct member: " + nb::repr(py_type_def).cast<std::string>() + ". Expected PrimitiveType or StructDescriptor.");

// 		// Add the converted member to our vector
// 		// struct_desc->members.emplace_back(name, std::move(member_descriptor));
// 	}

// 	return struct_desc;  // Return the populated StructDescriptor
// }

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
