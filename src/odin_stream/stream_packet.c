#include "odin_stream/stream_packet.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

/**
 * @brief Generates an identifier packet into the provided buffer.
 *
 * The packet contains the header and a list of index/size pairs for
 * parameters currently in the set.
 *
 * @param parameter_set Pointer to the initialized parameter set. Must not be NULL.
 * @param buffer Pointer to the output buffer. Must not be NULL.
 * @param buffer_size Size of the output buffer in bytes.
 * @return The number of bytes written to the buffer on success (always positive).
 * @return STREAM_PACKET_ERROR_INVALID if parameter_set or buffer is NULL.
 * @return STREAM_PACKET_ERROR_BADSIZE if buffer_size is insufficient.
 * @return STREAM_PACKET_ERROR_INTERNAL if parameter_set state is inconsistent (e.g., null parameters array).
 */
int stream_packet_create_identifier(stream_parameter_set_t *parameter_set,
                                    uint8_t                *buffer,
                                    size_t                  buffer_size,
                                    uint32_t                timestamp,
                                    uint32_t                header_transmission_interval)
{
    if (!parameter_set || !buffer)
    {
        return STREAM_PACKET_ERROR_INVALID;
    }
    if (!parameter_set->parameters && parameter_set->parameter_count > 0)
    {
        return STREAM_PACKET_ERROR_INTERNAL; // Inconsistent state
    }

    // Check if it's time to send the header again
    if (timestamp - parameter_set->last_header_transmission_timestamp < header_transmission_interval)
    {
        return STREAM_PACKET_SUCCESS; // No need to send header yet
    }
    parameter_set->last_header_transmission_timestamp = timestamp;

    // Calculate required size *before* writing anything
    size_t required_payload_size = parameter_set->parameter_count * sizeof(streaming_identifier_item_t);
    size_t required_total_size   = sizeof(streaming_identifier_packet_header_t) + required_payload_size;

    if (buffer_size < required_total_size)
    {
        return STREAM_PACKET_ERROR_BADSIZE; // Buffer too small
    }

    // Write payload (parameter index/size items)
    uint8_t *payload_ptr = buffer + sizeof(streaming_identifier_packet_header_t);
    for (size_t i = 0; i < parameter_set->parameter_count; i++)
    {
        const stream_fixed_size_parameter_t *parameter = &parameter_set->parameters[i];

        streaming_identifier_item_t item = {
            .index = parameter->index,
            .size  = (uint16_t)parameter->size,
        };

        memcpy(payload_ptr, &item, sizeof(streaming_identifier_item_t));
        payload_ptr += sizeof(streaming_identifier_item_t);
    }

    // Write header
    streaming_identifier_packet_header_t *packet_header = (streaming_identifier_packet_header_t *)buffer;

    packet_header->header.type           = STREAM_STREAM_PACKET_TYPE_IDENTIFIER;
    packet_header->header.identifier     = parameter_set->parameter_set_identifier;
    packet_header->definition_identifier = parameter_set->definition_identifier;

    // Return bytes written (cast is safe as required_total_size was checked against buffer_size)
    return (int)required_total_size;
}

/**
 * @brief Parses an identifier packet from a buffer and creates a new parameter set.
 *
 * Allocates a new parameter set based on the count derived from the packet size.
 * Populates the set with parameters containing index and size information from
 * the packet. The 'data' pointers in the created parameters will be NULL.
 * Verifies packet type and payload size consistency.
 *
 * @param buffer Pointer to the buffer containing the identifier packet. Must not be NULL.
 * @param buffer_size Size of the input buffer in bytes.
 * @return A pointer to a newly allocated parameter_set_t on success.
 * The caller is responsible for freeing this pointer using parameter_set_destroy().
 * @return NULL on failure:
 * - If buffer is NULL.
 * - If buffer_size is too small for the header.
 * - If the packet type is incorrect.
 * - If the payload size is inconsistent (not a multiple of item size).
 * - If memory allocation fails via parameter_set_create.
 */
