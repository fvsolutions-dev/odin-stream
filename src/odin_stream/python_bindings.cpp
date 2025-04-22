#include <arrow/api.h>
#include <arrow/io/api.h>
#include <arrow/ipc/api.h>
#include <arrow/util/logging.h>
// #include <nanobind/nanobind.h>
// #include <nanobind/stl/list.h>
// #include <nanobind/stl/shared_ptr.h>  // Make sure to include this for shared_ptr support
// #include <nanobind/stl/string.h>
// #include <nanobind/stl/unordered_map.h>  // May be needed for binding map access if desired
// #include <nanobind/stl/vector.h>
// #include <nanobind_pyarrow/pyarrow_import.h>
// #include <nanobind_pyarrow/table.h>

// #include "enums.hpp"
#include "fixed_size_parameter.h"
#include "parameterset.h"
#include "stream_processor.h"
#include "types/primitive.h"
#include "types/struct.h"
#include "types/typedescriptors.h"

namespace nb = nanobind;

// Nanobind automatically converts std::vector<nb::bytes> to a Python list
NB_MODULE(odin_stream, m) {  // Changed module name to avoid collision and be more descriptive
	static nb::detail::pyarrow::ImportPyarrow module;
	// init_enums(m);
	init_fixed_size_parameter(m);  // Initialize the FixedSizeParameter bindings
	init_parameterset(m);          // Initialize the ParameterSet bindings
	init_primitive(m);             // Initialize the PrimitiveTypeDescriptor bindings
	init_struct(m);                // Initialize the StructDescriptor bindings
	init_stream_processor(m);      // Initialize the StreamProcessor bindings
	init_type_desciptors(m);
}