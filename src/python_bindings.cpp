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

// #include "enums.hpp"
#include "fixed_size_parameter.h"
#include "parameterset.h"
#include "types/primitive.h"
#include "types/struct.h"
#include "stream_processor.h"
#include "types/typedescriptors.h"

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

// Nanobind automatically converts std::vector<nb::bytes> to a Python list
NB_MODULE(odin_stream, m) {  // Changed module name to avoid collision and be more descriptive
	static nb::detail::pyarrow::ImportPyarrow module;
	
	m.def("process_data", &create_mixed_table, "Creates an Arrow table with integer and float columns.");

	// init_enums(m);
	init_fixed_size_parameter(m);  // Initialize the FixedSizeParameter bindings
	init_parameterset(m);  // Initialize the ParameterSet bindings
	init_primitive(m);  // Initialize the PrimitiveTypeDescriptor bindings
	init_struct(m);  // Initialize the StructDescriptor bindings
	init_stream_processor(m);  // Initialize the StreamProcessor bindings
	init_type_desciptors(m);


}