cmake_minimum_required(VERSION 3.20)
project(MastersLavaSimulation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external/DirectX-Headers)
add_subdirectory(external/DirectXTK12)

add_subdirectory(src)

add_executable(${PROJECT_NAME} main.cc ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
${CMAKE_CURRENT_LIST_DIR}/include
${CMAKE_CURRENT_LIST_DIR}/external/DirectX-Headers/include
${CMAKE_CURRENT_LIST_DIR}/external/DirectXTK12/Inc
                    )

target_link_libraries(${PROJECT_NAME} PRIVATE DirectX-Headers DirectXTK12)


