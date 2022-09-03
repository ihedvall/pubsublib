# Copyright 2022 Ingemar Hedvall
# SPDX-License-Identifier: MIT

include(CMakePrintHelpers)
include (FetchContent)

FetchContent_Declare(tahu
        GIT_REPOSITORY https://github.com/eclipse/tahu.git
        GIT_TAG HEAD)
FetchContent_MakeAvailable(tahu)
set( TAHU_PROTOBUF_DIR ${tahu_SOURCE_DIR}/sparkplug_b)
set( TAHU_C_INCLUDE_DIR ${tahu_SOURCE_DIR}/c/core/include)
set( TAHU_C_SRC_DIR ${tahu_SOURCE_DIR}/c/core/src)
cmake_print_variables(tahu_POPULATED tahu_SOURCE_DIR tahu_BINARY_DIR
        TAHU_PROTOBUF_DIR TAHU_C_INCLUDE_DIR TAHU_C_SRC_DIR)

