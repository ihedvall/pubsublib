# Copyright 2022 Ingemar Hedvall
# SPDX-License-Identifier: MIT
if (NOT absl_FOUND)
    set( absl_ROOT "k:/grpc/master")
    find_package(absl CONFIG)
    message(STATUS "absl Found (Try 1): " ${absl_FOUND})
    cmake_print_properties(TARGETS absl::base PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES
            INTERFACE_LINK_LIBRARIES
            LOCATION)
    cmake_print_properties(TARGETS absl::log PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES
            INTERFACE_LINK_LIBRARIES
            LOCATION)

endif()

if (NOT utf8_range_FOUND)
    set( utf8_range_ROOT "k:/grpc/master")
    find_package(utf8_range CONFIG)
    message(STATUS "utf8_range Found (Try 1): " ${utf8_range_FOUND})
    cmake_print_properties(TARGETS utf8_range::utf8_range PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES
            INTERFACE_LINK_LIBRARIES
            LOCATION)
endif()

if (NOT Protobuf_FOUND)
    set(Protobuf_USE_STATIC_LIBS ON)
    set(Protobuf_DEBUG OFF)
    find_package(Protobuf)
    message(STATUS "Protobuf Found (Try 1): " ${Protobuf_FOUND})

    if (NOT Protobuf_FOUND)
        set(Protobuf_ROOT ${COMP_DIR}/protobuf/master)
        find_package(Protobuf REQUIRED)
        message(STATUS "Protobuf Found (Try 2): " ${Protobuf_FOUND})
    endif()
endif()

if (Protobuf_FOUND)
    cmake_print_variables(Protobuf_VERSION Protobuf_INCLUDE_DIRS Protobuf_LIBRARIES)
    cmake_print_variables(Protobuf_PROTOC_LIBRARIES Protobuf_LITE_LIBRARIES Protobuf_LIBRARY)
    cmake_print_variables(Protobuf_PROTOC_LIBRARY Protobuf_LITE_LIBRARY Protobuf_PROTOC_EXECUTABLE)
endif()