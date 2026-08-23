cmake_minimum_required(VERSION 3.24)
project(procengine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(VULKAN_SDK "C:/VulkanSDK/1.4.309.0" CACHE PATH "Vulkan SDK root")

set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${VULKAN_SDK}")

find_package(Vulkan REQUIRED)

set(GLSLC "${VULKAN_SDK}/Bin/glslc.exe" CACHE FILEPATH "Path to glslc")

include(FetchContent)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.10
    SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/sdl3-src
)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_BUILD_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_BUILD_DIR})

set(SHADER_SOURCES
    ${SHADER_DIR}/scene.vert
    ${SHADER_DIR}/scene.frag
    ${SHADER_DIR}/shadow.vert
    ${SHADER_DIR}/shadow.frag
)

set(SHADER_SPV_FILES "")
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPV_FILE ${SHADER_BUILD_DIR}/${SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT ${SPV_FILE}
        COMMAND ${GLSLC} ${SHADER} -o ${SPV_FILE}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${SHADER_NAME}"
    )
    list(APPEND SHADER_SPV_FILES ${SPV_FILE})
endforeach()

add_custom_target(shaders ALL DEPENDS ${SHADER_SPV_FILES})

set(ENGINE_SOURCES
    src/engine/window/Window.cpp
    src/engine/renderer/Renderer.cpp
    src/engine/renderer/Buffer.cpp
    src/engine/renderer/Pipeline.cpp
    src/engine/camera/Camera.cpp
    src/engine/input/Input.cpp
    src/procedural/terrain/TerrainGenerator.cpp
    src/procedural/vegetation/TreeGenerator.cpp
    src/procedural/building/HouseGenerator.cpp
    src/procedural/rock/RockGenerator.cpp
    src/procedural/world/WorldPlacer.cpp
    src/procedural/world/TerrainSurface.cpp
    src/procedural/world/ChunkGenerator.cpp
    src/procedural/world/ChunkPlacer.cpp
    src/procedural/world/ChunkStreamer.cpp
    src/procedural/world/WorldStore.cpp
)

add_library(procengine STATIC ${ENGINE_SOURCES})

target_include_directories(procengine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${VULKAN_SDK}/Include
)

find_package(glm CONFIG QUIET)
if(NOT glm_FOUND)
    set(GLM_INCLUDE_DIR "${VULKAN_SDK}/Include/glm")
    target_include_directories(procengine PUBLIC ${GLM_INCLUDE_DIR})
else()
    target_link_libraries(procengine PUBLIC glm::glm)
endif()

target_link_libraries(procengine PUBLIC
    Vulkan::Vulkan
    SDL3::SDL3-static
)

add_executable(procengine-demo src/demo/main.cpp)
target_link_libraries(procengine-demo PRIVATE procengine)
add_dependencies(procengine-demo shaders)

add_executable(procengine-test
    src/test/test_main.cpp
    src/test/test_world_seed.cpp
    src/test/test_chunk_generator.cpp
    src/test/test_chunk_dump.cpp
    src/test/test_chunk_placer.cpp
    src/test/test_chunk_streamer.cpp
    src/test/test_world_store.cpp)
target_link_libraries(procengine-test PRIVATE procengine)

add_custom_command(TARGET procengine-demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_BUILD_DIR} $<TARGET_FILE_DIR:procengine-demo>/shaders
)