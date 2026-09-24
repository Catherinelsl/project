cmake_minimum_required(VERSION 3.16)
project(test_assignment LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#download paho mqtt cpp library
#download and configure the Paho MQTT C++ library
if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
        message(FATAL_ERROR "Please set the CMAKE_BUILD_TYPE variable to one of the following: Debug, Release, RelWithDebInfo, MinSizeRel")
endif()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}")
execute_process(COMMAND conan install ${CMAKE_SOURCE_DIR}/conanfile.txt --output-folder=${CMAKE_BINARY_DIR} -s build_type=${CMAKE_BUILD_TYPE}  --build=missing)
include(${CMAKE_BINARY_DIR}/conan_toolchain.cmake)

if ("${CMAKE_GENERATOR}" STREQUAL "Ninja")
set(CMAKE_GENERATOR_PLATFORM "" CACHE STRING "" FORCE)
set(CMAKE_GENERATOR_TOOLSET "" CACHE STRING "" FORCE)
endif()

find_package(PahoMqttCpp REQUIRED)
find_package(eclipse-paho-mqtt-c REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenSSL REQUIRED)


#set output folder 
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)

add_executable(service
        src/service.cpp
        src/json_parser.cpp
        src/mqtt_client.cpp
        src/openssl_signer.cpp
        src/update_service.cpp
        src/application_paths.cpp
        src/key_loader.cpp
)
target_link_libraries(service PRIVATE
        PahoMqttCpp::paho-mqttpp3-static
        eclipse-paho-mqtt-c::paho-mqtt3as-static
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
)

# copy keyPair folder into the build directory
add_custom_command(TARGET service POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/keyPair
                ${CMAKE_BINARY_DIR}/bin/keyPair
        COMMENT "Copying keyPair folder to the build directory"
)

target_compile_features(service PRIVATE cxx_std_17)