stream_parameter_set_t *stream_packet_parse_identifier(const uint8_t *buffer, size_t buffer_size)
{
    if (!buffer)
    {
        return NULL; // Invalid argument
    }

    if (buffer_size < sizeof(streaming_identifier_packet_header_t))
    {
        return NULL; // Buffer too small for header
    }

    const streaming_identifier_packet_header_t *packet_header = (const streaming_identifier_packet_header_t *)buffer;
    if (packet_header->header.type != STREAM_STREAM_PACKET_TYPE_IDENTIFIER)
    {
        return NULL; // Invalid packet type
    }

    // Calculate expected payload size and parameter count
    size_t payload_size = buffer_size - sizeof(streaming_identifier_packet_header_t);
    if ((payload_size % sizeof(streaming_identifier_item_t)) != 0)
    {
        return NULL; // Payload size not a multiple of item size
    }
    size_t parameter_count = payload_size / sizeof(streaming_identifier_item_t);

    // Create a new parameter set (handles parameter_count == 0 case)
    stream_parameter_set_t *parameter_set = stream_parameter_set_create(parameter_count);
    if (!parameter_set)
    {
        return NULL; // Allocation failed
    }

    // Parse parameter items from payload and populate the new set
    const uint8_t *item_ptr = buffer + sizeof(streaming_identifier_packet_header_t);
    for (size_t i = 0; i < parameter_count; i++)
    {
        const streaming_identifier_item_t *item = (const streaming_identifier_item_t *)item_ptr;

        stream_parameter_set_status_t ret
            = stream_parameter_set_add(parameter_set,
                                       (stream_fixed_size_parameter_t) {

                                           .index = item->index,
                                           .size  = item->size,
                                           .data  = NULL, // Data pointer is NULL for header packets
                                       });

        // Check if the parameter was added successfully
        if (ret != STREAM_PARAM_SET_SUCCESS)
        {
            stream_parameter_set_destroy(parameter_set);
            return NULL; // Failed to add parameter
        }

        item_ptr += sizeof(streaming_identifier_item_t);
    }

    //  Verify hash and count directly (bypass update_hash as we fill from packet)
    if (parameter_set->parameter_count != parameter_count)
    {
        stream_parameter_set_destroy(parameter_set);
        return NULL; // Inconsistent state
    }
    if (parameter_set->parameter_set_identifier != packet_header->header.identifier)
    {
        stream_parameter_set_destroy(parameter_set);
        return NULL; // Hash mismatch
    }

    return parameter_set;
}

/**
 * @brief Generates a data packet into the provided buffer.
 *
 * The packet contains the header followed by the concatenated binary data
 * of all parameters currently in the set, in their defined order.
 *
 * @param parameter_set Pointer to the initialized parameter set. Must not be NULL.
 * The 'data' pointers within the set's parameters must be valid and readable,
 * and the 'size' must be correct for each.
 * @param buffer Pointer to the output buffer. Must not be NULL.
 * @param buffer_size Size of the output buffer in bytes.
 * @param timestamp The timestamp to include in the packet header.
 * @return The total number of bytes written to the buffer on success (always positive).
 * @return STREAM_PACKET_ERROR_INVALID if parameter_set or buffer is NULL, or internal parameter_set state is bad.
 * @return STREAM_PACKET_ERROR_NODATA if any parameter in the set has a NULL data pointer.
 * @return STREAM_PACKET_ERROR_BADSIZE if buffer_size is insufficient for the header and all parameter data.
 */
