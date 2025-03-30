#include "odin_stream/stream_parameter_set.h"

#include <stdlib.h> // For malloc, free
#include <string.h> // For memset, memmove
#include <assert.h> // For internal checks



static parameter_set_status_t parameter_set_update_hash(parameter_set_t *pset);
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
parameter_set_t *parameter_set_create(size_t max_parameters)
{
    if (max_parameters == 0) {
        return NULL; // Cannot create a set with zero capacity
    }

    parameter_set_t *pset = malloc(sizeof(parameter_set_t));
    if (!pset) {
        return NULL; // Allocation failed
    }

    pset->parameters = NULL;
    pset->parameter_count = 0;
    pset->parameter_count_max = max_parameters;
    pset->parameter_hash = 0; // Will be updated below

    pset->parameters = malloc(max_parameters * sizeof(fixed_size_parameter_t));
    if (!pset->parameters) {
        free(pset); // Clean up partially allocated struct
        return NULL; // Allocation failed
    }

    // Optional: Initialize parameter memory
    // memset(pset->parameters, 0, max_parameters * sizeof(fixed_size_parameter_t));

    parameter_set_update_hash(pset); // Initialize hash
    return pset;
}

/**
 * @brief Frees the memory associated with a parameter set.
 *
 * Frees the internal 'parameters' array and the set structure itself.
 * Does *not* free the data pointed to by individual parameter 'data' pointers,
 * as their lifetime is managed externally. Safe to call with NULL.
 *
 * @param pset Pointer to the parameter set to free. Can be NULL.
 */
void parameter_set_destroy(parameter_set_t *pset)
{
    if (!pset) {
        return; // Nothing to free
    }
    // Free the internal array first (if allocated)
    if (pset->parameters) {
        free(pset->parameters);
        pset->parameters = NULL; // Avoid dangling pointer
    }
    // Free the struct itself
    free(pset);
}

/**
 * @brief Adds a parameter (by copying its descriptor) to the set.
 *
 * Copies the provided `parameter` struct (including its index, size, and data pointer)
 * into the set's internal array if there is space and the index doesn't already exist.
 * Updates the set's hash after adding.
 *
 * @param pset Pointer to the parameter set. Must not be NULL.
 * @param parameter The parameter descriptor to add (copied by value).
 * @return parameter_set_status_t indicating success or failure reason.
 */
parameter_set_status_t parameter_set_add(parameter_set_t *pset, fixed_size_parameter_t parameter)
{
    if (!pset || !pset->parameters) {
        return PARAM_SET_E_INVALID; // Or assert(pset && pset->parameters)
    }

    if (pset->parameter_count >= pset->parameter_count_max) {
        return PARAM_SET_E_FULL;
    }

    // Check for duplicate index
    for (size_t i = 0; i < pset->parameter_count; i++) {
        if (pset->parameters[i].index == parameter.index) {
            return PARAM_SET_E_DUPLICATE;
        }
    }

    // Add the parameter (struct copy)
    pset->parameters[pset->parameter_count] = parameter;
    pset->parameter_count++; // Increment count *after* successful add

    parameter_set_update_hash(pset);

    return PARAM_SET_SUCCESS;
}

/**
 * @brief Removes a parameter from the set based on its index.
 *
 * Finds the parameter with the matching index and removes it by shifting
 * subsequent elements down in the internal array. Updates the set's hash.
 *
 * @param pset Pointer to the parameter set. Must not be NULL.
 * @param parameter_index The index of the parameter to remove.
 * @return parameter_set_status_t indicating success or failure reason.
 */
parameter_set_status_t parameter_set_remove_by_index(parameter_set_t *pset, uint32_t parameter_index)
{
     if (!pset || !pset->parameters) {
        return PARAM_SET_E_INVALID; // Or assert(pset && pset->parameters)
    }

    for (size_t i = 0; i < pset->parameter_count; i++) {
        if (pset->parameters[i].index == parameter_index) {
            // Found it. Calculate number of elements to move.
            size_t elements_to_move = pset->parameter_count - 1 - i;
            if (elements_to_move > 0) {
                // Shift remaining elements down using memmove for safety
                memmove(&pset->parameters[i],          // Destination
                        &pset->parameters[i + 1],      // Source
                        elements_to_move * sizeof(fixed_size_parameter_t));
            }

            // Decrement count and update hash
            pset->parameter_count--;
            parameter_set_update_hash(pset);
            return PARAM_SET_SUCCESS;
        }
    }

    return PARAM_SET_E_NOTFOUND; // Parameter index not found
}

/**
 * @brief Removes all parameters from the set (sets count to 0).
 *
 * Resets the `parameter_count` to zero and updates the hash.
 * Does not change the maximum capacity or free allocated memory
 * (use parameter_set_destroy for that).
 *
 * @param pset Pointer to the parameter set. Must not be NULL.
 * @return parameter_set_status_t indicating success or failure reason.
 */
parameter_set_status_t parameter_set_clear(parameter_set_t *pset)
{
     if (!pset) {
        return PARAM_SET_E_INVALID; // Or assert(pset)
    }

    pset->parameter_count = 0;
    parameter_set_update_hash(pset); // Recalculate hash for empty set
    return PARAM_SET_SUCCESS;
}


/**
 * @brief Recalculates and updates the parameter_hash field.
 * Intended for internal use but exposed if needed externally.
 * @param pset Pointer to the parameter set. Must not be NULL.
 * @return parameter_set_status_t indicating success or failure reason.
 */
parameter_set_status_t parameter_set_recalculate_hash(parameter_set_t *pset) {
     if (!pset) {
        return PARAM_SET_E_INVALID; // Or assert(pset)
    }
    return parameter_set_update_hash(pset);
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
    if (!data && length > 0) {
         // Programming error: should not happen if called correctly
         assert(0 && "NULL data pointer passed to crc16 with non-zero length");
         return crc; // Or some other error indication if asserts disabled
    }

    for (size_t i = 0; i < length; i++) {
        crc ^= ((uint16_t)data[i]) << 8;
        for (int j = 0; j < 8; j++) {
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
 * @brief Internal helper to update the parameter hash (CRC16 of indices).
 * Assumes pset is not NULL and pset->parameters is valid if count > 0.
 * @param pset Non-NULL pointer to the parameter set.
 * @return PARAM_SET_SUCCESS (currently always succeeds if preconditions met).
 */

static parameter_set_status_t parameter_set_update_hash(parameter_set_t *pset)
{
    assert(pset != NULL && "NULL pset passed to parameter_set_update_hash");

    uint16_t crc = 0xFFFF; // Initial value for CRC-16 CCITT-FALSE
    // Calculate CRC only if parameters array exists (it should if pset is valid)
    if (pset->parameters) {
        for (size_t i = 0; i < pset->parameter_count; i++) {
            // Calculate CRC based on index
            uint32_t index = pset->parameters[i].index;
            crc = crc16(crc, (const uint8_t *)&index, sizeof(index));
        }
    } else {
        // This indicates an inconsistent state if parameter_count > 0
         assert(pset->parameter_count == 0 && "Parameter array is NULL but count > 0");
    }
    pset->parameter_hash = crc;
    return PARAM_SET_SUCCESS;
}
