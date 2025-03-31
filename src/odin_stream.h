#include "odin_stream/stream_packet.h"
#include "odin_stream/stream_parameter_set.h"
#include "odin_core.h"
#include <stdint.h>

#ifndef ODIN_STREAM_H
#define ODIN_STREAM_H

typedef struct
{
    uint16_t packet_identifier;
    stream_parameter_set_t *parameter_set;
} header_set_t;

typedef void (*stream_event_callback_t)(const stream_event_t *event);


typedef struct
{
    uint32_t identifier_decoding_errors;
    uint32_t data_decoding_errors;
    uint32_t event_decoding_errors;
    uint32_t other_errors;

    uint32_t received_events_packets; // Total valid events received
    uint32_t received_identifier_packets; // Total valid identifiers received
    uint32_t received_data_packets;// Totaldata packets received
    uint32_t received_unresolved_data_packets; // Total unresolved packets received

    uint32_t identifier_count; // Numver of data formats in the manager
} decoding_manager_statistics_t;
typedef struct
{
    header_set_t data[16];
    size_t count;
    size_t max_count;
    stream_event_callback_t event_callback;
    decoding_manager_statistics_t statistics;
} decoding_manager_t;

stream_parameter_set_status_t parameter_set_add_parameter(stream_parameter_set_t *set,
                                                          const ODIN_parameter_t *parameter);
stream_parameter_set_status_t parameter_set_add_parameter_group(stream_parameter_set_t *set,
                                                                const ODIN_parameter_group_t *group);

void stream_decoding_manager_init(decoding_manager_t *manager, stream_event_callback_t event_callback);
void stream_decoding_manager_parse_packet(decoding_manager_t *manager,
                                          uint8_t *data,
                                          size_t length,
                                          const ODIN_parameter_group_t *group);

#endif // ODIN_STREAM_H