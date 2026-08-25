cmake_minimum_required(VERSION 3.16)
project(BERender VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BERENDER_VULKAN "Enable Vulkan backend" ON)
option(BERENDER_GLES "Enable OpenGL ES backend" ON)
option(BERENDER_EXAMPLES "Build examples" ON)

if(ANDROID)
    add_definitions(-DBERENDER_PLATFORM_ANDROID=1)
endif()

if(BERENDER_VULKAN)
    find_package(Vulkan QUIET)
    if(NOT Vulkan_FOUND)
        message(WARNING "Vulkan not found, disabling Vulkan backend")
        set(BERENDER_VULKAN OFF)
    endif()
endif()

set(BERENDER_SOURCES
    src/core/logger.cpp
    src/render/frame_registry.cpp
    src/render/chunk_mesh.cpp
    src/render/indirect_draw.cpp
    src/render/material.cpp
    src/render/renderer.cpp
)
if(BERENDER_VULKAN)
    list(APPEND BERENDER_SOURCES src/backend/vulkan_backend.cpp)
endif()
if(BERENDER_GLES)
    list(APPEND BERENDER_SOURCES src/backend/gles_backend.cpp)
endif()

add_library(berender STATIC ${BERENDER_SOURCES})
target_include_directories(berender PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(berender PUBLIC cxx_std_17)

if(BERENDER_VULKAN AND Vulkan_FOUND)
    target_link_libraries(berender PUBLIC Vulkan::Vulkan)
    target_compile_definitions(berender PUBLIC BERENDER_HAS_VULKAN=1)
endif()
if(BERENDER_GLES)
    target_compile_definitions(berender PUBLIC BERENDER_HAS_GLES=1)
    if(ANDROID)
        target_link_libraries(berender PUBLIC EGL GLESv3)
    endif()
endif()

if(BERENDER_EXAMPLES)
    add_executable(berender_example examples/main.cpp)
    target_link_libraries(berender_example PRIVATE berender)
endif()
