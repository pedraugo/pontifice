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

add_executable(pontifice
  src/main.cpp
  src/scenes/SceneManager.cpp
  src/scenes/MenuScene.cpp
  src/scenes/BattleScene.cpp
  src/scripts/Text.cpp
  src/scripts/Button.cpp
  src/scripts/ButtonPlay.cpp
  src/scripts/ButtonExit.cpp
)
target_include_directories(pontifice PRIVATE src)
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

# Copia assets e sprites para junto do binário.
add_custom_command(
  TARGET pontifice POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/src/assets $<TARGET_FILE_DIR:pontifice>/assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/src/sprites $<TARGET_FILE_DIR:pontifice>/sprites
)
