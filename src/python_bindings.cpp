#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h> // Required for string conversions (repr, setters)
#include <nanobind/stl/vector.h> // Required for get_parameter_indices
#include <nanobind/stl/list.h>   // For nb::list/iterable
// #include <nanobind/cast.h>       // Not needed, nb::cast is included by nanobind.h
#include <stdexcept> // For exceptions
#include <string>    // For std::string, std::to_string
#include <vector>
#include <cstdint> // For uint types
#include <cstdlib> // For free() - needed due to missing C free function
#include <sstream> // For building string in __repr__
#include <iomanip> // For std::setw, std::setfill

// Assuming these C structs/functions are declared in this header
extern "C"
{
#include "streaming_interface.h" // Defines fixed_size_parameter_t, streaming_parameterset_t
#include "streaming_payload.h"   // Declares the streaming_payload_* functions
}

namespace nb = nanobind;
using namespace nb::literals;

// --- Modifiable FixedSizeParameter wrapper (Updated Constructor) ---
struct FixedSizeParameter
{
    nb::bytes              data_buffer; // Keep the Python bytes object alive
    fixed_size_parameter_t parameter;   // The underlying C struct

    // Constructor (Updated: derives size from data)
    FixedSizeParameter(uint32_t index, nb::bytes data)
        : data_buffer(std::move(data)) // Store the bytes object
    {
        parameter.index = index;
        parameter.size  = data_buffer.size(); // Set size from the bytes object length
        parameter.data
            = (uint8_t *)data_buffer
                  .c_str(); // Point to the stored buffer
                            // Optional: Add printf for debugging if needed
                            // printf("FixedSizeParameter created: index=%u, size=%u, data_ptr=%p, this=%p\n",
                            //       parameter.index, parameter.size, parameter.data, this);
    }

    // Prevent copying/moving for simplicity if managed primarily by Python's GC
    FixedSizeParameter(const FixedSizeParameter &)            = delete;
    FixedSizeParameter &operator=(const FixedSizeParameter &) = delete;
    FixedSizeParameter(FixedSizeParameter &&)                 = delete;
    FixedSizeParameter &operator=(FixedSizeParameter &&)      = delete;

    // --- Getters ---
    uint32_t get_index() const
    {
        return parameter.index;
    }
    uint32_t get_size() const
    {
        return parameter.size;
    } // Getter for derived size
    nb::bytes get_data_bytes() const
    {
        return data_buffer;
    }

    // --- Setters ---
    void set_index(uint32_t index)
    {
        this->parameter.index = index;
        // (Same warning about hash applies)
    }

    void set_data(nb::bytes data)
    {
        this->data_buffer = std::move(data);
        // Update size and data pointer whenever data is set
        this->parameter.size = this->data_buffer.size();
        this->parameter.data = (uint8_t *)this->data_buffer.c_str();
    }

    // Internal access to the C struct (use with caution)
    const fixed_size_parameter_t &get_c_struct() const
    {
        return parameter;
    }
    fixed_size_parameter_t &get_c_struct()
    {
        return parameter;
    }

    // __repr__ for FixedSizeParameter itself
    std::string repr() const
    {
        std::stringstream ss;
        ss << "FixedSizeParameter(index=" << parameter.index << ", size=" << parameter.size << ", data=b'";
        ss << std::hex << std::setfill('0');
        for (size_t i = 0; i < data_buffer.size(); ++i)
        {
            ss << "\\x" << std::setw(2) << static_cast<int>(data_buffer.c_str()[i]);
        }
        ss << "')";
        return ss.str();
    }
};

// --- ParameterSet Class (Updated with Initialization Checks) ---
class ParameterSet
{
private:
    streaming_parameterset_t *parameterset_ = nullptr;

    // --- Helper to check initialization ---
    /**
     * @brief Throws std::runtime_error if parameterset_ is null.
     * Should be called at the beginning of methods operating on the set.
     */
    void check_initialized() const
    {
        if (!parameterset_)
        {
            throw std::runtime_error("ParameterSet is uninitialized or has been moved.");
        }
    }

public:
    // Constructor
    ParameterSet(int max_parameters)
    {
        parameterset_ = streaming_payload_new(max_parameters);
        if (parameterset_ == nullptr)
        {
            // Constructor failed, throw allocation error
            throw std::bad_alloc();
        }
        // Optional: printf("ParameterSet created: ptr=%p, max=%d\n", parameterset_, max_parameters);
    }

    // Destructor
    ~ParameterSet()
    {
        if (parameterset_)
        {
            // printf("ParameterSet destroying: ptr=%p\n", parameterset_); // Debugging
            // --- DANGER ZONE --- (Same warning as before applies about needing C free func)
            if (parameterset_->parameters)
            {
                free(parameterset_->parameters);
                parameterset_->parameters = nullptr;
            }
            free(parameterset_);
            parameterset_ = nullptr;
            // --- END DANGER ZONE ---
        }
        else
        {
            // printf("ParameterSet destructor called on null ptr\n"); // Debugging
        }
    }

