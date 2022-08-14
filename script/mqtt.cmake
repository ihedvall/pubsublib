# Copyright 2022 Ingemar Hedvall
# SPDX-License-Identifier: MIT
include(CMakePrintHelpers)

if (NOT eclipse-paho-mqtt-c_ROOT)
    set(eclipse-paho-mqtt-c_ROOT ${COMP_DIR}/PahoMqttC/master)
#    list(APPEND CMAKE_PREFIX_PATH ${eclipse-paho-mqtt-c_ROOT}/lib/cmake/eclipse-paho-mqtt-c)
endif ()

find_package(eclipse-paho-mqtt-c)

#if (NOT PahoMqttCpp_ROOT)
#    set(PahoMqttCpp_ROOT ${COMP_DIR}/PahoMqttCpp/master)
#    list(APPEND CMAKE_PREFIX_PATH ${PahoMqttCpp_ROOT}/lib/cmake/PahoMqttCpp)
#endif ()
cmake_print_variables(eclipse-paho-mqtt-c_FOUND
        eclipse-paho-mqtt-c_VERSION
        paho-mqtt3as-static
        PAHO_MQTT_C_INCLUDE_DIRS PAHO_MQTT_C_LIBRARIES)

#find_package(PahoMqttCpp)
#cmake_print_variables(PahoMqttCpp_FOUND PahoMqttCpp_VERSION3
#        eclipse-paho-mqtt-c_FOUND
#        eclipse-paho-mqtt-c_VERSION
#        PAHO_MQTT_C_INCLUDE_DIRS PAHO_MQTT_C_LIBRARIES
#        PAHO_MQTT_CPP_INCLUDE_DIRS PAHO_MQTT_CPP_LIBRARIES)
#cmake_print_properties(TARGETS eclipse-paho-mqtt-c::paho-mqtt3c-static
#                               eclipse-paho-mqtt-c::paho-mqtt3a-static
#                               eclipse-paho-mqtt-c::paho-mqtt3cs-static
#                               eclipse-paho-mqtt-c::paho-mqtt3as-static
#        PROPERTIES INTERFACE_INCLUDE_DIRECTORIES INTERFACE_LIBRARIES INTERFACE_LINK_LIBRARIES )
