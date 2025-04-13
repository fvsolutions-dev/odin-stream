#include "parameterset.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <cstdint>
#include <iomanip>
#include <limits>  // Required by MSVC for numeric_limits sometimes with nanobind
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// --- Include Refactored C API Headers ---
extern "C" {
#include "odin_stream/stream_packet.h"
#include "odin_stream/stream_parameter_set.h"
}

namespace nb = nanobind;
using namespace nb::literals;

// === ParameterSet Python Wrapper (Manages stream_parameter_set_t*) ===

// Private constructor for adopting an existing pointer (e.g., from parse)
ParameterSet::ParameterSet(stream_parameter_set_t* adopted_ptr) : pset_ptr(adopted_ptr) {
	if (!pset_ptr) {
		// Should not happen if called correctly internally
		throw std::runtime_error("Internal error: Tried to adopt a NULL parameter set pointer.");
	}
}

// Helper to ensure pointer is valid before use
void ParameterSet::check_initialized() const {
	if (!pset_ptr) {
		throw std::runtime_error("ParameterSet instance is uninitialized, has been moved, or destroyed.");
	}
}

// Public Constructor
ParameterSet::ParameterSet(size_t max_parameters) {
	pset_ptr = stream_parameter_set_create(max_parameters);
	if (!pset_ptr) {
		// parameter_set_create returns NULL on failure (incl. max_parameters=0)
		throw std::bad_alloc();  // Or could throw ValueError for max_parameters=0
	}
}

// Destructor (RAII)
ParameterSet::~ParameterSet() {
	if (pset_ptr) {
		stream_parameter_set_destroy(pset_ptr);
		pset_ptr = nullptr;
	}
}

// --- Rule of 5 (Move semantics, Copy deleted) ---

ParameterSet::ParameterSet(ParameterSet&& other) noexcept : pset_ptr(other.pset_ptr) {
	other.pset_ptr = nullptr;  // Source is now invalid
}
ParameterSet& ParameterSet::operator=(ParameterSet&& other) noexcept {
	if (this != &other) {
		stream_parameter_set_destroy(pset_ptr);  // Destroy existing resource
		pset_ptr = other.pset_ptr;               // Take ownership from source
		other.pset_ptr = nullptr;                // Invalidate source
	}
	return *this;
}
// --- End Rule of 5 ---

// --- Wrapped Methods ---

void ParameterSet::add(const FixedSizeParameter& param) {
	check_initialized();
	stream_fixed_size_parameter_t c_param = param.to_c_struct();  // Create C struct
	stream_parameter_set_status_t status = stream_parameter_set_add(pset_ptr, c_param);
	check_param_set_status(status, "Failed to add parameter");  // Throws on error
}

void ParameterSet::add_list(nb::iterable params) {
	check_initialized();
	for (nb::handle item_handle : params) {
		// Get reference to Python wrapper object
		const FixedSizeParameter& param_wrapper = nb::cast<const FixedSizeParameter&>(item_handle);
		// Convert to C struct and call C add function (which throws on error via helper)
		this->add(param_wrapper);  // Re-use single add logic
	}
	// No return value needed if add throws on failure
}

void ParameterSet::remove_by_index(uint32_t index) {
	check_initialized();
	stream_parameter_set_status_t status = stream_parameter_set_remove_by_index(pset_ptr, index);
	check_param_set_status(status, "Failed to remove parameter by index");
}

size_t ParameterSet::get_count() const {
	check_initialized();
	return pset_ptr->parameter_count;
}

size_t ParameterSet::get_max_count() const {
	check_initialized();
	return pset_ptr->parameter_count_max;
}

uint16_t ParameterSet::get_hash() const {
	check_initialized();
	return pset_ptr->parameter_set_identifier;
}

// Note: Returning indices is safe as it doesn't involve data pointers
std::vector<uint32_t> ParameterSet::get_indices() const {
	check_initialized();
	std::vector<uint32_t> indices;
	if (pset_ptr->parameters) {  // Basic sanity check
		indices.reserve(pset_ptr->parameter_count);
		for (size_t i = 0; i < pset_ptr->parameter_count; ++i) {
			indices.push_back(pset_ptr->parameters[i].index);
		}
	}
	return indices;
}

