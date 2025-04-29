#include <nanobind_pyarrow/pyarrow_import.h>

#include "descriptor/parameter_descriptor.h"
#include "descriptor/type_descriptor.h"
#include "fixed_size_parameter.h"
#include "parameterset.h"
#include "stream_processor.h"

namespace nb = nanobind;

// Nanobind automatically converts std::vector<nb::bytes> to a Python list
NB_MODULE(odin_stream_cpp, m) {  // Changed module name to avoid collision and be more descriptive
	static nb::detail::pyarrow::ImportPyarrow module;
	init_type_descriptor(m);       // Initialize the TypeDescriptor bindings
	init_parameter_descriptor(m);  // Initialize the ParameterDescriptor bindings
	init_fixed_size_parameter(m);  // Initialize the FixedSizeParameter bindings
	init_parameterset(m);          // Initialize the ParameterSet bindings
	init_stream_processor(m);      // Initialize the OdinStreamDecoder bindings
}