    // --- Rule of 5 (Move semantics, Copy deleted) ---
    ParameterSet(const ParameterSet &)            = delete;
    ParameterSet &operator=(const ParameterSet &) = delete;
    ParameterSet(ParameterSet &&other) noexcept
        : parameterset_(other.parameterset_)
    {
        // printf("ParameterSet move constructing: from %p to %p\n", other.parameterset_, parameterset_); // Debug
        other.parameterset_ = nullptr; // Prevent double free
    }
    ParameterSet &operator=(ParameterSet &&other) noexcept
    {
        // printf("ParameterSet move assigning: target=%p, source=%p\n", parameterset_, other.parameterset_); // Debug
        if (this != &other)
        {
            // Free existing resource first (if any)
            if (parameterset_)
            {
                if (parameterset_->parameters)
                    free(parameterset_->parameters);
                free(parameterset_);
            }
            // Transfer ownership
            parameterset_ = other.parameterset_;
            // Null out the source
            other.parameterset_ = nullptr;
        }
        return *this;
    }
    // --- End Rule of 5 ---

    // --- Wrapper Methods (with checks) ---
    bool add(FixedSizeParameter &parameter)
    {
        check_initialized(); // Throw if not initialized
        // Pass the address of the internal C struct from the wrapper
        int result = streaming_payload_add(parameterset_, &parameter.get_c_struct());
        return result == 0;
    }

    int add_list(nb::iterable params)
    {
        check_initialized(); // Throw if not initialized before starting loop

        int success_count = 0;
        for (nb::handle item_handle : params)
        {
            FixedSizeParameter &param = nb::cast<FixedSizeParameter &>(item_handle);
            // Call the checked single 'add' method
            if (this->add(param))
            {
                success_count++;
            }
            else
            {
                // Add failed (e.g., full, duplicate) - stop or continue?
                // Current behavior: continue adding others.
            }

            // No need for a generic std::exception catch here unless 'add' can throw others
        }
        return success_count;
    }

    bool remove(FixedSizeParameter &parameter)
    {
        check_initialized(); // Throw if not initialized
        // Pass the address of the internal C struct from the wrapper
        int result = streaming_payload_remove(parameterset_, &parameter.get_c_struct());
        return result == 0;
    }

    void clear()
    {
        check_initialized(); // Throw if not initialized
        streaming_payload_clear(parameterset_);
    }

    // --- Accessors (with checks) ---
    int get_count() const
    {
        check_initialized(); // Throw if not initialized
        return parameterset_->parameter_count;
    }

    int get_max_count() const
    {
        check_initialized(); // Throw if not initialized
        return parameterset_->parameter_count_max;
    }

    uint16_t get_hash() const
    {
        check_initialized(); // Throw if not initialized
        return parameterset_->parameter_hash;
    }

    std::vector<uint32_t> get_parameter_indices() const
    {
        check_initialized(); // Throw if not initialized
        std::vector<uint32_t> indices;
        // Reserve space based on current count
        indices.reserve(parameterset_->parameter_count);
        for (int i = 0; i < parameterset_->parameter_count; ++i)
        {
            // Basic safety check for the pointer itself before dereferencing
            if (parameterset_->parameters && parameterset_->parameters[i])
            {
                indices.push_back(parameterset_->parameters[i]->index);
            }
            else
            {
                // This case indicates an internal inconsistency if count > 0
                // but pointers are null. Could throw an internal error here too.
            }
        }
        return indices;
    }
    /**
     * @brief Generates the identifier packet for the current parameter set.
     *
     * @return nb::bytes object containing the generated packet data.
     */
    nb::bytes generate_identifier_packet() const
    {
        check_initialized();

        // Calculate required buffer size
        size_t required_size
            = sizeof(streaming_packet_header_t) + (size_t)parameterset_->parameter_count * sizeof(uint32_t);

        // Allocate buffer
        std::vector<uint8_t> buffer(required_size);

        // Call the C function
        int bytes_written = streaming_interface_generate_identifier_packet(
            parameterset_, buffer.data(), static_cast<int>(buffer.size()));

        // Basic check: C function should return the calculated size
        if (bytes_written < 0 || static_cast<size_t>(bytes_written) != required_size)
        {
            throw std::runtime_error("Identifier packet generation failed or returned unexpected size. Expected="
                                     + std::to_string(required_size) + ", Got=" + std::to_string(bytes_written));
        }

        // Return data as Python bytes
        return nb::bytes(buffer.data(), bytes_written);
    }

