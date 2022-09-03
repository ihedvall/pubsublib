# Copyright 2022 Ingemar Hedvall
# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.20)
project(pubsublib
        VERSION 1.0
        DESCRIPTION "C++ library including publish/subscribe protocols."
        LANGUAGES CXX)

option(BUILD_SHARED_LIBS "Static libraries are preferred" OFF)
option(BUILD_DOC "If doxygen is installed, then build documentation in Release mode" ON)
option(BUILD_TEST "If Google Test is installed, then build the unit tests" ON)
if (NOT COMP_DIR)
    set(COMP_DIR "k:")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_DEBUG_POSTFIX d)

include(CMakePrintHelpers)
include(script/utillib.cmake)
include(script/boost.cmake)
include(script/openssl.cmake)
include(script/expat.cmake)
include(script/googletest.cmake)
include(script/doxygen.cmake)
include(script/mqtt.cmake)
include(script/tahu.cmake)
include(script/protobuf.cmake)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${TAHU_PROTOBUF_DIR}/sparkplug_b.proto)
cmake_print_variables(PROTO_SRCS PROTO_HDRS)

add_library(pubsub STATIC
        src/ipubsubclient.cpp include/pubsub/ipubsubclient.h
        src/itopic.cpp include/pubsub/itopic.h
        src/mqttclient.cpp src/mqttclient.h
        src/mqtttopic.cpp src/mqtttopic.h
        ${PROTO_SRCS} ${PROTO_HDRS}
        ${TAHU_C_SRC_DIR}/pb_common.c ${TAHU_C_INCLUDE_DIR}/pb_common.h
        ${TAHU_C_INCLUDE_DIR}/pb.h
        ${TAHU_C_SRC_DIR}/pb_decode.c ${TAHU_C_INCLUDE_DIR}/pb_decode.h
        ${TAHU_C_SRC_DIR}/pb_encode.c ${TAHU_C_INCLUDE_DIR}/pb_encode.h
        ${TAHU_C_SRC_DIR}/tahu.c ${TAHU_C_INCLUDE_DIR}/tahu.h
        ${TAHU_C_SRC_DIR}/tahu.pb.c ${TAHU_C_INCLUDE_DIR}/tahu.pb.h
        src/imetric.cpp include/pubsub/imetric.h
        src/ipayload.cpp include/pubsub/ipayload.h
        src/payloadhelper.cpp src/payloadhelper.h
        src/pubsubfactory.cpp include/pubsub/pubsubfactory.h src/sparkplugnode.cpp src/sparkplugnode.h)

get_target_property(PAHO_C_INCLUDE_DIRS eclipse-paho-mqtt-c::paho-mqtt3as-static INTERFACE_INCLUDE_DIRECTORIES)

target_include_directories(pubsub PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        )
		
target_include_directories(pubsub PRIVATE ${utillib_SOURCE_DIR}/include)
target_include_directories(pubsub PRIVATE ${Boost_INCLUDE_DIRS})
target_include_directories(pubsub PRIVATE ${EXPAT_INCLUDE_DIRS})
#target_include_directories(pubsub PRIVATE ${ZLIB_INCLUDE_DIRS})
target_include_directories(pubsub PRIVATE ${OPENSSL_INCLUDE_DIR})
target_include_directories(pubsub PRIVATE ${PAHO_C_INCLUDE_DIRS})
target_include_directories(pubsub PRIVATE ${Protobuf_INCLUDE_DIRS})
target_include_directories(pubsub PRIVATE ${TAHU_C_INCLUDE_DIR})
target_include_directories(pubsub PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

cmake_print_properties(TARGETS pubsub PROPERTIES INCLUDE_DIRECTORIES)

target_compile_definitions(pubsub PRIVATE XML_STATIC)

if (MSVC)
    target_compile_definitions(pubsub PRIVATE _WIN32_WINNT=0x0A00)
endif()



if (GTEST_FOUND AND BUILD_TEST)
    enable_testing()
    add_subdirectory(test)
endif()

if (DOXYGEN_FOUND AND (CMAKE_BUILD_TYPE MATCHES "^[Rr]elease") )
    set(DOXYGEN_RECURSIVE NO)
    set(DOXYGEN_REPEAT_BRIEF NO)
    set(DOXYGEN_PROJECT_NAME "PubSubLib")
    set(DOXYGEN_HTML_EXTRA_STYLESHEET doxygen/utillib.css)
    set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)
    doxygen_add_docs( doc_pubsub ALL
            include/pubsub doxygen
            COMMENT "Publish/Subscribe Library generating API documentation with Doxygen")
endif()
