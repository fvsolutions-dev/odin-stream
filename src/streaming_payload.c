#include "streaming_payload.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int streaming_payload_update_hash(streaming_parameterset_t *parameterset);

streaming_parameterset_t *streaming_payload_new(int max_parameters)
{
    streaming_parameterset_t *parameterset = malloc(sizeof(streaming_parameterset_t));
    if (parameterset == NULL)
    {
        return NULL;
    }

    parameterset->parameters = malloc(max_parameters * sizeof(fixed_size_parameter_t *));
    if (parameterset->parameters == NULL)
    {
        free(parameterset);
        return NULL;
    }

    parameterset->parameter_count     = 0;
    parameterset->parameter_count_max = max_parameters;
    streaming_payload_update_hash(parameterset);
    return parameterset;
}

int streaming_payload_add(streaming_parameterset_t *parameterset, const fixed_size_parameter_t *parameter)
{
    // Check if parameter is already in the set
    for (int i = 0; i < parameterset->parameter_count; i++)
    {
        if (parameterset->parameters[i] == parameter)
        {
			printf("Parameter already exists in the set %p=%p\n", parameterset->parameters[i], parameter);
            return -1;
        }
    }

    if (parameterset->parameter_count >= parameterset->parameter_count_max)
    {
        return -1;
    }

    parameterset->parameters[parameterset->parameter_count++] = parameter;
    streaming_payload_update_hash(parameterset);

    return 0;
}

int streaming_payload_remove(streaming_parameterset_t *parameterset, const fixed_size_parameter_t *parameter)
{
    for (int i = 0; i < parameterset->parameter_count; i++)
    {
        if (parameterset->parameters[i] == parameter)
        {
            // Remove the parameter
            parameterset->parameters[i] = parameterset->parameters[parameterset->parameter_count - 1];
            parameterset->parameter_count--;
            streaming_payload_update_hash(parameterset);
            return 0;
        }
    }

    return -1;
}

int streaming_payload_clear(streaming_parameterset_t *parameterset)
{
    parameterset->parameter_count = 0;
    streaming_payload_update_hash(parameterset);
    return 0;
}

static uint16_t crc16(uint16_t crc, const uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        crc ^= ((uint16_t)data[i]) << 8;
        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

static int streaming_payload_update_hash(streaming_parameterset_t *parameterset)
{
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < parameterset->parameter_count; i++)
    {
        uint32_t index = parameterset->parameters[i]->index;
        crc            = crc16(crc, (uint8_t *)&index, sizeof(index));
    }
    parameterset->parameter_hash = crc;
    return 0;
}
