#include <stdint.h>
#include <stddef.h>

#ifndef STREAMING_PAYLOAD_H
#define STREAMING_PAYLOAD_H

typedef struct
{
    uint32_t index;
    uint32_t size;
    uint8_t *data;
} fixed_size_parameter_t;


typedef struct
{
    const fixed_size_parameter_t **parameters;
    size_t                   parameter_count;
    size_t                   parameter_count_max;
    uint16_t                 parameter_hash;
} streaming_parameterset_t;

streaming_parameterset_t *streaming_payload_new(int max_parameters);

int streaming_payload_add(streaming_parameterset_t *parameterset, const fixed_size_parameter_t *parameter);
int streaming_payload_remove(streaming_parameterset_t *parameterset, const fixed_size_parameter_t *parameter);
int streaming_payload_clear(streaming_parameterset_t *parameterset);

#endif // STREAMING_PAYLOAD_H