#ifndef STREAM_STREAM_PACKET_H
#define STREAM_STREAM_PACKET_H

#include <stdint.h>
#include <stddef.h>
#include "odin_stream/stream_parameter_set.h"

/**
 * @brief Defines the type of streaming packet.
 */
typedef enum
{
    STREAM_STREAM_PACKET_TYPE_INVALID    = 0x00, ///< Unknown packet type.
    STREAM_STREAM_PACKET_TYPE_IDENTIFIER = 0x01, ///< Packet contains parameter identifiers and sizes.
    STREAM_STREAM_PACKET_TYPE_DATA       = 0x02, ///< Packet contains parameter data.
    STREAM_STREAM_PACKET_TYPE_EVENT      = 0x0B, ///< Packet contains an event.
} stream_packet_type_t;

// Packing ensures structs match byte layout in packets exactly.
#pragma pack(push, 1)

/** @brief Header for all packets. */
typedef struct
{
    stream_packet_type_t type : 8;        // Identifies the packet type.
    uint16_t             identifier : 16; // Identifier for the packet structure.
    uint8_t              reserved : 8;    // Reserved for future use, should be 0.
} stream_packet_header_t;

/** @brief Header for an identifier packet.
    type must be STREAM_STREAM_PACKET_TYPE_IDENTIFIER
*/
typedef struct
{
    stream_packet_header_t header;                ///< Header for the packet.
    uint32_t               definition_identifier; ///< Identity of the overall parameter definitions.
} streaming_identifier_packet_header_t;

/** @brief Describes a single parameter within an identifier packet's payload. */
typedef struct
{
    uint32_t index; ///< Parameter index.
    uint16_t size;  ///< Parameter size in bytes.
} streaming_identifier_item_t;

/** @brief Header for a data packet.
    type must be STREAM_STREAM_PACKET_TYPE_DATA
*/
typedef struct
{
    stream_packet_header_t header;    // Header for the packet.
    uint32_t               timestamp; // Timestamp for the data sample.
    uint16_t sequence_number; // Sequence number for the data sample, incremented for each packet with the format.
} streaming_data_packet_header_t;

/** @brief Header for a event packet.
    type must be STREAM_STREAM_PACKET_TYPE_EVENT
*/
typedef struct
{
    stream_packet_header_t header;          ///< Header for the packet.
    uint32_t               timestamp;       ///< Timestamp for the event.
    uint16_t               sequence_number; ///< Sequence number for the event, incremented for each packet.
} streaming_event_packet_header_t;

#pragma pack(pop)

typedef struct
{
    uint16_t       event_id;       // Identifier for the event.
    uint16_t       event_sequence; // Last sequence number for the event.
    uint32_t       timestamp;      // Timestamp for the event.
    const uint8_t *event_data;     // Pointer to the event data.
    uint16_t       event_size;     // Size of the event data in bytes.
} stream_event_t;

/** @brief Error codes for streaming_packet functions */
typedef enum
{
    STREAM_PACKET_SUCCESS        = 0,  // Operation successful
    STREAM_PACKET_ERROR_INVALID  = -1, // Invalid argument (e.g., NULL pointer)
    STREAM_PACKET_ERROR_BADSIZE  = -2, // Input/output buffer too small or invalid size reported
    STREAM_PACKET_ERROR_BADTYPE  = -3, // Incorrect packet type found during parsing
    STREAM_PACKET_ERROR_BADHASH  = -4, // Parameter group hash mismatch during parsing
    STREAM_PACKET_ERROR_NODATA   = -5, // Required parameter data pointer is NULL
    STREAM_PACKET_ERROR_OVERFLOW = -6, // Data size exceeds packet format limits (e.g., uint16_t)
    STREAM_PACKET_ERROR_INTERNAL = -7, // Internal inconsistency or logic error
    STREAM_PACKET_ERROR_NOMEM    = -8  // Memory allocation failed (relevant for parsing funcs)
} stream_packet_status_t;

/**
 * @brief Generates an identifier packet into the provided buffer.
 * @see streaming_packet_create_identifier in streaming_packet.c for details.
 */
int stream_packet_create_identifier(stream_parameter_set_t *parameter_set,
                                    uint8_t                *buffer,
                                    size_t                  buffer_size,
                                    uint32_t                timestamp,
                                    uint32_t                header_transmission_interval);

/**
 * @brief Parses an identifier packet and creates a new parameter set.
 * @see streaming_packet_parse_identifier in streaming_packet.c for details.
 */
stream_parameter_set_t *stream_packet_parse_identifier(const uint8_t *buffer, size_t buffer_size);

/**
 * @brief Generates a data packet into the provided buffer.
 * @see streaming_packet_create_data in streaming_packet.c for details.
 */
int stream_packet_create_data(stream_parameter_set_t *parameter_set,
                              uint8_t                *buffer,
                              size_t                  buffer_size,
                              uint32_t                timestamp,
                              uint32_t                data_transmission_interval);
/**
 * @brief Parses a data packet and populates data pointers of a compatible parameter set.
 * @see streaming_packet_parse_data in streaming_packet.c for details.
 */
stream_packet_status_t stream_packet_parse_data(const uint8_t          *buffer,
                                                size_t                  buffer_size,
                                                stream_parameter_set_t *parameter_set);

/**
 * @brief Parses an event packet and populates the event structure.
 * @see streaming_packet_parse_event in streaming_packet.c for details.
 */
stream_packet_status_t stream_packet_parse_event(const uint8_t *buffer, size_t buffer_size, stream_event_t *event);

/**
 * @brief Generates an event packet into the provided buffer.
 * @see streaming_packet_create_event in streaming_packet.c for details.
 */
int stream_packet_create_event(uint8_t *buffer, size_t buffer_size, stream_event_t event);

/**
 * @brief Retrieves the packet type from the provided buffer.
 * @param buffer Pointer to the buffer containing the packet data
 * @param buffer_size Size of the buffer in bytes.
 * @return The packet type as defined in stream_packet_type_t.
 */
inline stream_packet_type_t stream_packet_get_type(const uint8_t *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size < sizeof(stream_packet_header_t))
    {
        return STREAM_STREAM_PACKET_TYPE_INVALID; // Invalid input
    }
    return ((stream_packet_header_t *)buffer)->type;
}

#endif // STREAM_STREAM_PACKET_H