#include "parameter_set.h"
#include "streaming_packet.h"
#include "odin_core.h"

parameter_set_status_t parameter_set_add_parameter(parameter_set_t *set, const ODIN_parameter_t *parameter);
parameter_set_status_t parameter_set_add_parameter_group(parameter_set_t *set, const ODIN_parameter_group_t *group);
