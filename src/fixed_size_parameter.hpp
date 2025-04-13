#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <cstdint>
#include <string>

extern "C" {
#include "odin_stream/stream_packet.h"
#include "odin_stream/stream_parameter_set.h"
}

namespace nb = nanobind;

class FixedSizeParameter {
   private:
	uint32_t index;
	nb::bytes data_buffer;

   public:
	FixedSizeParameter(uint32_t idx, nb::bytes data);

	uint32_t get_index() const;
	void set_index(uint32_t new_index);

	nb::bytes get_data() const;
	void set_data(nb::bytes new_data);

	uint32_t get_size() const;

	stream_fixed_size_parameter_t to_c_struct() const;

	std::string repr() const;
};

void init_fixed_size_parameter(nb::module_& m);