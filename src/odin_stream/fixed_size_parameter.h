#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind_pyarrow/table.h>
#include "./types/typedescriptors.h"

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

#include "./types/struct.h"

#define ARROW_THROW_NOT_OK(status)                                     \
	do {                                                               \
		arrow::Status _s = (status);                                   \
		if (!_s.ok()) {                                                \
			throw std::runtime_error("Arrow Error: " + _s.ToString()); \
		}                                                              \
	} while (0)

// Basecalss
class GenericBuilder {
   public:
	virtual ~GenericBuilder() = default;

	virtual void add_data(const uint8_t* data, size_t size) = 0;
	// virtual size_t get_size() const = 0;
	virtual uint32_t get_datapoints() const = 0;
};

class PrimitiveBuilder : public GenericBuilder {
   public:
	std::string name;
	std::shared_ptr<arrow::ArrayBuilder> builder;
	std::shared_ptr<PrimitiveTypeDescriptor> type_descriptor;
	std::shared_ptr<arrow::DataType> arrow_type;
	std::shared_ptr<arrow::Field> arrow_field;
	uint32_t datapoints = 0;

   public:
	PrimitiveBuilder(std::string name, std::shared_ptr<PrimitiveTypeDescriptor> type_descriptor) : name(name), type_descriptor(type_descriptor) {
		arrow_type = type_descriptor->get_arrow_type();
		arrow_field = arrow::field(name, arrow_type);

		// Create the appropriate builder
		std::unique_ptr<arrow::ArrayBuilder> unique_builder;
		ARROW_THROW_NOT_OK(arrow::MakeBuilder(arrow::default_memory_pool(), arrow_type, &unique_builder));

		builder = std::move(unique_builder);

		if (!builder) {
			throw std::runtime_error("Failed to create Arrow builder for type: " + arrow_type->ToString());
		}
	}

	template <typename ArrowType, typename BuilderType>
	arrow::Status append_typed_value(const uint8_t* data, size_t size) {
		// Ensure the builder is actually of the expected type (safety check)
		auto specific_builder = dynamic_cast<BuilderType*>(builder.get());
		if (!specific_builder) {
			return arrow::Status::TypeError("Internal error: Builder type mismatch.");
		}
		// Check size matches type size
		if (size != sizeof(typename ArrowType::c_type)) {
			return arrow::Status::Invalid("Input data size (", size, ") does not match expected size (", sizeof(typename ArrowType::c_type), ") for type ",
			                              arrow_type->ToString());
		}
		// Reinterpret cast and append
		return specific_builder->Append(*reinterpret_cast<const typename ArrowType::c_type*>(data));
	}

	void add_data(const uint8_t* data, size_t size) {
		arrow::Status st = arrow::Status::NotImplemented("Type not handled in add_data: ", arrow_type->ToString());

		// Dispatch based on the stored arrow_type's ID
		switch (arrow_type->id()) {
			case arrow::Type::INT8:
				st = append_typed_value<arrow::Int8Type, arrow::Int8Builder>(data, size);
				break;
			case arrow::Type::UINT8:
				st = append_typed_value<arrow::UInt8Type, arrow::UInt8Builder>(data, size);
				break;
			case arrow::Type::INT16:
				st = append_typed_value<arrow::Int16Type, arrow::Int16Builder>(data, size);
				break;
			case arrow::Type::UINT16:
				st = append_typed_value<arrow::UInt16Type, arrow::UInt16Builder>(data, size);
				break;
			case arrow::Type::INT32:
				st = append_typed_value<arrow::Int32Type, arrow::Int32Builder>(data, size);
				break;
			case arrow::Type::UINT32:
				st = append_typed_value<arrow::UInt32Type, arrow::UInt32Builder>(data, size);
				break;
			case arrow::Type::INT64:
				st = append_typed_value<arrow::Int64Type, arrow::Int64Builder>(data, size);
				break;
			case arrow::Type::UINT64:
				st = append_typed_value<arrow::UInt64Type, arrow::UInt64Builder>(data, size);
				break;
			case arrow::Type::FLOAT:
				st = append_typed_value<arrow::FloatType, arrow::FloatBuilder>(data, size);
				break;
			case arrow::Type::DOUBLE:
				st = append_typed_value<arrow::DoubleType, arrow::DoubleBuilder>(data, size);
				break;

			default:
				// Error status is already set before the switch
				break;
		}

		ARROW_THROW_NOT_OK(st);  // Throw if any error occurred during append
		datapoints++;
	}

