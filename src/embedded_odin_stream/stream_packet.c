#include "./stream_packet.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
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
                                    uint8_t *buffer,
                                    size_t buffer_size,
                                    uint32_t timestamp,
                                    uint32_t header_transmission_interval)
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
    size_t required_total_size = sizeof(streaming_identifier_packet_header_t) + required_payload_size;

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
            .size = (uint16_t)parameter->size,
        };

        memcpy(payload_ptr, &item, sizeof(streaming_identifier_item_t));
        payload_ptr += sizeof(streaming_identifier_item_t);
    }

    // Write header
    streaming_identifier_packet_header_t *packet_header = (streaming_identifier_packet_header_t *)buffer;

    packet_header->header.type = STREAM_STREAM_PACKET_TYPE_IDENTIFIER;
    packet_header->header.identifier = parameter_set->parameter_set_identifier;
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
#define DEBUG_PRINTF(...)                          \
    do                                             \
    {                                              \
        if (0)                                    \
        { /* Change to 1 to enable debug output */ \
            printf(__VA_ARGS__);                   \
        }                                          \
    } while (0)

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
        DEBUG_PRINTF("Invalid packet type: %d\n", packet_header->header.type);
        return NULL; // Invalid packet type
    }
    // Calculate payload size and parameter count. A trailing partial item is
    // tolerated (truncated transport, e.g. a BLE notify capped at ATT_MTU-3):
    // parse the complete items and ignore the remainder, so a receiver learns
    // at least the leading part of the schema instead of rejecting all of it.
    size_t payload_size = buffer_size - sizeof(streaming_identifier_packet_header_t);
    if ((payload_size % sizeof(streaming_identifier_item_t)) != 0)
    {
        DEBUG_PRINTF("Payload truncated mid-item (%zu bytes); parsing complete items only\n", payload_size);
    }
    size_t parameter_count = payload_size / sizeof(streaming_identifier_item_t);

    // Create a new parameter set (handles parameter_count == 0 case)
    stream_parameter_set_t *parameter_set = stream_parameter_set_create(parameter_count);
    if (!parameter_set)
    {
        DEBUG_PRINTF("Failed to allocate parameter set\n");
        return NULL; // Allocation failed
    }

    // Parse parameter items from payload and populate the new set
    const uint8_t *item_ptr = buffer + sizeof(streaming_identifier_packet_header_t);
    for (size_t i = 0; i < parameter_count; i++)
    {
        const streaming_identifier_item_t *item = (const streaming_identifier_item_t *)item_ptr;

        stream_parameter_set_status_t ret = stream_parameter_set_add(parameter_set,
                                                                     (stream_fixed_size_parameter_t){

                                                                         .index = item->index,
                                                                         .size = item->size,
                                                                         .data = NULL, // Data pointer is NULL for header packets
                                                                     });

        // Check if the parameter was added successfully
        if (ret != STREAM_PARAM_SET_SUCCESS)
        {
            DEBUG_PRINTF("Failed to add parameter %zu: %d\n", i, ret);
            stream_parameter_set_destroy(parameter_set);
            return NULL; // Failed to add parameter
        }

        item_ptr += sizeof(streaming_identifier_item_t);
    }

    //  Verify count directly (bypass update_hash as we fill from packet)
    if (parameter_set->parameter_count != parameter_count)
    {
        DEBUG_PRINTF("Parameter count mismatch: expected %zu, got %zu\n", parameter_count, parameter_set->parameter_count);
        stream_parameter_set_destroy(parameter_set);
        return NULL; // Inconsistent state
    }

    // Trust the identifier carried in the header rather than the CRC recomputed
    // from the items we parsed. The header identifier is the *full*-set hash,
    // and every DATA packet carries that same value; keying the set by it is
    // what lets DATA packets resolve. When the payload arrives truncated (e.g.
    // a BLE advertisement capped below the full IDENTIFIER size) the recomputed
    // CRC necessarily differs — adopting the header value lets the receiver
    // learn the leading parameters instead of rejecting the schema outright.
    // For an untruncated packet the two values are equal, so this is a no-op.
    parameter_set->parameter_set_identifier = packet_header->header.identifier;

    return parameter_set;
}

