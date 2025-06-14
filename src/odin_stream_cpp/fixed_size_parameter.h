#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind_pyarrow/table.h>

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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "descriptor/parameter_descriptor.h"
#include "descriptor/type_descriptor.h"
#include "builder.h"


class FixedSizeParameter {
   private:
	uint32_t index;
	uint16_t size;

	std::shared_ptr<ParameterDescriptor> descriptor;
	std::shared_ptr<GenericBuilder> builder;

   public:
	FixedSizeParameter(uint32_t idx, uint16_t data_size, std::shared_ptr<ParameterDescriptor> parameter) : index(idx), size(data_size), descriptor(parameter) {
		if (size == 0) {
			throw std::invalid_argument("Size must be greater than 0.");
		}
		auto type_descriptor = parameter->get_type_descriptor();

		printf("New FixedSizeParameter: '%s'\n", std::string(parameter->get_name()).c_str());
		// print type descriptor
		printf("Type descriptor: %s\n", parameter->repr().c_str());
		if (auto primitive_shared_desc = std::dynamic_pointer_cast<PrimitiveTypeDescriptor>(type_descriptor)) {
			builder = std::make_shared<PrimitiveBuilder>(std::to_string(index), primitive_shared_desc);

		} else if (auto struct_shared_desc = std::dynamic_pointer_cast<CompositeTypeDescriptor>(type_descriptor)) {
			builder = std::make_shared<CompositeBuilder>(std::to_string(index), parameter);

		} else {
			throw std::runtime_error("Unsupported type descriptor for FixedSizeParameter.");
		}
	}

	// Destructor
	~FixedSizeParameter() {
		// No explicit cleanup needed, unique_ptr will handle it
	}

	void add_data(const uint8_t* data, size_t size) {
		if (size != this->size) {
			throw std::length_error("Invalid data size provided. Expected " + std::to_string(this->size) + " bytes, but got " + std::to_string(size) +
			                        " bytes.");
		}
		builder->add_data(data, size);
	}

	uint32_t get_index() const { return index; }
	uint16_t get_size() const { return size; }
	std::shared_ptr<ParameterDescriptor> get_parameter() const { return descriptor; }
	std::string repr() const { return "FixedSizeParameter()"; }

	std::vector<std::pair<std::shared_ptr<arrow::Array>, std::shared_ptr<arrow::Field>>> finish() {
		return builder->finish();
	}

	uint32_t get_datapoint_count() const { return builder->get_datapoint_count(); }
};

void init_fixed_size_parameter(nanobind::module_& m);