    /**
     * @brief Generates the data packet for the current parameter set with a timestamp.
     *
     * @param timestamp The timestamp to include in the data packet header.
     * @return nb::bytes object containing the generated packet data.
     * @throws std::runtime_error if buffer overflow occurs during C function call.
     */
    nb::bytes generate_data_packet(uint32_t timestamp) const
    {
        check_initialized();

        // Calculate required buffer size by summing parameter sizes
        size_t data_payload_size = 0;
        for (int i = 0; i < parameterset_->parameter_count; ++i)
        {
            if (parameterset_->parameters && parameterset_->parameters[i])
            {
                data_payload_size += parameterset_->parameters[i]->size;
            }
            else
            {
                throw std::logic_error("Internal error: Null parameter pointer found in ParameterSet.");
            }
        }
        size_t required_size = sizeof(streaming_data_header_t) + data_payload_size;

        // Allocate buffer
        std::vector<uint8_t> buffer(required_size);

        // Call the C function
        int bytes_written = streaming_interface_generate_data_packet(
            parameterset_, buffer.data(), static_cast<int>(buffer.size()), timestamp);

        // Check for errors reported by the C function
        if (bytes_written < 0)
        {
            // The C function signals buffer overflow with -1
            throw std::runtime_error("Buffer overflow during data packet generation (internal error). Required size: "
                                     + std::to_string(required_size)
                                     + ", Reported error code: " + std::to_string(bytes_written));
        }

        // Optional: Sanity check size. For data packets, it should match required_size.
        if (static_cast<size_t>(bytes_written) != required_size)
        {
            throw std::runtime_error("Data packet generation returned unexpected size. Expected="
                                     + std::to_string(required_size) + ", Got=" + std::to_string(bytes_written));
        }

        // Return data as Python bytes
        // Note: Using bytes_written which should equal required_size if no error occurred
        return nb::bytes(buffer.data(), bytes_written);
    }

    // --- __repr__ Implementation (with check) ---
    std::string repr() const
    {
        // Keep the specific repr for null state, or throw? Let's throw for consistency.
        check_initialized(); // Throw if not initialized

        std::stringstream ss;
        ss << "<ParameterSet count=" << parameterset_->parameter_count << ", max=" << parameterset_->parameter_count_max
           << ", hash=0x" << std::hex << std::setw(4) << std::setfill('0') << parameterset_->parameter_hash << ">";
        return ss.str();
    }
}; // End of ParameterSet class

// --- Nanobind Module Definition ---
NB_MODULE(odin_streaming_interface_c, m)
{ // Use your desired module name

    // --- Bind MODIFIABLE FixedSizeParameter (Updated) ---
    nb::class_<FixedSizeParameter>(m, "FixedSizeParameter")
        // Updated init: only index and data
        .def(nb::init<uint32_t, nb::bytes>(),
             "index"_a,
             "data"_a,
             "Creates a parameter, size is derived from data length.")
        // Read-write properties
        .def_prop_rw(
            "index", &FixedSizeParameter::get_index, &FixedSizeParameter::set_index, "Parameter index (uint32)")
        .def_prop_rw("data",
                     &FixedSizeParameter::get_data_bytes,
                     &FixedSizeParameter::set_data,
                     "Parameter data (bytes), updates size implicitly.")
        // Read-only size property (derived from data)
        .def_prop_ro("size", &FixedSizeParameter::get_size, "Size of parameter data (derived from data length).")
        // Bind the __repr__ method
        .def("__repr__", &FixedSizeParameter::repr);

    // --- Bind the ParameterSet Wrapper ---
    nb::class_<ParameterSet>(m, "ParameterSet")
        .def(nb::init<int>(), "max_parameters"_a, "Creates a new parameter set with a maximum capacity.")
        // Methods
        .def("add",
             &ParameterSet::add,
             "parameter"_a,
             nb::rv_policy::reference_internal,
             "Adds a single parameter. Returns True on success, False if full/duplicate.")
        .def("add_list",
             &ParameterSet::add_list,
             "parameters"_a,
             "Adds parameters from a Python list/iterable. Returns number successfully added.")
        .def("remove",
             &ParameterSet::remove,
             "parameter"_a,
             "Removes a parameter. Returns True on success, False if not found.")
        .def("clear", &ParameterSet::clear, "Removes all parameters from the set.")
        // Read-only Properties (now throw if object was moved)
        .def_prop_ro("count", &ParameterSet::get_count, "Current number of parameters.")
        .def_prop_ro("max_count", &ParameterSet::get_max_count, "Maximum capacity.")
        .def_prop_ro("hash", &ParameterSet::get_hash, "Current parameter index hash.")
        .def_prop_ro(
            "indices", &ParameterSet::get_parameter_indices, "List of indices of parameters currently in the set.")
        // Pythonic length and representation (now throw if object was moved)
        .def("__len__", &ParameterSet::get_count)
        .def("__repr__", &ParameterSet::repr)

        // --- Bind NEW Packet Generation Methods ---
        .def("generate_identifier_packet",
             &ParameterSet::generate_identifier_packet,
             "Generates the identifier packet for the current set as bytes.")
        .def("generate_data_packet",
             &ParameterSet::generate_data_packet,
             "timestamp"_a,
             "Generates the data packet for the current set with a timestamp as bytes.");

} // End of NB_MODULE