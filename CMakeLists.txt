cmake_minimum_required(VERSION 3.24)
project(DeadZoneEngine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# ---------------------------------------------------------------- SDL3
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.20
    GIT_SHALLOW    TRUE)

# ---------------------------------------------------------------- bgfx
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS    ON  CACHE BOOL "" FORCE) # shaderc, texturec... (utile pour les shaders)
set(BGFX_CUSTOM_TARGETS OFF CACHE BOOL "" FORCE)
# NB : on nomme ce package "bgfx_cmake" (et surtout pas "bgfx") : bgfx.cmake telecharge
# lui-meme bgfx/bx/bimg sous ces noms, et un conflit de nom casse la configuration.
FetchContent_Declare(bgfx_cmake
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
    GIT_TAG        v1.168.9624-612
    GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable(SDL3 bgfx_cmake)

# -------------------------------------------------------------- Moteur
add_executable(DeadZone src/main.cpp)
target_link_libraries(DeadZone PRIVATE SDL3::SDL3 bgfx bx bimg bimg_decode)

if(WIN32 AND MSVC)
    target_compile_options(DeadZone PRIVATE /W4 /utf-8)
else()
    target_compile_options(DeadZone PRIVATE -Wall -Wextra)
endif()
