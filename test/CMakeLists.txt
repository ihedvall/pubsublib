# Copyright 2022 Ingemar Hedvall
# SPDX-License-Identifier: MIT

project(TestPubSub
        VERSION 1.0
        DESCRIPTION "Google unit tests for the pubsub library"
        LANGUAGES CXX C)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDebugDLL)
add_executable(test_pubsub
       test_mqtt.cpp test_mqtt.h
       test_payload.cpp
        )

target_include_directories(test_pubsub PRIVATE ../include)
target_include_directories(test_pubsub PRIVATE ../src)
target_include_directories(test_pubsub PRIVATE ${GTEST_INCLUDE_DIRS})
target_include_directories(test_pubsub PRIVATE ${Boost_INCLUDE_DIRS})
target_include_directories(test_pubsub PRIVATE ${Protobuf_INCLUDE_DIRS})
target_include_directories(test_pubsub PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../proto)

cmake_print_properties(TARGETS test_pubsub PROPERTIES INCLUDE_DIRECTORIES)
target_link_libraries(test_pubsub PRIVATE util)
target_link_libraries(test_pubsub PRIVATE pubsub)
target_link_libraries(test_pubsub PRIVATE ${Boost_LIBRARIES})
target_link_libraries(test_pubsub PRIVATE EXPAT::EXPAT)
#target_link_libraries(test_pubsub PRIVATE ${ZLIB_LIBRARIES})
target_link_libraries(test_pubsub PRIVATE GTest::gtest GTest::gtest_main)
#target_link_libraries(test_pubsub PRIVATE ${OPENSSL_LIBRARIES})
target_link_libraries(test_pubsub PRIVATE eclipse-paho-mqtt-c::paho-mqtt3as-static)
target_link_libraries(test_pubsub PRIVATE protobuf::libprotobuf)
#target_link_libraries(test_pubsub PRIVATE protobuf::libprotoc)
#target_link_libraries(test_pubsub PRIVATE protobuf::libprotobuf-lite)
target_link_libraries(test_pubsub PRIVATE
#        absl::algorithm
#        absl::base
#        absl::cleanup
#        absl::debugging
#        absl::flat_hash_map
        absl::flags
        absl::log
        absl::log_internal_check_op
#        absl::memory
#        absl::meta
#        absl::numeric
        absl::status
        absl::statusor
#        absl::strings
#        absl::synchronization
#        absl::time
#        absl::utility
#        utf8_range::utf8_range
        utf8_range::utf8_validity
)

if (WIN32)
    target_link_libraries(test_pubsub PRIVATE ws2_32)
    target_link_libraries(test_pubsub PRIVATE mswsock)
    target_link_libraries(test_pubsub PRIVATE bcrypt)
endif()

if (MINGW)
    target_link_options(test_pubsub PRIVATE -static -fstack-protector )
elseif (MSVC)
    target_compile_options(test_pubsub PRIVATE -D_WIN32_WINNT=0x0A00)
endif()


include(GoogleTest)
gtest_discover_tests(test_pubsub)

