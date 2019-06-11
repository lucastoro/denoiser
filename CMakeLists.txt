cmake_minimum_required(VERSION 3.8) # CMAKE_CXX_STANDARD = 17 requires CMake 3.8
project(artifact-denoiser)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(DENOISER_TESTS "Build the unit test suite" ON)
option(DENOISER_THREAD_POOL "Enable the thread pool" ON)

add_subdirectory(yaml-cpp)
add_subdirectory(curlpp)

file(GLOB headers src/*.hpp)
file(GLOB sources src/*.cpp)

if(DENOISER_TESTS)
  list(APPEND headers src/test/test.hpp)
  list(APPEND sources src/test/test.cpp)
endif()

include_directories(src)
include_directories(yaml-cpp/include)
include_directories(curlpp/include)

add_executable(${PROJECT_NAME} ${headers} ${sources})
target_link_libraries(${PROJECT_NAME} yaml-cpp)
target_link_libraries(${PROJECT_NAME} curlpp)
target_link_libraries(${PROJECT_NAME} pthread)


if(DENOISER_THREAD_POOL)
  target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_THREAD_POOL)
endif()

if(DENOISER_TESTS)
  target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_TESTS)
  add_subdirectory(googletest)
  include_directories(googletest/googletest/include)
  include_directories(googletest/googlemock/include)
  target_link_libraries(${PROJECT_NAME} gtest)
endif()
