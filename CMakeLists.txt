cmake_minimum_required(VERSION 3.21)
project(Project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PROJECT_USE_SANITIZERS "Enable ASan/UBSan" ON)
option(BUILD_GUI "Build GUI app" OFF)
option(BUILD_TESTS "Build unit tests" ON)

function(apply_warnings tgt)
  if (MSVC)
    target_compile_options(${tgt} PRIVATE /W4 /permissive- /Zc:__cplusplus /utf-8)
  else()
    target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

function(apply_sanitizers tgt)
  if (PROJECT_USE_SANITIZERS AND NOT MSVC)
    target_compile_options(${tgt} PRIVATE -fsanitize=address,undefined)
    target_link_options(${tgt}    PRIVATE -fsanitize=address,undefined)
  endif()
endfunction()


add_library(my_library
  lib.cpp
  src/db.cpp
  src/parser.cpp
  src/exec.cpp
  src/format.cpp
)
target_include_directories(my_library PUBLIC include)
apply_warnings(my_library)
apply_sanitizers(my_library)

add_executable(my_cli_tool cli.cpp)
target_link_libraries(my_cli_tool PRIVATE my_library)
apply_warnings(my_cli_tool)
apply_sanitizers(my_cli_tool)

if (BUILD_GUI)
  find_package(Qt6 COMPONENTS Core Widgets REQUIRED)
  add_executable(my_gui_tool gui.cpp)
  target_link_libraries(my_gui_tool PRIVATE my_library Qt6::Core Qt6::Widgets)
  apply_warnings(my_gui_tool)
  apply_sanitizers(my_gui_tool)
endif()

if (BUILD_TESTS)
  enable_testing()

  find_package(Catch2 3 CONFIG REQUIRED)     
  find_package(GTest CONFIG REQUIRED)      

 
  add_executable(my_catch2_tests tests.cpp)
  target_link_libraries(my_catch2_tests PRIVATE my_library Catch2::Catch2WithMain)
  apply_warnings(my_catch2_tests)
  apply_sanitizers(my_catch2_tests)

  include(Catch)
  catch_discover_tests(my_catch2_tests)

  add_executable(my_gtest_tests tests2.cpp)
  target_link_libraries(my_gtest_tests PRIVATE my_library GTest::gtest_main)
  apply_warnings(my_gtest_tests)
  apply_sanitizers(my_gtest_tests)

  include(GoogleTest)
  gtest_discover_tests(my_gtest_tests)
endif()
