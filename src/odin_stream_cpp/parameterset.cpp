#include "parameterset.h"

#include <nanobind/nanobind.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

// Arrow headers
#include <arrow/api.h>
#include <arrow/builder.h>
#include <arrow/io/api.h>   // Potentially needed, good to include
#include <arrow/ipc/api.h>  // Potentially needed, good to include
#include <arrow/memory_pool.h>
#include <arrow/result.h>
#include <arrow/status.h>
// #include <arrow/table.h>
#include <arrow/type.h>

#include <cstdint>
#include <iomanip>
#include <limits>  // Required by MSVC for numeric_limits sometimes with nanobind
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// --- Include Refactored C API Headers ---
extern "C" {
#include "embedded_odin_stream/stream_packet.h"
#include "embedded_odin_stream/stream_parameter_set.h"
}

namespace nb = nanobind;

// Use arrow's status checking macros for cleaner error handling
#define ARROW_THROW_NOT_OK(status)                                     \
	do {                                                               \
		arrow::Status _s = (status);                                   \
		if (!_s.ok()) {                                                \
			throw std::runtime_error("Arrow Error: " + _s.ToString()); \
		}                                                              \
	} while (0)

#define ARROW_ASSIGN_OR_THROW_IMPL(result_name, lhs, rexpr) \
	auto result_name = (rexpr);                             \
	ARROW_THROW_NOT_OK((result_name).status());             \
	lhs = std::move(result_name).ValueUnsafe(); /* Using ValueUnsafe because we checked ok */

#define ARROW_ASSIGN_OR_THROW(lhs, rexpr) ARROW_ASSIGN_OR_THROW_IMPL(ARROW_ASSIGN_OR_RAISE_NAME(_error_or_value, __COUNTER__), lhs, rexpr)

ParameterSet::ParameterSet(uint16_t identifier, uint32_t definition_identifier, std::shared_ptr<ParameterMapDescriptor> parameter_map)
	: parameter_set_identifier(identifier), definition_identifier(definition_identifier), parameter_map(parameter_map) {}

ParameterSet::~ParameterSet() {
	// Destructor body can be empty if all resources are managed by the map
	// The map will automatically clean up its contents when it goes out of scope
}

void ParameterSet::add(std::shared_ptr<FixedSizeParameter> param) {
	parameters.push_back(param);
	data_size += param->get_size();  // Update the total data size
}

std::shared_ptr<ParameterSet> ParameterSet::from_identifier_data(nanobind::bytes data, std::shared_ptr<ParameterMapDescriptor> parameter_map) {
	stream_parameter_set_t* new_pset_ptr = stream_packet_parse_identifier((const uint8_t*)data.c_str(), data.size());

	if (!new_pset_ptr) {
		throw nb::value_error("Failed to parse identifier packet (invalid format, size, type, or memory allocation failed).");
	}

	ParameterSet parameterset = ParameterSet(new_pset_ptr->parameter_set_identifier, new_pset_ptr->definition_identifier, parameter_map);

	// Add the fixed size parameters to the map
	for (size_t i = 0; i < new_pset_ptr->parameter_count; ++i) {
		const stream_fixed_size_parameter_t& c_param = new_pset_ptr->parameters[i];

		std::optional<std::shared_ptr<ParameterDescriptor>> type = parameter_map->find_by_id(c_param.index);

		if (!type) {
			printf("Warning: Type descriptor not found for index %u. Skipping parameter.\n", c_param.index);
			continue;  // Skip if type descriptor is not found
		}

		auto parawm =
			std::make_shared<FixedSizeParameter>(FixedSizeParameter(c_param.index, c_param.size, type.value()));  // Create a new FixedSizeParameter object
		parameterset.add(parawm);                                                                                 // Use shared_ptr for memory management
	}

	// Clean up the C struct
	stream_parameter_set_destroy(new_pset_ptr);
	return std::make_shared<ParameterSet>(parameterset);  // Return a shared pointer to the new ParameterSet
}

