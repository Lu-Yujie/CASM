cmake_minimum_required(VERSION 3.0)
project(SubgraphMatching)

include_directories(
        ${PROJECT_SOURCE_DIR}/utils
        ${PROJECT_SOURCE_DIR}/
)

set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -std=c++14 -O3 -g -Wall -march=native -pthread")
        # "${CMAKE_CXX_FLAGS} -std=c++14 -O3 -fsanitize=address -g -Wall -march=native -pthread")

add_subdirectory(utils)
add_subdirectory(graph)
add_subdirectory(matching)
