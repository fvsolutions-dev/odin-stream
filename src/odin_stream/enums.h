#include <string>
#include <nanobind/nanobind.h>

extern "C" {
#include "odin_stream/stream_packet.h"
#include "odin_stream/stream_parameter_set.h"
}

#ifndef ENUMS_HPP
#define ENUMS_HPP


void init_enums(nanobind::module_& m);
void check_param_set_status(stream_parameter_set_status_t status, const std::string& context = "");
void check_packet_status(int status, const std::string& context = "");


#endif  // ENUMS_HPP