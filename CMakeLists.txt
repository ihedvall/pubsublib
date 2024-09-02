# Copyright 2022 Ingemar Hedvall
# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.10)
project(pubsublib
        VERSION 1.0
        DESCRIPTION "C++ library including publish/subscribe protocols."
        LANGUAGES CXX C )
cmake_policy(SET CMP0074 NEW)
cmake_policy(SET CMP0144 NEW)

option(BUILD_SHARED_LIBS "Static libraries are preferred" OFF)
option(PUB_BUILD_DOC "If doxygen is installed, then build documentation in Release mode" OFF)
option(PUB_BUILD_TEST "If Google Test is installed, then build the unit tests" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_DEBUG_POSTFIX d)

include(CMakePrintHelpers)
include(script/utillib.cmake)
include(script/boost.cmake)
include(script/openssl.cmake)
include(script/expat.cmake)

include(script/doxygen.cmake)
include(script/mqtt.cmake)

include(script/protobuf.cmake)
# include(script/tahu.cmake)




add_library(pubsub STATIC
        src/ipubsubclient.cpp include/pubsub/ipubsubclient.h
        src/itopic.cpp include/pubsub/itopic.h
        src/mqttclient.cpp src/mqttclient.h
        src/mqtttopic.cpp src/mqtttopic.h
        proto/sparkplug_b_c_sharp.proto
        src/ivalue.cpp include/pubsub/ivalue.h
        src/ipayload.cpp include/pubsub/ipayload.h
        src/payloadhelper.cpp src/payloadhelper.h
        src/pubsubfactory.cpp include/pubsub/pubsubfactory.h
        src/sparkplugnode.cpp src/sparkplugnode.h
        src/inode.cpp
        include/pubsub/inode.h
        src/detectbroker.cpp
        src/detectbroker.h
        src/sparkplughost.cpp
        src/sparkplughost.h
        src/sparkplughelper.cpp
        src/sparkplughelper.h
        src/sparkplugtopic.cpp
        src/sparkplugtopic.h)

protobuf_generate(
        TARGET pubsub
        OUT_VAR PROTO_GENERATED_FILES
        LANGUAGE cpp
        IMPORT_DIRS ${CMAKE_CURRENT_DIR}/proto
        #PROTOS proto/sparkplug_b_c_sharp.proto
        PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

cmake_print_variables(CMAKE_CURRENT_BINARY_DIR PROTO_GENERATED_FILES)

get_target_property(PAHO_C_INCLUDE_DIRS eclipse-paho-mqtt-c::paho-mqtt3as-static INTERFACE_INCLUDE_DIRECTORIES)

target_include_directories(pubsub PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        )
target_include_directories(pubsub PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/proto)
target_include_directories(pubsub PRIVATE ${utillib_SOURCE_DIR}/include)
target_include_directories(pubsub PRIVATE ${Boost_INCLUDE_DIRS})
target_include_directories(pubsub PRIVATE ${EXPAT_INCLUDE_DIRS})
#target_include_directories(pubsub PRIVATE ${ZLIB_INCLUDE_DIRS})
target_include_directories(pubsub PRIVATE ${OPENSSL_INCLUDE_DIR})
target_include_directories(pubsub PRIVATE ${PAHO_C_INCLUDE_DIRS})
target_include_directories(pubsub PRIVATE ${Protobuf_INCLUDE_DIRS})
#target_include_directories(pubsub PRIVATE ${TAHU_C_INCLUDE_DIR})
target_include_directories(pubsub PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

cmake_print_properties(TARGETS pubsub PROPERTIES INCLUDE_DIRECTORIES)

target_compile_definitions(pubsub PRIVATE XML_STATIC)

if (MSVC)
    target_compile_definitions(pubsub PRIVATE _WIN32_WINNT=0x0A00)
endif()



if (PUB_BUILD_TEST)
    include(script/googletest.cmake)
    enable_testing()
    add_subdirectory(test)
endif()

if (PUB_BUILD_DOC AND DOXYGEN_FOUND AND (CMAKE_BUILD_TYPE MATCHES "^[Rr]elease") )
    set(DOXYGEN_RECURSIVE NO)
    set(DOXYGEN_REPEAT_BRIEF NO)
    set(DOXYGEN_PROJECT_NAME "PubSubLib")
    set(DOXYGEN_HTML_EXTRA_STYLESHEET doxygen/utillib.css)
    set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/docs/manual)
    doxygen_add_docs( doc_pubsub ALL
            include/pubsub doxygen
            COMMENT "Publish/Subscribe Library generating API documentation with Doxygen")
endif()
