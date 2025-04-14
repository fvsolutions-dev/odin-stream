#pragma once

#include <arrow/builder.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind_pyarrow/table.h>
#include <types/typedescriptors.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <optional>

class FixedSizeParameter {
   private:
	uint32_t index;
	uint16_t size;
	std::optional<std::shared_ptr<TypeDescriptor>> type_descriptor;
	std::vector<uint8_t> packets;
	uint32_t num_packets = 0;  // Number of packets received

   public:
	FixedSizeParameter(uint32_t idx, uint16_t data_size, std::optional<std::shared_ptr<TypeDescriptor>> type_descriptor)
		: index(idx), size(data_size), type_descriptor(type_descriptor) {
		if (size == 0) {
			throw std::invalid_argument("Size must be greater than 0.");
		}
	}
	// Rule 5
	// FixedSizeParameter(const FixedSizeParameter&) = delete;  // Disable copy constructor
	// FixedSizeParameter& operator=(const FixedSizeParameter&) = delete;  // Disable copy assignment
	// FixedSizeParameter(FixedSizeParameter&&) = default;  // Default move constructor
	// FixedSizeParameter& operator=(FixedSizeParameter&&) = default;  // Default move assignment
	uint32_t get_index() const;
	void set_index(uint32_t new_index);

	// add data method, to use with Arrow's builder
	void add_data(const uint8_t* data, size_t size);
	uint32_t get_num_packets() const { return num_packets; }
	uint16_t get_size() const { return size; }
	size_t get_packets_buffer_size() const { return packets.size(); }
	// Clears the stored packet data and resets the count
	void clear_data() {
		packets.clear();
		packets.shrink_to_fit();  // Optional: release memory
		num_packets = 0;
	}

	const std::vector<uint8_t>& get_packets_buffer() const { return packets; }

	// --- Other Methods ---
	std::string repr() const {
		return "FixedSizeParameter(index=" + std::to_string(index) + ", packet_size=" + std::to_string(size) + ", num_packets=" + std::to_string(num_packets) +
		       ", buffer_size=" + std::to_string(packets.size()) + ")";
		// Add more details if needed, but avoid printing large buffers
	}
};

void init_fixed_size_parameter(nanobind::module_& m);