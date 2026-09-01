cmake_minimum_required(VERSION 3.15...3.26)

# If ESP-IDF is reading this CMakeLists.txt, register it as an ESP-IDF component.
if(ESP_PLATFORM)
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

# Recent pyarrow headers (>=18) lean on C++20 (std::span, std::bit_width,
# std::popcount, std::has_single_bit), so the consumer of pyarrow's headers
# has to advertise C++20 too. Without this the build fails with
# "‘span’ in namespace ‘std’ does not name a template type".
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

target_include_directories(odin_stream_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(odin_stream_cpp PRIVATE nanobind_pyarrow::nanobind_pyarrow nanobind_pyarrow::pyarrow)

# Type stub.
#
# Deliberately NOT nanobind_add_stub(): its INSTALL_TIME mode runs stubgen from an
# execute_process() with no result check, so any failure is swallowed and the wheel
# silently ships without a .pyi. It also imports the extension in a bare interpreter,
# which cannot find pyarrow's Arrow C++ libraries on Windows (no RPATH there). Both
# problems go away by generating at build time through tools/gen_stub.py, which
# preloads pyarrow first -- see the comment in that file.
set(ODIN_STREAM_STUB "${CMAKE_CURRENT_BINARY_DIR}/odin_stream_cpp.pyi")
add_custom_command(
  OUTPUT "${ODIN_STREAM_STUB}"
  COMMAND "${Python_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_stub.py"
          -q
          -i "$<TARGET_FILE_DIR:odin_stream_cpp>"
          -m odin_stream_cpp
          -o "${ODIN_STREAM_STUB}"
  DEPENDS odin_stream_cpp "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_stub.py"
  COMMENT "Generating odin_stream_cpp.pyi"
  VERBATIM)
add_custom_target(odin_stream_cpp_stub ALL DEPENDS "${ODIN_STREAM_STUB}")
install(FILES "${ODIN_STREAM_STUB}" DESTINATION .)

# Add O3
# target_compile_options(odin_stream_cpp PRIVATE -O3)
# Install directive for scikit-build-core
# install(TARGETS odin_stream_cpp LIBRARY DESTINATION ".")


# We deliberately do NOT vendor Arrow C++ into the wheel.
#
# pyarrow is a hard runtime dependency, so its bundled libarrow / arrow.dll is always
# present, and we link against exactly those. Copying a second set in would be worse
# than useless: auditwheel and delvewheel both rename what they graft (libarrow-<hash>.so,
# arrow-<hash>.dll), so the loader can no longer match it against the copy pyarrow has
# already mapped, and the process ends up with two Arrow C++ runtimes -- two memory
# pools, two type registries -- passing shared_ptr<arrow::Table> between them. Keeping
# the linked names unmangled means the loader binds to pyarrow's single copy instead.
# The flip side is that the wheel is tied to one Arrow soname, which is why pyproject.toml
# pins pyarrow to the major version this was built against.
#
# The rpath below is what lets the extension resolve pyarrow's libraries on its own,
# without something having imported pyarrow first: odin_stream_cpp installs to the
# site-packages root, so pyarrow sits right next to it. Windows has no rpath equivalent
# -- there, src/odin_stream/__init__.py calls os.add_dll_directory instead.
set_property(
  TARGET odin_stream_cpp
  APPEND
  PROPERTY INSTALL_RPATH "$<IF:$<PLATFORM_ID:Darwin>,@loader_path,$ORIGIN>/pyarrow")


install(TARGETS odin_stream_cpp LIBRARY DESTINATION .)
