add_library(memogent-c SHARED memogent_c.cpp)
add_library(memogent::c ALIAS memogent-c)

target_link_libraries(memogent-c PRIVATE memogent::core)
target_include_directories(memogent-c
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>)

target_compile_definitions(memogent-c PRIVATE MEM_BUILDING_SHARED=1)
set_target_properties(memogent-c PROPERTIES
    OUTPUT_NAME memogent
    SOVERSION ${PROJECT_VERSION_MAJOR}
    VERSION   ${PROJECT_VERSION})
