#include "odin_stream/stream_parameter_set.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static stream_parameter_set_status_t stream_parameter_set_update_contents(stream_parameter_set_t *parameter_set);

static uint16_t crc16(uint16_t crc, const uint8_t *data, size_t length);

/**
 * @brief Creates and allocates a new parameter set.
 *
 * Allocates memory for the parameter_set_t structure and its internal
 * 'parameters' array. Initializes count to 0 and calculates the initial hash.
 *
 * @param max_parameters The maximum number of parameters the set can hold. Must be > 0.
 * @return A pointer to the newly allocated parameter_set_t, or NULL if
 * allocation fails or max_parameters is 0.
 * @note The returned pointer must be freed using parameter_set_destroy().
 */
stream_parameter_set_t *stream_parameter_set_create(size_t max_parameters)
{
    if (max_parameters == 0)
    {
        return NULL; // Cannot create a set with zero capacity
    }

    stream_parameter_set_t *parameter_set = malloc(sizeof(stream_parameter_set_t));
    if (!parameter_set)
    {
        return NULL; // Allocation failed
    }

    // Memset to zero
    memset(parameter_set, 0, sizeof(stream_parameter_set_t));

    // Initialize the data
    parameter_set->parameter_count_max = max_parameters;

    // Allocate the internal array of parameters
    parameter_set->parameters = malloc(max_parameters * sizeof(stream_fixed_size_parameter_t));
    if (!parameter_set->parameters)
    {
        free(parameter_set); // Clean up partially allocated struct
        return NULL;         // Allocation failed
    }

    stream_parameter_set_update_contents(parameter_set); // Initialize hash
    return parameter_set;
}

/**
 * @brief Frees the memory associated with a parameter set.
 *
 * Frees the internal 'parameters' array and the set structure itself.
 * Does *not* free the data pointed to by individual parameter 'data' pointers,
 * as their lifetime is managed externally. Safe to call with NULL.
 *
 * @param parameter_set Pointer to the parameter set to free. Can be NULL.
 */
void stream_parameter_set_destroy(stream_parameter_set_t *parameter_set)
{
    if (!parameter_set)
    {
        return; // Nothing to free
    }
    // Free the internal array first (if allocated)
    if (parameter_set->parameters)
    {
        free(parameter_set->parameters);
        parameter_set->parameters = NULL; // Avoid dangling pointer
    }
    // Free the struct itself
    free(parameter_set);
}

/**
 * @brief Adds a parameter (by copying its descriptor) to the set.
 *
 * Copies the provided `parameter` struct (including its index, size, and data pointer)
 * into the set's internal array if there is space and the index doesn't already exist.
 * Updates the set's hash after adding.
 *
 * @param parameter_set Pointer to the parameter set. Must not be NULL.
 * @param parameter The parameter descriptor to add (copied by value).
 * @return parameter_set_status_t indicating success or failure reason.
 */
stream_parameter_set_status_t stream_parameter_set_add(stream_parameter_set_t       *parameter_set,
                                                       stream_fixed_size_parameter_t parameter)
{
    if (!parameter_set || !parameter_set->parameters)
    {
        return STREAM_PARAM_SET_ERROR_INVALID; // Or assert(parameter_set && parameter_set->parameters)
    }

    if (parameter_set->parameter_count >= parameter_set->parameter_count_max)
    {
        return STREAM_PARAM_SET_ERROR_FULL;
    }

    // Check for duplicate indexes
    for (size_t i = 0; i < parameter_set->parameter_count; i++)
    {
        if (parameter_set->parameters[i].index == parameter.index)
        {
            return STREAM_PARAM_SET_ERROR_DUPLICATE;
        }
    }

    // Add the parameter (struct copy)
    parameter_set->parameters[parameter_set->parameter_count] = parameter;
    parameter_set->parameter_count++; // Increment count *after* successful add

    stream_parameter_set_update_contents(parameter_set);

    return STREAM_PARAM_SET_SUCCESS;
}

/**
 * @brief Removes a parameter from the set based on its index.
 *
 * Finds the parameter with the matching index and removes it by shifting
 * subsequent elements down in the internal array. Updates the set's hash.
 *
 * @param parameter_set Pointer to the parameter set. Must not be NULL.
 * @param parameter_index The index of the parameter to remove.
 * @return parameter_set_status_t indicating success or failure reason.
 */
