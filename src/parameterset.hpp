// odin_stream_wrapper.hpp
#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>

#include <cstdint>
#include <string>
#include <vector>
#include "fixed_size_parameter.hpp"
#include "enums.hpp"


// ParameterSet Python Wrapper (Manages stream_parameter_set_t*)
class ParameterSet {
   private:
	stream_parameter_set_t* pset_ptr = nullptr;

	// Private constructor for adopting an existing pointer (e.g., from parse)
	ParameterSet(stream_parameter_set_t* adopted_ptr);

	// Helper to ensure pointer is valid before use
	void check_initialized() const;

   public:
	// Public Constructor
	ParameterSet(size_t max_parameters);

	// Destructor (RAII)
	~ParameterSet();

	// --- Rule of 5 (Move semantics, Copy deleted) ---
	ParameterSet(const ParameterSet&) = delete;
	ParameterSet& operator=(const ParameterSet&) = delete;

	ParameterSet(ParameterSet&& other) noexcept;
	ParameterSet& operator=(ParameterSet&& other) noexcept;
	// --- End Rule of 5 ---

	// --- Wrapped Methods ---

	void add(const FixedSizeParameter& param);

	void add_list(nb::iterable params);

	void remove_by_index(uint32_t index);

	// --- Properties ---
	size_t get_count() const;

	size_t get_max_count() const;

	uint16_t get_hash() const;

	std::vector<uint32_t> get_indices() const;

	// --- Packet Generation ---
	nb::bytes generate_identifier_packet() const;

	nb::bytes generate_data_packet(uint32_t timestamp) const;

	// --- Parsing (Class Method) ---
	static ParameterSet parse_identifier_packet(nb::bytes data);

	void parse_data_packet(nb::bytes data) const;

	// --- Representation ---
	std::string repr() const;
};

void init_parameterset(nb::module_& m);
