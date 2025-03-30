#include "odin_stream/stream_packet.h"
#include "odin_stream/stream_parameter_set.h"
#include "odin_core.h"

typedef struct
{
    uint16_t packet_identifier;
    parameter_set_t *parameter_set;
} header_set_t;

typedef struct
{
    header_set_t data[16];
    size_t count;
    size_t max_count;
} decoding_manager_t;


parameter_set_status_t parameter_set_add_parameter(parameter_set_t *set, const ODIN_parameter_t *parameter);
parameter_set_status_t parameter_set_add_parameter_group(parameter_set_t *set, const ODIN_parameter_group_t *group);
void decoding_manager_parse_packet(decoding_manager_t *manager, uint8_t *data, size_t length, const ODIN_parameter_group_t *group);
void decoding_manager_init(decoding_manager_t *manager);
