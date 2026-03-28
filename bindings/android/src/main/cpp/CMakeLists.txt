cmake_minimum_required(VERSION 3.24)
project(memogent-android CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build the Memogent core from its top-level CMake.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../../.."
                 "${CMAKE_BINARY_DIR}/memogent-core-build" EXCLUDE_FROM_ALL)

# The JNI shared library that Kotlin's System.loadLibrary("memogent_jni")
# resolves at runtime.
add_library(memogent_jni SHARED memogent_jni.cpp)

target_link_libraries(memogent_jni PRIVATE memogent::core memogent::c log)
target_compile_definitions(memogent_jni PRIVATE MEM_BUILDING_SHARED=1)
