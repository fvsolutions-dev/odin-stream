#include "fixed_size_parameter.hpp"
#include <iomanip>
#include <sstream>

namespace nb = nanobind;

FixedSizeParameter::FixedSizeParameter(uint32_t idx, nb::bytes data) : index(idx), data_buffer(std::move(data)) {}

uint32_t FixedSizeParameter::get_index() const { return index; }
void FixedSizeParameter::set_index(uint32_t new_index) { index = new_index; }

nb::bytes FixedSizeParameter::get_data() const { return data_buffer; }
void FixedSizeParameter::set_data(nb::bytes new_data) { data_buffer = std::move(new_data); }

uint32_t FixedSizeParameter::get_size() const { return (uint32_t)data_buffer.size(); }

stream_fixed_size_parameter_t FixedSizeParameter::to_c_struct() const {
    stream_fixed_size_parameter_t c_param;
    c_param.index = this->index;
    c_param.size = this->get_size();
    c_param.data = (uint8_t*)this->data_buffer.c_str();
    
    return c_param;
}

std::string FixedSizeParameter::repr() const {
    std::stringstream ss;
    ss << "FixedSizeParameter(index=" << index << ", size=" << get_size() << ", data=b'";
    ss << std::hex << std::setfill('0');
    const char* buf_data = data_buffer.c_str();
    size_t buf_size = data_buffer.size();
    for (size_t i = 0; i < buf_size; ++i) {
        ss << "\\x" << std::setw(2) << static_cast<int>((unsigned char)buf_data[i]);
    }
    ss << "')";
    return ss.str();
}

void init_fixed_size_parameter(nb::module_& m) {
    nb::class_<FixedSizeParameter>(m, "FixedSizeParameter", "Wrapper for parameter descriptor (index/data)")
        .def(nb::init<uint32_t, nb::bytes>(), "index", "data", "Create a parameter descriptor. Size is derived from data.")
        .def_prop_rw("index", &FixedSizeParameter::get_index, &FixedSizeParameter::set_index, "Parameter index (uint32)")
        .def_prop_rw("data", &FixedSizeParameter::get_data, &FixedSizeParameter::set_data, "Parameter data (bytes), updates size implicitly.")
        .def_prop_ro("size", &FixedSizeParameter::get_size,
                        "Size of parameter data in bytes (derived from data).")
        .def("__repr__", &FixedSizeParameter::repr);
}