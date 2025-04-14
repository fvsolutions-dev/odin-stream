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
#include "odin_stream/stream_packet.h"
#include "odin_stream/stream_parameter_set.h"
}

namespace nb = nanobind;
// class ParameterSet {
// 	private:
// 	 std::unordered_map<uint16_t, FixedSizeParameter> parameters;  // Map of parameters by index
// 	 uint16_t parameter_set_identifier;                            // Identifier of the parameter set
// 	 uint32_t definition_identifier;                            // Identifier for the definition of the parameter set
// 	public:
// 	 ParameterSet(uint16_t identifier,uint32_t definition_identifier);
// 	 ~ParameterSet();

// 	 ParameterSet(const ParameterSet&) = delete;
// 	 ParameterSet& operator=(const ParameterSet&) = delete;

// 	 ParameterSet(ParameterSet&& other) noexcept;
// 	 ParameterSet& operator=(ParameterSet&& other) noexcept;

// 	 void add(const FixedSizeParameter& param);

// 	 static ParameterSet from_identifier_data(nanobind::bytes data);
// 	 void parse_data_packet(nanobind::bytes data) const;

// 	 std::string repr() const;
//  };

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

ParameterSet::ParameterSet(uint16_t identifier, uint32_t definition_identifier, std::shared_ptr<TypeDescriptors> type_descriptors)
	: parameter_set_identifier(identifier), definition_identifier(definition_identifier), type_descriptors(type_descriptors) {}

ParameterSet::~ParameterSet() {
	// Destructor body can be empty if all resources are managed by the map
	// The map will automatically clean up its contents when it goes out of scope
}

void ParameterSet::add(std::shared_ptr<FixedSizeParameter> param) {
	parameters.push_back(param);
	data_size += param->get_size();  // Update the total data size
}

std::shared_ptr<ParameterSet> ParameterSet::from_identifier_data(nanobind::bytes data, std::shared_ptr<TypeDescriptors> type_descriptors) {
	stream_parameter_set_t* new_pset_ptr = stream_packet_parse_identifier((const uint8_t*)data.c_str(), data.size());

	if (!new_pset_ptr) {
		throw nb::value_error("Failed to parse identifier packet (invalid format, size, type, or memory allocation failed).");
	}

	ParameterSet parameterset = ParameterSet(new_pset_ptr->parameter_set_identifier, new_pset_ptr->definition_identifier, type_descriptors);

	// Add the fixed size parameters to the map
	for (size_t i = 0; i < new_pset_ptr->parameter_count; ++i) {
		const stream_fixed_size_parameter_t& c_param = new_pset_ptr->parameters[i];

        std::optional<std::shared_ptr<TypeDescriptor>> type = type_descriptors->get_type_descriptor(c_param.index);

        if (!type) {
            printf("Warning: Type descriptor not found for index %u. Skipping parameter.\n", c_param.index);
            continue;  // Skip if type descriptor is not found
        }

		auto parawm =
			std::make_shared<FixedSizeParameter>(FixedSizeParameter(c_param.index, c_param.size, type.value()));  // Create a new FixedSizeParameter object
		parameterset.add(parawm);  // Use shared_ptr for memory management
	}

	// Clean up the C struct
	stream_parameter_set_destroy(new_pset_ptr);
	return std::make_shared<ParameterSet>(parameterset);  // Return a shared pointer to the new ParameterSet
}

// /**
//  * @brief Parses a data packet, verifies it against the set, and returns data segments.
//  *
//  * Checks the packet header (type, hash) against the current ParameterSet state.
//  * Verifies the packet size matches the total expected data size for the parameters in this set.
//  * If all checks pass, extracts the data payload corresponding to each parameter
//  * defined in this set and returns them as a list of new bytes objects.
//  *
//  * @param data The Python bytes object containing the data packet.
//  * @return A list of Python bytes objects, one for each parameter in the set's defined order.
//  * @throws nb::value_error or std::runtime_error on validation failure (bad type, hash, size).
//  * @throws std::logic_error if the ParameterSet instance has internal inconsistencies.
//  */
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
		
        std::vector<std::pair<std::shared_ptr<arrow::Array>, std::shared_ptr<arrow::Field>>> data = param->finish();
        datapoints = param->get_datapoints();

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
		.def(nb::init<uint16_t, uint32_t, std::shared_ptr<TypeDescriptors>>(), "identifier"_a, "definition_identifier"_a, "type_descriptors"_a,
	         "Create a new ParameterSet with the given identifier and type descriptors.")
		// .def("add", &ParameterSet::add, "param"_a, nb::rv_policy::reference_internal, "Add a FixedSizeParameter to the set.")
		.def_static("from_identifier_data", &ParameterSet::from_identifier_data, "data"_a, "type_descriptors"_a,
	                "Create a new ParameterSet from identifier data.")
		.def("flush_to_arrow_table", &ParameterSet::flush_to_arrow_table, "Creates an Arrow table from the data in the parameters and clears them.");

	//OLD
	// .def(nb::init<size_t>(), "max_parameters"_a, "Create a new, empty parameter set with a maximum capacity.")
	// // Methods (throwing exceptions on C API errors)
	// .def("add", &ParameterSet::add, "parameter"_a, nb::rv_policy::reference_internal,  // param must outlive set
	//      "Add a parameter descriptor (FixedSizeParameter) to the set.")
	// .def("add_list", &ParameterSet::add_list, "parameters"_a, "Add multiple parameter descriptors from a Python iterable.")
	// .def("remove_by_index", &ParameterSet::remove_by_index, "index"_a, "Remove a parameter from the set by its index.")
	// // .def("clear", &ParameterSet::clear, "Remove all parameters from the set.")
	// // .def("recalculate_hash", &ParameterSet::recalculate_hash, "Force recalculation of the internal parameter hash (usually not needed).")
	// // Packet Generation
	// .def("generate_identifier_packet", &ParameterSet::generate_identifier_packet, "Generate the identifier packet for this set as bytes.")
	// .def("generate_data_packet", &ParameterSet::generate_data_packet, "timestamp"_a,
	//      "Generate the data packet for this set as bytes, including a timestamp.")
	// // Properties (read-only)
	// .def_prop_ro("count", &ParameterSet::get_count, "Current number of parameters.")
	// .def_prop_ro("max_count", &ParameterSet::get_max_count, "Maximum capacity.")
	// .def_prop_ro("hash", &ParameterSet::get_hash, "Current parameter index hash (CRC16).")
	// .def_prop_ro("indices", &ParameterSet::get_indices, "List of indices currently in the set.")
	// // Special methods
	// .def("__len__", &ParameterSet::get_count)
	// .def("__repr__", &ParameterSet::repr)

	// // --- Bind NEW Data Parsing Method ---
	// .def("parse_data_packet", &ParameterSet::parse_data_packet, "data"_a,
	//      "Parses a data packet (bytes), verifies against the set definition,\n"
	//      "and returns a list of bytes objects containing the data for each parameter.")

	// Class method for parsing
	// Note: nb::classmethod requires C++17. Need static method binding otherwise.
	// Using static method binding here for broader compatibility.
	// .def_static("parse_identifier_packet", &ParameterSet::parse_identifier_packet, "data"_a,
	//             "Parse an identifier packet (bytes) and create a new ParameterSet instance.");
}