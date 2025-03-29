#ifndef PARAMETER_SET_H
#define PARAMETER_SET_H

#include <stdint.h>
#include <stddef.h> // For size_t

/** @brief Error codes for parameter_set functions */
typedef enum
{
    PARAM_SET_SUCCESS     = 0,  ///< Operation successful
    PARAM_SET_E_NOMEM     = -1, ///< Memory allocation failed
    PARAM_SET_E_FULL      = -2, ///< Parameter set is full
    PARAM_SET_E_DUPLICATE = -3, ///< Parameter index already exists
    PARAM_SET_E_NOTFOUND  = -4, ///< Parameter index not found
    PARAM_SET_E_INVALID   = -5, ///< Invalid argument (e.g., NULL pointer)
    PARAM_SET_E_INTERNAL  = -6  ///< Internal inconsistency detected
} parameter_set_status_t;

/**
 * @brief Represents a single parameter with its data.
 * @note The lifetime of the data pointed to by 'data' is managed externally.
 */
typedef struct fixed_size_parameter
{
    uint32_t index; ///< Unique identifier for the parameter.
    uint32_t size;  ///< Size of the parameter data in bytes.
    uint8_t *data;  ///< Pointer to the parameter's data buffer.
} fixed_size_parameter_t;

/**
 * @brief Represents a collection of parameters for streaming.
 * @note The structure and its internal 'parameters' array are dynamically
 * allocated and must be managed via parameter_set_create() and
 * parameter_set_destroy().
 */
typedef struct parameter_set
{
    fixed_size_parameter_t *parameters;                         ///< Dynamically allocated array of parameters.
    size_t                  parameter_count;                    ///< Current number of parameters in the set.
    size_t                  parameter_count_max;                ///< Maximum capacity of the 'parameters' array.
    uint16_t                parameter_hash;                     ///< CRC16 hash of parameter indices.
    uint32_t                last_header_transmission_timestamp; ///< Timestamp of the last header transmission.
} parameter_set_t;

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
#define DEFINE_STATIC_PARAMETER_SET(NAME, MAX_PARAMETERS)                            \
    static fixed_size_parameter_t NAME##_parameters[MAX_PARAMETERS];                 \
    static parameter_set_t        NAME = { .parameters          = NAME##_parameters, \
                                           .parameter_count     = 0,                 \
                                           .parameter_count_max = MAX_PARAMETERS,    \
                                           .parameter_hash      = 0 }

parameter_set_t       *parameter_set_create(size_t max_parameters);
void                   parameter_set_destroy(parameter_set_t *pset);
parameter_set_status_t parameter_set_add(parameter_set_t *pset, fixed_size_parameter_t parameter);
parameter_set_status_t parameter_set_remove_by_index(parameter_set_t *pset, uint32_t parameter_index);
parameter_set_status_t parameter_set_clear(parameter_set_t *pset);
parameter_set_status_t parameter_set_recalculate_hash(parameter_set_t *pset);

#endif // PARAMETER_SET_H