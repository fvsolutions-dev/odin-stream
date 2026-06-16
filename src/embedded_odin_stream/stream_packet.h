#ifndef STREAM_STREAM_PACKET_H
#define STREAM_STREAM_PACKET_H

#include <stdint.h>
#include <stddef.h>
#include "./stream_parameter_set.h"

/**
 * @brief Defines the type of streaming packet.
 */
typedef enum
{
    STREAM_STREAM_PACKET_TYPE_INVALID = 0x00,    ///< Unknown packet type.
    STREAM_STREAM_PACKET_TYPE_IDENTIFIER = 0x01, ///< Packet contains parameter identifiers and sizes.
    STREAM_STREAM_PACKET_TYPE_DATA = 0x02,       ///< Packet contains parameter data.
    STREAM_STREAM_PACKET_TYPE_IDENTIFIER_EXT = 0x03, ///< Self-describing schema: index+size+element_type+name+type_id, chunked.
    STREAM_STREAM_PACKET_TYPE_TYPE_DESCRIPTOR = 0x04, ///< CUSTOM-type field layouts (struct introspection), keyed by type_id, chunked.
    STREAM_STREAM_PACKET_TYPE_EVENT = 0x0B,      ///< Packet contains an event.
} stream_packet_type_t;

// Packing ensures structs match byte layout in packets exactly.
#pragma pack(push, 1)

/** @brief Header for all packets. */
typedef struct
{
    uint8_t type;        // Identifies the packet type.
    uint16_t identifier; // Identifier for the packet structure.
    uint8_t reserved;    // Reserved for future use, should be 0.
} stream_packet_header_t;

/** @brief Header for an identifier packet.
    type must be STREAM_STREAM_PACKET_TYPE_IDENTIFIER
*/
typedef struct
{
    stream_packet_header_t header;  ///< Header for the packet.
    uint32_t definition_identifier; ///< Identity of the overall parameter definitions.
} streaming_identifier_packet_header_t;

/** @brief Describes a single parameter within an identifier packet's payload. */
typedef struct
{
    uint32_t index; ///< Parameter index.
    uint16_t size;  ///< Parameter size in bytes.
} streaming_identifier_item_t;

/** @brief Header for an extended-identifier packet.
    type must be STREAM_STREAM_PACKET_TYPE_IDENTIFIER_EXT

    A superset/replacement of the plain IDENTIFIER: each item additionally
    carries the parameter's element type and its (dotted) name, so a receiver
    can reconstruct a human-readable, correctly-typed schema with no external
    object dictionary. Names make the full schema too large for one PDU, so the
    packet is CHUNKED: each one carries a contiguous run of `total_count`
    parameters starting at `start_ordinal`; an encoder pages through the set,
    wrapping, so late joiners eventually learn every parameter. `header.identifier`
    is the same CRC16(indices) as the plain IDENTIFIER and DATA, so DATA still
    resolves against a schema learned from either header.
*/
typedef struct
{
    stream_packet_header_t header;  ///< Header for the packet (type = IDENTIFIER_EXT).
    uint32_t definition_identifier; ///< Identity of the overall parameter definitions.
    uint16_t total_count;           ///< Total parameters in the full schema (completion signal).
    uint16_t start_ordinal;         ///< 0-based ordinal of the first item in THIS chunk.
} streaming_identifier_ext_packet_header_t;

/** @brief Fixed-size prefix of one item in an extended-identifier payload.
    Immediately followed by `name_len` raw bytes of name (NOT NUL-terminated). */
typedef struct
{
    uint32_t index;        ///< Parameter index.
    uint16_t size;         ///< Parameter size in bytes (total: element_size * count for arrays).
    uint8_t  element_type; ///< ODIN_element_type_t value (for value interpretation).
    uint8_t  name_len;     ///< Number of name bytes that follow this prefix.
    uint16_t type_id;      ///< For element_type==CUSTOM: id of the TYPE_DESCRIPTOR (0x04) giving the
                           ///< struct field layout; 0 otherwise. Lets composites decode with no OD.
} streaming_identifier_ext_item_header_t;

/** @brief Header for a type-descriptor packet (STREAM_STREAM_PACKET_TYPE_TYPE_DESCRIPTOR).

    Carries the field layout of one or more CUSTOM types so a receiver can split
    a struct value into its named, typed fields with no external object
    dictionary. Chunked like IDENTIFIER_EXT (a type is atomic within a chunk).
    Each described type is referenced by `type_id` from IDENTIFIER_EXT items. */
typedef struct
{
    stream_packet_header_t header;  ///< type = STREAM_STREAM_PACKET_TYPE_TYPE_DESCRIPTOR.
    uint32_t definition_identifier; ///< Same as the IDENTIFIER_EXT definition_identifier.
    uint16_t total_count;           ///< Total CUSTOM types in the schema (completion signal).
    uint16_t start_ordinal;         ///< 0-based ordinal of the first type in THIS chunk.
} streaming_type_descriptor_packet_header_t;

