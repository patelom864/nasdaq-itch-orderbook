include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
)
FetchContent_MakeAvailable(googletest)

add_executable(itch_tests
  test_message_table.cpp
)
target_link_libraries(itch_tests PRIVATE itch GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(itch_tests)