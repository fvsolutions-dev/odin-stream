#include "parameterset.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/stl/list.h>
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

#define ARROW_ASSIGN_OR_THROW_IMPL(result_name, lhs, rexpr)            \
    auto result_name = (rexpr);                                        \
    ARROW_THROW_NOT_OK((result_name).status());                        \
    lhs = std::move(result_name).ValueUnsafe(); /* Using ValueUnsafe because we checked ok */

#define ARROW_ASSIGN_OR_THROW(lhs, rexpr) \
    ARROW_ASSIGN_OR_THROW_IMPL(ARROW_ASSIGN_OR_RAISE_NAME(_error_or_value, __COUNTER__), lhs, rexpr)


ParameterSet::ParameterSet(uint16_t identifier, uint32_t definition_identifier)
	: parameter_set_identifier(identifier), definition_identifier(definition_identifier) {
	// Initialize the data size to 0
	data_size = 0;

	// Constructor body can be empty if all initialization is done above
	// You could add validation here if needed (e.g., check if size > 0)
}

ParameterSet::~ParameterSet() {
	// Destructor body can be empty if all resources are managed by the map
	// The map will automatically clean up its contents when it goes out of scope
}
void ParameterSet::add(FixedSizeParameter param) {
	parameters.push_back(param);
	data_size += param.get_size();
}

