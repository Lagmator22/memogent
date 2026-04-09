add_executable(memogent_tests
    test_predictor.cpp
    test_cache.cpp
    test_orchestrator.cpp)

target_link_libraries(memogent_tests PRIVATE
    memogent::core
    Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(memogent_tests)
