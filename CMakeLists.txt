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

# --------------------------------------------------------------- Assimp
# Import de modeles 3D. On ne compile que les formats utiles (build plus rapide).
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
foreach(fmt FBX OBJ STL GLTF PLY COLLADA 3DS)
    set(ASSIMP_BUILD_${fmt}_IMPORTER ON CACHE BOOL "" FORCE)
endforeach()
FetchContent_Declare(assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG        v6.0.5
    GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable(SDL3 bgfx_cmake assimp)

# -------------------------------------------------------------- Shaders
# Les shaders (dossier shaders/) sont compiles par shaderc a chaque build, puis integres
# au code sous forme de headers (plus aucun fichier a distribuer a cote de l'exe).
include(${bgfx_cmake_SOURCE_DIR}/cmake/bgfxToolUtils.cmake)
FetchContent_GetProperties(bgfx) # -> bgfx_SOURCE_DIR (pour bgfx_shader.sh)

set(DZ_SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(DZ_SHADER_OUT ${CMAKE_CURRENT_BINARY_DIR}/generated/shaders)

# Profils : spirv = Vulkan, 430 = OpenGL, s_5_0 = Direct3D 11 (Windows uniquement)
set(DZ_SHADER_PROFILES spirv 430)
if(WIN32)
    list(APPEND DZ_SHADER_PROFILES s_5_0)
endif()

bgfx_compile_shaders(
    TYPE VERTEX
    SHADERS ${DZ_SHADER_DIR}/vs_cube.sc ${DZ_SHADER_DIR}/vs_mesh.sc
    VARYING_DEF ${DZ_SHADER_DIR}/varying.def.sc
    OUTPUT_DIR ${DZ_SHADER_OUT}
    OUT_FILES_VAR DZ_VS_FILES
    INCLUDE_DIRS ${bgfx_SOURCE_DIR}/src
    PROFILES ${DZ_SHADER_PROFILES}
    AS_HEADERS)
bgfx_compile_shaders(
    TYPE FRAGMENT
    SHADERS ${DZ_SHADER_DIR}/fs_cube.sc ${DZ_SHADER_DIR}/fs_mesh.sc
    VARYING_DEF ${DZ_SHADER_DIR}/varying.def.sc
    OUTPUT_DIR ${DZ_SHADER_OUT}
    OUT_FILES_VAR DZ_FS_FILES
    INCLUDE_DIRS ${bgfx_SOURCE_DIR}/src
    PROFILES ${DZ_SHADER_PROFILES}
    AS_HEADERS)

# -------------------------------------------------------------- Moteur
add_executable(DeadZone src/main.cpp src/model.cpp ${DZ_VS_FILES} ${DZ_FS_FILES})
target_include_directories(DeadZone PRIVATE ${DZ_SHADER_OUT})
target_link_libraries(DeadZone PRIVATE SDL3::SDL3 assimp::assimp bgfx bx bimg bimg_decode)

if(WIN32 AND MSVC)
    target_compile_options(DeadZone PRIVATE /W4 /utf-8)
else()
    target_compile_options(DeadZone PRIVATE -Wall -Wextra)
endif()
