#include "enums.hpp"

namespace nb = nanobind;
using namespace nb::literals;

// === Helper Function for Status Checking ===
// Converts C API status codes to Python exceptions
void check_param_set_status(stream_parameter_set_status_t status, const std::string& context) {
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

void check_packet_status(int status, const std::string& context) {
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

void init_enums(nb::module_& m) {
	// --- Bind Status Enums ---
	nb::enum_<stream_parameter_set_status_t>(m, "ParameterSetStatus")
		.value("SUCCESS", STREAM_PARAM_SET_SUCCESS)
		.value("ERROR_NOMEM", STREAM_PARAM_SET_ERROR_NOMEM)
		.value("ERROR_FULL", STREAM_PARAM_SET_ERROR_FULL)
		.value("ERROR_DUPLICATE", STREAM_PARAM_SET_ERROR_DUPLICATE)
		.value("ERROR_NOTFOUND", STREAM_PARAM_SET_ERROR_NOTFOUND)
		.value("ERROR_INVALID", STREAM_PARAM_SET_ERROR_INVALID)
		.value("ERROR_INTERNAL", STREAM_PARAM_SET_ERROR_INTERNAL);
		// .export_values();

	nb::enum_<stream_packet_status_t>(m, "StreamingPacketStatus")
		.value("SUCCESS", STREAM_PACKET_SUCCESS)
		.value("ERROR_INVALID", STREAM_PACKET_ERROR_INVALID)
		.value("ERROR_BADSIZE", STREAM_PACKET_ERROR_BADSIZE)
		.value("ERROR_BADTYPE", STREAM_PACKET_ERROR_BADTYPE)
		.value("ERROR_BADHASH", STREAM_PACKET_ERROR_BADHASH)
		.value("ERROR_NODATA", STREAM_PACKET_ERROR_NODATA)
		.value("ERROR_OVERFLOW", STREAM_PACKET_ERROR_OVERFLOW)
		.value("ERROR_INTERNAL", STREAM_PACKET_ERROR_INTERNAL)
		.value("ERROR_NOMEM", STREAM_PACKET_ERROR_NOMEM);
		// .export_values();
}