if(EXISTS "/home/user/wind-laucnher/build/Release/wind_launcher_tests")
  if(NOT EXISTS "/home/user/wind-laucnher/build/wind_launcher_tests[1]_tests-Release.cmake" OR
     NOT "/home/user/wind-laucnher/build/wind_launcher_tests[1]_tests-Release.cmake" IS_NEWER_THAN "/home/user/wind-laucnher/build/Release/wind_launcher_tests" OR
     NOT "/home/user/wind-laucnher/build/wind_launcher_tests[1]_tests-Release.cmake" IS_NEWER_THAN "${CMAKE_CURRENT_LIST_FILE}")
    include("/usr/share/cmake-3.28/Modules/GoogleTestAddTests.cmake")
    gtest_discover_tests_impl(
      TEST_EXECUTABLE [==[/home/user/wind-laucnher/build/Release/wind_launcher_tests]==]
      TEST_EXECUTOR [==[]==]
      TEST_WORKING_DIR [==[/home/user/wind-laucnher/build]==]
      TEST_EXTRA_ARGS [==[]==]
      TEST_PROPERTIES [==[]==]
      TEST_PREFIX [==[]==]
      TEST_SUFFIX [==[]==]
      TEST_FILTER [==[]==]
      NO_PRETTY_TYPES [==[FALSE]==]
      NO_PRETTY_VALUES [==[FALSE]==]
      TEST_LIST [==[wind_launcher_tests_TESTS]==]
      CTEST_FILE [==[/home/user/wind-laucnher/build/wind_launcher_tests[1]_tests-Release.cmake]==]
      TEST_DISCOVERY_TIMEOUT [==[5]==]
      TEST_XML_OUTPUT_DIR [==[]==]
    )
  endif()
  include("/home/user/wind-laucnher/build/wind_launcher_tests[1]_tests-Release.cmake")
else()
  add_test(wind_launcher_tests_NOT_BUILT wind_launcher_tests_NOT_BUILT)
endif()
