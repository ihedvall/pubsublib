# Copyright 2022 Ingemar Hedvall
# SPDX-License-Identifier: MIT

set(Protobuf_USE_STATIC_LIBS ON)
set(Protobuf_IMPORT_DIRS "L:/protobuf/src/google/protobuf") # ToDo: Needs to be solved later

if (NOT absl_FOUND)
    find_package(absl CONFIG)
    message(STATUS "absl Found (Try 1): " ${absl_FOUND})
    if (NOT absl_FOUND AND NOT absl_ROOT)
        set( absl_ROOT ${COMP_DIR}/protobuf/latest)
        find_package(absl)
    endif()
endif()

cmake_print_properties(TARGETS absl::base PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES
            INTERFACE_LINK_LIBRARIES
            LOCATION)
cmake_print_properties(TARGETS absl::log PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES
            INTERFACE_LINK_LIBRARIES
            LOCATION)

if (NOT utf8_range_FOUND)
    find_package(utf8_range CONFIG)
    message(STATUS "utf8_range Found (Try 1): " ${utf8_range_FOUND})
    if (NOT utf8_range_FOUND AND NOT utf8_range_ROOT)
        set( utf8_range_ROOT ${COMP_DIR}/protobuf/latest)
        find_package(utf8_range)
    endif()

endif()

cmake_print_properties(TARGETS utf8_range::utf8_range PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES
        INTERFACE_LINK_LIBRARIES
        LOCATION)

if (NOT Protobuf_FOUND)

    set(Protobuf_DEBUG OFF)
    find_package(Protobuf)
    message(STATUS "Protobuf Found (Try 1): " ${Protobuf_FOUND})

    if (NOT Protobuf_FOUND AND NOT Protobuf_ROOT)
        set(Protobuf_ROOT ${COMP_DIR}/protobuf/latest)
        find_package(Protobuf REQUIRED)
    endif()
endif()

cmake_print_variables(Protobuf_VERSION Protobuf_INCLUDE_DIRS Protobuf_LIBRARIES)
cmake_print_variables(Protobuf_PROTOC_LIBRARIES Protobuf_LITE_LIBRARIES Protobuf_LIBRARY)
cmake_print_variables(Protobuf_PROTOC_LIBRARY Protobuf_LITE_LIBRARY Protobuf_PROTOC_EXECUTABLE)
