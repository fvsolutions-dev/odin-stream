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

// === Helper Function for Status Checking ===
// Converts C API status codes to Python exceptions
inline void check_param_set_status(stream_parameter_set_status_t status, const std::string& context = "") {
	std::string prefix = context.empty() ? "" : context + ": ";
	switch (status) {
		case STREAM_PARAM_SET_SUCCESS:
			return;  // No error
		case STREAM_PARAM_SET_ERROR_NOMEM:
			throw std::bad_alloc();  // Map to standard C++ exception
		case STREAM_PARAM_SET_ERROR_FULL:
			throw nb::index_error((prefix + "Parameter set is full.").c_str());
		case STREAM_PARAM_SET_ERROR_DUPLICATE:
			throw nb::value_error((prefix + "Parameter index already exists.").c_str());
		case STREAM_PARAM_SET_ERROR_NOTFOUND:
			throw nb::key_error((prefix + "Parameter index not found.").c_str());  // Map to key error
		case STREAM_PARAM_SET_ERROR_INVALID:
			throw nb::value_error((prefix + "Invalid argument (e.g., NULL pointer internally, or bad state).").c_str());
		case STREAM_PARAM_SET_ERROR_INTERNAL:
			throw std::runtime_error((prefix + "Internal parameter set inconsistency detected.").c_str());
		default:
			throw std::runtime_error((prefix + "Unknown parameter set error code: " + std::to_string(status)).c_str());
	}
}

inline void check_packet_status(int status, const std::string& context = "") {
	std::string prefix = context.empty() ? "" : context + ": ";
	// Handle positive return value (bytes written) as success for generation functions
	if (status >= 0) {
		return;
	}
	// Handle specific negative error codes
	switch ((stream_packet_status_t)status) {
		case STREAM_PACKET_SUCCESS:  // Should have been caught by status >= 0
			return;
		case STREAM_PACKET_ERROR_INVALID:
			throw nb::value_error((prefix + "Invalid argument (e.g., NULL pointer).").c_str());
		case STREAM_PACKET_ERROR_BADSIZE:
			throw nb::value_error((prefix + "Buffer size error (too small, too large, or inconsistent).").c_str());
		case STREAM_PACKET_ERROR_BADTYPE:
			throw nb::value_error((prefix + "Incorrect packet type found during parsing.").c_str());
		case STREAM_PACKET_ERROR_BADHASH:
			throw nb::value_error((prefix + "Parameter group hash mismatch during parsing.").c_str());
		case STREAM_PACKET_ERROR_NODATA:
			throw nb::value_error((prefix + "Required parameter data pointer is NULL.").c_str());
		case STREAM_PACKET_ERROR_OVERFLOW:
			throw nb::value_error((prefix + "Data size exceeds packet format limits.").c_str());
		case STREAM_PACKET_ERROR_INTERNAL:
			throw std::runtime_error((prefix + "Internal packet processing inconsistency.").c_str());
		case STREAM_PACKET_ERROR_NOMEM:
			throw std::bad_alloc();  // Map to standard C++ exception
		default:
			throw std::runtime_error((prefix + "Unknown packet processing error code: " + std::to_string(status)).c_str());
	}
}

// === FixedSizeParameter Python Wrapper ===
struct FixedSizeParameter {
	// Store data in Python object to manage lifetime via Python GC
	uint32_t index;
	nb::bytes data_buffer;
	// No raw C struct needed directly *if* we always reconstruct on C API calls

	FixedSizeParameter(uint32_t idx, nb::bytes data) : index(idx), data_buffer(std::move(data)) {}

	// Getter/Setter for Index
	uint32_t get_index() const { return index; }
	void set_index(uint32_t new_index) { index = new_index; }

	// Getter/Setter for Data
	nb::bytes get_data() const { return data_buffer; }
	void set_data(nb::bytes new_data) { data_buffer = std::move(new_data); }

	// Getter for Size (derived from data)
	uint32_t get_size() const { return (uint32_t)data_buffer.size(); }

	// Helper to create the C struct on the fly when needed
	stream_fixed_size_parameter_t to_c_struct() const {
		stream_fixed_size_parameter_t c_param;
		c_param.index = this->index;
		c_param.size = this->get_size();
		// NOTE: This pointer is only valid as long as data_buffer exists!
		// C API calls using this must not store the pointer long-term.
		c_param.data = (uint8_t*)this->data_buffer.c_str();
		return c_param;
	}

	// __repr__
	std::string repr() const {
		std::stringstream ss;
		ss << "FixedSizeParameter(index=" << index << ", size=" << get_size() << ", data=b'";
		ss << std::hex << std::setfill('0');
		const char* buf_data = data_buffer.c_str();
		size_t buf_size = data_buffer.size();
		for (size_t i = 0; i < buf_size; ++i) {
			ss << "\\x" << std::setw(2) << static_cast<int>((unsigned char)buf_data[i]);
		}
		ss << "')";
		return ss.str();
	}
};

