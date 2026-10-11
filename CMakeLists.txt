cmake_minimum_required(VERSION 3.20)
project(wind_launcher LANGUAGES CXX)

# The Wind SDK. CMAKE_PREFIX_PATH / Wind_DIR point at it (docs/manual/platforms/Desktop.md, "Exporting without a window").
find_package(Wind REQUIRED)

# The list, SDK, project, and template logic, so wind_launcher_tests compiles the same files without the UI.
set(_wind_launcher_parts
    src/launcher_state.cpp
    src/project_entry.cpp
    src/project_template.cpp
    src/sdk_catalog.cpp
    src/user_paths.cpp)

engine_add_game(wind_launcher
    src/main.cpp
    src/launcher_app.cpp
    src/launcher_view_model.cpp
    src/project_row_view_model.cpp
    src/sdk_option_view_model.cpp
    src/sdk_row_view_model.cpp
    ${_wind_launcher_parts})
# Not a .app bundle on macOS: it installs as a plain executable with assets/ beside it, like the editor.
set_target_properties(wind_launcher PROPERTIES MACOSX_BUNDLE OFF)
target_include_directories(wind_launcher PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

enable_testing()
include(GoogleTest)
add_executable(wind_launcher_tests tests/launcher_test.cpp ${_wind_launcher_parts})
set_target_properties(wind_launcher_tests PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(wind_launcher_tests PRIVATE /W4 /permissive- /utf-8)
    set_target_properties(wind_launcher_tests PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
endif()
target_link_libraries(wind_launcher_tests PRIVATE engine GTest::gtest_main)
target_include_directories(wind_launcher_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
# templates/, which the SDK installs: a test makes a project from it.
target_compile_definitions(wind_launcher_tests PRIVATE WIND_SDK_TEMPLATES_DIR="${WIND_SDK_DIR}/templates")
gtest_discover_tests(wind_launcher_tests DISCOVERY_MODE PRE_TEST)
