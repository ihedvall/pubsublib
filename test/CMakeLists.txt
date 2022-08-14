# Copyright 2022 Ingemar Hedvall
# SPDX-License-Identifier: MIT

project(TestPubSub
        VERSION 1.0
        DESCRIPTION "Google unit tests for the pubsub library"
        LANGUAGES CXX)

add_executable(test_pubsub
       test_mqtt.cpp)

target_include_directories(test_pubsub PRIVATE ../include)
target_include_directories(test_pubsub PRIVATE ../src)
target_include_directories(test_pubsub PRIVATE ${GTEST_INCLUDE_DIRS})
target_include_directories(test_pubsub PRIVATE ${Boost_INCLUDE_DIRS})



target_link_libraries(test_pubsub PRIVATE util)
target_link_libraries(test_pubsub PRIVATE pubsub)
target_link_libraries(test_pubsub PRIVATE ${Boost_LIBRARIES})
target_link_libraries(test_pubsub PRIVATE ${EXPAT_LIBRARIES})
target_link_libraries(test_pubsub PRIVATE ${ZLIB_LIBRARIES})
target_link_libraries(test_pubsub PRIVATE ${GTEST_BOTH_LIBRARIES})
target_link_libraries(test_pubsub PRIVATE ${OPENSSL_LIBRARIES})
target_link_libraries(test_pubsub PRIVATE eclipse-paho-mqtt-c::paho-mqtt3as-static)

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

