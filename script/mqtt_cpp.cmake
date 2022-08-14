
include(FetchContent)
include(CMakePrintHelpers)

FetchContent_Declare(mqtt_cpp
        GIT_REPOSITORY https://github.com/redboltz/mqtt_cpp.git
        GIT_TAG HEAD)
SET(CMAKE_CXX_STANDARD 20)
set(MQTT_BUILD_EXAMPLES OFF)
set(MQTT_BUILD_TESTS OFF)
set(MQTT_USE_STATIC_BOOST ON)
set(MQTT_USE_STATIC_OPENSSL ON)
set(MQTT_STD_VARIANT ON)
set(MQTT_USE_WS ON)
set(MQTT_USE_TLS ON)
set(BUILD_SHARED_LIBS OFF)
FetchContent_MakeAvailable(mqtt_cpp)

cmake_print_variables(mqtt_cpp_POPULATED mqtt_cpp_SOURCE_DIR)