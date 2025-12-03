cmake_minimum_required(VERSION 3.20)
project(MastersLavaSimulation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external/DirectX-Headers)
add_subdirectory(external/DirectXTK12)

include(FetchContent)
set(WIL_BUILD_TESTS FALSE)
FetchContent_Declare(
    WIL
    GIT_REPOSITORY https://github.com/microsoft/wil.git
    GIT_TAG v1.0.250325.1
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/wil
)
FetchContent_MakeAvailable(WIL)

add_subdirectory(src)
add_subdirectory(GPUSorting)

add_executable(${PROJECT_NAME} main.cc ${SRC_FILES} ${SORT_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
${CMAKE_CURRENT_LIST_DIR}/include
${CMAKE_CURRENT_LIST_DIR}/external/DirectX-Headers/include
${CMAKE_CURRENT_LIST_DIR}/external/DirectXTK12/Inc
${CMAKE_CURRENT_LIST_DIR}/external/wil/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
DirectX-Headers DirectXTK12 d3d12 dxgi dxcompiler WIL windowsapp runtimeobject
)


