cmake_minimum_required(VERSION 3.24)
project(pontifice LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# SDL3: usa a versão instalada no sistema se existir; senão baixa e compila.
find_package(SDL3 CONFIG QUIET)
if(NOT SDL3_FOUND)
  include(FetchContent)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.18
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(SDL3)
endif()

# Bibliotecas header-only: cgltf (glTF) e stb_image (PNG/JPEG).
include(FetchContent)
FetchContent_Declare(cgltf
  GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
  GIT_TAG v1.15
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR cmake-none)   # sem add_subdirectory, só o header
FetchContent_Declare(stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG master
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR cmake-none)
FetchContent_MakeAvailable(cgltf stb)

# Shader Metal embutido no binário como array de bytes.
set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
set(METAL_INC ${GENERATED_DIR}/scripts/gpu/card_metal.inc)
add_custom_command(
  OUTPUT ${METAL_INC}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_DIR}/scripts/gpu
  COMMAND ${CMAKE_COMMAND} -DIN=${CMAKE_SOURCE_DIR}/src/shaders/card.metal -DOUT=${METAL_INC}
          -P ${CMAKE_SOURCE_DIR}/cmake/embed.cmake
  DEPENDS ${CMAKE_SOURCE_DIR}/src/shaders/card.metal ${CMAKE_SOURCE_DIR}/cmake/embed.cmake
  COMMENT "Embutindo card.metal")

# SPIR-V (Vulkan): compilado só se glslc estiver instalado. No Metal não é necessário.
find_program(GLSLC glslc)
set(SPIRV_OUTPUTS)
if(GLSLC)
  foreach(stage vert frag)
    set(spv ${CMAKE_BINARY_DIR}/shaders/card.${stage}.spv)
    add_custom_command(
      OUTPUT ${spv}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
      COMMAND ${GLSLC} -fshader-stage=${stage}
              ${CMAKE_SOURCE_DIR}/src/shaders/card.${stage}.glsl -o ${spv}
      DEPENDS ${CMAKE_SOURCE_DIR}/src/shaders/card.${stage}.glsl
      COMMENT "Compilando card.${stage}.glsl para SPIR-V")
    list(APPEND SPIRV_OUTPUTS ${spv})
  endforeach()
else()
  message(STATUS "glslc nao encontrado: shaders SPIR-V (Vulkan) nao serao compilados")
endif()

add_executable(pontifice
  src/main.cpp
  src/scenes/SceneManager.cpp
  src/scenes/menu/MenuScene.cpp
  src/scenes/battle/BattleScene.cpp
  src/scripts/text/Text.cpp
  src/scripts/button/Button.cpp
  src/scripts/button/ButtonPlay.cpp
  src/scripts/button/ButtonExit.cpp
  src/scripts/image/Image.cpp
  src/scripts/gpu/Gpu.cpp
  src/scripts/model/Model.cpp
  src/scripts/hand/Hand.cpp
  src/scripts/hud/Hud.cpp
  ${METAL_INC}
  ${SPIRV_OUTPUTS}
)
# Windows: ícone do executável como recurso (src/icons/icon.ico, gerado por tools/make_icons.py).
if(WIN32)
  enable_language(RC)
  target_sources(pontifice PRIVATE src/icons/pontifice.rc)
endif()
target_include_directories(pontifice PRIVATE src ${GENERATED_DIR})
target_include_directories(pontifice SYSTEM PRIVATE ${cgltf_SOURCE_DIR} ${stb_SOURCE_DIR})
target_link_libraries(pontifice PRIVATE SDL3::SDL3)
target_compile_options(pontifice PRIVATE -Wall -Wextra)

# macOS: alguns Command Line Tools deixam um diretório libc++ antigo e incompleto em
# /Library/Developer/CommandLineTools/usr/include/c++/v1, que esconde o libc++ do SDK.
# Se for o caso, aponta o compilador direto para o libc++ do SDK.
if(APPLE)
  set(_stale_libcxx "/Library/Developer/CommandLineTools/usr/include/c++/v1")
  if(EXISTS "${_stale_libcxx}" AND NOT EXISTS "${_stale_libcxx}/cstdio")
    execute_process(COMMAND xcrun --show-sdk-path
                    OUTPUT_VARIABLE _sdk_path OUTPUT_STRIP_TRAILING_WHITESPACE)
    message(STATUS "libc++ incompleto em ${_stale_libcxx}; usando ${_sdk_path}/usr/include/c++/v1")
    target_compile_options(pontifice PRIVATE
      $<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>
      $<$<COMPILE_LANGUAGE:CXX>:-isystem${_sdk_path}/usr/include/c++/v1>)
  endif()
endif()

# Copia assets, sprites e modelos para junto do binário em toda build, mesmo sem mudança no
# código (POST_BUILD só rodaria quando o executável fosse religado).
add_custom_target(copy_assets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/src/assets $<TARGET_FILE_DIR:pontifice>/assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/src/sprites $<TARGET_FILE_DIR:pontifice>/sprites
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/src/models $<TARGET_FILE_DIR:pontifice>/models
  COMMENT "Copiando assets, sprites e models")
add_dependencies(copy_assets pontifice)
