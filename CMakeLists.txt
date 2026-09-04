cmake_minimum_required(VERSION 3.20)
project(orderbook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GNU extensions stay on: src/mapped_file.cpp needs the glibc declarations for
# mmap/madvise, which -std=c++20 (strict) hides behind __STRICT_ANSI__.
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

option(ITCH_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(ITCH_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)

add_library(itch
  src/message_table.cpp
)
target_include_directories(itch PUBLIC include)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()