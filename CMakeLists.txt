cmake_minimum_required(VERSION 2.8)
project(artifact-denoiser)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(yaml-cpp)
add_subdirectory(curlpp)

file(GLOB headers src/*.hpp)
file(GLOB sources src/*.cpp)

include_directories(yaml-cpp/include)
include_directories(curlpp/include)
add_executable(${PROJECT_NAME} ${headers} ${sources} main.cpp)
target_link_libraries(${PROJECT_NAME} yaml-cpp)
target_link_libraries(${PROJECT_NAME} curlpp)
target_link_libraries(${PROJECT_NAME} pthread)

# tests

add_subdirectory(googletest)
include_directories(googletest/googletest/include)
include_directories(googletest/googlemock/include)
include_directories(src)
add_executable(${PROJECT_NAME}-test ${headers} ${sources} test.cpp)
target_link_libraries(${PROJECT_NAME}-test gtest)
target_link_libraries(${PROJECT_NAME}-test curlpp_static)
