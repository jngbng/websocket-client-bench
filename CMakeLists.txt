cmake_minimum_required(VERSION 3.14...3.22)

project(
  websocket_client_bench
  LANGUAGES C CXX
  VERSION 0.1
  DESCRIPTION "Simple Websocket for C++ client benchmarks"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)

if(supported)
    message(STATUS "LTO enabled")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "LTO not supported: ${error}")
endif()

# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

# ---- Cmake Module Path ----
set(CUSTOM_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
if (NOT "${CUSTOM_MODULE_PATH}" IN_LIST CMAKE_MODULE_PATH)
  list(APPEND CMAKE_MODULE_PATH ${CUSTOM_MODULE_PATH})
endif()

find_package(uwebsockets)
find_package(OpenSSL REQUIRED)
find_package(magic_enum)
find_package(argparse REQUIRED)
find_package(Boost)
find_package(certify)

# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" CACHE STRING "Directory to generate executables" FORCE)

add_subdirectory(common)
add_subdirectory(uwebsockets)
add_subdirectory(wspp)
add_subdirectory(boost_beast)
add_subdirectory(libwebsockets)