// === ParameterSet Python Wrapper (Manages stream_parameter_set_t*) ===
class ParameterSet {
   private:
	stream_parameter_set_t* pset_ptr = nullptr;

	// Private constructor for adopting an existing pointer (e.g., from parse)
	ParameterSet(stream_parameter_set_t* adopted_ptr) : pset_ptr(adopted_ptr) {
		if (!pset_ptr) {
			// Should not happen if called correctly internally
			throw std::runtime_error("Internal error: Tried to adopt a NULL parameter set pointer.");
		}
	}

	// Helper to ensure pointer is valid before use
	void check_initialized() const {
		if (!pset_ptr) {
			throw std::runtime_error("ParameterSet instance is uninitialized, has been moved, or destroyed.");
		}
	}

   public:
	// Public Constructor
	ParameterSet(size_t max_parameters) {
		pset_ptr = stream_parameter_set_create(max_parameters);
		if (!pset_ptr) {
			// parameter_set_create returns NULL on failure (incl. max_parameters=0)
			throw std::bad_alloc();  // Or could throw ValueError for max_parameters=0
		}
	}

	// Destructor (RAII)
	~ParameterSet() {
		if (pset_ptr) {
			stream_parameter_set_destroy(pset_ptr);
			pset_ptr = nullptr;
		}
	}

	// --- Rule of 5 (Move semantics, Copy deleted) ---
	ParameterSet(const ParameterSet&) = delete;
	ParameterSet& operator=(const ParameterSet&) = delete;

	ParameterSet(ParameterSet&& other) noexcept : pset_ptr(other.pset_ptr) {
		other.pset_ptr = nullptr;  // Source is now invalid
	}
	ParameterSet& operator=(ParameterSet&& other) noexcept {
		if (this != &other) {
			stream_parameter_set_destroy(pset_ptr);  // Destroy existing resource
			pset_ptr = other.pset_ptr;               // Take ownership from source
			other.pset_ptr = nullptr;                // Invalidate source
		}
		return *this;
	}
	// --- End Rule of 5 ---

	// --- Wrapped Methods ---

	void add(const FixedSizeParameter& param) {
		check_initialized();
		stream_fixed_size_parameter_t c_param = param.to_c_struct();  // Create C struct
		stream_parameter_set_status_t status = stream_parameter_set_add(pset_ptr, c_param);
		check_param_set_status(status, "Failed to add parameter");  // Throws on error
	}

	void add_list(nb::iterable params) {
		check_initialized();
		for (nb::handle item_handle : params) {
			// Get reference to Python wrapper object
			const FixedSizeParameter& param_wrapper = nb::cast<const FixedSizeParameter&>(item_handle);
			// Convert to C struct and call C add function (which throws on error via helper)
			this->add(param_wrapper);  // Re-use single add logic
		}
		// No return value needed if add throws on failure
	}

	void remove_by_index(uint32_t index) {
		check_initialized();
		stream_parameter_set_status_t status = stream_parameter_set_remove_by_index(pset_ptr, index);
		check_param_set_status(status, "Failed to remove parameter by index");
	}

	// void clear() {
	// 	check_initialized();
	// 	stream_parameter_set_status_t status = stream_parameter_set_clear(pset_ptr);
	// 	check_param_set_status(status, "Failed to clear parameter set");
	// }

	// void recalculate_hash() {
	// 	check_initialized();
	// 	stream_parameter_set_status_t status = stream_parameter_set_recalculate_hash(pset_ptr);
	// 	check_param_set_status(status, "Failed to recalculate hash");
	// }

	// --- Properties ---
	size_t get_count() const {
		check_initialized();
		return pset_ptr->parameter_count;
	}

	size_t get_max_count() const {
		check_initialized();
		return pset_ptr->parameter_count_max;
	}

	uint16_t get_hash() const {
		check_initialized();
		return pset_ptr->parameter_set_identifier;
	}

