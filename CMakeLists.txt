cmake_minimum_required(VERSION 3.24)
project(memogent
    VERSION 0.1.0
    DESCRIPTION "Context-aware adaptive memory orchestrator for on-device agentic systems"
    HOMEPAGE_URL "https://github.com/Lagmator22/memogent"
    LANGUAGES CXX C)

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------
option(MEM_BUILD_TESTS           "Build Catch2 unit tests"                   ON)
option(MEM_BUILD_EXAMPLES        "Build CLI / example binaries"              ON)
option(MEM_BUILD_BINDING_C       "Build the stable C ABI shared lib"         ON)
option(MEM_BUILD_BINDING_PYTHON  "Build the pybind11 Python wheel"           OFF)
option(MEM_WITH_SQLCIPHER        "Use SQLCipher for at-rest encryption"      OFF)
option(MEM_WITH_ONNX             "Enable ONNX Runtime predictor"             OFF)
option(MEM_WITH_LLAMACPP         "Enable direct llama.cpp KV-cache hooks"    OFF)
option(MEM_WARNINGS_AS_ERRORS    "Treat compiler warnings as errors"         OFF)

# -----------------------------------------------------------------------------
# Standards / flags
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies.cmake)

# -----------------------------------------------------------------------------
# Core library
# -----------------------------------------------------------------------------
file(GLOB_RECURSE MEM_CORE_SOURCES CONFIGURE_DEPENDS
    core/src/*.cpp
    core/src/*.c)
file(GLOB_RECURSE MEM_CORE_HEADERS CONFIGURE_DEPENDS
    core/include/memogent/*.hpp)

add_library(memogent-core STATIC ${MEM_CORE_SOURCES} ${MEM_CORE_HEADERS})
add_library(memogent::core ALIAS memogent-core)

target_include_directories(memogent-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
        $<INSTALL_INTERFACE:include>)

target_compile_features(memogent-core PUBLIC cxx_std_23)
target_compile_definitions(memogent-core
    PUBLIC
        $<$<BOOL:${MEM_WITH_SQLCIPHER}>:MEM_WITH_SQLCIPHER=1>
        $<$<BOOL:${MEM_WITH_ONNX}>:MEM_WITH_ONNX=1>
        $<$<BOOL:${MEM_WITH_LLAMACPP}>:MEM_WITH_LLAMACPP=1>)

target_link_libraries(memogent-core PUBLIC memogent::deps)

if(APPLE OR UNIX)
    target_compile_options(memogent-core PRIVATE
        -Wall -Wextra -Wpedantic -Wno-missing-field-initializers
        $<$<BOOL:${MEM_WARNINGS_AS_ERRORS}>:-Werror>)
elseif(MSVC)
    target_compile_options(memogent-core PRIVATE /W4 /permissive-
        $<$<BOOL:${MEM_WARNINGS_AS_ERRORS}>:/WX>)
endif()

# -----------------------------------------------------------------------------
# Optional targets
# -----------------------------------------------------------------------------
if(MEM_BUILD_BINDING_C)
    add_subdirectory(bindings/c)
endif()

if(MEM_BUILD_EXAMPLES AND NOT MEM_PLATFORM_MOBILE)
    add_subdirectory(examples/cpp_cli)
endif()

if(MEM_BUILD_TESTS AND NOT MEM_PLATFORM_MOBILE)
    enable_testing()
    add_subdirectory(cpp_tests)
endif()

if(MEM_BUILD_BINDING_PYTHON)
    add_subdirectory(bindings/python)
endif()

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "memogent ${PROJECT_VERSION}")
message(STATUS "  Platform          : ${MEM_PLATFORM_NAME}")
message(STATUS "  C++ standard      : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type        : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Tests             : ${MEM_BUILD_TESTS}")
message(STATUS "  Examples          : ${MEM_BUILD_EXAMPLES}")
message(STATUS "  C ABI             : ${MEM_BUILD_BINDING_C}")
message(STATUS "  Python binding    : ${MEM_BUILD_BINDING_PYTHON}")
message(STATUS "  SQLCipher         : ${MEM_WITH_SQLCIPHER}")
message(STATUS "  ONNX predictor    : ${MEM_WITH_ONNX}")
message(STATUS "  llama.cpp KV hook : ${MEM_WITH_LLAMACPP}")
message(STATUS "")
