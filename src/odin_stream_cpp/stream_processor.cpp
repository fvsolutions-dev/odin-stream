#include "stream_processor.h"

extern "C" {
#include "embedded_odin_stream/stream_packet.h"
}

namespace nb = nanobind;

StreamProcessor::StreamProcessor(std::shared_ptr<ParameterMapDescriptor> parameter_map) : parameter_map(parameter_map) {}

void StreamProcessor::process_bytes_list(nb::list bytes_list) {
	for (const auto& handle : bytes_list) {
		nb::bytes item = nb::cast<nb::bytes>(handle);

		if (item.size() < sizeof(streaming_data_packet_header_t)) {
			fprintf(stderr, "Warning: Input data too small (%zu bytes) to contain expected header. Skipping.\n", item.size());
			continue;
		}

		const streaming_data_packet_header_t* header = reinterpret_cast<const streaming_data_packet_header_t*>(item.c_str());

		auto it = parameter_sets_map.find(header->header.identifier);

		if (header->header.type == STREAM_STREAM_PACKET_TYPE_IDENTIFIER) {
			// If it exists, skip
			if (it != parameter_sets_map.end()) {
				continue;
			}

			std::shared_ptr<ParameterSet> parsed_set = ParameterSet::from_identifier_data(item, parameter_map);
			uint16_t identifier = parsed_set->get_hash();

			parameter_sets_map.insert_or_assign(identifier, parsed_set);

			printf("Stored/Updated ParameterSet with ID: %hu\n", identifier);
		}
		// // --- Process Data Packet ---
		else if (header->header.type == STREAM_STREAM_PACKET_TYPE_DATA) {
			uint16_t identifier = header->header.identifier;

			if (it == parameter_sets_map.end()) {
				// Parameter not yet defined
				continue;
			}
			ParameterSet& param_set = *it->second;
			param_set.parse_data_packet(item);

		} else {
			fprintf(stderr, "Warning: Encountered unknown packet type: %hu\n", header->header.type);
		}
	}
}

void StreamProcessor::clear_parameter_sets() {
	parameter_sets_map.clear();
	printf("Cleared all stored ParameterSets.\n");
}

size_t StreamProcessor::get_parameter_set_count() const { return parameter_sets_map.size(); }

std::shared_ptr<ParameterSet> StreamProcessor::get_parameter_set(uint16_t identifier) {
	auto it = parameter_sets_map.find(identifier);
	if (it != parameter_sets_map.end()) {
		return it->second;
	} else {
		throw nb::key_error("ParameterSet with the given identifier not found.");
	}
}

void init_stream_processor(nb::module_& m) {
	using namespace nb::literals;

	nb::class_<StreamProcessor>(m, "StreamProcessor")
		.def(nb::init<std::shared_ptr<ParameterMapDescriptor>>(), "parameter_map"_a, "Constructor for the StreamProcessor. Initializes with the parameter map.")
		.def("process_bytes_list", &StreamProcessor::process_bytes_list, "bytes_list"_a, "Process a list of bytes.")
		.def("get_parameter_set_count", &StreamProcessor::get_parameter_set_count, "Get the number of stored ParameterSets.")
		.def("get_parameter_set", &StreamProcessor::get_parameter_set, "identifier"_a, "Get a ParameterSet by its identifier.")
		.def("clear_parameter_sets", &StreamProcessor::clear_parameter_sets, "Clear all stored ParameterSets.");
}