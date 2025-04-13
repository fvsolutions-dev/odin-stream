#include <arrow/api.h>
#include <arrow/io/api.h>
#include <arrow/ipc/api.h>
#include <arrow/util/logging.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/shared_ptr.h>  // Make sure to include this for shared_ptr support
#include <nanobind/stl/string.h>
#include <nanobind/stl/unordered_map.h>  // May be needed for binding map access if desired
#include <nanobind/stl/vector.h>
#include <nanobind_pyarrow/pyarrow_import.h>
#include <nanobind_pyarrow/table.h>
// --- Include Refactored C API Headers ---
extern "C" {
#include "odin_stream/stream_packet.h"
#include "odin_stream/stream_parameter_set.h"
}

// #include <cstdint>
// #include <iomanip>
// #include <limits>  // Required by MSVC for numeric_limits sometimes with nanobind
// #include <memory>
// #include <sstream>
// #include <stdexcept>
// #include <string>
// #include <vector>

// #include "enums.hpp"
#include "fixed_size_parameter.hpp"
#include "parameterset.hpp"

namespace nb = nanobind;

// Helper function to create an Arrow Table with mixed int and float columns
std::shared_ptr<arrow::Table> create_mixed_table() {
	// 1. Create integer array
	arrow::Int32Builder int_builder;
	std::vector<int32_t> int_data = {1, 2, 3, 4, 5};
	for (int i = 0; i < 500; ++i) {
		// Append values to the builder
		ARROW_CHECK_OK(int_builder.AppendValues(int_data));
	}
	std::shared_ptr<arrow::Array> int_array;
	ARROW_CHECK_OK(int_builder.Finish(&int_array));

	// 2. Create float array
	arrow::DoubleBuilder float_builder;
	std::vector<double> float_data = {1.1, 2.2, 3.3, 4.4, 5.5};
	for (int i = 0; i < 500; ++i) {
		// Append values to the builder
		ARROW_CHECK_OK(float_builder.AppendValues(float_data));
	}

	std::shared_ptr<arrow::Array> float_array;
	ARROW_CHECK_OK(float_builder.Finish(&float_array));

	// 3. Create fields (column names and types)
	auto int_field = arrow::field("int_column", arrow::int32());
	auto float_field = arrow::field("float_column", arrow::float64());

	// 4. Create schema
	auto schema = arrow::schema({int_field, float_field});

	// 5. Create table from arrays and schema
	std::shared_ptr<arrow::Table> table;
	table = arrow::Table::Make(schema, {int_array, float_array});
	if (!table) {
		// Handle table creation failure
		ARROW_LOG(ERROR) << "Failed to create table";
		return nullptr;  // Or throw an exception, depending on your error handling policy
	}
	return table;
}

class StreamProcessor {
   private:
	std::unordered_map<uint16_t, ParameterSet> parameter_sets_map;

public:
	StreamProcessor() = default;
	// --- Rule of 5/0 ---
	// Explicitly delete copy operations because the map member
	// holds non-copyable ParameterSet objects.
	StreamProcessor(const StreamProcessor&) = delete;
	StreamProcessor& operator=(const StreamProcessor&) = delete;

	// Default move operations are likely okay since std::unordered_map
	// and ParameterSet (based on your .hpp) are movable.
	StreamProcessor(StreamProcessor&&) = delete;
	StreamProcessor& operator=(StreamProcessor&&) = delete;
	// --- End Rule of 5/0 ---

	void process_bytes_list(nb::list bytes_list) {
		for (const auto& handle : bytes_list) {
			nb::bytes item = nb::cast<nb::bytes>(handle);

			if (item.size() < sizeof(streaming_data_packet_header_t)) {
				// %zu is the format specifier for size_t
				fprintf(stderr, "Warning: Input data too small (%zu bytes) to contain expected header. Skipping.\n", item.size());
				continue;
			}

			const streaming_data_packet_header_t* header = reinterpret_cast<const streaming_data_packet_header_t*>(item.c_str());

			// --- Process Identifier Packet ---
			if (header->header.type == STREAM_STREAM_PACKET_TYPE_IDENTIFIER) {
				// If it exits, skip
				if (parameter_sets_map.find(header->header.identifier) != parameter_sets_map.end()) {
					continue;
				}

				ParameterSet parsed_set = ParameterSet::from_identifier_data(item);
				uint16_t identifier = parsed_set.get_hash();

				parameter_sets_map.insert_or_assign(identifier, std::move(parsed_set));

				// Use printf for standard output messages
				// %hu is the format specifier for uint16_t
				printf("Stored/Updated ParameterSet with ID: %hu\n", identifier);

			}
			// --- Process Data Packet ---
			else if (header->header.type == STREAM_STREAM_PACKET_TYPE_DATA) {
				uint16_t identifier = header->header.identifier;
				auto it = parameter_sets_map.find(identifier);

				if (it == parameter_sets_map.end()) {
					// fprintf(stderr, "Warning: ParameterSet for identifier %hu not found. Cannot process data packet.\n", identifier);
					continue;
				}
				ParameterSet& param_set = it->second;
				param_set.parse_data_packet(item);

			} else {
				fprintf(stderr, "Warning: Encountered unknown packet type: %hu\n", header->header.type);
			}
		}
	}

	size_t get_parameter_set_count() const { return parameter_sets_map.size(); }