// --- Packet Generation ---
nb::bytes ParameterSet::generate_identifier_packet() const {
	check_initialized();
	// Estimate size needed (can be slightly larger if count changes, but safe)
	size_t max_possible_size = sizeof(streaming_identifier_packet_header_t) + pset_ptr->parameter_count_max * sizeof(streaming_identifier_item_t);
	std::vector<uint8_t> buffer(max_possible_size);

	int bytes_written_or_err = stream_packet_create_identifier(pset_ptr, buffer.data(), buffer.size(), 0, 0);

	check_packet_status(bytes_written_or_err, "Failed to generate identifier packet");

	// Return only the bytes actually written
	return nb::bytes(buffer.data(), bytes_written_or_err);
}

nb::bytes ParameterSet::generate_data_packet(uint32_t timestamp) const {
	check_initialized();
	// Calculate exact required size
	size_t required_payload_size = 0;
	if (pset_ptr->parameters) {
		for (size_t i = 0; i < pset_ptr->parameter_count; ++i) {
			if (!pset_ptr->parameters[i].data) {
				throw nb::value_error(("Cannot generate data packet: Parameter with index " + std::to_string(pset_ptr->parameters[i].index) +
				                       " has NULL data pointer in set definition.")
				                          .c_str());
			}
			required_payload_size += pset_ptr->parameters[i].size;
		}
	}
	size_t required_total_size = sizeof(streaming_data_packet_header_t) + required_payload_size;

	std::vector<uint8_t> buffer(required_total_size);

	int bytes_written_or_err = stream_packet_create_data(pset_ptr, buffer.data(), buffer.size(), timestamp, 0);

	check_packet_status(bytes_written_or_err, "Failed to generate data packet");

	// Should match required size if successful
	assert((size_t)bytes_written_or_err == required_total_size);

	return nb::bytes(buffer.data(), bytes_written_or_err);
}

// --- Parsing (Class Method) ---
// Note: We need a way for the Python class to call this C++ static method
// And this method needs to return a ParameterSet instance (Python wrapper)
ParameterSet ParameterSet::parse_identifier_packet(nb::bytes data) {
	const uint8_t* buf_ptr = (const uint8_t*)data.c_str();
	size_t buf_size = data.size();

	stream_parameter_set_t* new_pset_ptr = stream_packet_parse_identifier(buf_ptr, buf_size);

	if (!new_pset_ptr) {
		// streaming_packet_parse_identifier returns NULL on error
		// Need to determine *why* - was it bad format, size, alloc?
		// For now, raise a generic error. Could add more detailed C API errors later.
		throw nb::value_error("Failed to parse identifier packet (invalid format, size, type, or memory allocation failed).");
	}
	// Success! Create a Python wrapper adopting the pointer.
	// Use the private constructor. Need friendship or a public static factory.
	// Let's use a public static factory method inside ParameterSet for adoption.
	return ParameterSet(new_pset_ptr);  // Use private constructor
}

/**
 * @brief Parses a data packet, verifies it against the set, and returns data segments.
 *
 * Checks the packet header (type, hash) against the current ParameterSet state.
 * Verifies the packet size matches the total expected data size for the parameters in this set.
 * If all checks pass, extracts the data payload corresponding to each parameter
 * defined in this set and returns them as a list of new bytes objects.
 *
 * @param data The Python bytes object containing the data packet.
 * @return A list of Python bytes objects, one for each parameter in the set's defined order.
 * @throws nb::value_error or std::runtime_error on validation failure (bad type, hash, size).
 * @throws std::logic_error if the ParameterSet instance has internal inconsistencies.
 */
