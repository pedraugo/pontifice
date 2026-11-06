cmake_minimum_required(VERSION 3.24)
project(pontifice LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependências já baixadas em build/_deps não são atualizadas na reconfiguração, então o
# jogo configura e compila sem internet. Para atualizar, apague build/.
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

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

# Bibliotecas header-only: cgltf (glTF) e stb (stb_image PNG/JPEG, stb_truetype fonte,
# stb_vorbis trilha sonora em Ogg Vorbis).
# libwebp (decodificação dos sprites em WebP) é compilada junto, só a biblioteca, sem ferramentas.
include(FetchContent)
FetchContent_Declare(cgltf
  GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
  GIT_TAG v1.15
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR cmake-none)   # sem add_subdirectory, só o header
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
  foreach(shader card.vert card.frag cardShadow.frag)
    string(REGEX REPLACE ".*\\." "" stage ${shader})
    set(spv ${CMAKE_BINARY_DIR}/shaders/${shader}.spv)
    add_custom_command(
      OUTPUT ${spv}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
      COMMAND ${GLSLC} -fshader-stage=${stage}
              ${CMAKE_SOURCE_DIR}/src/shaders/${shader}.glsl -o ${spv}
      DEPENDS ${CMAKE_SOURCE_DIR}/src/shaders/${shader}.glsl
      COMMENT "Compilando ${shader}.glsl para SPIR-V")
    list(APPEND SPIRV_OUTPUTS ${spv})
  endforeach()
else()
  message(STATUS "glslc nao encontrado: shaders SPIR-V (Vulkan) nao serao compilados")
endif()

add_executable(pontifice
  src/main.cpp
  src/scenes/sceneManager.cpp
  src/scenes/language/languageScene.cpp
  src/scenes/menu/menuScene.cpp
  src/scenes/loading/loadingScene.cpp
  src/scenes/battle/battleScene.cpp
  src/scripts/text/text.cpp
  src/scripts/ui/ui.cpp
  src/scripts/button/button.cpp
  src/scripts/button/buttonText.cpp
  src/scripts/button/buttonIcon.cpp
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
# Windows: ícone do executável como recurso (src/icons/icon.ico, gerado por tools/makeIcons.py).
if(WIN32)
  enable_language(RC)
  target_sources(pontifice PRIVATE src/icons/pontifice.rc)
endif()
target_include_directories(pontifice PRIVATE src ${GENERATED_DIR})
target_include_directories(pontifice SYSTEM PRIVATE ${cgltf_SOURCE_DIR} ${stb_SOURCE_DIR}
                           ${libwebp_SOURCE_DIR}/src)
target_link_libraries(pontifice PRIVATE SDL3::SDL3 webp)
# Decodificação de imagens (libwebp/stb_image), rasterização da fonte (stb_truetype) e composição das
# frentes das cartas rodam na carga da batalha sobre imagens grandes: sem otimização (Debug)
# a carga leva vários segundos. A trilha e os clipes (stb_vorbis) decodificam na thread de áudio ou na carga. Esses
# arquivos são compilados otimizados em qualquer build.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  set_source_files_properties(src/scripts/image/image.cpp src/scripts/text/text.cpp
                              src/scripts/cardface/cardFace.cpp src/scripts/audios/music.cpp
                              src/scripts/audios/clip.cpp
                              PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Debug>:-O2>")
elseif(MSVC)
  set_source_files_properties(src/scripts/image/image.cpp src/scripts/text/text.cpp
                              src/scripts/cardface/cardFace.cpp src/scripts/audios/music.cpp
                              src/scripts/audios/clip.cpp
                              PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Debug>:/O2;/Ob2>")
endif()
target_compile_options(pontifice PRIVATE -Wall -Wextra)

# Testes (ctest --test-dir build): componentes sem SDL, como o monte de cartas.
enable_testing()
add_executable(deckTest tests/deckTest.cpp src/scripts/deck/deck.cpp)
target_include_directories(deckTest PRIVATE src)
target_compile_options(deckTest PRIVATE -Wall -Wextra)
add_test(NAME deckTest COMMAND deckTest)
add_executable(rulesTest tests/rulesTest.cpp src/scripts/rules/rules.cpp src/scripts/ai/ai.cpp)
target_include_directories(rulesTest PRIVATE src)
target_compile_options(rulesTest PRIVATE -Wall -Wextra)
add_test(NAME rulesTest COMMAND rulesTest)

# macOS: alguns Command Line Tools deixam um diretório libc++ antigo e incompleto em
# /Library/Developer/CommandLineTools/usr/include/c++/v1, que esconde o libc++ do SDK.
# Se for o caso, aponta o compilador direto para o libc++ do SDK.
if(APPLE)
  set(_stale_libcxx "/Library/Developer/CommandLineTools/usr/include/c++/v1")
  if(EXISTS "${_stale_libcxx}" AND NOT EXISTS "${_stale_libcxx}/cstdio")
    execute_process(COMMAND xcrun --show-sdk-path
                    OUTPUT_VARIABLE _sdk_path OUTPUT_STRIP_TRAILING_WHITESPACE)
    message(STATUS "libc++ incompleto em ${_stale_libcxx}; usando ${_sdk_path}/usr/include/c++/v1")
    foreach(_target pontifice deckTest rulesTest)
      target_compile_options(${_target} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>
        $<$<COMPILE_LANGUAGE:CXX>:-isystem${_sdk_path}/usr/include/c++/v1>)
    endforeach()
  endif()
endif()

# Copia assets, sprites, modelos e sons para junto do binário em toda build, mesmo sem mudança no
# código (POST_BUILD só rodaria quando o executável fosse religado).
add_custom_target(copy_assets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/src/assets $<TARGET_FILE_DIR:pontifice>/assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/src/sprites $<TARGET_FILE_DIR:pontifice>/sprites
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/src/models $<TARGET_FILE_DIR:pontifice>/models
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/src/audios $<TARGET_FILE_DIR:pontifice>/audios
  COMMENT "Copiando assets, sprites, models e audios")
add_dependencies(copy_assets pontifice)
