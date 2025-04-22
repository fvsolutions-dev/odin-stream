#pragma once

#include <arrow/type.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/unordered_map.h>

#include <string>

#include "type_descriptor.h"

class ParameterDescriptor {
   private:
	uint32_t id;
	std::string name;
	std::shared_ptr<GenericTypeDescriptor> type_descriptor;

   public:
	ParameterDescriptor(uint32_t id, const std::string& name, std::shared_ptr<CompositeTypeDescriptor> type_descriptor)
		: id(id), name(name), type_descriptor(type_descriptor) {}
	ParameterDescriptor(uint32_t id, const std::string& name, std::shared_ptr<PrimitiveTypeDescriptor> type_descriptor)
		: id(id), name(name), type_descriptor(type_descriptor) {}

	static std::shared_ptr<ParameterDescriptor> unknown_type(uint32_t id, const std::string& name) {
		return std::make_shared<ParameterDescriptor>(id, name, PrimitiveTypeDescriptor::get_by_name("unknown"));
	}

	uint32_t get_id() const { return id; }
	std::string get_name() const { return name; }
	std::shared_ptr<GenericTypeDescriptor> get_type_descriptor() const { return type_descriptor; }
	size_t get_size() const { return type_descriptor->get_size(); }
	std::string repr() const { return "Parameter(id=" + std::to_string(id) + ", name=" + name + ", type_descriptor=" + type_descriptor->repr() + ")"; }
};

class ParameterMapDescriptor {
   private:
	std::unordered_map<uint32_t, std::shared_ptr<ParameterDescriptor>> parameter_map;

   public:
	ParameterMapDescriptor(nanobind::dict parameter_map);
	ParameterMapDescriptor() = default;

	std::optional<std::shared_ptr<ParameterDescriptor>> find_by_id(uint32_t key);
	std::optional<std::shared_ptr<ParameterDescriptor>> find_by_name(std::string name);

	void add_parameter(std::shared_ptr<ParameterDescriptor> parameter) { parameter_map[parameter->get_id()] = parameter; }
	std::string repr() const {
		std::string result = "ParameterMapDescriptor(";
		for (const auto& pair : parameter_map) {
			result += pair.second->repr() + ", ";
		}
		result += ")";
		return result;
	}
	
};

void init_parameter_descriptor(nanobind::module_& m);
