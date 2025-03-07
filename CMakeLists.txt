cmake_minimum_required(VERSION 3.30)
set(CORE_VERSION "0.0.1")

project(main CXX)

message(STATUS "Compiler: ${CMAKE_CXX_COMPILER}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
message(STATUS "Mode: ${CMAKE_BUILD_TYPE}")

# ---------------------------------------------------------------------------------

if (DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_ROOT $ENV{VCPKG_ROOT})
    message(STATUS "Using VCPKG_ROOT from environment: ${VCPKG_ROOT}")
    include(${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)

    set(VCPKG_LIBRARY_LINKAGE static)
else ()
    message(FATAL_ERROR "VCPKG_ROOT is not defined in the environment.")
endif ()

# ---------------------------------------------------------------------------------

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -flto=auto -fopenmp")
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS
            "${CMAKE_CXX_FLAGS} -fwhole-program -march=x86-64 -mtune=generic -funroll-loops -fPIC"
    )
endif ()

if (UNIX AND NOT APPLE)
    file(READ "/etc/os-release" OS_RELEASE_CONTENT)
    if (OS_RELEASE_CONTENT MATCHES "ID=debian"
            OR OS_RELEASE_CONTENT MATCHES "ID=ubuntu"
            OR OS_RELEASE_CONTENT MATCHES "ID_LIKE=.*ubuntu.*"
            OR OS_RELEASE_CONTENT MATCHES "ID_LIKE=.*debian.*")
        if (CMAKE_BUILD_TYPE STREQUAL "Release")
            add_definitions(-DSTATIC_LIB_ENABLED)
            set(CMAKE_CXX_FLAGS
                    "${CMAKE_CXX_FLAGS} -static -static-libgcc -static-libstdc++")
            set(BINARY_SUFFIX "")
        endif ()
    else()
        set(BINARY_SUFFIX "-dynamic")
    endif ()
endif ()

add_executable(main src/main.cc)

target_compile_definitions(main PRIVATE CORE_VERSION="${CORE_VERSION}")

target_compile_options(
        main
        PRIVATE -Wall
        -Wextra
        -Wunused-variable
        -Wunused-parameter
        -Wunused-function
        -Wdeprecated-declarations
        -Wconversion
        -Wreturn-type
        -Wformat)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(main PRIVATE -s -ffunction-sections -fdata-sections
            -Wl,--gc-sections)
endif ()

set(EXECUTABLE_NAME "core${BINARY_SUFFIX}")
set_target_properties(
        main PROPERTIES OUTPUT_NAME ${EXECUTABLE_NAME}
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../bin")

# ---------------------------------------------------------------------------------

find_package(asio CONFIG REQUIRED)
target_link_libraries(main PRIVATE asio::asio)

find_package(jsoncpp CONFIG REQUIRED)
target_link_libraries(main PRIVATE JsonCpp::JsonCpp)

add_subdirectory(vendor/cxxopts)
target_link_libraries(main PRIVATE cxxopts)

# ---------------------------------------------------------------------------------

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()

message(STATUS "< ============================== >")
message(STATUS "${CMAKE_CXX_FLAGS}")
message(STATUS "< ============================== >")
