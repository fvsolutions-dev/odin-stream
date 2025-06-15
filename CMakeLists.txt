cmake_minimum_required(VERSION 3.15...3.26)

# If ESP-IDF is reading this CMakeLists.txt, register it as an ESP-IDF component
if(DEFINED ENV{IDF_PATH})
  idf_component_register(SRCS 
    "src/embedded_odin_stream/stream_packet.c"
    "src/embedded_odin_stream/stream_parameter_set.c"
    "src/embedded_odin_stream/odin_stream.c"
    INCLUDE_DIRS "src"
    REQUIRES "odin")

  return()
endif()

# If Zephyr is reading this CMakeLists.txt, register it as a Zephyr module
if(Zephyr_FOUND)
  zephyr_library_named(odin-stream)
  zephyr_library_sources(
    "src/embedded_odin_stream/stream_packet.c"
    "src/embedded_odin_stream/stream_parameter_set.c"
    "src/embedded_odin_stream/odin_stream.c"
  )

  target_include_directories(odin-stream PUBLIC src)
  target_link_libraries(odin-stream PRIVATE odin)

  return()
endif()

project(i_think_this_name_does_not_matter LANGUAGES C CXX)

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL CACHE STRING "" FORCE)
  add_compile_options(/EHsc)             # ensure exceptions are enabled
endif()

# create compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT SKBUILD)
  message(WARNING "\
  This CMake file is meant to be executed using 'scikit-build'. Running
  it directly will almost certainly not produce the desired result. If
  you are a user trying to install this package, please use the command
  below, which will install all necessary build dependencies, compile
  the package in an isolated environment, and then install it.
  =====================================================================
   $ pip install .
  =====================================================================
  If you are a software developer, and this is your own package, then
  it is usually much more efficient to install the build dependencies
  in your environment once and use the following command that avoids
  a costly creation of a new virtual environment at every compilation:
  =====================================================================
   $ pip install nanobind scikit-build-core[pyproject]
   $ pip install --no-build-isolation -ve .
  =====================================================================
  You may optionally add -Ceditable.rebuild=true to auto-rebuild when
  the package is imported. Otherwise, you need to re-run the above
  after editing C++ files.")
endif()

# Try to import all Python components potentially needed by nanobind
find_package(Python 3.8
  REQUIRED COMPONENTS Interpreter Development.Module
  OPTIONAL_COMPONENTS Development.SABIModule)


if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

execute_process(
  COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
  OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE NB_DIR)
list(APPEND CMAKE_PREFIX_PATH "${NB_DIR}")

# Import nanobind through CMake's find_package mechanism
find_package(nanobind CONFIG REQUIRED)



include(FetchContent)
FetchContent_Declare(
  nanobind_pyarrow
  GIT_REPOSITORY https://github.com/maximiliank/nanobind_pyarrow.git
  GIT_TAG origin/main
  UPDATE_DISCONNECTED ON)

FetchContent_MakeAvailable(nanobind_pyarrow)


if(NOT TARGET nanobind_pyarrow::pyarrow)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${nanobind_pyarrow_SOURCE_DIR}/cmake")
find_package(PyArrow REQUIRED)


if (WIN32)
  # PyArrow’s CMake config sets only IMPORTED_LOCATION, not IMPORTED_IMPLIB,
  # so point CMake at the .lib import libraries it needs on Windows:

  # arrow::arrow
  get_target_property(_arrow_loc arrow::arrow IMPORTED_LOCATION)
  get_filename_component(_arrow_dir ${_arrow_loc} DIRECTORY)
  set_target_properties(arrow::arrow PROPERTIES
    IMPORTED_IMPLIB "${_arrow_dir}/arrow.lib"
  )

  # pyarrow::pyarrow
  get_target_property(_pa_loc pyarrow::pyarrow IMPORTED_LOCATION)
  get_filename_component(_pa_dir ${_pa_loc} DIRECTORY)
  set_target_properties(pyarrow::pyarrow PROPERTIES
    IMPORTED_IMPLIB "${_pa_dir}/arrow_python.lib"
  )

endif()

endif()

# We are now ready to compile the actual extension module
nanobind_add_module(

  # Name of the extension
  odin_stream_cpp

  # domain name
  NB_DOMAIN burst_interface_domain

  # Target the stable ABI for Python 3.12+, which reduces
  # the number of binary wheels that must be built. This
  # does nothing on older Python versions
  STABLE_ABI

  # Build libnanobind statically and merge it into the
  # extension (which itself remains a shared library)
  #
  # If your project builds multiple extensions, you can
  # replace this flag by NB_SHARED to conserve space by
  # reusing a shared libnanobind across libraries
  NB_STATIC
  src/odin_stream_cpp/python_bindings.cpp


  # Source code goes here
  src/embedded_odin_stream/stream_packet.c
  src/embedded_odin_stream/stream_parameter_set.c
  
  src/odin_stream_cpp/fixed_size_parameter.cpp
  src/odin_stream_cpp/parameterset.cpp
  src/odin_stream_cpp/stream_processor.cpp
  src/odin_stream_cpp/builder.cpp
  src/odin_stream_cpp/descriptor/parameter_descriptor.cpp
  src/odin_stream_cpp/descriptor/type_descriptor.cpp
  )

nanobind_add_stub(
  odin_stream_cpp_stub
  INSTALL_TIME
  MODULE odin_stream_cpp
  OUTPUT odin_stream_cpp.pyi
  PYTHON_PATH $<TARGET_FILE_DIR:odin_stream_cpp>
  DEPENDS odin_stream_cpp 
)

target_include_directories(odin_stream_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(odin_stream_cpp PRIVATE nanobind_pyarrow::nanobind_pyarrow nanobind_pyarrow::pyarrow)

# Add O3
# target_compile_options(odin_stream_cpp PRIVATE -O3)
# Install directive for scikit-build-core
# install(TARGETS odin_stream_cpp LIBRARY DESTINATION ".")


set_property(
  TARGET odin_stream_cpp
  APPEND
  PROPERTY INSTALL_RPATH "$ORIGIN/lib")
  
install(CODE [[
  file(GET_RUNTIME_DEPENDENCIES
    LIBRARIES $<TARGET_FILE:odin_stream_cpp>
    RESOLVED_DEPENDENCIES_VAR _r_deps
    UNRESOLVED_DEPENDENCIES_VAR _u_deps
  )
  foreach(_file ${_r_deps})
    if(_file MATCHES ".*lib(arrow|parquet).*\\.so")
      file(INSTALL
        DESTINATION "${CMAKE_INSTALL_PREFIX}/lib"
        TYPE SHARED_LIBRARY
        FOLLOW_SYMLINK_CHAIN
        FILES "${_file}"
      )
    endif()
  endforeach()
  list(LENGTH _u_deps _u_length)
  if("${_u_length}" GREATER 0)
    message(WARNING "Unresolved dependencies detected!")
  endif()
]])


install(TARGETS odin_stream_cpp LIBRARY DESTINATION .)