/** @brief Fixed-size prefix of one type in a type-descriptor payload. Followed by
    `name_len` raw type-name bytes, then `field_count` field entries (each a
    streaming_type_field_header_t prefix + `name_len` raw field-name bytes). */
typedef struct
{
    uint16_t type_id;      ///< Stable id referenced by IDENTIFIER_EXT items.
    uint16_t size;         ///< Total struct size in bytes.
    uint8_t  field_count;  ///< Number of field entries that follow (after the type name).
    uint8_t  name_len;     ///< Type-name bytes following this prefix.
} streaming_type_descriptor_header_t;

/** @brief Fixed-size prefix of one field within a type descriptor.
    Immediately followed by `name_len` raw field-name bytes. */
typedef struct
{
    uint8_t  element_type; ///< ODIN_element_type_t value of the field (primitive).
    uint16_t size;         ///< Field size in bytes.
    uint16_t offset;       ///< Byte offset of the field within the struct.
    uint8_t  name_len;     ///< Field-name bytes following this prefix.
} streaming_type_field_header_t;

/** @brief Header for a data packet.
    type must be STREAM_STREAM_PACKET_TYPE_DATA
*/
typedef struct
{
    stream_packet_header_t header; // Header for the packet.
    uint32_t timestamp;            // Timestamp for the data sample.
    uint16_t sequence_number;      // Sequence number for the data sample, incremented for each packet with the format.
} streaming_data_packet_header_t;

/** @brief Header for a event packet.
    type must be STREAM_STREAM_PACKET_TYPE_EVENT
*/
typedef struct
{
    stream_packet_header_t header; ///< Header for the packet.
    uint32_t timestamp;            ///< Timestamp for the event.
    uint16_t sequence_number;      ///< Sequence number for the event, incremented for each packet.
} streaming_event_packet_header_t;

#pragma pack(pop)

typedef struct
{
    uint16_t event_id;         // Identifier for the event.
    uint16_t event_sequence;   // Last sequence number for the event.
    uint32_t timestamp;        // Timestamp for the event.
    const uint8_t *event_data; // Pointer to the event data.
    uint16_t event_size;       // Size of the event data in bytes.
} stream_event_t;

/** @brief Error codes for streaming_packet functions */
typedef enum
{
    STREAM_PACKET_SUCCESS = 0,         // Operation successful
    STREAM_PACKET_ERROR_INVALID = -1,  // Invalid argument (e.g., NULL pointer)
    STREAM_PACKET_ERROR_BADSIZE = -2,  // Input/output buffer too small or invalid size reported
    STREAM_PACKET_ERROR_BADTYPE = -3,  // Incorrect packet type found during parsing
    STREAM_PACKET_ERROR_BADHASH = -4,  // Parameter group hash mismatch during parsing
    STREAM_PACKET_ERROR_NODATA = -5,   // Required parameter data pointer is NULL
    STREAM_PACKET_ERROR_OVERFLOW = -6, // Data size exceeds packet format limits (e.g., uint16_t)
    STREAM_PACKET_ERROR_INTERNAL = -7, // Internal inconsistency or logic error
    STREAM_PACKET_ERROR_NOMEM = -8     // Memory allocation failed (relevant for parsing funcs)
} stream_packet_status_t;

/* ---- Type-descriptor input model (what a producer hands the encoder) ----
   These are plain C structs (not wire layout). A producer (e.g. data_logger,
   from the OD's ODIN_type_descriptor_t) fills a stream_type_set_t describing the
   CUSTOM types its parameters use; stream_packet_create_type_descriptor pages
   them onto the wire as 0x04 packets. */
typedef struct
{
    const char *name;        // field name (NUL-terminated)
    uint8_t     element_type; // ODIN_element_type_t value
    uint16_t    size;        // field size in bytes
    uint16_t    offset;      // byte offset within the struct
} stream_type_field_t;

typedef struct
{
    uint16_t                   type_id;     // stable id referenced by IDENTIFIER_EXT items
    const char                *type_name;   // NUL-terminated
    uint16_t                   size;        // total struct size
    uint8_t                    field_count;
    const stream_type_field_t *fields;
} stream_type_descriptor_t;

typedef struct
{
    const stream_type_descriptor_t *types;
    size_t                          count;
    uint32_t                        definition_identifier;
    uint32_t                        last_transmission_timestamp; // managed by the encoder (self-throttle)
} stream_type_set_t;

/**
 * @brief Generates an identifier packet into the provided buffer.
 * @see streaming_packet_create_identifier in streaming_packet.c for details.
 */
int stream_packet_create_identifier(stream_parameter_set_t *parameter_set,
                                    uint8_t *buffer,
                                    size_t buffer_size,
                                    uint32_t timestamp,
                                    uint32_t header_transmission_interval);

/**
 * @brief Parses an identifier packet and creates a new parameter set.
 * @see streaming_packet_parse_identifier in streaming_packet.c for details.
 */
stream_parameter_set_t *stream_packet_parse_identifier(const uint8_t *buffer, size_t buffer_size);

