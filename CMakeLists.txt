cmake_minimum_required(VERSION 3.15...3.26)

# If ESP-IDF is reading this CMakeLists.txt, register it as an ESP-IDF component
if(DEFINED ENV{IDF_PATH})
  idf_component_register(SRCS 
    "src/odin_stream/stream_packet.c"
    "src/odin_stream/stream_parameter_set.c"
    "src/odin_stream.c"
    INCLUDE_DIRS "src"
    REQUIRES "odin")

  return()
endif()

# If Zephyr is reading this CMakeLists.txt, register it as a Zephyr module
if(DEFINED ENV{ZEPHYR_BASE})
  zephyr_library_named(odin-stream)
  zephyr_library_sources(
    "src/odin_stream/stream_packet.c"
    "src/odin_stream/stream_parameter_set.c"
    "src/odin_stream.c"
  )

  target_include_directories(odin-stream PUBLIC src)
  return()
endif()

project(i_think_this_name_does_not_matter LANGUAGES C CXX)

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

option(COVERAGE "Enable coverage reporting" ON)

if(COVERAGE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -fprofile-arcs -ftest-coverage")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -fprofile-arcs -ftest-coverage")
endif()

execute_process(
  COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
  OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE NB_DIR)
list(APPEND CMAKE_PREFIX_PATH "${NB_DIR}")

# Import nanobind through CMake's find_package mechanism
find_package(nanobind CONFIG REQUIRED)

# We are now ready to compile the actual extension module
nanobind_add_module(

  # Name of the extension
  odin_stream

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

  # Source code goes here
  src/odin_stream/stream_packet.c
  src/odin_stream/stream_parameter_set.c
  src/python_bindings.cpp
)

nanobind_add_stub(

  odin_stream_stub
  INSTALL_TIME
  MODULE odin_stream
  OUTPUT odin_stream.pyi
  PYTHON_PATH $<TARGET_FILE_DIR:odin_stream>
  DEPENDS odin_stream
)

target_include_directories(odin_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Install directive for scikit-build-core
install(TARGETS odin_stream LIBRARY DESTINATION ".")
