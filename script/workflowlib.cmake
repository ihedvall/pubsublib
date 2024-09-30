# Copyright 2024 Ingemar Hedvall
# SPDX-License-Identifier: MIT

include (FetchContent)
include(CMakePrintHelpers)

FetchContent_Declare(workflowlib
        GIT_REPOSITORY https://github.com/ihedvall/workflowlib.git
        GIT_TAG HEAD)
FetchContent_MakeAvailable(workflowlib)

cmake_print_variables( workflowlib_POPULATED workflowlib_SOURCE_DIR workflowlib_BINARY_DIR)
