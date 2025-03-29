#include "odin_stream.h"

parameter_set_status_t parameter_set_add_parameter(parameter_set_t *set, const ODIN_parameter_t *parameter)
{
    return parameter_set_add(set,
                             (fixed_size_parameter_t) { .index = parameter->global_index,
                                                        .size  = ODIN_get_max_data_size(parameter),
                                                        .data  = parameter->data });
}

parameter_set_status_t parameter_set_add_parameter_group(parameter_set_t *set, const ODIN_parameter_group_t *group)
{
    for (int i = 0; i < group->count; i++)
    {
        parameter_set_status_t ret = parameter_set_add_parameter(set, group->parameters[i]);
        if (ret != PARAM_SET_SUCCESS)
        {
            return ret;
        }
    }
    return PARAM_SET_SUCCESS;
}
