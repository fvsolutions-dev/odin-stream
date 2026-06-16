
#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/unordered_map.h>

#include "./primitive.h"
#include <optional>

class ParameterDefinition {
   private:
	std::unordered_map<uint32_t, std::shared_ptr<TypeDescriptor>> type_descriptors_map;

   public:
	ParameterDefinition(nanobind::dict type_descriptors);
    std::optional<std::shared_ptr<TypeDescriptor>> find_by_id(uint32_t key);

};

void init_type_desciptors(nanobind::module_& m);
