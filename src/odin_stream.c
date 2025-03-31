#include "odin_stream.h"
#include "odin_lookup.h"
#include "odin_stream/stream_packet.h"


//TODO: Pake production worthy

static header_set_t *decoding_manager_find_identifier(decoding_manager_t *manager, uint16_t identifier);

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

void stream_decoding_manager_init(decoding_manager_t *manager, stream_event_callback_t event_callback)
{
    manager->event_callback = event_callback;
    manager->count          = 0;
    manager->max_count      = sizeof(manager->data) / sizeof(header_set_t);
}

void stream_decoding_manager_parse_packet(decoding_manager_t           *manager,
                                          uint8_t                      *data,
                                          size_t                        length,
                                          const ODIN_parameter_group_t *group)
{
    // check if length is large enough for header
    if (length < sizeof(stream_packet_header_t))
    {
        // Not enough data for header, ignore packet
        manager->statistics.other_errors++;
        return;
    }

    // Check packet id
    stream_packet_header_t *header = (stream_packet_header_t *)data;

    if (header->type == STREAM_STREAM_PACKET_TYPE_EVENT)
    {

        stream_event_t event = { 0 };
        if (stream_packet_parse_event(data, length, &event) != STREAM_PACKET_SUCCESS)
        {
            // Failed to parse event packet, ignore it
            manager->statistics.event_decoding_errors++;
            return;
        }

        manager->statistics.received_events_packets++;
        // Call the event callback if set
        if (manager->event_callback != NULL)
        {
            manager->event_callback(&event);
        }
        return;
    }

    // Try to find the identifier in the manager
    header_set_t *header_set = decoding_manager_find_identifier(manager, header->identifier);

    switch (header->type)
    {
        case STREAM_STREAM_PACKET_TYPE_IDENTIFIER:

            // Identifier already known, we can ignore packet
            if (header_set != NULL)
            {
                manager->statistics.received_identifier_packets++;
                return;
            }

            // Check if we have space for a new packet id
            if (manager->count >= manager->max_count)
            {
                // No space for new packet id, ignore packet
                manager->statistics.identifier_decoding_errors++;
                return;
            }
            stream_parameter_set_t *parameter_set = stream_packet_parse_identifier(data, length);

            if (parameter_set == NULL)
            {

                // Failed to parse identifier packet, ignore it
                manager->statistics.identifier_decoding_errors++;
                return;
            }

            // Find and populate the parameters
            for (size_t i = 0; i < parameter_set->parameter_count; i++)
            {
                int                     id    = parameter_set->parameters[i].index;
                const ODIN_parameter_t *param = ODIN_get_parameter_by_id(group, id, 0);
                if (param == NULL)
                {
                    stream_parameter_set_destroy(parameter_set);
                    manager->statistics.identifier_decoding_errors++;
                    return;
                }

                // Check if the size matches
                if (parameter_set->parameters[i].size != ODIN_get_max_data_size(param))
                {
                    stream_parameter_set_destroy(parameter_set);
                    manager->statistics.identifier_decoding_errors++;
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
            manager->statistics.received_identifier_packets++;
            manager->statistics.identifier_count++;
            break;

        case STREAM_STREAM_PACKET_TYPE_DATA: {
            if (header_set == NULL)
            {
                // No matching identifier packet, ignore data packet
                manager->statistics.received_unresolved_data_packets++;
                return;
            }

            stream_packet_status_t status = stream_packet_parse_data(data, length, header_set->parameter_set);
            if (status != STREAM_PACKET_SUCCESS)
            {
                manager->statistics.data_decoding_errors++;
                // Failed to parse data packet, ignore it
                return;
            }
            
            manager->statistics.received_data_packets++;
            break;
        }

        default:
            // Unknown packet type, ignore it
            manager->statistics.other_errors++;
            return;
    }
}

static header_set_t *decoding_manager_find_identifier(decoding_manager_t *manager, uint16_t identifier)
{
    for (size_t i = 0; i < manager->count; i++)
    {
        if (manager->data[i].packet_identifier == identifier)
        {
            return &manager->data[i];
        }
    }
    return NULL; // Not found
}