stream_parameter_set_status_t stream_parameter_set_remove_by_index(stream_parameter_set_t *parameter_set,
                                                                   uint32_t                parameter_index)
{
    if (!parameter_set || !parameter_set->parameters)
    {
        return STREAM_PARAM_SET_ERROR_INVALID; // Or assert(parameter_set && parameter_set->parameters)
    }

    for (size_t i = 0; i < parameter_set->parameter_count; i++)
    {
        if (parameter_set->parameters[i].index == parameter_index)
        {
            // Found it. Calculate number of elements to move.
            size_t elements_to_move = parameter_set->parameter_count - 1 - i;
            if (elements_to_move > 0)
            {
                // Shift remaining elements down using memmove for safety
                memmove(&parameter_set->parameters[i],     // Destination
                        &parameter_set->parameters[i + 1], // Source
                        elements_to_move * sizeof(stream_fixed_size_parameter_t));
            }

            // Decrement count and update hash
            parameter_set->parameter_count--;
            stream_parameter_set_update_contents(parameter_set);
            return STREAM_PARAM_SET_SUCCESS;
        }
    }

    return STREAM_PARAM_SET_ERROR_NOTFOUND; // Parameter index not found
}

/**
 * @brief Removes all parameters from the set (sets count to 0).
 *
 * Resets the `parameter_count` to zero and updates the hash.
 * Does not change the maximum capacity or free allocated memory
 * (use parameter_set_destroy for that).
 *
 * @param parameter_set Pointer to the parameter set. Must not be NULL.
 * @return parameter_set_status_t indicating success or failure reason.
 */
stream_parameter_set_status_t parameter_set_clear(stream_parameter_set_t *parameter_set)
{
    if (!parameter_set)
    {
        return STREAM_PARAM_SET_ERROR_INVALID; // Or assert(parameter_set)
    }

    parameter_set->parameter_count = 0;
    stream_parameter_set_update_contents(parameter_set); // Recalculate hash for empty set
    return STREAM_PARAM_SET_SUCCESS;
}

/**
 * @brief Recalculates and updates the parameter_hash field.
 * Intended for internal use but exposed if needed externally.
 * @param parameter_set Pointer to the parameter set. Must not be NULL.
 * @return parameter_set_status_t indicating success or failure reason.
 */
stream_parameter_set_status_t parameter_set_recalculate_hash(stream_parameter_set_t *parameter_set)
{
    if (!parameter_set)
    {
        return STREAM_PARAM_SET_ERROR_INVALID; // Or assert(parameter_set)
    }
    return stream_parameter_set_update_contents(parameter_set);
}

/**
 * @internal
 * @brief Calculates CRC-16 CCITT-FALSE.
 * Polynomial: 0x1021, Initial Value: 0xFFFF, No XOR Out, No Reflect In/Out.
 * @param crc Starting CRC value.
 * @param data Pointer to data buffer. Can be NULL if length is 0.
 * @param length Number of bytes in data buffer.
 * @return Calculated CRC16 value.
 */
static uint16_t crc16(uint16_t crc, const uint8_t *data, size_t length)
{
    if (!data && length > 0)
    {
        // Programming error: should not happen if called correctly
        assert(0 && "NULL data pointer passed to crc16 with non-zero length");
        return crc; // Or some other error indication if asserts disabled
    }

    for (size_t i = 0; i < length; i++)
    {
        crc ^= ((uint16_t)data[i]) << 8;
        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021; // Polynomial 0x1021
            else
                crc <<= 1;
        }
    }
    return crc;
}

/**
 * @internal
 * @brief Internal helper to update the parameter hash (CRC16 of indices) and payload size.
 * Assumes parameter_set is not NULL and parameter_set->parameters is valid if count > 0.
 * @param parameter_set Non-NULL pointer to the parameter set.
 * @return STREAM_PARAM_SET_SUCCESS (currently always succeeds if preconditions met).
 */

static stream_parameter_set_status_t stream_parameter_set_update_contents(stream_parameter_set_t *parameter_set)
{
    assert(parameter_set != NULL && "NULL parameter_set passed to parameter_set_update_hash");

    uint16_t crc = 0xFFFF; // Initial value for CRC-16 CCITT-FALSE
    // Calculate CRC only if parameters array exists (it should if parameter_set is valid)
    if (parameter_set->parameters)
    {
        for (size_t i = 0; i < parameter_set->parameter_count; i++)
        {
            // Calculate CRC based on index
            uint32_t index = parameter_set->parameters[i].index;
            crc            = crc16(crc, (const uint8_t *)&index, sizeof(index));
        }
    }
    else
    {
        // This indicates an inconsistent state if parameter_count > 0
        assert(parameter_set->parameter_count == 0 && "Parameter array is NULL but count > 0");
    }
    parameter_set->parameter_set_identifier = crc;

    // Calculate required payload size
    size_t required_payload_size = 0;
    for (size_t i = 0; i < parameter_set->parameter_count; i++)
    {
        required_payload_size += parameter_set->parameters[i].size;
    }
    // Set payload size in the parameter set
    parameter_set->payload_size = (uint16_t)required_payload_size;

    return STREAM_PARAM_SET_SUCCESS;
}
