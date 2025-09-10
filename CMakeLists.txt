cmake_minimum_required(VERSION 3.20)
project(MastersLavaSimulation)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PR_ROOT ${CMAKE_CURRENT_LIST_DIR})

include_directories("${PR_ROOT}/include")

add_subdirectory(src)

add_executable(${PROJECT_NAME} "${PR_ROOT}/main.cc" ${SRC_FILES})

