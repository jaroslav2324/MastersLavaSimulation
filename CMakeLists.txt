cmake_minimum_required(VERSION 3.20)
project(MastersLavaSimulation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include headers
include_directories("${CMAKE_CURRENT_LIST_DIR}/include")

# Add subdirectory for sources
add_subdirectory(src)

# Add main executable
add_executable(${PROJECT_NAME} main.cc)

add_subdirectory(external/DirectXTK12)

# Link with the src library (see below)DirectXTK
target_link_libraries(${PROJECT_NAME} PRIVATE MastersLavaSimulationLib DirectXTK12)

