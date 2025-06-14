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

class GenericBuilder {
   public:
	virtual void add_data(const uint8_t* data, size_t size) = 0;
	virtual uint32_t get_datapoint_count() const = 0;
	virtual std::vector<std::pair<std::shared_ptr<arrow::Array>, std::shared_ptr<arrow::Field>>> finish() = 0;
};

class PrimitiveBuilder : public GenericBuilder {
   public:
	std::string name;
	std::vector<uint8_t> data_vector;
	std::shared_ptr<PrimitiveTypeDescriptor> type_descriptor;
	std::shared_ptr<arrow::DataType> arrow_type;

   public:
	PrimitiveBuilder(std::string name, std::shared_ptr<PrimitiveTypeDescriptor> type_descriptor);

	void add_data(const uint8_t* data, size_t size);
	template <typename ArrowType, typename BuilderType>
	arrow::Status append_values(arrow::ArrayBuilder* builder, const uint8_t* data, size_t size);
	void add_data_to_builder(arrow::ArrayBuilder* builder, const uint8_t* data, size_t size);
	std::vector<std::pair<std::shared_ptr<arrow::Array>, std::shared_ptr<arrow::Field>>> finish();

	size_t get_size() const { return type_descriptor->get_size(); }
    uint32_t get_datapoint_count() const { return data_vector.size() / type_descriptor->get_size(); }
};

class CompositeBuilder : public GenericBuilder {
   public:
	std::string name;
	std::vector<std::shared_ptr<PrimitiveBuilder>> builders;

   public:
	CompositeBuilder(std::string name, std::shared_ptr<ParameterDescriptor> parameter);
	void add_data(const uint8_t* data, size_t size);

	uint32_t get_datapoint_count() const { 
        // Get the number of data points from the builders check they are all the same
        if (builders.empty()) {
            throw std::runtime_error("No builders available.");
        }
        uint32_t datapoints = builders[0]->get_datapoint_count();
        for (const auto& builder : builders) {
            if (builder->get_datapoint_count() != datapoints) {
                throw std::runtime_error("Inconsistent number of data points across builders.");
            }
        }
        return datapoints;
    }
    
	std::vector<std::pair<std::shared_ptr<arrow::Array>, std::shared_ptr<arrow::Field>>> finish();
};