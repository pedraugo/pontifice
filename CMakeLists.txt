cmake_minimum_required(VERSION 3.25)

# Oldest macOS the release build runs on; set before project() so dependencies inherit it.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(pontifice VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# SPIR-V shaders feed the Vulkan backend (Windows, Linux); macOS embeds the Metal shader instead.
if(APPLE)
  set(_spirv_default OFF)
else()
  set(_spirv_default ON)
endif()
option(PONTIFICE_SPIRV "Compile the GLSL shaders to SPIR-V for Vulkan" ${_spirv_default})

# MSVC refuses /O2 next to /RTC1, and some files below are optimized even in Debug.
if(MSVC)
  foreach(_lang C CXX)
    string(REPLACE "/RTC1" "" CMAKE_${_lang}_FLAGS_DEBUG "${CMAKE_${_lang}_FLAGS_DEBUG}")
  endforeach()
endif()

# macOS: some Command Line Tools ship a stale, incomplete libc++ that hides the SDK one.
if(APPLE)
  set(_stale_libcxx "/Library/Developer/CommandLineTools/usr/include/c++/v1")
  if(EXISTS "${_stale_libcxx}" AND NOT EXISTS "${_stale_libcxx}/cstdio")
    execute_process(COMMAND xcrun --show-sdk-path
                    OUTPUT_VARIABLE _sdk_path OUTPUT_STRIP_TRAILING_WHITESPACE)
    message(STATUS "Incomplete libc++ in ${_stale_libcxx}; using ${_sdk_path}/usr/include/c++/v1")
    string(APPEND CMAKE_CXX_FLAGS " -nostdinc++ -isystem ${_sdk_path}/usr/include/c++/v1")
  endif()
endif()

# Downloaded dependencies stay as they are on reconfigure, so builds work offline.
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

# Reuse dependency sources another build directory under build/ already downloaded.
file(GLOB _shared_sources LIST_DIRECTORIES true
     "${CMAKE_SOURCE_DIR}/build/_deps/*-src" "${CMAKE_SOURCE_DIR}/build/*/_deps/*-src")
foreach(_dir IN LISTS _shared_sources)
  cmake_path(IS_PREFIX CMAKE_BINARY_DIR "${_dir}" _own)
  get_filename_component(_name "${_dir}" NAME)
  string(REGEX REPLACE "-src$" "" _name "${_name}")
  string(TOUPPER "${_name}" _name)
  if(NOT _own AND NOT DEFINED FETCHCONTENT_SOURCE_DIR_${_name} AND EXISTS "${_dir}/.git")
    set(FETCHCONTENT_SOURCE_DIR_${_name} "${_dir}")
  endif()
endforeach()

include(FetchContent)

# SDL3: the system package when installed, otherwise downloaded and linked statically.
find_package(SDL3 CONFIG QUIET)
if(NOT SDL3_FOUND)
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