void ParameterSet::parse_data_packet(nb::bytes data) {
	// Check minimum size for header
	if (data.size() < sizeof(streaming_data_packet_header_t)) {
		throw nb::value_error("Input data too small to contain data packet header.");
	}

	// Check header type
	const streaming_data_packet_header_t* header = (const streaming_data_packet_header_t*)data.c_str();
	if (header->header.type != STREAM_STREAM_PACKET_TYPE_DATA) {
		throw nb::value_error("Incorrect packet type");
	}

	// Check hash match (consider if recalculation is needed)
	// parameter_set_recalculate_hash(pset_ptr); // Maybe? Or trust stored hash.
	if (header->header.identifier != parameter_set_identifier) {
		throw nb::value_error("Packet hash mismatch");
	}

	if (data.size() != sizeof(streaming_data_packet_header_t) + data_size) {
		// printf("Data size: %zu, expected size: %zu\n", data.size(), sizeof(streaming_data_packet_header_t) + data_size);
		// throw nb::value_error("Packet size mismatch");
		return;  // Skip if size doesn't match
	}
	const uint8_t* payload_ptr = (const uint8_t*)data.c_str() + sizeof(streaming_data_packet_header_t);
	for (size_t i = 0; i < parameters.size(); ++i) {
		size_t param_size = parameters[i]->get_size();
		parameters[i]->add_data(payload_ptr, param_size);  // Add data to the parameter
		payload_ptr += param_size;
	}

	// --- NEW: Extract and store sequence ID and timestamp ---
	// !!! REPLACE `sequence_id` AND `timestamp` WITH ACTUAL MEMBER NAMES !!!
	// Example assumes they exist directly in streaming_data_packet_header_t
	// sequence_ids_.push_back(header->sequence_number);
	// timestamps_.push_back(header->timestamp);
	// --- END NEW ---

	// 	// Sanity check: did we consume the whole payload exactly?
	// 	assert(payload_ptr == buffer_end);
	// }

	// // --- Representation ---
	// std::string ParameterSet::repr() const {
	// 	if (!pset_ptr) {
	// 		return "<ParameterSet (moved or destroyed)>";
	// 	}
	// 	std::stringstream ss;
	// 	ss << "<ParameterSet count=" << pset_ptr->parameter_count << ", max=" << pset_ptr->parameter_count_max << ", hash=0x" << std::hex << std::setw(4)
	// 	   << std::setfill('0') << pset_ptr->parameter_set_identifier << ">";
	// 	return ss.str();
}

// --- flush_to_arrow_table (Modified) ---
std::shared_ptr<arrow::Table> ParameterSet::flush_to_arrow_table() {
	std::vector<std::shared_ptr<arrow::Field>> fields;
	std::vector<std::shared_ptr<arrow::Array>> arrays;
	
	uint32_t datapoints = 0;
	for (const auto& param : parameters) {
		datapoints = param->get_datapoints();

		std::vector<std::pair<std::shared_ptr<arrow::Array>, std::shared_ptr<arrow::Field>>> data = param->finish();

		for (const auto& [array, field] : data) {
			fields.push_back(field);
			arrays.push_back(array);
		}
	}

	auto schema = arrow::schema(fields);
	auto table = arrow::Table::Make(schema, arrays, datapoints);

	return table;
}

// --- Nanobind Module Definition ---
void init_parameterset(nb::module_& m) {
	using namespace nb::literals;

	// --- Bind ParameterSet Wrapper ---
	nb::class_<ParameterSet>(m, "ParameterSet", "Manages a set of streaming parameters")
		.def(nb::init<uint16_t, uint32_t, std::shared_ptr<ParameterMapDescriptor>>(), "identifier"_a, "definition_identifier"_a, "type_descriptors"_a,
	         "Create a new ParameterSet with the given identifier and type descriptors.")
		// .def("add", &ParameterSet::add, "param"_a, nb::rv_policy::reference_internal, "Add a FixedSizeParameter to the set.")
		.def_static("from_identifier_data", &ParameterSet::from_identifier_data, "data"_a, "type_descriptors"_a,
	                "Create a new ParameterSet from identifier data.")
		.def("flush_to_arrow_table", &ParameterSet::flush_to_arrow_table, "Creates an Arrow table from the data in the parameters and clears them.");
}