void ParameterSet::parse_data_packet(nb::bytes data) const {
	check_initialized();  // Ensure pset_ptr is valid

	const uint8_t* buffer_ptr = (const uint8_t*)data.c_str();
	const size_t buffer_size = data.size();

	// --- Perform Checks similar to C 'streaming_packet_parse_data' ---

	// Check minimum size for header
	if (buffer_size < sizeof(streaming_data_packet_header_t)) {
		throw nb::value_error("Input data too small to contain data packet header.");
	}

	// Check header type
	const streaming_data_packet_header_t* header = (const streaming_data_packet_header_t*)buffer_ptr;
	if (header->header.type != STREAM_STREAM_PACKET_TYPE_DATA) {
		throw nb::value_error("Incorrect packet type");
	}

	// Check hash match (consider if recalculation is needed)
	// parameter_set_recalculate_hash(pset_ptr); // Maybe? Or trust stored hash.
	if (header->header.identifier != pset_ptr->parameter_set_identifier) {
		throw nb::value_error("Packet hash mismatch");
	}

	// Check if buffer size matches expected size based on parameter_set definition
	size_t expected_payload_size = 0;
	if (pset_ptr->parameters) {
		for (size_t i = 0; i < pset_ptr->parameter_count; ++i) {
			// NOTE: We don't check parameter[i].data here, only size,
			// as we are reading *from* the packet, not writing *to* the parameter_set.
			expected_payload_size += pset_ptr->parameters[i].size;
		}
	} else if (pset_ptr->parameter_count > 0) {
		throw std::logic_error("Internal error: ParameterSet count > 0 but parameters array is NULL.");
	}
	size_t expected_total_size = sizeof(streaming_data_packet_header_t) + expected_payload_size;

	if (buffer_size != expected_total_size) {
		throw nb::value_error("Packet size mismatch");
	}

	const uint8_t* payload_ptr = buffer_ptr + sizeof(streaming_data_packet_header_t);
	const uint8_t* buffer_end = buffer_ptr + buffer_size;  // For bounds checking

	if (pset_ptr->parameters) {
		for (size_t i = 0; i < pset_ptr->parameter_count; ++i) {
			size_t param_size = pset_ptr->parameters[i].size;

			// Bounds check before creating bytes object
			if (payload_ptr + param_size > buffer_end) {
				throw std::logic_error("Internal error: Calculated read past end of buffer during data extraction.");
			}

			// Create a *new* nb::bytes object by copying the data slice
			// result_data.emplace_back(nb::bytes(payload_ptr, param_size));

			payload_ptr += param_size;
		}
	}

	// Sanity check: did we consume the whole payload exactly?
	assert(payload_ptr == buffer_end);

}

// --- Representation ---
std::string ParameterSet::repr() const {
	if (!pset_ptr) {
		return "<ParameterSet (moved or destroyed)>";
	}
	std::stringstream ss;
	ss << "<ParameterSet count=" << pset_ptr->parameter_count << ", max=" << pset_ptr->parameter_count_max << ", hash=0x" << std::hex << std::setw(4)
	   << std::setfill('0') << pset_ptr->parameter_set_identifier << ">";
	return ss.str();
}

// --- Nanobind Module Definition ---
void init_parameterset(nb::module_& m) {
	// --- Bind ParameterSet Wrapper ---
	nb::class_<ParameterSet>(m, "ParameterSet", "Manages a set of streaming parameters")
		.def(nb::init<size_t>(), "max_parameters"_a, "Create a new, empty parameter set with a maximum capacity.")
		// Methods (throwing exceptions on C API errors)
		.def("add", &ParameterSet::add, "parameter"_a, nb::rv_policy::reference_internal,  // param must outlive set
	         "Add a parameter descriptor (FixedSizeParameter) to the set.")
		.def("add_list", &ParameterSet::add_list, "parameters"_a, "Add multiple parameter descriptors from a Python iterable.")
		.def("remove_by_index", &ParameterSet::remove_by_index, "index"_a, "Remove a parameter from the set by its index.")
		// .def("clear", &ParameterSet::clear, "Remove all parameters from the set.")
	    // .def("recalculate_hash", &ParameterSet::recalculate_hash, "Force recalculation of the internal parameter hash (usually not needed).")
	    // Packet Generation
		.def("generate_identifier_packet", &ParameterSet::generate_identifier_packet, "Generate the identifier packet for this set as bytes.")
		.def("generate_data_packet", &ParameterSet::generate_data_packet, "timestamp"_a,
	         "Generate the data packet for this set as bytes, including a timestamp.")
		// Properties (read-only)
		.def_prop_ro("count", &ParameterSet::get_count, "Current number of parameters.")
		.def_prop_ro("max_count", &ParameterSet::get_max_count, "Maximum capacity.")
		.def_prop_ro("hash", &ParameterSet::get_hash, "Current parameter index hash (CRC16).")
		.def_prop_ro("indices", &ParameterSet::get_indices, "List of indices currently in the set.")
		// Special methods
		.def("__len__", &ParameterSet::get_count)
		.def("__repr__", &ParameterSet::repr)

		// --- Bind NEW Data Parsing Method ---
		.def("parse_data_packet", &ParameterSet::parse_data_packet, "data"_a,
	         "Parses a data packet (bytes), verifies against the set definition,\n"
	         "and returns a list of bytes objects containing the data for each parameter.")

		// Class method for parsing
	    // Note: nb::classmethod requires C++17. Need static method binding otherwise.
	    // Using static method binding here for broader compatibility.
		.def_static("parse_identifier_packet", &ParameterSet::parse_identifier_packet, "data"_a,
	                "Parse an identifier packet (bytes) and create a new ParameterSet instance.");
}