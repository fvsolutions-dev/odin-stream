#include <memory>
#include <vector>
#include <nanobind/nanobind.h>
#include <nanobind/stl/shared_ptr.h> // Make sure to include this for shared_ptr support

#include <arrow/api.h>
#include <arrow/io/api.h>
#include <arrow/ipc/api.h>
#include <arrow/util/logging.h>
#include <nanobind_pyarrow/table.h>
namespace nb = nanobind;

// Helper function to create an Arrow Table with mixed int and float columns
std::shared_ptr<arrow::Table> create_mixed_table() {
    // 1. Create integer array
    arrow::Int32Builder int_builder;
    std::vector<int32_t> int_data = {1, 2, 3, 4, 5};
    ARROW_CHECK_OK(int_builder.AppendValues(int_data));
    std::shared_ptr<arrow::Array> int_array;
    ARROW_CHECK_OK(int_builder.Finish(&int_array));

    // 2. Create float array
    arrow::DoubleBuilder float_builder;
    std::vector<double> float_data = {1.1, 2.2, 3.3, 4.4, 5.5};
    ARROW_CHECK_OK(float_builder.AppendValues(float_data));
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
        return nullptr; // Or throw an exception, depending on your error handling policy
    }
    return table;
}

NB_MODULE(odin_stream, m) { // Changed module name to avoid collision and be more descriptive
    // Import PyArrow (if needed, but nanobind-pyarrow often handles this)
    // static nb::detail::pyarrow::ImportPyarrow pyarrow_module;

    // Expose the function to create the table
    m.def("create_table_with_mixed_data", &create_mixed_table,
          "Creates an Arrow table with integer and float columns.");

    // Example function (from the original code)
     m.def("my_pyarrow_function", [](std::shared_ptr<arrow::DoubleArray> arr) {
         auto data = arr->data()->Copy();
         return std::make_shared<arrow::DoubleArray>(std::move(data));
     });

      m.def("test_table", [](std::shared_ptr<arrow::Table> table) {
         return arrow::Table::Make(table->schema(), table->columns(), table->num_rows());
     });
}