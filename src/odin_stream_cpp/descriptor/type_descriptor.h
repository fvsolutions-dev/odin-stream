#pragma once

#include <arrow/type.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/unordered_map.h>

#include <string>

//Generic type to inherit from
class GenericTypeDescriptor {
   public:
	virtual ~GenericTypeDescriptor() = default;
	virtual size_t get_size() const = 0;
	virtual std::string repr() const { return "TypeDescriptor()"; }
};

class PrimitiveTypeDescriptor : public GenericTypeDescriptor {
   private:
	std::string name;
	size_t size;
	std::shared_ptr<arrow::DataType> arrow_data_type;

   public:
	PrimitiveTypeDescriptor(const std::string& name, size_t size, std::shared_ptr<arrow::DataType> arrow_data_type)
		: name(name), size(size), arrow_data_type(arrow_data_type) {}

	size_t get_size() const override { return size; }

	std::string repr() const override { return "PrimitiveTypeDescriptor(name=" + name + ", size=" + std::to_string(size) + ")"; }

	std::shared_ptr<arrow::DataType> get_arrow_type() const { return arrow_data_type; }
	std::string get_name() const { return name; }

	// Get by name class method
	static std::shared_ptr<PrimitiveTypeDescriptor> get_by_name(const std::string& name);
};

class CompositeTypeDescriptor : public GenericTypeDescriptor {
   public:
	// Store the members
	std::vector<std::pair<std::string, std::shared_ptr<GenericTypeDescriptor>>> members;

	void add_member(const std::string& name, std::shared_ptr<PrimitiveTypeDescriptor> descriptor) { members.emplace_back(name, std::move(descriptor)); }
	void add_member(const std::string& name, std::shared_ptr<CompositeTypeDescriptor> descriptor) { members.emplace_back(name, std::move(descriptor)); }

	// Calculate total size (sum of members)
	size_t get_size() const override {
		size_t total_size = 0;
		for (const auto& member : members) {
			total_size += member.second->get_size();
		}
		return total_size;
		
	}

	// Repr method which prints all items in a horizontal list
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
};


void init_type_descriptor(nanobind::module_& m);
