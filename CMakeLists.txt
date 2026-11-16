cmake_minimum_required(VERSION 3.24)
project(Sovereignia VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Disable third-party tests, examples, docs, and installs
set(ENTT_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ENTT_BUILD_DOCS OFF CACHE BOOL "" FORCE)

set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    entt
    URL https://github.com/skypjack/entt/archive/refs/tags/v3.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_Declare(
    ftxui
    URL https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v6.1.9.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(entt ftxui nlohmann_json)

add_library(sovereignia_core STATIC
    src/core/paths.cpp
    src/core/clock.cpp
    src/data/config_registry.cpp
    src/world/world_map.cpp
    src/world/pathfinding.cpp
    src/world/map_generator.cpp
    src/sim/history_log.cpp
    src/sim/economy_system.cpp
    src/sim/ecology_system.cpp
    src/sim/family_tree.cpp
    src/sim/dynasty_system.cpp
    src/sim/governance_war_system.cpp
    src/sim/theology_system.cpp
    src/sim/engine.cpp
)

target_include_directories(sovereignia_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(sovereignia_core
    PUBLIC
        EnTT::EnTT
        nlohmann_json::nlohmann_json
)

target_compile_options(sovereignia_core
    PUBLIC
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W4 /permissive->
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

if(WIN32)
    target_compile_definitions(sovereignia_core
        PUBLIC
            NOMINMAX
            WIN32_LEAN_AND_MEAN
    )
endif()

add_library(sovereignia_ui STATIC
    src/ui/terminal_ui.cpp
)

target_link_libraries(sovereignia_ui
    PUBLIC
        sovereignia_core
        ftxui::screen
        ftxui::dom
        ftxui::component
)

add_executable(sovereignia
    src/main.cpp
)

target_link_libraries(sovereignia
    PRIVATE
        sovereignia_ui
)

enable_testing()

add_executable(sovereignia_tests
    tests/test_main.cpp
)

target_link_libraries(sovereignia_tests
    PRIVATE
        sovereignia_ui
)

add_test(NAME sovereignia_tests COMMAND sovereignia_tests)

