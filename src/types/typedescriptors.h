
#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/unordered_map.h>

#include "types/primitive.h"
#include <optional>

class TypeDescriptors {
   private:
	std::unordered_map<uint32_t, std::shared_ptr<TypeDescriptor>> type_descriptors_map;

   public:
	TypeDescriptors(nanobind::dict type_descriptors);
    std::optional<std::shared_ptr<TypeDescriptor>> get_type_descriptor(uint32_t key);

};

void init_type_desciptors(nanobind::module_& m);
