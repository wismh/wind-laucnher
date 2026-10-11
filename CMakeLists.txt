cmake_minimum_required(VERSION 3.20)
project(wind_launcher LANGUAGES CXX)

# The Wind SDK. CMAKE_PREFIX_PATH / Wind_DIR point at it (docs/manual/platforms/Desktop.md, "Exporting without a window").
find_package(Wind REQUIRED)
# The SDK root. WIND_SDK_DIR is set only when the SDK has player libraries; a source build (macOS) leaves it unset.
get_filename_component(_wind_launcher_sdk "${Wind_DIR}/.." ABSOLUTE)

# The list, SDK, project, and template logic, so wind_launcher_tests compiles the same files without the UI.
set(_wind_launcher_parts
    src/downloader.cpp
    src/extractor.cpp
    src/json.cpp
    src/launcher_state.cpp
    src/project_entry.cpp
    src/project_template.cpp
    src/release_feed.cpp
    src/sdk_catalog.cpp
    src/sdk_installer.cpp
    src/sdk_release.cpp
    src/sha256.cpp
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
if(NOT TARGET GTest::gtest_main)
    # An export build that compiles the engine from <sdk>/source (no player libraries) does not import GoogleTest;
    # the SDK ships it beside the engine's source.
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory("${_wind_launcher_sdk}/source/external/googletest" googletest EXCLUDE_FROM_ALL)
endif()
add_executable(wind_launcher_tests
    tests/downloader_test.cpp
    tests/extractor_test.cpp
    tests/json_test.cpp
    tests/launcher_test.cpp
    tests/release_feed_test.cpp
    tests/sdk_installer_test.cpp
    tests/sdk_release_test.cpp
    tests/sha256_test.cpp
    ${_wind_launcher_parts})
set_target_properties(wind_launcher_tests PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(wind_launcher_tests PRIVATE /W4 /permissive- /utf-8)
    set_target_properties(wind_launcher_tests PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
endif()
target_link_libraries(wind_launcher_tests PRIVATE engine GTest::gtest_main)
target_include_directories(wind_launcher_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/tests")
# templates/, which the SDK installs: a test makes a project from it.
target_compile_definitions(wind_launcher_tests PRIVATE
    WIND_SDK_TEMPLATES_DIR="${_wind_launcher_sdk}/templates"
    LAUNCHER_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
gtest_discover_tests(wind_launcher_tests DISCOVERY_MODE PRE_TEST)
