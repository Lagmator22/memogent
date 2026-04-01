include_guard(GLOBAL)

# SQLite
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    CPMAddPackage(
        NAME SQLite3Amalgamation
        URL https://github.com/sqlite/sqlite/archive/refs/tags/version-3.45.1.tar.gz
        DOWNLOAD_ONLY YES)
    if(SQLite3Amalgamation_ADDED)
        # Fallback when the amalgamation is not accessible: try the system package.
        message(STATUS "memogent: SQLite amalgamation fetched; building locally.")
    endif()
    # If neither path works, the user can install sqlite via their package manager.
endif()

# JSON
CPMAddPackage(
    NAME nlohmann_json
    VERSION 3.11.3
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    OPTIONS "JSON_BuildTests OFF")

# fmt / spdlog
CPMAddPackage("gh:fmtlib/fmt#10.2.1")
CPMAddPackage(
    NAME spdlog
    GITHUB_REPOSITORY gabime/spdlog
    VERSION 1.13.0
    OPTIONS "SPDLOG_FMT_EXTERNAL ON")

# Catch2 tests
if(MEM_BUILD_TESTS)
    CPMAddPackage("gh:catchorg/Catch2@3.5.2")
endif()

# Aggregate interface target
add_library(memogent-deps INTERFACE)
add_library(memogent::deps ALIAS memogent-deps)
target_link_libraries(memogent-deps INTERFACE
    nlohmann_json::nlohmann_json
    fmt::fmt
    spdlog::spdlog)

if(TARGET SQLite::SQLite3)
    target_link_libraries(memogent-deps INTERFACE SQLite::SQLite3)
endif()