ParameterSet ParameterSet::from_identifier_data(nanobind::bytes data) {
	stream_parameter_set_t* new_pset_ptr = stream_packet_parse_identifier((const uint8_t*)data.c_str(), data.size());

	if (!new_pset_ptr) {
		// streaming_packet_parse_identifier returns NULL on error
		// Need to determine *why* - was it bad format, size, alloc?
		// For now, raise a generic error. Could add more detailed C API errors later.
		throw nb::value_error("Failed to parse identifier packet (invalid format, size, type, or memory allocation failed).");
	}

	ParameterSet set = ParameterSet(new_pset_ptr->parameter_set_identifier, new_pset_ptr->definition_identifier);
	// Add the fixed size parameters to the map
	for (size_t i = 0; i < new_pset_ptr->parameter_count; ++i) {
		const stream_fixed_size_parameter_t& c_param = new_pset_ptr->parameters[i];
		FixedSizeParameter param(c_param.index, c_param.size);

		set.add(param);  // Add to the vector
	}

	// Clean up the C struct
	stream_parameter_set_destroy(new_pset_ptr);

	return set;  // Return the new ParameterSet instance
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
		printf("Data size: %zu, expected size: %zu\n", data.size(), sizeof(streaming_data_packet_header_t) + data_size);
		throw nb::value_error("Packet size mismatch");
	}

	const uint8_t* payload_ptr = (const uint8_t*)data.c_str() + sizeof(streaming_data_packet_header_t);
	for (size_t i = 0; i < parameters.size(); ++i) {
		size_t param_size = parameters[i].get_size();

		parameters[i].add_data(payload_ptr, param_size);  // Add data to the parameter

		payload_ptr += param_size;
	}

	// --- NEW: Extract and store sequence ID and timestamp ---
	// !!! REPLACE `sequence_id` AND `timestamp` WITH ACTUAL MEMBER NAMES !!!
	// Example assumes they exist directly in streaming_data_packet_header_t
	sequence_ids_.push_back(header->sequence_number);
	timestamps_.push_back(header->timestamp);
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
    arrow::MemoryPool* pool = arrow::default_memory_pool();

    // --- 1. Handle Empty Parameter Set (Definition) ---
    // If parameters vector is empty, but we might have received packets? Unlikely scenario.
    // The main check is whether any *packets* have been received.
    if (sequence_ids_.empty()) { // Check if any packets were processed
        // If parameters is also empty, return truly empty table
        if (parameters.empty()) {
             auto schema = arrow::schema({}); // Empty schema
             return arrow::Table::Make(schema, std::vector<std::shared_ptr<arrow::Array>>{}, 0);
        } else {
             // We have a definition but no data. Return table with schema but 0 rows.
             std::vector<std::shared_ptr<arrow::Field>> fields;
             fields.reserve(2 + parameters.size()); // SeqId, Timestamp + Params

             // Add SeqId/Timestamp fields even with 0 rows
             fields.push_back(arrow::field("sequence_id", arrow::uint64())); // Adjust type if needed
             fields.push_back(arrow::field("timestamp", arrow::uint64()));   // Adjust type if needed

             for (const auto& param : parameters) {
                 int32_t packet_size = static_cast<int32_t>(param.get_size());
                 if (packet_size <= 0) {
                    throw std::runtime_error("Parameter index " + std::to_string(param.get_index()) + " has invalid packet size: " + std::to_string(packet_size));
                 }
                 auto field_type = arrow::fixed_size_binary(packet_size);
                 fields.push_back(arrow::field(std::to_string(param.get_index()), field_type));
             }
             auto schema = arrow::schema(fields);
             // Create empty arrays matching the schema
             std::vector<std::shared_ptr<arrow::Array>> arrays;
             arrays.reserve(fields.size());
             for(const auto& field : fields) {
                 std::unique_ptr<arrow::ArrayBuilder> builder;
                 ARROW_THROW_NOT_OK(arrow::MakeBuilder(pool, field->type(), &builder));
                 std::shared_ptr<arrow::Array> empty_array;
                 ARROW_ASSIGN_OR_THROW(empty_array, builder->Finish());
                 arrays.push_back(empty_array);
             }
             return arrow::Table::Make(schema, arrays, 0); // Table with schema, 0 rows
        }
    }

    // --- 2. Determine Number of Rows and Check Consistency ---
    int64_t num_rows = static_cast<int64_t>(sequence_ids_.size());

    // Check if timestamp vector size matches
    if (static_cast<int64_t>(timestamps_.size()) != num_rows) {
         throw std::runtime_error("Internal inconsistency: Number of sequence IDs (" +
                                std::to_string(num_rows) + ") does not match number of timestamps (" +
                                std::to_string(timestamps_.size()) + ").");
    }

    // Check if all parameters have the same number of packets received
    for (const auto& param : parameters) {
        if (static_cast<int64_t>(param.get_num_packets()) != num_rows) {
            throw std::runtime_error("Inconsistent number of packets across parameters. Expected " +
                                     std::to_string(num_rows) + " (based on headers received), but parameter index " +
                                     std::to_string(param.get_index()) + " has " +
                                     std::to_string(param.get_num_packets()) + " packets.");
        }
        // Optional: Re-check internal consistency of parameter buffers (already present in original code)
        if (param.get_packets_buffer_size() != static_cast<size_t>(num_rows) * param.get_size()) {
             throw std::runtime_error("Internal inconsistency: buffer size (" + std::to_string(param.get_packets_buffer_size()) +
                                      ") does not match num_packets * packet_size (" + std::to_string(num_rows) + "*" +
                                      std::to_string(param.get_size()) + ") for index " + std::to_string(param.get_index()));
        }
         if (param.get_size() == 0 && num_rows > 0) { // Check only if we expect rows
              throw std::runtime_error("Parameter index " + std::to_string(param.get_index()) + " has packets but reports zero packet size.");
         }
    }

    // --- 3. Initialize Builders and Schema Fields ---
    std::vector<std::shared_ptr<arrow::Field>> fields;
    std::vector<std::shared_ptr<arrow::Array>> arrays; // Store finalized arrays
    fields.reserve(2 + parameters.size());
    arrays.reserve(2 + parameters.size());

    // --- 4. Build Sequence ID Column ---
    // Choose Arrow type (e.g., uint64, uint32) - MUST match std::vector type
    auto seq_id_type = arrow::uint16();
    fields.push_back(arrow::field("sequence_id", seq_id_type));
    arrow::UInt16Builder seq_id_builder(pool);
    ARROW_THROW_NOT_OK(seq_id_builder.Reserve(num_rows));
    ARROW_THROW_NOT_OK(seq_id_builder.AppendValues(sequence_ids_.data(), num_rows));
    std::shared_ptr<arrow::Array> seq_id_array;
    ARROW_ASSIGN_OR_THROW(seq_id_array, seq_id_builder.Finish());
    arrays.push_back(seq_id_array);


    // --- 5. Build Timestamp Column ---
    // Choose Arrow type (e.g., uint64, or timestamp[unit]) - MUST match std::vector type
    // If using arrow::timestamp, the builder expects int64_t*. Cast needed if vector is uint64_t.
    // Using uint64 is often simpler unless you need Arrow's time semantics immediately.
    auto ts_type = arrow::uint32(); // Or arrow::timestamp(arrow::TimeUnit::NANOSECOND) etc.
    fields.push_back(arrow::field("timestamp", ts_type));
    arrow::UInt32Builder ts_builder(pool); // Use matching builder type
    ARROW_THROW_NOT_OK(ts_builder.Reserve(num_rows));
    ARROW_THROW_NOT_OK(ts_builder.AppendValues(timestamps_.data(), num_rows)); // Assumes timestamps_ is vector<uint64_t>
    std::shared_ptr<arrow::Array> ts_array;
    ARROW_ASSIGN_OR_THROW(ts_array, ts_builder.Finish());
    arrays.push_back(ts_array);


    // --- 6. Build Parameter Columns ---
    std::vector<std::unique_ptr<arrow::FixedSizeBinaryBuilder>> param_builders;
    param_builders.reserve(parameters.size());

    for (const auto& param : parameters) {
        int32_t packet_size = static_cast<int32_t>(param.get_size());
         // Size check already done, but belt-and-suspenders doesn't hurt
         if (packet_size <= 0) {
             throw std::runtime_error("Parameter index " + std::to_string(param.get_index()) + " has invalid packet size: " + std::to_string(packet_size));
         }

        auto field_type = arrow::fixed_size_binary(packet_size);
        fields.push_back(arrow::field(std::to_string(param.get_index()), field_type));

        param_builders.emplace_back(std::make_unique<arrow::FixedSizeBinaryBuilder>(field_type, pool));
        ARROW_THROW_NOT_OK(param_builders.back()->Reserve(num_rows));
    }

    // Populate Parameter Builders (row by row conceptually, but builder appends efficiently)
    for (size_t j = 0; j < parameters.size(); ++j) {
         const auto& param = parameters[j];
         auto& builder = *param_builders[j];
         const std::vector<uint8_t>& buffer = param.get_packets_buffer();
         int32_t packet_size = static_cast<int32_t>(param.get_size()); // Already checked > 0

         // Check buffer size again just before access
         if(buffer.size() != static_cast<size_t>(num_rows) * packet_size) {
             throw std::runtime_error("Buffer size mismatch just before appending for param index " + std::to_string(param.get_index()));
         }

         // Append all values at once using AppendValues for efficiency
         // Need pointer to start of buffer and number of items (num_rows)
          ARROW_THROW_NOT_OK(builder.AppendValues(buffer.data(), num_rows));

         // // Alternatively, append one by one (less efficient for large N)
         // for (int64_t i = 0; i < num_rows; ++i) {
         //     const uint8_t* packet_ptr = buffer.data() + (static_cast<size_t>(i) * packet_size);
         //     ARROW_THROW_NOT_OK(builder.Append(packet_ptr));
         // }
    }

     // Finalize Parameter Arrays
     for (auto& builder_ptr : param_builders) {
         std::shared_ptr<arrow::Array> param_array;
         ARROW_ASSIGN_OR_THROW(param_array, builder_ptr->Finish());
         arrays.push_back(param_array);
     }


    // --- 7. Create Schema and Table ---
    auto schema = arrow::schema(fields);
    auto table = arrow::Table::Make(schema, arrays, num_rows);

    // --- 8. Clear Source Data (Flush) ---
    sequence_ids_.clear();
    timestamps_.clear();
    for (auto& param : parameters) {
        param.clear_data();
    }
    // parameters_.clear(); // Only clear this if the ParameterSet definition itself is flushed

    return table;
}

// --- Nanobind Module Definition ---
void init_parameterset(nb::module_& m) {
	using namespace nb::literals;

	// --- Bind ParameterSet Wrapper ---
	nb::class_<ParameterSet>(m, "ParameterSet", "Manages a set of streaming parameters")
		.def(nb::init<uint16_t, uint32_t>(), "identifier"_a, "definition_identifier"_a,
	         "Create a new ParameterSet with the given identifier and definition identifier.")
		// .def("add", &ParameterSet::add, "param"_a, nb::rv_policy::reference_internal, "Add a FixedSizeParameter to the set.")
		.def_static("from_identifier_data", &ParameterSet::from_identifier_data, "data"_a, "Create a new ParameterSet from an identifier packet (bytes).")
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