/**
 * @brief Generates one chunk of an extended-identifier (self-describing) packet.
 *
 * Self-throttled like stream_packet_create_identifier (returns 0 when not due).
 * When due, packs a contiguous run of parameters starting at *cursor — taking
 * each parameter's `name` and `element_type` from the set — for as many as fit
 * in buffer_size, then advances *cursor (wrapping to 0 past the last parameter)
 * so successive calls page through the whole set. Pass buffer_size = the
 * smallest downstream transport budget (e.g. the BLE PDU cap) so no chunk is
 * truncated mid-item on air.
 *
 * @param cursor Caller-owned paging cursor; initialise to 0. Updated in place.
 * @return bytes written (>0), 0 when not due, or a negative stream_packet_status_t.
 */
int stream_packet_create_identifier_ext(stream_parameter_set_t *parameter_set,
                                        uint8_t *buffer,
                                        size_t buffer_size,
                                        uint32_t timestamp,
                                        uint32_t header_transmission_interval,
                                        uint16_t *cursor);

/** @brief Per-item callback for stream_packet_parse_identifier_ext.
    `name` points into the supplied buffer and is NOT NUL-terminated; use name_len. */
typedef void (*stream_identifier_ext_item_cb)(void *ctx,
                                              uint32_t index,
                                              uint16_t size,
                                              uint8_t element_type,
                                              const char *name,
                                              uint8_t name_len,
                                              uint16_t type_id);

/**
 * @brief Parses an extended-identifier chunk, invoking `cb` per complete item.
 *
 * Reports the header fields via the out-pointers (any may be NULL) and invokes
 * `cb` for each fully-present item, tolerating a trailing item truncated by a
 * capped transport (parses complete items only). Returns BADTYPE if the packet
 * is not an extended identifier.
 */
stream_packet_status_t stream_packet_parse_identifier_ext(const uint8_t *buffer,
                                                          size_t buffer_size,
                                                          uint32_t *definition_identifier_out,
                                                          uint16_t *total_count_out,
                                                          uint16_t *start_ordinal_out,
                                                          stream_identifier_ext_item_cb cb,
                                                          void *ctx);

/**
 * @brief Generates one chunk of a TYPE_DESCRIPTOR (0x04) packet from a type set.
 *
 * Self-throttled like the identifier encoders (returns 0 when not due). Packs a
 * contiguous run of CUSTOM-type field layouts starting at *cursor for as many as
 * fit in buffer_size (a type is atomic — never split across chunks), then
 * advances *cursor (wrapping). Pass buffer_size = the smallest downstream
 * transport budget so no chunk truncates mid-type.
 *
 * @return bytes written (>0), 0 when not due, or a negative stream_packet_status_t.
 */
int stream_packet_create_type_descriptor(stream_type_set_t *type_set,
                                         uint8_t *buffer,
                                         size_t buffer_size,
                                         uint32_t timestamp,
                                         uint32_t transmission_interval,
                                         uint16_t *cursor);

/** @brief Callbacks for stream_packet_parse_type_descriptor. `name` pointers
    point into the buffer and are NOT NUL-terminated; use name_len. `type_cb` is
    invoked once per type, immediately followed by `field_cb` for each of its
    fields. */
typedef void (*stream_type_descriptor_type_cb)(void *ctx, uint16_t type_id, uint16_t size,
                                               uint8_t field_count, const char *name, uint8_t name_len);
typedef void (*stream_type_descriptor_field_cb)(void *ctx, uint8_t element_type, uint16_t size,
                                                uint16_t offset, const char *name, uint8_t name_len);

/**
 * @brief Parses a TYPE_DESCRIPTOR chunk, invoking type_cb per type then field_cb
 * per field. Tolerates a trailing type/field truncated by a capped transport.
 */
stream_packet_status_t stream_packet_parse_type_descriptor(const uint8_t *buffer,
                                                           size_t buffer_size,
                                                           uint32_t *definition_identifier_out,
                                                           uint16_t *total_count_out,
                                                           uint16_t *start_ordinal_out,
                                                           stream_type_descriptor_type_cb type_cb,
                                                           stream_type_descriptor_field_cb field_cb,
                                                           void *ctx);

/**
 * @brief Generates a data packet into the provided buffer.
 * @see streaming_packet_create_data in streaming_packet.c for details.
 */
int stream_packet_create_data(stream_parameter_set_t *parameter_set,
                              uint8_t *buffer,
                              size_t buffer_size,
                              uint32_t timestamp,
                              uint32_t data_transmission_interval);
/**
 * @brief Parses a data packet and populates data pointers of a compatible parameter set.
 * @see streaming_packet_parse_data in streaming_packet.c for details.
 */
stream_packet_status_t stream_packet_parse_data(const uint8_t *buffer,
                                                size_t buffer_size,
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
    return (stream_packet_type_t)((stream_packet_header_t *)buffer)->type;
}

#endif // STREAM_STREAM_PACKET_H