int stream_packet_create_data(stream_parameter_set_t *parameter_set,
                              uint8_t                      *buffer,
                              size_t                        buffer_size,
                              uint32_t                      timestamp,
                              uint32_t                      data_transmission_interval)
{

    if (!parameter_set || !buffer)
    {
        return STREAM_PACKET_ERROR_INVALID;
    }

    if (timestamp - parameter_set->last_data_transmission_timestamp < data_transmission_interval)
    {
        return STREAM_PACKET_SUCCESS; // No need to send data yet
    }

    if (!parameter_set->parameters && parameter_set->parameter_count > 0)
    {
        return STREAM_PACKET_ERROR_INTERNAL; // Inconsistent state
    }

    size_t required_total_size = sizeof(streaming_data_packet_header_t) + parameter_set->payload_size;

    // Packet shoud fit in the buffer
    if (buffer_size < required_total_size)
    {
        return STREAM_PACKET_ERROR_BADSIZE; // Buffer too small
    }

    // Write payload data by concatenating parameter data
    uint8_t *payload_write_ptr = buffer + sizeof(streaming_data_packet_header_t);
    for (size_t i = 0; i < parameter_set->parameter_count; i++)
    {
        const stream_fixed_size_parameter_t *parameter = &parameter_set->parameters[i];

        // Data pointer must be valid and non-NULL
        if (!parameter->data)
        {
            return STREAM_PACKET_ERROR_NODATA; // Data pointer is NULL
        }

        memcpy(payload_write_ptr, parameter->data, parameter->size);
        payload_write_ptr += parameter->size;
    }

    // Write header
    streaming_data_packet_header_t *packet_header = (streaming_data_packet_header_t *)buffer;
    packet_header->header.type                    = STREAM_STREAM_PACKET_TYPE_DATA;
    packet_header->header.identifier              = parameter_set->parameter_set_identifier;
    packet_header->timestamp                      = timestamp;
    packet_header->sequence_number                = parameter_set->last_transmission_sequence_number++;

    // Sanity check that we wrote exactly the expected number of bytes
    assert((size_t)(payload_write_ptr - buffer) == required_total_size);
    
    parameter_set->last_data_transmission_timestamp = timestamp;

    // Return bytes written (cast is safe as required_total_size checked against buffer_size)
    return (int)required_total_size;
}

/**
 * @brief Parses a data packet and populates the data pointers of a compatible parameter set.
 *
 * Reads the header, verifies packet type and parameter hash against the provided set.
 * If checks pass, copies the data payload from the buffer into the memory locations
 * pointed to by the `data` members of the parameters in the provided `parameter_set`.
 *
 * @warning Assumes the `data` pointers in the target `parameter_set` struct point to valid,
 * allocated memory locations large enough to hold `size` bytes for each
 * respective parameter *before* calling this function.
 * @warning Assumes the parameter order, count, and sizes in `parameter_set` exactly match
 * the data layout within the packet's payload. The primary check is the hash.
 *
 * @param buffer Pointer to the buffer containing the data packet. Must not be NULL.
 * @param buffer_size Size of the input buffer in bytes.
 * @param parameter_set Pointer to the parameter set structure to populate. Must not be NULL,
 * must be initialized, and its hash must match the packet's hash.
 * Its parameters must have valid 'data' pointers and correct 'size' values.
 * @return streaming_packet_status_t indicating success or failure reason.
 */
stream_packet_status_t stream_packet_parse_data(const uint8_t          *buffer,
                                                size_t                  buffer_size,
                                                stream_parameter_set_t *parameter_set)
{
    if (!buffer || !parameter_set)
    {
        return STREAM_PACKET_ERROR_INVALID;
    }
    if (!parameter_set->parameters && parameter_set->parameter_count > 0)
    {
        return STREAM_PACKET_ERROR_INVALID; // Invalid target parameterset
    }

    if (buffer_size < sizeof(streaming_data_packet_header_t))
    {
        return STREAM_PACKET_ERROR_BADSIZE; // Buffer too small for header
    }

    const streaming_data_packet_header_t *packet_header = (const streaming_data_packet_header_t *)buffer;
    if (packet_header->header.type != STREAM_STREAM_PACKET_TYPE_DATA)
    {
        return STREAM_PACKET_ERROR_BADTYPE;
    }

    // Verify itentifier match.
    if (packet_header->header.identifier != parameter_set->parameter_set_identifier)
    {
        return STREAM_PACKET_ERROR_BADHASH;
    }

    size_t expected_total_size = sizeof(streaming_data_packet_header_t) + parameter_set->payload_size;

    // Check if the provided buffer size matches exactly what's expected
    if (buffer_size != expected_total_size)
    {
        return STREAM_PACKET_ERROR_BADSIZE; // Mismatch indicates corrupted packet or wrong parameter_set definition
    }

    // Copy data from buffer payload into the target parameter set's data pointers
    const uint8_t *payload_ptr = buffer + sizeof(streaming_data_packet_header_t);
    for (size_t i = 0; i < parameter_set->parameter_count; i++)
    {
        stream_fixed_size_parameter_t *parameter = &parameter_set->parameters[i];

        if (parameter->size == 0)
        {
            return STREAM_PACKET_ERROR_BADSIZE; // Invalid parameter size
        }

        memcpy(parameter->data, payload_ptr, parameter->size);
        payload_ptr += parameter->size;
    }

    // Sanity check: did we consume the whole buffer exactly?
    assert((size_t)(payload_ptr - buffer) == expected_total_size);

    return STREAM_PACKET_SUCCESS;
}

