cmake_minimum_required(VERSION 3.20)

project(karma_find_package_example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(karma REQUIRED)

add_executable(karma_find_package_example main.cpp)
target_link_libraries(karma_find_package_example PRIVATE karma::karma)