	// Note: Returning indices is safe as it doesn't involve data pointers
	std::vector<uint32_t> get_indices() const {
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
	nb::bytes generate_identifier_packet() const {
		check_initialized();
		// Estimate size needed (can be slightly larger if count changes, but safe)
		size_t max_possible_size = sizeof(streaming_identifier_packet_header_t) + pset_ptr->parameter_count_max * sizeof(streaming_identifier_item_t);
		std::vector<uint8_t> buffer(max_possible_size);

		int bytes_written_or_err = stream_packet_create_identifier(pset_ptr, buffer.data(), buffer.size(), 0, 0);

		check_packet_status(bytes_written_or_err, "Failed to generate identifier packet");

		// Return only the bytes actually written
		return nb::bytes(buffer.data(), bytes_written_or_err);
	}

	nb::bytes generate_data_packet(uint32_t timestamp) const {
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
	static ParameterSet parse_identifier_packet(nb::bytes data) {
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

	// ... existing methods (add, add_list, remove, clear, recalculate_hash, properties) ...
	// ... existing packet generation methods (generate_identifier_packet, generate_data_packet) ...

	// --- NEW: Safe Data Packet Parsing Method ---

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
	std::vector<nb::bytes> parse_data_packet(nb::bytes data) const {
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

		// --- Checks passed, extract data ---

		std::vector<nb::bytes> result_data;
		result_data.reserve(pset_ptr->parameter_count);

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
				result_data.emplace_back(nb::bytes(payload_ptr, param_size));

				payload_ptr += param_size;
			}
		}

		// Sanity check: did we consume the whole payload exactly?
		assert(payload_ptr == buffer_end);

		// Nanobind automatically converts std::vector<nb::bytes> to a Python list
		return result_data;
	}

	// --- Representation ---
	std::string repr() const {
		if (!pset_ptr) {
			return "<ParameterSet (moved or destroyed)>";
		}
		std::stringstream ss;
		ss << "<ParameterSet count=" << pset_ptr->parameter_count << ", max=" << pset_ptr->parameter_count_max << ", hash=0x" << std::hex << std::setw(4)
		   << std::setfill('0') << pset_ptr->parameter_set_identifier << ">";
		return ss.str();
	}

};  // End of ParameterSet class

// --- Nanobind Module Definition ---
NB_MODULE(odin_stream, m) {  // Choose a suitable module name

	m.doc() = "Python bindings for the Odin Streaming C API (ParameterSet and Packet Generation)";

	// --- Bind Status Enums ---
	nb::enum_<stream_parameter_set_status_t>(m, "ParameterSetStatus")
		.value("SUCCESS", STREAM_PARAM_SET_SUCCESS)
		.value("E_NOMEM", STREAM_PARAM_SET_ERROR_NOMEM)
		.value("E_FULL", STREAM_PARAM_SET_ERROR_FULL)
		.value("E_DUPLICATE", STREAM_PARAM_SET_ERROR_DUPLICATE)
		.value("E_NOTFOUND", STREAM_PARAM_SET_ERROR_NOTFOUND)
		.value("E_INVALID", STREAM_PARAM_SET_ERROR_INVALID)
		.value("E_INTERNAL", STREAM_PARAM_SET_ERROR_INTERNAL)
		.export_values();

	nb::enum_<stream_packet_status_t>(m, "StreamingPacketStatus")
		.value("SUCCESS", STREAM_PACKET_SUCCESS)
		.value("E_INVALID", STREAM_PACKET_ERROR_INVALID)
		.value("E_BADSIZE", STREAM_PACKET_ERROR_BADSIZE)
		.value("E_BADTYPE", STREAM_PACKET_ERROR_BADTYPE)
		.value("E_BADHASH", STREAM_PACKET_ERROR_BADHASH)
		.value("E_NODATA", STREAM_PACKET_ERROR_NODATA)
		.value("E_OVERFLOW", STREAM_PACKET_ERROR_OVERFLOW)
		.value("E_INTERNAL", STREAM_PACKET_ERROR_INTERNAL)
		.value("E_NOMEM", STREAM_PACKET_ERROR_NOMEM)
		.export_values();

	// --- Bind FixedSizeParameter Wrapper ---
	nb::class_<FixedSizeParameter>(m, "FixedSizeParameter", "Wrapper for parameter descriptor (index/data)")
		.def(nb::init<uint32_t, nb::bytes>(), "index"_a, "data"_a, "Create a parameter descriptor. Size is derived from data.")
		.def_prop_rw("index", &FixedSizeParameter::get_index, &FixedSizeParameter::set_index, "Parameter index (uint32)")
		.def_prop_rw("data", &FixedSizeParameter::get_data, &FixedSizeParameter::set_data, "Parameter data (bytes), updates size implicitly.")
		.def_prop_ro("size", &FixedSizeParameter::get_size,  // Read-only size
	                 "Size of parameter data in bytes (derived from data).")
		.def("__repr__", &FixedSizeParameter::repr);

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
	// .def_classmethod("parse_identifier_packet", &ParameterSet::parse_identifier_packet, "data"_a,
	//            "Parse an identifier packet (bytes) and create a new ParameterSet instance."); // C++17 way

	// NOTE: Did not bind streaming_packet_parse_data as it requires careful
	// management of pre-allocated buffers within the Python FixedSizeParameter objects,
	// which adds significant complexity and safety concerns to the wrapper design.

}  // End of NB_MODULE