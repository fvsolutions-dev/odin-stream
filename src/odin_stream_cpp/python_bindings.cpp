// #include <arrow/api.h>
// #include <arrow/io/api.h>
// #include <arrow/ipc/api.h>
// #include <arrow/util/logging.h>
// #include "parameterset.h"
// #include "stream_processor.h"
// #include "types/primitive.h"
// #include "types/struct.h"
// #include "types/typedescriptors.h"
#include "fixed_size_parameter.h"
#include "descriptor/type_descriptor.h"
#include "descriptor/parameter_descriptor.h"
#include <nanobind_pyarrow/pyarrow_import.h>


namespace nb = nanobind;

// Nanobind automatically converts std::vector<nb::bytes> to a Python list
NB_MODULE(odin_stream_cpp, m) {  // Changed module name to avoid collision and be more descriptive
	static nb::detail::pyarrow::ImportPyarrow module;
	init_type_descriptor(m);  // Initialize the TypeDescriptor bindings
	init_parameter_descriptor(m);  // Initialize the ParameterDescriptor bindings
	init_fixed_size_parameter(m);  // Initialize the FixedSizeParameter bindings
	// init_parameterset(m);          // Initialize the ParameterSet bindings
	// init_primitive(m);             // Initialize the PrimitiveTypeDescriptor bindings
	// init_struct(m);                // Initialize the StructDescriptor bindings
	// init_stream_processor(m);      // Initialize the StreamProcessor bindings
	// init_type_desciptors(m);
}