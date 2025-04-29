#ifndef INFORMATION_H
#define INFORMATION_H

#include <stdint.h>

typedef struct {
	uint32_t identifier_decoding_errors;  // Number of identifier decoding errors
	uint32_t data_decoding_errors;        // Number of data decoding errors
	uint32_t event_decoding_errors;       // Number of event decoding errors
	uint32_t other_errors;                // Number of other errors

	uint32_t received_events_packets;           // Total valid events received
	uint32_t received_identifier_packets;       // Total valid identifiers received
	uint32_t received_data_packets;             // Total data packets received
	uint32_t received_unresolved_data_packets;  // Total unresolved packets received
	uint32_t received_other_packets;            // Total other packets received

} odin_stream_decoder_statistics_t;

typedef struct {
} parameter_set_statistics_t;

#endif  // INFORMATION_H