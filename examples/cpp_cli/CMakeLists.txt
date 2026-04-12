add_executable(mem main.cpp)
target_link_libraries(mem PRIVATE memogent::core)
set_target_properties(mem PROPERTIES OUTPUT_NAME mem)
