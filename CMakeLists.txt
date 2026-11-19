cmake_minimum_required(VERSION 3.22)
project(WarpedVinylHiFi VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
endif()

# JUCE: use a local checkout if present, otherwise fetch it.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
  add_subdirectory(JUCE)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

set(PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND PLUGIN_FORMATS AU)
endif()

add_subdirectory(WarpedVinyl)

option(BUILD_DSP_TESTS "Build the offline DSP test runner" ON)
if(BUILD_DSP_TESTS)
  add_executable(dsp_tests tests/dsp_tests.cpp)
  target_compile_definitions(dsp_tests PRIVATE TEST_WV)
endif()
