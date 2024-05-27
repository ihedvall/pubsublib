# Copyright 2021 Ingemar Hedvall
# SPDX-License-Identifier: MIT
set(EXPAT_USE_STATIC_LIBS ON)
if (NOT EXPAT_FOUND)
    find_package(EXPAT)
    message(STATUS "EXPAT Found (Try 1): "  ${EXPAT_FOUND})
    message(STATUS "EXPAT Include Dirs: "  ${EXPAT_INCLUDE_DIRS})
    message(STATUS "EXPAT Libraries: " ${EXPAT_LIBRARIES})
    if (NOT EXPAT_FOUND)
        if (NOT EXPAT_ROOT)
            set(EXPAT_ROOT ${COMP_DIR}/expat/master)
        endif()
        find_package(EXPAT REQUIRED)
     endif()
endif()

message(STATUS "EXPAT Found: "  ${EXPAT_FOUND})
message(STATUS "EXPAT Include Dirs: "  ${EXPAT_INCLUDE_DIRS})
message(STATUS "EXPAT Libraries: " ${EXPAT_LIBRARIES})