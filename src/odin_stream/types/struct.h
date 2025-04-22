#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <optional>

#include <vector>

#include "./primitive.h"

// Descriptor for structure types
class StructDescriptor : public TypeDescriptor {
   public:
	// Store members: name and shared_ptr to their TypeDescriptor
	std::vector<std::pair<std::string, std::shared_ptr<TypeDescriptor>>> members;

	// Helper to add members during construction from Python
	void add_member(const std::string& name, std::shared_ptr<PrimitiveTypeDescriptor> descriptor);
	void add_member(const std::string& name, std::shared_ptr<StructDescriptor> descriptor);

	// Calculate total size (sum of members)
	size_t get_size() const override;

	std::string repr() const override {
		// Print all items in a horizontal list
		std::string repr = "StructDescriptor(";
		for (const auto& member : members) {
			repr += member.first + ": " + member.second->repr() + ", ";
		}
		if (!members.empty()) {
			repr.pop_back();  // Remove last space
			repr.pop_back();  // Remove last comma
		}
		repr += ")";
		return repr;
	}
    
    // static std::shared_ptr<StructDescriptor> from_list(nanobind::list py_definition_list);

};

void init_struct(nanobind::module_& m);
