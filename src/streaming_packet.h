#ifndef STREAMING_PACKET_H
#define STREAMING_PACKET_H

#include <stdint.h>
#include <stddef.h>        // For size_t
#include "parameter_set.h" // Needs definition of parameter_set_t

/** @brief Error codes for streaming_packet functions */
typedef enum
{
    PACKET_SUCCESS    = 0,  ///< Operation successful
    PACKET_E_INVALID  = -1, ///< Invalid argument (e.g., NULL pointer)
    PACKET_E_BADSIZE  = -2, ///< Input/output buffer too small or invalid size reported
    PACKET_E_BADTYPE  = -3, ///< Incorrect packet type found during parsing
    PACKET_E_BADHASH  = -4, ///< Parameter group hash mismatch during parsing
    PACKET_E_NODATA   = -5, ///< Required parameter data pointer is NULL
    PACKET_E_OVERFLOW = -6, ///< Data size exceeds packet format limits (e.g., uint16_t)
    PACKET_E_INTERNAL = -7, ///< Internal inconsistency or logic error
    PACKET_E_NOMEM    = -8  ///< Memory allocation failed (relevant for parsing funcs)
} streaming_packet_status_t;

/**
 * @brief Defines the type of streaming packet.
 */
typedef enum
{
    STREAMING_PACKET_TYPE_IDENTIFIER = 0x01, ///< Packet contains parameter identifiers and sizes.
    STREAMING_PACKET_TYPE_DATA       = 0x02, ///< Packet contains parameter data.
} streaming_packet_type_t;

// Packing ensures structs match byte layout in packets exactly.
#pragma pack(push, 1)

/** @brief Header for an identifier packet. */
typedef struct
{
    streaming_packet_type_t type : 8;        // Identifies the packet type.
    uint16_t                identifier : 16; // Identifier for the packet structure.
    uint8_t                 reserved : 8;    // Reserved for future use, should be 0.
} streaming_packet_header_t;

/** @brief Header for an identifier packet. */
typedef struct
{
    streaming_packet_header_t header;             ///< Header for the packet.
    uint32_t                  odin_definition_id; ///< Hash identifying the overall parameter definitions.
} streaming_identifier_packet_header_t;

/** @brief Header for a data packet. */
typedef struct
{
    streaming_packet_header_t header;    ///< Header for the packet.
    uint32_t                  timestamp; ///< Timestamp for the data sample.
} streaming_data_packet_header_t;

/** @brief Describes a single parameter within an identifier packet's payload. */
typedef struct
{
    uint32_t index; ///< Parameter index.
    uint16_t size;  ///< Parameter size in bytes.
} ident_payload_item_t;

#pragma pack(pop)

// --- Function Declarations ---

/**
 * @brief Generates an identifier packet into the provided buffer.
 * @see streaming_packet_create_identifier in streaming_packet.c for details.
 */
int streaming_packet_create_identifier(const parameter_set_t *pset, uint8_t *buffer, size_t buffer_size);

/**
 * @brief Parses an identifier packet and creates a new parameter set.
 * @see streaming_packet_parse_identifier in streaming_packet.c for details.
 */
parameter_set_t *streaming_packet_parse_identifier(const uint8_t *buffer, size_t buffer_size);

/**
 * @brief Generates a data packet into the provided buffer.
 * @see streaming_packet_create_data in streaming_packet.c for details.
 */
int streaming_packet_create_data(const parameter_set_t *pset, uint8_t *buffer, size_t buffer_size, uint32_t timestamp);

/**
 * @brief Parses a data packet and populates data pointers of a compatible parameter set.
 * @see streaming_packet_parse_data in streaming_packet.c for details.
 */
streaming_packet_status_t streaming_packet_parse_data(const uint8_t *buffer, size_t buffer_size, parameter_set_t *pset);

#endif // STREAMING_PACKET_H