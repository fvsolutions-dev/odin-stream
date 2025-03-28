#include "streaming_payload.h"

typedef enum {
	STREAMING_INTERFACE_PACKET_TYPE_IDENTIFIER = 0x01,
	STREAMING_INTERFACE_PACKET_TYPE_DATA = 0x02,
} streaming_interface_packet_type_t;

#pragma pack(push, 1)
typedef struct {
	uint8_t type;
	uint16_t parameter_group_hash;
	uint8_t reserved;
	uint32_t odin_definition_hash;
} streaming_packet_header_t;

typedef struct {
	uint8_t type;
	uint16_t parameter_group_hash;
	uint8_t reserved;
	uint32_t timestamp;
} streaming_data_header_t;
#pragma pack(pop)



int streaming_interface_generate_identifier_packet(streaming_parameterset_t *params, uint8_t *buffer, int buffer_size);
int streaming_interface_generate_data_packet(streaming_parameterset_t *params, uint8_t *buffer, int buffer_size,uint32_t timestamp);