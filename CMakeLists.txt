cmake_minimum_required(VERSION 3.17)

enable_language(CXX)

project(linkollector-cli LANGUAGES CXX VERSION 0.0.1)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

add_subdirectory(extern)
add_subdirectory(src)
