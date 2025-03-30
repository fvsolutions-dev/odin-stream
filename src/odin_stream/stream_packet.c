#include "odin_stream/stream_packet.h"
#include <stdlib.h>
#include <string.h> // For memcpy
#include <assert.h> // For internal checks
#include <limits.h> // For UINT16_MAX

/**
 * @brief Generates an identifier packet into the provided buffer.
 *
 * The packet contains the header and a list of index/size pairs for
 * parameters currently in the set.
 *
 * @param pset Pointer to the initialized parameter set. Must not be NULL.
 * @param buffer Pointer to the output buffer. Must not be NULL.
 * @param buffer_size Size of the output buffer in bytes.
 * @return The number of bytes written to the buffer on success (always positive).
 * @return PACKET_E_INVALID if pset or buffer is NULL.
 * @return PACKET_E_BADSIZE if buffer_size is insufficient.
 * @return PACKET_E_OVERFLOW if any parameter's size exceeds UINT16_MAX.
 * @return PACKET_E_INTERNAL if pset state is inconsistent (e.g., null parameters array).
 */
int streaming_packet_create_identifier(
    parameter_set_t *pset, uint8_t *buffer, size_t buffer_size, uint32_t timestamp, uint32_t header_transmission_interval)
{
    if (!pset || !buffer)
    {
        return PACKET_E_INVALID;
    }
    if (!pset->parameters && pset->parameter_count > 0)
    {
        return PACKET_E_INTERNAL; // Inconsistent state
    }

    // Check if it's time to send the header again
    if (timestamp - pset->last_header_transmission_timestamp < header_transmission_interval)
    {
        return 0;
    }
    pset->last_header_transmission_timestamp = timestamp;

    // Calculate required size *before* writing anything
    size_t required_payload_size = pset->parameter_count * sizeof(ident_payload_item_t);
    size_t required_total_size   = sizeof(streaming_identifier_packet_header_t) + required_payload_size;

    if (buffer_size < required_total_size)
    {
        return PACKET_E_BADSIZE; // Buffer too small
    }

    // Write payload (parameter index/size items)
    uint8_t *payload_ptr = buffer + sizeof(streaming_identifier_packet_header_t);
    for (size_t i = 0; i < pset->parameter_count; i++)
    {
        const fixed_size_parameter_t *parameter = &pset->parameters[i];
        ident_payload_item_t          item;
        item.index = parameter->index;

        // Ensure size fits in uint16_t for the packet item
        if (parameter->size > UINT16_MAX)
        {
            return PACKET_E_OVERFLOW; // Parameter size too large for packet format
        }
        item.size = (uint16_t)parameter->size;

        memcpy(payload_ptr, &item, sizeof(ident_payload_item_t));
        payload_ptr += sizeof(ident_payload_item_t);
    }

    // Write header
    streaming_identifier_packet_header_t *header = (streaming_identifier_packet_header_t *)buffer;
    header->header.type                          = STREAMING_PACKET_TYPE_IDENTIFIER;
    header->header.identifier                    = pset->parameter_hash;
    header->odin_definition_id                   = 0xDEADBEEF; // TODO: Replace with actual definition hash

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
parameter_set_t *streaming_packet_parse_identifier(const uint8_t *buffer, size_t buffer_size)
{
    if (!buffer)
    {
        return NULL; // Invalid argument
    }

    if (buffer_size < sizeof(streaming_identifier_packet_header_t))
    {
        return NULL; // Buffer too small for header
    }

    const streaming_identifier_packet_header_t *header = (const streaming_identifier_packet_header_t *)buffer;
    if (header->header.type != STREAMING_PACKET_TYPE_IDENTIFIER)
    {
        return NULL; // Invalid packet type
    }

    // Calculate expected payload size and parameter count
    size_t payload_size = buffer_size - sizeof(streaming_identifier_packet_header_t);
    if ((payload_size % sizeof(ident_payload_item_t)) != 0)
    {
        return NULL; // Payload size not a multiple of item size
    }
    size_t parameter_count = payload_size / sizeof(ident_payload_item_t);

    // Create a new parameter set (handles parameter_count == 0 case)
    parameter_set_t *pset = parameter_set_create(parameter_count);
    if (!pset)
    {
        return NULL; // Allocation failed
    }

    // Set hash and count directly (bypass update_hash as we fill from packet)
    pset->parameter_hash  = header->header.identifier;
    pset->parameter_count = parameter_count;
    // pset->parameter_count_max is already set correctly by _create

    // Parse parameter items from payload into the newly created set
    const uint8_t *item_ptr = buffer + sizeof(streaming_identifier_packet_header_t);
    for (size_t i = 0; i < parameter_count; i++)
    {
        const ident_payload_item_t *item = (const ident_payload_item_t *)item_ptr;

        // Create parameter struct - data pointer is NULL for header packets
        // Directly place into allocated array, no need for _add checks
        pset->parameters[i].index = item->index;
        pset->parameters[i].size  = item->size;
        pset->parameters[i].data  = NULL;

        item_ptr += sizeof(ident_payload_item_t);
    }

    // Optional sanity check: recalculate hash and compare to header->parameter_group_hash
    // parameter_set_recalculate_hash(pset);
    // if (pset->parameter_hash != header->parameter_group_hash) {
    //     parameter_set_destroy(pset);
    //     return NULL; // Hash mismatch indicates corrupted data or internal error
    // }

    return pset;
}

/**
 * @brief Generates a data packet into the provided buffer.
 *
 * The packet contains the header followed by the concatenated binary data
 * of all parameters currently in the set, in their defined order.
 *
 * @param pset Pointer to the initialized parameter set. Must not be NULL.
 * The 'data' pointers within the set's parameters must be valid and readable,
 * and the 'size' must be correct for each.
 * @param buffer Pointer to the output buffer. Must not be NULL.
 * @param buffer_size Size of the output buffer in bytes.
 * @param timestamp The timestamp to include in the packet header.
 * @return The total number of bytes written to the buffer on success (always positive).
 * @return PACKET_E_INVALID if pset or buffer is NULL, or internal pset state is bad.
 * @return PACKET_E_NODATA if any parameter in the set has a NULL data pointer.
 * @return PACKET_E_BADSIZE if buffer_size is insufficient for the header and all parameter data.
 */
int streaming_packet_create_data(const parameter_set_t *pset, uint8_t *buffer, size_t buffer_size, uint32_t timestamp)
{
    if (!pset || !buffer)
    {
        return PACKET_E_INVALID;
    }
    if (!pset->parameters && pset->parameter_count > 0)
    {
        return PACKET_E_INTERNAL; // Inconsistent state
    }

    // Calculate required payload size first & check data pointers
    size_t required_payload_size = 0;
    for (size_t i = 0; i < pset->parameter_count; i++)
    {
        if (!pset->parameters[i].data)
        {
            return PACKET_E_NODATA; // Cannot serialize parameter with NULL data
        }
        required_payload_size += pset->parameters[i].size;
    }
    size_t required_total_size = sizeof(streaming_data_packet_header_t) + required_payload_size;

    if (buffer_size < required_total_size)
    {
        return PACKET_E_BADSIZE; // Buffer too small
    }

    // Write payload data by concatenating parameter data
    uint8_t *payload_ptr = buffer + sizeof(streaming_data_packet_header_t);
    for (size_t i = 0; i < pset->parameter_count; i++)
    {
        const fixed_size_parameter_t *parameter = &pset->parameters[i];
        // Data pointer was checked above
        assert(parameter->data != NULL);
        memcpy(payload_ptr, parameter->data, parameter->size);
        payload_ptr += parameter->size;
    }

    // Write header
    streaming_data_packet_header_t *header = (streaming_data_packet_header_t *)buffer;
    header->header.type                    = STREAMING_PACKET_TYPE_DATA;
    header->header.identifier              = pset->parameter_hash;
    header->timestamp                      = timestamp;

    // Sanity check that we wrote exactly the expected number of bytes
    assert((size_t)(payload_ptr - buffer) == required_total_size);

    // Return bytes written (cast is safe as required_total_size checked against buffer_size)
    return (int)required_total_size;
}

/**
 * @brief Parses a data packet and populates the data pointers of a compatible parameter set.
 *
 * Reads the header, verifies packet type and parameter hash against the provided set.
 * If checks pass, copies the data payload from the buffer into the memory locations
 * pointed to by the `data` members of the parameters in the provided `pset`.
 *
 * @warning Assumes the `data` pointers in the target `pset` struct point to valid,
 * allocated memory locations large enough to hold `size` bytes for each
 * respective parameter *before* calling this function.
 * @warning Assumes the parameter order, count, and sizes in `pset` exactly match
 * the data layout within the packet's payload. The primary check is the hash.
 *
 * @param buffer Pointer to the buffer containing the data packet. Must not be NULL.
 * @param buffer_size Size of the input buffer in bytes.
 * @param pset Pointer to the parameter set structure to populate. Must not be NULL,
 * must be initialized, and its hash must match the packet's hash.
 * Its parameters must have valid 'data' pointers and correct 'size' values.
 * @return streaming_packet_status_t indicating success or failure reason.
 */
streaming_packet_status_t streaming_packet_parse_data(const uint8_t *buffer, size_t buffer_size, parameter_set_t *pset)
{
    if (!buffer || !pset)
    {
        return PACKET_E_INVALID;
    }
    if (!pset->parameters && pset->parameter_count > 0)
    {
        return PACKET_E_INVALID; // Invalid target parameterset
    }

    if (buffer_size < sizeof(streaming_data_packet_header_t))
    {
        return PACKET_E_BADSIZE; // Buffer too small for header
    }

    const streaming_data_packet_header_t *header = (const streaming_data_packet_header_t *)buffer;
    if (header->header.type != STREAMING_PACKET_TYPE_DATA)
    {
        return PACKET_E_BADTYPE;
    }

    // Verify hash match. Consider recalculating pset hash if modification is possible.
    // parameter_set_recalculate_hash(pset); // Only if needed
    if (header->header.identifier != pset->parameter_hash)
    {
        return PACKET_E_BADHASH;
    }

    // Calculate expected total size based on the target pset's definition
    size_t expected_payload_size = 0;
    for (size_t i = 0; i < pset->parameter_count; i++)
    {
        if (!pset->parameters[i].data)
        {
            return PACKET_E_NODATA; // Target parameter data pointer is NULL
        }
        expected_payload_size += pset->parameters[i].size;
    }
    size_t expected_total_size = sizeof(streaming_data_packet_header_t) + expected_payload_size;

    // Check if the provided buffer size matches exactly what's expected
    if (buffer_size != expected_total_size)
    {
        return PACKET_E_BADSIZE; // Mismatch indicates corrupted packet or wrong pset definition
    }

    // Copy data from buffer payload into the target parameter set's data pointers
    const uint8_t *payload_ptr = buffer + sizeof(streaming_data_packet_header_t);
    for (size_t i = 0; i < pset->parameter_count; i++)
    {
        fixed_size_parameter_t *parameter = &pset->parameters[i];
        // Data pointer checked above
        assert(parameter->data != NULL);
        memcpy(parameter->data, payload_ptr, parameter->size);
        payload_ptr += parameter->size;
    }

    // Sanity check: did we consume the whole buffer exactly?
    assert((size_t)(payload_ptr - buffer) == expected_total_size);

    return PACKET_SUCCESS;
}