	// Get the ParameterSet by identifier
	ParameterSet get_parameter_set(uint16_t identifier) {
		auto it = parameter_sets_map.find(identifier);
		if (it != parameter_sets_map.end()) {
			return it->second;
		} else {
			throw nb::key_error("ParameterSet with the given identifier not found.");
		}
	}

	void clear_parameter_sets() {
		parameter_sets_map.clear();
		printf("Cleared all stored ParameterSets.\n");
	}
};

// Create map to hold the parameterset collection

// ParameterSet process_bytes_list(nb::list bytes_list) {
// 	// Iterate over the list and process each item
// 	for (size_t i = 0; i < bytes_list.size(); ++i) {
// 		nb::bytes item = nb::cast<nb::bytes>(bytes_list[i]);

// 		// Check minimum size for header
// 		if (item.size() < sizeof(streaming_data_packet_header_t)) {
// 			throw nb::value_error("Input data too small to contain data packet header.");
// 		}

// 		// Check type

// 		const streaming_data_packet_header_t* header = (const streaming_data_packet_header_t*)item.c_str();

// 		if (header->header.type == STREAM_STREAM_PACKET_TYPE_IDENTIFIER) {
// 			ParameterSet data = ParameterSet::parse_identifier_packet(item);
// 			uint16_t identifier = data.get_hash();
// 			return data;

// 			// process_identifier_packet(item);
// 		}

// 		if (header->header.type == STREAM_STREAM_PACKET_TYPE_DATA) {
// 			uint16_t identifier = header->header.identifier;

// 			// Find the corresponding ParameterSet by identifier

// 			// Call the parse method
// 			param.parse_data_packet(item);
// 		}
// 	}
// }

// // Check hash match (consider if recalculation is needed)
// // parameter_set_recalculate_hash(pset_ptr); // Maybe? Or trust stored hash.
// if (header->header.identifier != pset_ptr->parameter_set_identifier) {
// 	throw nb::value_error("Packet hash mismatch");
// }

// // Check if buffer size matches expected size based on parameter_set definition
// size_t expected_payload_size = 0;
// if (pset_ptr->parameters) {
// 	for (size_t i = 0; i < pset_ptr->parameter_count; ++i) {
// 		// NOTE: We don't check parameter[i].data here, only size,
// 		// as we are reading *from* the packet, not writing *to* the parameter_set.
// 		expected_payload_size += pset_ptr->parameters[i].size;
// 	}
// } else if (pset_ptr->parameter_count > 0) {
// 	throw std::logic_error("Internal error: ParameterSet count > 0 but parameters array is NULL.");
// }
// size_t expected_total_size = sizeof(streaming_data_packet_header_t) + expected_payload_size;

// if (buffer_size != expected_total_size) {
// 	throw nb::value_error("Packet size mismatch");
// }

// // --- Checks passed, extract data ---

// std::vector<nb::bytes> result_data;
// result_data.reserve(pset_ptr->parameter_count);

// const uint8_t* payload_ptr = buffer_ptr + sizeof(streaming_data_packet_header_t);
// const uint8_t* buffer_end = buffer_ptr + buffer_size;  // For bounds checking

// if (pset_ptr->parameters) {
// 	for (size_t i = 0; i < pset_ptr->parameter_count; ++i) {
// 		size_t param_size = pset_ptr->parameters[i].size;

// 		// Bounds check before creating bytes object
// 		if (payload_ptr + param_size > buffer_end) {
// 			throw std::logic_error("Internal error: Calculated read past end of buffer during data extraction.");
// 		}

// 		// Create a *new* nb::bytes object by copying the data slice
// 		result_data.emplace_back(nb::bytes(payload_ptr, param_size));

// 		payload_ptr += param_size;
// 	}
// }

// // Sanity check: did we consume the whole payload exactly?
// assert(payload_ptr == buffer_end);

// Nanobind automatically converts std::vector<nb::bytes> to a Python list
NB_MODULE(odin_stream, m) {  // Changed module name to avoid collision and be more descriptive
	static nb::detail::pyarrow::ImportPyarrow module;
	m.def("process_data", &create_mixed_table, "Creates an Arrow table with integer and float columns.");

	// Expose the function to create the table
	// m.def("test_create", &create_mixed_table, "Creates an Arrow table with integer and float columns.");

	nb::class_<StreamProcessor>(m, "StreamProcessor")
		.def(nb::init<>(), "Constructor for the StreamProcessor.")
		.def("process_bytes_list", &StreamProcessor::process_bytes_list, nb::arg("bytes_list"),
	         "Processes a list of byte packets. Stores ParameterSets from "
	         "identifier packets and uses them to parse corresponding data packets.")
		.def("get_parameter_set_count", &StreamProcessor::get_parameter_set_count, "Returns the number of ParameterSets currently stored.")
		.def("clear_parameter_sets", &StreamProcessor::clear_parameter_sets, "Removes all stored ParameterSets.")
		.def("get_parameter_set", &StreamProcessor::get_parameter_set, nb::arg("identifier"),
	         "Returns the ParameterSet corresponding to the given identifier.");

	// init_enums(m);
	init_fixed_size_parameter(m);  // Initialize the FixedSizeParameter bindings
	init_parameterset(m);  // Initialize the ParameterSet bindings
}