int stream_packet_create_identifier_ext(stream_parameter_set_t *parameter_set,
                                        uint8_t *buffer,
                                        size_t buffer_size,
                                        uint32_t timestamp,
                                        uint32_t header_transmission_interval,
                                        uint16_t *cursor)
{
    if (!parameter_set || !buffer || !cursor)
    {
        return STREAM_PACKET_ERROR_INVALID;
    }
    if (!parameter_set->parameters && parameter_set->parameter_count > 0)
    {
        return STREAM_PACKET_ERROR_INTERNAL; // Inconsistent state
    }

    // Self-throttle on the extended header's own cadence (independent of the
    // plain IDENTIFIER), mirroring stream_packet_create_identifier.
    if (timestamp - parameter_set->last_ext_header_transmission_timestamp < header_transmission_interval)
    {
        return STREAM_PACKET_SUCCESS; // Not due yet
    }

    if (parameter_set->parameter_count == 0)
    {
        return STREAM_PACKET_SUCCESS; // Nothing to describe
    }

    if (buffer_size <= sizeof(streaming_identifier_ext_packet_header_t))
    {
        return STREAM_PACKET_ERROR_BADSIZE; // No room for even the header + one item
    }

    // Pack a contiguous run starting at the cursor — never wrapping mid-packet,
    // so start_ordinal + the item count describe one unbroken span.
    if (*cursor >= parameter_set->parameter_count)
    {
        *cursor = 0;
    }
    const uint16_t start = *cursor;

    uint8_t *write_ptr = buffer + sizeof(streaming_identifier_ext_packet_header_t);
    const uint8_t *buffer_end = buffer + buffer_size;
    size_t count = 0;

    for (size_t i = start; i < parameter_set->parameter_count; i++)
    {
        const stream_fixed_size_parameter_t *parameter = &parameter_set->parameters[i];
        const char *name = parameter->name ? parameter->name : "";
        size_t name_len = strlen(name);
        if (name_len > UINT8_MAX)
        {
            name_len = UINT8_MAX; // name_len is a u8 on the wire; truncate defensively
        }

        const size_t item_size = sizeof(streaming_identifier_ext_item_header_t) + name_len;
        if (write_ptr + item_size > buffer_end)
        {
            break; // This item doesn't fit; it leads the next chunk.
        }

        streaming_identifier_ext_item_header_t item = {
            .index = parameter->index,
            .size = (uint16_t)parameter->size,
            .element_type = parameter->element_type,
            .name_len = (uint8_t)name_len,
            .type_id = parameter->type_id,
        };
        memcpy(write_ptr, &item, sizeof(item));
        write_ptr += sizeof(item);
        memcpy(write_ptr, name, name_len);
        write_ptr += name_len;
        count++;
    }

    if (count == 0)
    {
        // Even the single item at the cursor didn't fit the budget. Don't stall
        // forever: skip it so paging can make progress on the next call.
        *cursor = (uint16_t)((start + 1) % parameter_set->parameter_count);
        return STREAM_PACKET_ERROR_BADSIZE;
    }

    // Commit the throttle only once we actually emit.
    parameter_set->last_ext_header_transmission_timestamp = timestamp;

    streaming_identifier_ext_packet_header_t *packet_header =
        (streaming_identifier_ext_packet_header_t *)buffer;
    packet_header->header.type = STREAM_STREAM_PACKET_TYPE_IDENTIFIER_EXT;
    packet_header->header.identifier = parameter_set->parameter_set_identifier;
    packet_header->header.reserved = 0;
    packet_header->definition_identifier = parameter_set->definition_identifier;
    packet_header->total_count = (uint16_t)parameter_set->parameter_count;
    packet_header->start_ordinal = start;

    // Advance the cursor past the run we just emitted, wrapping at the end.
    uint16_t next = (uint16_t)(start + count);
    if (next >= parameter_set->parameter_count)
    {
        next = 0;
    }
    *cursor = next;

    return (int)(write_ptr - buffer);
}

