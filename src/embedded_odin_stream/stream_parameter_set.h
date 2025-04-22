#ifndef PARAMETER_SET_H
#define PARAMETER_SET_H

#include <stdint.h>
#include <stddef.h>

/** @brief Error codes for parameter_set functions */
typedef enum
{
    STREAM_PARAM_SET_SUCCESS         = 0,  // Operation successful
    STREAM_PARAM_SET_ERROR_NOMEM     = -1, // Memory allocation failed
    STREAM_PARAM_SET_ERROR_FULL      = -2, // Parameter set is full
    STREAM_PARAM_SET_ERROR_DUPLICATE = -3, // Parameter index already exists
    STREAM_PARAM_SET_ERROR_NOTFOUND  = -4, // Parameter index not found
    STREAM_PARAM_SET_ERROR_INVALID   = -5, // Invalid argument (e.g., NULL pointer)
    STREAM_PARAM_SET_ERROR_INTERNAL  = -6  // Internal inconsistency detected
} stream_parameter_set_status_t;

/**
 * @brief Represents a single parameter with its data.
 * @note The lifetime of the data pointed to by 'data' is managed externally.
 * The data pointer can be NULL, meaning no data is associated with the parameter. (yet)
 */
typedef struct fixed_size_parameter
{
    uint32_t index; ///< Unique identifier for the parameter.
    uint16_t size;  ///< Size of the parameter data in bytes.
    uint8_t *data;  ///< Pointer to the parameter's data buffer.
} stream_fixed_size_parameter_t;

/**
 * @brief Represents a collection of parameters for streaming.
 * @note The structure and its internal 'parameters' array are dynamically
 * allocated and must be managed via parameter_set_create() and
 * parameter_set_destroy().
 */

typedef struct
{
    stream_fixed_size_parameter_t *parameters; // Dynamically allocated array of parameters.

    size_t   parameter_count;                    // Current number of parameters in the set.
    size_t   parameter_count_max;                // Maximum capacity of the 'parameters' array.
    uint16_t parameter_set_identifier;           // Identifier for the parameter set (CRC16 of indices).
    uint16_t payload_size;                       // Total size of the parameter data in bytes.
    uint16_t definition_identifier;              // Identifier for the parameter group.
    uint32_t last_header_transmission_timestamp; // Timestamp of the last header transmission.
    uint32_t last_data_transmission_timestamp;   // Timestamp of the last data transmission.
    uint16_t last_transmission_sequence_number;  // Sequence number of the last header transmission.
} stream_parameter_set_t;

// --- Function Declarations ---

/**
 * @brief Macro to define a static parameter set with a fixed maximum size.
 *
 * This macro creates a static parameter set with a fixed size, avoiding
 * dynamic memory allocation. The parameter set is initialized at compile time.
 *
 * @param NAME The name of the static parameter set variable.
 * @param MAX_PARAMETERS The maximum number of parameters the set can hold.
 */
#define DEFINE_STATIC_PARAMETER_SET(NAME, MAX_PARAMETERS)                   \
    static stream_fixed_size_parameter_t NAME##_parameters[MAX_PARAMETERS]; \
    static stream_parameter_set_t NAME = { .parameters = NAME##_parameters, .parameter_count_max = MAX_PARAMETERS }

stream_parameter_set_t       *stream_parameter_set_create(size_t max_parameters);
void                          stream_parameter_set_destroy(stream_parameter_set_t *parameter_set);
stream_parameter_set_status_t stream_parameter_set_add(stream_parameter_set_t       *parameter_set,
                                                       stream_fixed_size_parameter_t parameter);
stream_parameter_set_status_t stream_parameter_set_remove_by_index(stream_parameter_set_t *parameter_set,
                                                                   uint32_t                parameter_index);
// stream_parameter_set_status_t stream_parameter_set_clear(stream_parameter_set_t *parameter_set);
// stream_parameter_set_status_t stream_parameter_set_recalculate_hash(stream_parameter_set_t *parameter_set);

#endif // PARAMETER_SET_H