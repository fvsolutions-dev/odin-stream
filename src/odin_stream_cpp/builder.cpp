#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind_pyarrow/table.h>

// Arrow headers
#include <arrow/api.h>
#include <arrow/builder.h>
#include <arrow/io/api.h>  // Potentially needed, good to include
#include <arrow/ipc/api.h> // Potentially needed, good to include
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

#include "builder.h"
#include "descriptor/parameter_descriptor.h"
#include "descriptor/type_descriptor.h"

#define ARROW_THROW_NOT_OK(status)                                     \
	do                                                                 \
	{                                                                  \
		arrow::Status _s = (status);                                   \
		if (!_s.ok())                                                  \
		{                                                              \
			throw std::runtime_error("Arrow Error: " + _s.ToString()); \
		}                                                              \
	} while (0)

PrimitiveBuilder::PrimitiveBuilder(std::string name, std::shared_ptr<PrimitiveTypeDescriptor> type_descriptor) : name(name), type_descriptor(type_descriptor)
{
	arrow_type = type_descriptor->get_arrow_type();
	data_vector.reserve(1024);
}

void PrimitiveBuilder::add_data(const uint8_t *data, size_t size)
{
	// check if the size is correct
	if (data == nullptr || size != type_descriptor->get_size())
	{
		throw std::length_error("Invalid data size provided. Expected " + std::to_string(type_descriptor->get_size()) + " bytes, but got " +
								std::to_string(size) + " bytes.");
	}
	data_vector.insert(data_vector.end(), data, data + size);
}

template <typename ArrowType, typename BuilderType>
arrow::Status PrimitiveBuilder::append_values(arrow::ArrayBuilder *builder, const uint8_t *data, size_t size)
{
	auto specific_builder = dynamic_cast<BuilderType *>(builder);
	if (!specific_builder)
	{
		return arrow::Status::TypeError("Internal error: Builder type mismatch.");
	}

	// Reinterpret cast and append
	arrow::Status status = specific_builder->AppendValues(reinterpret_cast<const typename ArrowType::c_type *>(data), size / sizeof(typename ArrowType::c_type), nullptr);
	return status;
}

void PrimitiveBuilder::add_data_to_builder(arrow::ArrayBuilder *builder, const uint8_t *data, size_t size)
{
	arrow::Status st = arrow::Status::NotImplemented("Type not handled in add_data: ", type_descriptor->get_name());

	// Dispatch based on the stored arrow_type's ID
	switch (arrow_type->id())
	{
	case arrow::Type::INT8:
		st = append_values<arrow::Int8Type, arrow::Int8Builder>(builder, data, size);
		break;
	case arrow::Type::UINT8:
		st = append_values<arrow::UInt8Type, arrow::UInt8Builder>(builder, data, size);
		break;
	case arrow::Type::INT16:
		st = append_values<arrow::Int16Type, arrow::Int16Builder>(builder, data, size);
		break;
	case arrow::Type::UINT16:
		st = append_values<arrow::UInt16Type, arrow::UInt16Builder>(builder, data, size);
		break;
	case arrow::Type::INT32:
		st = append_values<arrow::Int32Type, arrow::Int32Builder>(builder, data, size);
		break;
	case arrow::Type::UINT32:
		st = append_values<arrow::UInt32Type, arrow::UInt32Builder>(builder, data, size);
		break;
	case arrow::Type::INT64:
		st = append_values<arrow::Int64Type, arrow::Int64Builder>(builder, data, size);
		break;
	case arrow::Type::UINT64:
		st = append_values<arrow::UInt64Type, arrow::UInt64Builder>(builder, data, size);
		break;
	case arrow::Type::FLOAT:
		st = append_values<arrow::FloatType, arrow::FloatBuilder>(builder, data, size);
		break;
	case arrow::Type::DOUBLE:
		st = append_values<arrow::DoubleType, arrow::DoubleBuilder>(builder, data, size);
		break;
	case arrow::Type::BOOL: {
		// Wire bools are one byte each; BooleanBuilder consumes them as raw
		// bytes (zero/non-zero), not via the generic c_type template path.
		auto *bool_builder = dynamic_cast<arrow::BooleanBuilder *>(builder);
		if (!bool_builder)
		{
			st = arrow::Status::TypeError("Internal error: Builder type mismatch.");
			break;
		}
		st = bool_builder->AppendValues(data, static_cast<int64_t>(size));
		break;
	}

	default:
		// Error status is already set before the switch
		break;
	}


	ARROW_THROW_NOT_OK(st); // Throw if any error occurred during append
}