stream_packet_status_t stream_packet_parse_identifier_ext(const uint8_t *buffer,
                                                          size_t buffer_size,
                                                          uint32_t *definition_identifier_out,
                                                          uint16_t *total_count_out,
                                                          uint16_t *start_ordinal_out,
                                                          stream_identifier_ext_item_cb cb,
                                                          void *ctx)
{
    if (!buffer)
    {
        return STREAM_PACKET_ERROR_INVALID;
    }
    if (buffer_size < sizeof(streaming_identifier_ext_packet_header_t))
    {
        return STREAM_PACKET_ERROR_BADSIZE;
    }

    streaming_identifier_ext_packet_header_t header;
    memcpy(&header, buffer, sizeof(header));
    if (header.header.type != STREAM_STREAM_PACKET_TYPE_IDENTIFIER_EXT)
    {
        return STREAM_PACKET_ERROR_BADTYPE;
    }

    if (definition_identifier_out) *definition_identifier_out = header.definition_identifier;
    if (total_count_out) *total_count_out = header.total_count;
    if (start_ordinal_out) *start_ordinal_out = header.start_ordinal;

    const uint8_t *ptr = buffer + sizeof(streaming_identifier_ext_packet_header_t);
    const uint8_t *end = buffer + buffer_size;

    while (ptr + sizeof(streaming_identifier_ext_item_header_t) <= end)
    {
        streaming_identifier_ext_item_header_t item;
        memcpy(&item, ptr, sizeof(item));
        const uint8_t *name = ptr + sizeof(item);
        if (name + item.name_len > end)
        {
            break; // Trailing item truncated by a capped transport; stop here.
        }
        if (cb)
        {
            cb(ctx, item.index, item.size, item.element_type, (const char *)name, item.name_len, item.type_id);
        }
        ptr = name + item.name_len;
    }

    return STREAM_PACKET_SUCCESS;
}

/* Serialised size of one type descriptor on the wire. */
static size_t type_descriptor_wire_size(const stream_type_descriptor_t *t)
{
    size_t s = sizeof(streaming_type_descriptor_header_t);
    s += t->type_name ? strlen(t->type_name) : 0;
    for (uint8_t f = 0; f < t->field_count; f++)
    {
        s += sizeof(streaming_type_field_header_t);
        s += t->fields[f].name ? strlen(t->fields[f].name) : 0;
    }
    return s;
}

int stream_packet_create_type_descriptor(stream_type_set_t *type_set,
                                         uint8_t *buffer,
                                         size_t buffer_size,
                                         uint32_t timestamp,
                                         uint32_t transmission_interval,
                                         uint16_t *cursor)
{
    if (!type_set || !buffer || !cursor) return STREAM_PACKET_ERROR_INVALID;
    if (!type_set->types && type_set->count > 0) return STREAM_PACKET_ERROR_INTERNAL;

    if (timestamp - type_set->last_transmission_timestamp < transmission_interval)
        return STREAM_PACKET_SUCCESS; // not due

    if (type_set->count == 0) return STREAM_PACKET_SUCCESS; // nothing to describe
    if (buffer_size <= sizeof(streaming_type_descriptor_packet_header_t))
        return STREAM_PACKET_ERROR_BADSIZE;

    if (*cursor >= type_set->count) *cursor = 0;
    const uint16_t start = *cursor;

    uint8_t *wp = buffer + sizeof(streaming_type_descriptor_packet_header_t);
    const uint8_t *end = buffer + buffer_size;
    size_t count = 0;

    for (size_t i = start; i < type_set->count; i++)
    {
        const stream_type_descriptor_t *t = &type_set->types[i];
        if (wp + type_descriptor_wire_size(t) > end) break; // type is atomic in a chunk

        size_t tname_len = t->type_name ? strlen(t->type_name) : 0;
        if (tname_len > UINT8_MAX) tname_len = UINT8_MAX;

        streaming_type_descriptor_header_t th = {
            .type_id = t->type_id,
            .size = t->size,
            .field_count = t->field_count,
            .name_len = (uint8_t)tname_len,
        };
        memcpy(wp, &th, sizeof(th)); wp += sizeof(th);
        memcpy(wp, t->type_name ? t->type_name : "", tname_len); wp += tname_len;

        for (uint8_t f = 0; f < t->field_count; f++)
        {
            const stream_type_field_t *fld = &t->fields[f];
            size_t fname_len = fld->name ? strlen(fld->name) : 0;
            if (fname_len > UINT8_MAX) fname_len = UINT8_MAX;
            streaming_type_field_header_t fh = {
                .element_type = fld->element_type,
                .size = fld->size,
                .offset = fld->offset,
                .name_len = (uint8_t)fname_len,
            };
            memcpy(wp, &fh, sizeof(fh)); wp += sizeof(fh);
            memcpy(wp, fld->name ? fld->name : "", fname_len); wp += fname_len;
        }
        count++;
    }

    if (count == 0)
    {
        // A single type bigger than the budget would stall paging; skip it.
        *cursor = (uint16_t)((start + 1) % type_set->count);
        return STREAM_PACKET_ERROR_BADSIZE;
    }

    type_set->last_transmission_timestamp = timestamp;

    streaming_type_descriptor_packet_header_t *h =
        (streaming_type_descriptor_packet_header_t *)buffer;
    h->header.type = STREAM_STREAM_PACKET_TYPE_TYPE_DESCRIPTOR;
    h->header.identifier = 0;
    h->header.reserved = 0;
    h->definition_identifier = type_set->definition_identifier;
    h->total_count = (uint16_t)type_set->count;
    h->start_ordinal = start;

    uint16_t next = (uint16_t)(start + count);
    if (next >= type_set->count) next = 0;
    *cursor = next;

    return (int)(wp - buffer);
}