	std::shared_ptr<arrow::DataType> get_arrow_type() const { return arrow_type; }
	std::shared_ptr<arrow::Field> get_arrow_field() const { return arrow_field; }

	size_t get_size() const { return type_descriptor->get_size(); }
	uint32_t get_datapoints() const { return datapoints; }
};

class CompositeBuilder : public GenericBuilder {
   public:
	std::string name;
	std::vector<std::shared_ptr<PrimitiveBuilder>> builders;
	uint32_t datapoints = 0;

   public:
	CompositeBuilder(std::string name, std::shared_ptr<StructDescriptor> struct_descriptor) : name(name) {
		// Create the appropriate builders for each field in the struct
		for (const auto& field : struct_descriptor->members) {
			auto field_name = field.first;
			auto field_descriptor = field.second;

			if (auto primitive_shared_desc = std::dynamic_pointer_cast<PrimitiveTypeDescriptor>(field_descriptor)) {
				std::string merged_name = name + "_" + field_name;
				builders.push_back(std::make_shared<PrimitiveBuilder>(merged_name, primitive_shared_desc));

			} else if (auto struct_desc = dynamic_cast<StructDescriptor*>(field_descriptor.get())) {
				throw std::runtime_error("Nested structs are not supported in CompositeBuilder.");

			} else {
				throw std::runtime_error("Unsupported type in CompositeBuilder.");
			}
		}
	}

	void add_data(const uint8_t* data, size_t size) {
		const uint8_t* data_ptr = data;
		for (const auto& builder : builders) {
			size_t field_size = builder->get_size();
			builder->add_data(data_ptr, field_size);
			data_ptr += field_size;  // Move the pointer forward by the size of the field
		}
		datapoints++;
	}
	uint32_t get_datapoints() const { return datapoints; }
};

class FixedSizeParameter {
   private:
	uint32_t index;
	uint16_t size;

	std::shared_ptr<TypeDescriptor> type_descriptor;
	std::shared_ptr<GenericBuilder> builder;

   public:
	FixedSizeParameter(uint32_t idx, uint16_t data_size, std::shared_ptr<TypeDescriptor> type_descriptor)
		: index(idx), size(data_size), type_descriptor(type_descriptor) {
		if (size == 0) {
			throw std::invalid_argument("Size must be greater than 0.");
		}

		if (auto primitive_shared_desc = std::dynamic_pointer_cast<PrimitiveTypeDescriptor>(type_descriptor)) {
			builder = std::make_shared<PrimitiveBuilder>(std::to_string(index), primitive_shared_desc);

		} else if (auto struct_shared_desc = std::dynamic_pointer_cast<StructDescriptor>(type_descriptor)) {
			builder = std::make_shared<CompositeBuilder>(std::to_string(index), struct_shared_desc);

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

	uint32_t get_index() const;
	void set_index(uint32_t new_index);

	// add data method, to use with Arrow's builder
	uint16_t get_size() const { return size; }

	// --- Other Methods ---
	std::string repr() const { return "FixedSizeParameter()"; }

	std::vector<std::pair<std::shared_ptr<arrow::Array>, std::shared_ptr<arrow::Field>>> finish() {
		if (PrimitiveBuilder* primitive_builder = dynamic_cast<PrimitiveBuilder*>(builder.get())) {
			std::shared_ptr<arrow::Array> array;
			ARROW_THROW_NOT_OK(primitive_builder->builder->Finish(&array));
			return {{array, primitive_builder->get_arrow_field()}};
		} else if (CompositeBuilder* composite_builder = dynamic_cast<CompositeBuilder*>(builder.get())) {
			std::vector<std::pair<std::shared_ptr<arrow::Array>, std::shared_ptr<arrow::Field>>> result;
			for (const auto& field_builder : composite_builder->builders) {
				std::shared_ptr<arrow::Array> array;
				ARROW_THROW_NOT_OK(field_builder->builder->Finish(&array));
				result.push_back({array, field_builder->get_arrow_field()});
			}
			return result;
		} else {
			throw std::runtime_error("Unsupported builder type in FixedSizeParameter::finish.");
		}
	}

	uint32_t get_datapoints() const { return builder->get_datapoints(); }
};

void init_fixed_size_parameter(nanobind::module_& m);