std::vector<std::pair<std::shared_ptr<arrow::Array>, std::shared_ptr<arrow::Field>>> PrimitiveBuilder::finish()
{

	// Create the appropriate builder
	std::unique_ptr<arrow::ArrayBuilder> builder;
	ARROW_THROW_NOT_OK(arrow::MakeBuilder(arrow::default_memory_pool(), arrow_type, &builder));

	if (!builder)
	{
		throw std::runtime_error("Failed to create Arrow builder for type: " + arrow_type->ToString());
	}

	// Add data to the builder
	add_data_to_builder(builder.get(), data_vector.data(), data_vector.size());

	std::shared_ptr<arrow::Array> array;
	ARROW_THROW_NOT_OK(builder->Finish(&array));

	// Flush the vector
	data_vector.clear();

	return {{array, arrow::field(name, arrow_type)}};
}

CompositeBuilder::CompositeBuilder(std::string name, std::shared_ptr<ParameterDescriptor> parameter) : name(name)
{
	auto type = std::dynamic_pointer_cast<CompositeTypeDescriptor>(parameter->get_type_descriptor());
	if (!type)
	{
		throw std::runtime_error("Expected a struct type descriptor in CompositeBuilder.");
	}
	build_from_descriptor(type);
}

CompositeBuilder::CompositeBuilder(std::string name, std::shared_ptr<CompositeTypeDescriptor> type_descriptor) : name(name)
{
	build_from_descriptor(type_descriptor);
}

void CompositeBuilder::build_from_descriptor(std::shared_ptr<CompositeTypeDescriptor> struct_desc)
{
	// One column per field, named `<name>.<field>`.
	for (const auto &field : struct_desc->members)
	{
		auto field_name = field.first;
		auto field_descriptor = field.second;

		if (auto primitive_shared_desc = std::dynamic_pointer_cast<PrimitiveTypeDescriptor>(field_descriptor))
		{
			std::string merged_name = name + "." + field_name;
			builders.push_back(std::make_shared<PrimitiveBuilder>(merged_name, primitive_shared_desc));
		}
		else if (std::dynamic_pointer_cast<CompositeTypeDescriptor>(field_descriptor))
		{
			throw std::runtime_error("Nested structs are not yet supported in CompositeBuilder.");
		}
		else
		{
			throw std::runtime_error("Unsupported type in CompositeBuilder.");
		}
	}
}

void CompositeBuilder::add_data(const uint8_t *data, size_t size)
{
	const uint8_t *data_ptr = data;
	for (const auto &builder : builders)
	{
		size_t field_size = builder->get_size();
		builder->add_data(data_ptr, field_size);
		data_ptr += field_size; // Move the pointer forward by the size of the field
	}
}

std::vector<std::pair<std::shared_ptr<arrow::Array>, std::shared_ptr<arrow::Field>>> CompositeBuilder::finish()
{
	std::vector<std::pair<std::shared_ptr<arrow::Array>, std::shared_ptr<arrow::Field>>> result;
	for (const auto &field_builder : builders)
	{
		auto data = field_builder->finish(); // Call finish on each field builder

		for (const auto &[array, field] : data)
		{
			// Create a new field with the same name and type
			auto new_field = arrow::field(field->name(), field->type());
			result.push_back({array, new_field});
		}
	}
	return result;
}

ArrayBuilder::ArrayBuilder(std::string name, std::shared_ptr<GenericTypeDescriptor> element, size_t count) : name(name)
{
	element_size = element->get_size();
	if (element_size == 0)
	{
		throw std::runtime_error("Array element size must be greater than 0.");
	}
	for (size_t i = 0; i < count; ++i)
	{
		std::string element_name = name + "[" + std::to_string(i) + "]";
		if (auto primitive = std::dynamic_pointer_cast<PrimitiveTypeDescriptor>(element))
		{
			elements.push_back(std::make_shared<PrimitiveBuilder>(element_name, primitive));
		}
		else if (auto composite = std::dynamic_pointer_cast<CompositeTypeDescriptor>(element))
		{
			// `<name>[i].<field>` columns.
			elements.push_back(std::make_shared<CompositeBuilder>(element_name, composite));
		}
		else
		{
			throw std::runtime_error("Unsupported element type in ArrayBuilder.");
		}
	}
}

void ArrayBuilder::add_data(const uint8_t *data, size_t size)
{
	if (size != element_size * elements.size())
	{
		throw std::length_error("Invalid array data size provided. Expected " + std::to_string(element_size * elements.size()) +
								" bytes, but got " + std::to_string(size) + " bytes.");
	}
	const uint8_t *data_ptr = data;
	for (const auto &element : elements)
	{
		element->add_data(data_ptr, element_size);
		data_ptr += element_size;
	}
}

std::vector<std::pair<std::shared_ptr<arrow::Array>, std::shared_ptr<arrow::Field>>> ArrayBuilder::finish()
{
	std::vector<std::pair<std::shared_ptr<arrow::Array>, std::shared_ptr<arrow::Field>>> result;
	for (const auto &element : elements)
	{
		auto data = element->finish();
		for (const auto &[array, field] : data)
		{
			result.push_back({array, arrow::field(field->name(), field->type())});
		}
	}
	return result;
}