stream_packet_status_t stream_packet_parse_type_descriptor(const uint8_t *buffer,
                                                           size_t buffer_size,
                                                           uint32_t *definition_identifier_out,
                                                           uint16_t *total_count_out,
                                                           uint16_t *start_ordinal_out,
                                                           stream_type_descriptor_type_cb type_cb,
                                                           stream_type_descriptor_field_cb field_cb,
                                                           void *ctx)
{
    if (!buffer) return STREAM_PACKET_ERROR_INVALID;
    if (buffer_size < sizeof(streaming_type_descriptor_packet_header_t))
        return STREAM_PACKET_ERROR_BADSIZE;

    streaming_type_descriptor_packet_header_t header;
    memcpy(&header, buffer, sizeof(header));
    if (header.header.type != STREAM_STREAM_PACKET_TYPE_TYPE_DESCRIPTOR)
        return STREAM_PACKET_ERROR_BADTYPE;

    if (definition_identifier_out) *definition_identifier_out = header.definition_identifier;
    if (total_count_out) *total_count_out = header.total_count;
    if (start_ordinal_out) *start_ordinal_out = header.start_ordinal;

    const uint8_t *ptr = buffer + sizeof(streaming_type_descriptor_packet_header_t);
    const uint8_t *end = buffer + buffer_size;

    while (ptr + sizeof(streaming_type_descriptor_header_t) <= end)
    {
        streaming_type_descriptor_header_t th;
        memcpy(&th, ptr, sizeof(th));
        const uint8_t *tname = ptr + sizeof(th);
        if (tname + th.name_len > end) break; // truncated type name
        const uint8_t *fptr = tname + th.name_len;

        // Verify all fields are fully present before emitting anything for this
        // type (keep the type atomic for the receiver).
        const uint8_t *scan = fptr;
        bool complete = true;
        for (uint8_t f = 0; f < th.field_count; f++)
        {
            if (scan + sizeof(streaming_type_field_header_t) > end) { complete = false; break; }
            streaming_type_field_header_t fh;
            memcpy(&fh, scan, sizeof(fh));
            scan += sizeof(fh);
            if (scan + fh.name_len > end) { complete = false; break; }
            scan += fh.name_len;
        }
        if (!complete) break;

        if (type_cb)
            type_cb(ctx, th.type_id, th.size, th.field_count, (const char *)tname, th.name_len);

        for (uint8_t f = 0; f < th.field_count; f++)
        {
            streaming_type_field_header_t fh;
            memcpy(&fh, fptr, sizeof(fh));
            const uint8_t *fname = fptr + sizeof(fh);
            if (field_cb)
                field_cb(ctx, fh.element_type, fh.size, fh.offset, (const char *)fname, fh.name_len);
            fptr = fname + fh.name_len;
        }
        ptr = fptr;
    }

    return STREAM_PACKET_SUCCESS;
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
                              uint8_t *buffer,
                              size_t buffer_size,
                              uint32_t timestamp,
                              uint32_t data_transmission_interval)
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
    packet_header->header.type = STREAM_STREAM_PACKET_TYPE_DATA;
    packet_header->header.identifier = parameter_set->parameter_set_identifier;
    packet_header->timestamp = timestamp;
    packet_header->sequence_number = parameter_set->last_transmission_sequence_number++;

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
stream_packet_status_t stream_packet_parse_data(const uint8_t *buffer,
                                                size_t buffer_size,
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
    packet_header->header.type = STREAM_STREAM_PACKET_TYPE_EVENT;
    packet_header->header.identifier = event.event_id;
    packet_header->timestamp = event.timestamp;

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
    event->event_data = buffer + sizeof(streaming_event_packet_header_t);
    event->event_size = buffer_size - sizeof(streaming_event_packet_header_t);
    event->event_id = packet_header->header.identifier;
    event->timestamp = packet_header->timestamp;
    event->event_sequence = packet_header->sequence_number;
    return STREAM_PACKET_SUCCESS;
}
