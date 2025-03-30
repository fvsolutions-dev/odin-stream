#include "odin_stream.h"
#include "odin_lookup.h"

stream_parameter_set_status_t parameter_set_add_parameter(stream_parameter_set_t *set,
                                                          const ODIN_parameter_t *parameter)
{
    return stream_parameter_set_add(set,
                                    (stream_fixed_size_parameter_t) { .index = parameter->global_index,
                                                                      .size  = ODIN_get_max_data_size(parameter),
                                                                      .data  = parameter->data });
}

stream_parameter_set_status_t parameter_set_add_parameter_group(stream_parameter_set_t       *set,
                                                                const ODIN_parameter_group_t *group)
{
    for (int i = 0; i < group->count; i++)
    {
        stream_parameter_set_status_t ret = parameter_set_add_parameter(set, group->parameters[i]);
        if (ret != STREAM_PARAM_SET_SUCCESS)
        {
            return ret;
        }
    }
    return STREAM_PARAM_SET_SUCCESS;
}

void decoding_manager_init(decoding_manager_t *manager)
{
    manager->count     = 0;
    manager->max_count = sizeof(manager->data) / sizeof(header_set_t);
}

void decoding_manager_find_identifier(decoding_manager_t *manager, uint16_t identifier, header_set_t **header)
{
    for (size_t i = 0; i < manager->count; i++)
    {
        if (manager->data[i].packet_identifier == identifier)
        {
            *header = &manager->data[i];
            return;
        }
    }
    *header = NULL;
}

void decoding_manager_parse_packet(decoding_manager_t           *manager,
                                   uint8_t                      *data,
                                   size_t                        length,
                                   const ODIN_parameter_group_t *group)
{

    // check if length is large enough for header
    if (length < sizeof(stream_packet_header_t))
    {
        return;
    }

    // Check packet id
    stream_packet_header_t *header = (stream_packet_header_t *)data;

    header_set_t *header_set = NULL;
    decoding_manager_find_identifier(manager, header->identifier, &header_set);

    switch (header->type)
    {
        case STREAM_STREAM_PACKET_TYPE_IDENTIFIER:

            // Identifier already known, ignore packet
            if (header_set != NULL)
            {
                return;
            }

            // Check if we have space for a new packet id
            if (manager->count >= manager->max_count)
            {
                // No space for new packet id, ignore packet
                return;
            }
            stream_parameter_set_t *parameter_set = stream_packet_parse_identifier(data, length);

            if (parameter_set == NULL)
            {
                // Failed to parse identifier packet, ignore it
                printf("Failed to parse identifier packet\n");
                return;
            }

            // Find and populate the parameters
            for (size_t i = 0; i < parameter_set->parameter_count; i++)
            {
                int                     id    = parameter_set->parameters[i].index;
                const ODIN_parameter_t *param = ODIN_get_parameter_by_id(group, id, 0);
                if (param == NULL)
                {
                    printf("Parameter %d not found in group\n", id);
                    stream_parameter_set_destroy(parameter_set);

                    return;
                }

                // Check if the size matches
                if (parameter_set->parameters[i].size != ODIN_get_max_data_size(param))
                {
                    printf("Parameter %d size mismatch\n", id);
                    stream_parameter_set_destroy(parameter_set);
                    return;
                }

                // Add the parameter to the data field
                parameter_set->parameters[i].data = param->data;
            }

            // Add the new header set to the manager
            manager->data[manager->count] = (header_set_t) {
                .packet_identifier = header->identifier,
                .parameter_set     = parameter_set,
            };
            manager->count++;
            break;

        case STREAM_STREAM_PACKET_TYPE_DATA: {
            if (header_set == NULL)
            {
                // No matching identifier packet, ignore data packet
                return;
            }

            stream_packet_status_t status = stream_packet_parse_data(data, length, header_set->parameter_set);
            if (status != STREAM_PACKET_SUCCESS)
            {
                // Failed to parse data packet, ignore it
                printf("Failed to parse data packet: %d\n", status);
                return;
            }

            break;
        }
        default:
            return;
    }

    // Check if packet id is already in use
}