/**
 * @brief Creates a stream event packet and writes it to the provided buffer.
 *
 * @param buffer Pointer to the buffer where the packet will be written.
 * @param buffer_size Size of the buffer in bytes.
 * @param event The stream event to be serialized into the packet.
 * @return int The number of bytes written to the buffer on success, or an error code:
 *         - STREAM_PACKET_ERROR_INVALID: If the buffer or event data is NULL.
 *         - STREAM_PACKET_ERROR_BADSIZE: If the buffer is too small to hold the packet.
 */
int stream_packet_create_event(uint8_t *buffer, size_t buffer_size, stream_event_t event);

int stream_packet_create_event(uint8_t *buffer, size_t buffer_size, stream_event_t event)
{
    if (!buffer || !event.event_data)
    {
        return STREAM_PACKET_ERROR_INVALID;
    }
    size_t required_total_size = sizeof(streaming_event_packet_header_t) + event.event_size;

    // Packet should fit in the buffer
    if (buffer_size < required_total_size)
    {
        return STREAM_PACKET_ERROR_BADSIZE; // Buffer too small
    }

    // Write payload data by concatenating parameter data
    uint8_t *payload_write_ptr = buffer + sizeof(streaming_event_packet_header_t);
    memcpy(payload_write_ptr, event.event_data, event.event_size);

    // Write header
    streaming_event_packet_header_t *packet_header = (streaming_event_packet_header_t *)buffer;
    packet_header->header.type                     = STREAM_STREAM_PACKET_TYPE_EVENT;
    packet_header->header.identifier               = event.event_id;
    packet_header->timestamp                       = event.timestamp;

    // Return bytes written (cast is safe as required_total_size checked against buffer_size)
    return (int)required_total_size;
}

/**
 * @brief Parses a stream event packet from the provided buffer.
 *
 * @param buffer Pointer to the buffer containing the packet data.
 * @param buffer_size Size of the buffer in bytes.
 * @param event Pointer to the stream_event_t structure where the parsed event will be stored.
 *          Note: The event_data pointer in the event structure will point to the data in the buffer
 *          the caller must ensure that the buffer remains valid for the lifetime of the event handling.
 * @return stream_packet_status_t Status of the parsing operation:
 *         - STREAM_PACKET_SUCCESS: If the packet was successfully parsed.
 *         - STREAM_PACKET_ERROR_INVALID: If the buffer or event pointer is NULL.
 *         - STREAM_PACKET_ERROR_BADSIZE: If the buffer is too small to contain the header.
 *         - STREAM_PACKET_ERROR_BADTYPE: If the packet type is not STREAM_STREAM_PACKET_TYPE_EVENT.
 */
stream_packet_status_t stream_packet_parse_event(const uint8_t *buffer, size_t buffer_size, stream_event_t *event)
{
    if (!buffer || !event)
    {
        return STREAM_PACKET_ERROR_INVALID;
    }
    if (buffer_size < sizeof(streaming_event_packet_header_t))
    {
        return STREAM_PACKET_ERROR_BADSIZE; // Buffer too small for header
    }

    const streaming_event_packet_header_t *packet_header = (const streaming_event_packet_header_t *)buffer;
    if (packet_header->header.type != STREAM_STREAM_PACKET_TYPE_EVENT)
    {
        return STREAM_PACKET_ERROR_BADTYPE;
    }

    // Parse event data
    event->event_data     = buffer + sizeof(streaming_event_packet_header_t);
    event->event_size     = buffer_size - sizeof(streaming_event_packet_header_t);
    event->event_id       = packet_header->header.identifier;
    event->timestamp      = packet_header->timestamp;
    event->event_sequence = packet_header->sequence_number;
    return STREAM_PACKET_SUCCESS;
}
