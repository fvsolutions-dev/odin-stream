#pragma once
#include <stdlib.h>
#include <nanobind/nanobind.h>

enum class PrimitiveType { INT8, UINT8, INT16, UINT16, INT32, UINT32, INT64, UINT64, FLOAT32, FLOAT64, BOOL };

// Base class or variant alternative for type information
class TypeDescriptor {
   public:
	virtual ~TypeDescriptor() = default;
	// Returns size in bytes
	virtual size_t get_size() const = 0;

    virtual std::string repr() const {
        return "TypeDescriptor()";  // Placeholder, replace with actual representation
    }

};

class PrimitiveTypeDescriptor : public TypeDescriptor {
   private:
	PrimitiveType type;
	size_t size;  // Cache size

   public:
	PrimitiveTypeDescriptor(PrimitiveType t);
	PrimitiveType get_primitive_type() const { return type; }

	size_t get_size() const override { return size; }

    std::string repr() const override {
        return "PrimitiveTypeDescriptor(type=" + std::to_string(static_cast<int>(type)) + ", size=" + std::to_string(size) + ")";
    }
};

void init_primitive(nanobind::module_& m);
