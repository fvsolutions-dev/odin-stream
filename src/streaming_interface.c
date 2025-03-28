#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "streaming_payload.h"
#include "streaming_interface.h"

int streaming_interface_generate_identifier_packet(streaming_parameterset_t *params, uint8_t *buffer, int buffer_size) {
	// Derive a aligned buffer so we can use 32 bit operations
	uint32_t *buffer_32_align = (uint32_t *)(buffer + sizeof(streaming_packet_header_t));
	for (int i = 0; i < params->parameter_count; i++) {
		const fixed_size_parameter_t *parameter = params->parameters[i];
		buffer_32_align[i] = parameter->index;
	}

	streaming_packet_header_t *header = (streaming_packet_header_t *)buffer;
	header->type = STREAMING_INTERFACE_PACKET_TYPE_IDENTIFIER;
	header->parameter_group_hash = params->parameter_hash;
	header->reserved = 0;
	header->odin_definition_hash = 0xDEADBEEF;  // TODO
	return sizeof(streaming_packet_header_t) + params->parameter_count * sizeof(uint32_t);
}

int streaming_interface_generate_data_packet(streaming_parameterset_t *params, uint8_t *buffer, int buffer_size,uint32_t timestamp) { 
	size_t buffer_offset = sizeof(streaming_data_header_t);
	for (int i = 0; i < params->parameter_count; i++) {
		const fixed_size_parameter_t *parameter = params->parameters[i];

		// We currenly only allow streaming of a single parameter
		int read_size = parameter->size;

		if (buffer_offset + read_size > buffer_size) {
			// Buffer overflow
			return -1;
		}

		// Copy the data
		memcpy(buffer + buffer_offset, parameter->data, read_size);
		buffer_offset += read_size;
	}

	streaming_data_header_t *header = (streaming_data_header_t *)buffer;
	header->type = STREAMING_INTERFACE_PACKET_TYPE_DATA;
	header->parameter_group_hash = params->parameter_hash;
	header->reserved = 0;
	header->timestamp = timestamp;
	
	return buffer_offset;
}
