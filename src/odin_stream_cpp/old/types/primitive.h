#pragma once
#include <arrow/type.h>
#include <nanobind/nanobind.h>
#include <stdlib.h>

enum class PrimitiveType { INT8, UINT8, INT16, UINT16, INT32, UINT32, INT64, UINT64, FLOAT32, FLOAT64, BOOL, CHAR };

class PrimitiveTypeDescriptor : public TypeDescriptor {
   private:
	PrimitiveType type;
	size_t size;  // Cache size

   public:
	PrimitiveTypeDescriptor(PrimitiveType t);
	PrimitiveType get_primitive_type() const { return type; }

	std::shared_ptr<arrow::DataType> get_arrow_type() const {
		switch (type) {
			case PrimitiveType::INT8:
				return arrow::int8();
			case PrimitiveType::UINT8:
				return arrow::uint8();
			case PrimitiveType::INT16:
				return arrow::int16();
			case PrimitiveType::UINT16:
				return arrow::uint16();
			case PrimitiveType::INT32:
				return arrow::int32();
			case PrimitiveType::UINT32:
				return arrow::uint32();
			case PrimitiveType::FLOAT32:
				return arrow::float32();
			case PrimitiveType::INT64:
				return arrow::int64();
			case PrimitiveType::UINT64:
				return arrow::uint64();
			case PrimitiveType::FLOAT64:
				return arrow::float64();
			case PrimitiveType::BOOL:
				return arrow::boolean();
			case PrimitiveType::CHAR:
				return arrow::utf8();  // Assuming CHAR is represented as a string
			default:
				throw std::runtime_error("Unknown primitive type in C++.");
		}
	}
    
	size_t get_size() const override { return size; }

	std::string repr() const override {
		return "PrimitiveTypeDescriptor(type=" + std::to_string(static_cast<int>(type)) + ", size=" + std::to_string(size) + ")";
	}
};

void init_primitive(nanobind::module_& m);
