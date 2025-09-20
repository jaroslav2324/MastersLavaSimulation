cmake_minimum_required(VERSION 3.20)

file(GLOB SRC_FILES "*.cc")

add_library(MastersLavaSimulationLib STATIC ${SRC_FILES})

target_include_directories(MastersLavaSimulationLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../include
)