# Header-only cgltf and stb (images, font, Ogg Vorbis); libwebp built as a library only.
FetchContent_Declare(cgltf
  GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
  GIT_TAG v1.15
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR cmake-none)
FetchContent_Declare(stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG d14d46f6b44456eb88beac58cba833a4135d4c89
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR cmake-none)
FetchContent_MakeAvailable(cgltf stb)
foreach(opt ANIM_UTILS CWEBP DWEBP GIF2WEBP IMG2WEBP VWEBP WEBPINFO LIBWEBPMUX WEBPMUX EXTRAS)
  set(WEBP_BUILD_${opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(WEBP_LINK_STATIC ON CACHE BOOL "" FORCE)
FetchContent_Declare(libwebp
  GIT_REPOSITORY https://github.com/webmproject/libwebp.git
  GIT_TAG v1.6.0
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(libwebp)

find_package(Threads REQUIRED)

# Metal shader embedded in the binary as a byte array.
set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
set(METAL_INC ${GENERATED_DIR}/scripts/gpu/card_metal.inc)
add_custom_command(
  OUTPUT ${METAL_INC}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_DIR}/scripts/gpu
  COMMAND ${CMAKE_COMMAND} -DIN=${CMAKE_SOURCE_DIR}/src/shaders/card.metal -DOUT=${METAL_INC}
          -P ${CMAKE_SOURCE_DIR}/cmake/embed.cmake
  DEPENDS ${CMAKE_SOURCE_DIR}/src/shaders/card.metal ${CMAKE_SOURCE_DIR}/cmake/embed.cmake
  COMMENT "Embedding card.metal")

# SPIR-V: glslc or glslangValidator when installed, otherwise glslang built from source.
set(SPIRV_DIR ${GENERATED_DIR}/shaders)
set(SPIRV_OUTPUTS)
if(PONTIFICE_SPIRV)
  find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/bin" "$ENV{VULKAN_SDK}/Bin")
  find_program(GLSLANG NAMES glslangValidator glslang HINTS "$ENV{VULKAN_SDK}/bin"
               "$ENV{VULKAN_SDK}/Bin")
  if(GLSLC)
    set(_spirv_tool ${GLSLC})
    set(_spirv_depends)
  elseif(GLSLANG)
    set(_spirv_tool ${GLSLANG})
    set(_spirv_depends)
  else()
    message(STATUS "glslc not found: building glslang to compile the SPIR-V shaders")
    foreach(_opt ENABLE_OPT ENABLE_HLSL ENABLE_SPVREMAPPER ENABLE_CTEST GLSLANG_TESTS
                 GLSLANG_ENABLE_INSTALL BUILD_EXTERNAL)
      set(${_opt} OFF CACHE BOOL "" FORCE)
    endforeach()
    set(ENABLE_GLSLANG_BINARIES ON CACHE BOOL "" FORCE)
    FetchContent_Declare(glslang
      GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
      GIT_TAG 16.0.0
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(glslang)
    set(_spirv_tool $<TARGET_FILE:glslang-standalone>)
    set(_spirv_depends glslang-standalone)
  endif()
  foreach(shader card.vert card.frag cardShadow.frag)
    string(REGEX REPLACE ".*\\." "" stage ${shader})
    set(spv ${SPIRV_DIR}/${shader}.spv)
    set(_src ${CMAKE_SOURCE_DIR}/src/shaders/${shader}.glsl)
    if(GLSLC)
      set(_args -fshader-stage=${stage} ${_src} -o ${spv})
    else()
      set(_args -V -S ${stage} -o ${spv} ${_src})
    endif()
    add_custom_command(
      OUTPUT ${spv}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${SPIRV_DIR}
      COMMAND ${_spirv_tool} ${_args}
      DEPENDS ${_src} ${_spirv_depends}
      COMMENT "Compiling ${shader}.glsl to SPIR-V")
    list(APPEND SPIRV_OUTPUTS ${spv})
  endforeach()
endif()

# Warnings and source encoding per compiler; MSVC reads the UTF-8 sources as UTF-8.
function(pontifice_compile_options target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W3 /utf-8)
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra)
  endif()
endfunction()

add_executable(pontifice
  src/main.cpp
  src/scenes/sceneManager.cpp
  src/scenes/language/languageScene.cpp
  src/scenes/menu/menuScene.cpp
  src/scenes/loading/loadingScene.cpp
  src/scenes/battle/battleScene.cpp
  src/scenes/grimoire/grimoireScene.cpp
  src/scripts/text/text.cpp
  src/scripts/ui/ui.cpp
  src/scripts/layout/layout.cpp
  src/scripts/button/button.cpp
  src/scripts/button/buttonText.cpp
  src/scripts/button/buttonIcon.cpp
  src/scripts/input/textInput.cpp
  src/scripts/save/save.cpp
  src/scripts/vignette/vignette.cpp
  src/scripts/dialog/confirmDialog.cpp
  src/scripts/board/board.cpp
  src/scripts/sword/sword.cpp
  src/scripts/shield/shield.cpp
  src/scripts/frame/frame.cpp
  src/scripts/draw/draw.cpp
  src/scripts/rules/rules.cpp
  src/scripts/ai/ai.cpp
  src/scripts/image/image.cpp
  src/scripts/gpu/gpu.cpp
  src/scripts/model/model.cpp
  src/scripts/hand/hand.cpp
  src/scripts/aura/cardAura.cpp
  src/scripts/hud/hud.cpp
  src/scripts/json/json.cpp
  src/scripts/log/log.cpp
  src/scripts/config/config.cpp
  src/scripts/i18n/i18n.cpp
  src/scripts/flag/flag.cpp
  src/scripts/colors/colors.cpp
  src/scripts/opacity/opacity.cpp
  src/scripts/torch/torch.cpp
  src/scripts/deck/deck.cpp
  src/scripts/cards/cards.cpp
  src/scripts/cardface/cardFace.cpp
  src/scripts/audios/music.cpp
  src/scripts/audios/clip.cpp
  src/scripts/audios/sfx.cpp
  src/scripts/audios/voices.cpp
  ${METAL_INC}
  ${SPIRV_OUTPUTS}
)
target_include_directories(pontifice PRIVATE src ${GENERATED_DIR})
target_include_directories(pontifice SYSTEM PRIVATE ${cgltf_SOURCE_DIR} ${stb_SOURCE_DIR}
                           ${libwebp_SOURCE_DIR}/src)
target_link_libraries(pontifice PRIVATE SDL3::SDL3 webp Threads::Threads)
pontifice_compile_options(pontifice)

# Windows: executable icon resource, and no console window outside Debug.
if(WIN32)
  enable_language(RC)
  target_sources(pontifice PRIVATE src/icons/pontifice.rc)
  set_target_properties(pontifice PROPERTIES
    WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>
    VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:pontifice>)
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT pontifice)
endif()

# Image, font, card face and audio decoding stay optimized in Debug, or loading takes seconds.
set(_hot_sources src/scripts/image/image.cpp src/scripts/text/text.cpp
    src/scripts/cardface/cardFace.cpp src/scripts/audios/music.cpp src/scripts/audios/clip.cpp)
if(MSVC)
  set_source_files_properties(${_hot_sources} PROPERTIES
                              COMPILE_OPTIONS "$<$<CONFIG:Debug>:/O2;/Ob2>")
else()
  set_source_files_properties(${_hot_sources} PROPERTIES
                              COMPILE_OPTIONS "$<$<CONFIG:Debug>:-O2>")
endif()

# Tests (ctest): game components without a window.
enable_testing()
add_executable(deckTest tests/deckTest.cpp src/scripts/deck/deck.cpp src/scripts/log/log.cpp)
target_include_directories(deckTest PRIVATE src)
target_link_libraries(deckTest PRIVATE SDL3::SDL3)
pontifice_compile_options(deckTest)
add_test(NAME deckTest COMMAND deckTest)
add_executable(rulesTest tests/rulesTest.cpp src/scripts/rules/rules.cpp src/scripts/ai/ai.cpp
               src/scripts/log/log.cpp)
target_include_directories(rulesTest PRIVATE src)
target_link_libraries(rulesTest PRIVATE SDL3::SDL3)
pontifice_compile_options(rulesTest)
add_test(NAME rulesTest COMMAND rulesTest)
add_executable(saveTest tests/saveTest.cpp src/scripts/save/save.cpp src/scripts/cards/cards.cpp
               src/scripts/rules/rules.cpp src/scripts/deck/deck.cpp src/scripts/i18n/i18n.cpp
               src/scripts/json/json.cpp src/scripts/log/log.cpp)
target_include_directories(saveTest PRIVATE src)
target_link_libraries(saveTest PRIVATE SDL3::SDL3)
pontifice_compile_options(saveTest)
add_test(NAME saveTest COMMAND saveTest)

# Game data next to the binary on every build; icons/ only needs icon.png at runtime.
set(_data_dirs assets fonts sprites models audios)
set(_copy_commands)
foreach(_dir IN LISTS _data_dirs)
  list(APPEND _copy_commands COMMAND ${CMAKE_COMMAND} -E copy_directory
       ${CMAKE_SOURCE_DIR}/src/${_dir} $<TARGET_FILE_DIR:pontifice>/${_dir})
endforeach()
if(PONTIFICE_SPIRV)
  list(APPEND _copy_commands COMMAND ${CMAKE_COMMAND} -E copy_directory
       ${SPIRV_DIR} $<TARGET_FILE_DIR:pontifice>/shaders)
endif()
add_custom_target(copy_assets ALL
  ${_copy_commands}
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:pontifice>/icons
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/src/icons/icon.png $<TARGET_FILE_DIR:pontifice>/icons/
  COMMENT "Copying game data next to the binary")
add_dependencies(copy_assets pontifice)

# Distributable folder (cmake --install, cpack): binary and data side by side, game files only.
install(TARGETS pontifice RUNTIME DESTINATION . COMPONENT game)
foreach(_dir IN LISTS _data_dirs)
  install(DIRECTORY src/${_dir} DESTINATION . COMPONENT game)
endforeach()
install(FILES src/icons/icon.png DESTINATION icons COMPONENT game)
if(PONTIFICE_SPIRV)
  install(FILES ${SPIRV_OUTPUTS} DESTINATION shaders COMPONENT game)
endif()

if(APPLE)
  set(_os macos)
elseif(WIN32)
  set(_os windows)
else()
  set(_os linux)
endif()
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _arch)
set(CPACK_PACKAGE_NAME "PONTIFICE")
set(CPACK_PACKAGE_VENDOR "PONTIFICE")
set(CPACK_PACKAGE_FILE_NAME "pontifice-${PROJECT_VERSION}-${_os}-${_arch}")
set(CPACK_GENERATOR ZIP)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL game)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_PACKAGE_DIRECTORY ${CMAKE_BINARY_DIR}/package)
include(CPack)
