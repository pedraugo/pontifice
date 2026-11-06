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

# Bibliotecas header-only: cgltf (glTF), stb_image (PNG/JPEG) e dr_libs (dr_mp3, trilha sonora).
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
FetchContent_Declare(dr_libs
  GIT_REPOSITORY https://github.com/mackron/dr_libs.git
  GIT_TAG master
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR cmake-none)
FetchContent_MakeAvailable(cgltf stb dr_libs)

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
  foreach(shader card.vert card.frag card_shadow.frag)
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
  src/scenes/SceneManager.cpp
  src/scenes/language/LanguageScene.cpp
  src/scenes/menu/MenuScene.cpp
  src/scenes/loading/LoadingScene.cpp
  src/scenes/battle/BattleScene.cpp
  src/scripts/text/Text.cpp
  src/scripts/button/Button.cpp
  src/scripts/button/ButtonText.cpp
  src/scripts/button/ButtonIcon.cpp
  src/scripts/dialog/ConfirmDialog.cpp
  src/scripts/board/Board.cpp
  src/scripts/sword/Sword.cpp
  src/scripts/shield/Shield.cpp
  src/scripts/frame/Frame.cpp
  src/scripts/draw/Draw.cpp
  src/scripts/rules/Rules.cpp
  src/scripts/ai/Ai.cpp
  src/scripts/image/Image.cpp
  src/scripts/gpu/Gpu.cpp
  src/scripts/model/Model.cpp
  src/scripts/hand/Hand.cpp
  src/scripts/aura/CardAura.cpp
  src/scripts/hud/Hud.cpp
  src/scripts/json/Json.cpp
  src/scripts/config/Config.cpp
  src/scripts/i18n/I18n.cpp
  src/scripts/flag/Flag.cpp
  src/scripts/colors/Colors.cpp
  src/scripts/torch/Torch.cpp
  src/scripts/deck/Deck.cpp
  src/scripts/cards/Cards.cpp
  src/scripts/cardface/CardFace.cpp
  src/scripts/music/Music.cpp
  ${METAL_INC}
  ${SPIRV_OUTPUTS}
)
# Windows: ícone do executável como recurso (src/icons/icon.ico, gerado por tools/make_icons.py).
if(WIN32)
  enable_language(RC)
  target_sources(pontifice PRIVATE src/icons/pontifice.rc)
endif()
target_include_directories(pontifice PRIVATE src ${GENERATED_DIR})
target_include_directories(pontifice SYSTEM PRIVATE ${cgltf_SOURCE_DIR} ${stb_SOURCE_DIR}
                           ${dr_libs_SOURCE_DIR})
target_link_libraries(pontifice PRIVATE SDL3::SDL3)
# Decodificação de PNG (stb_image), rasterização da fonte (stb_truetype) e composição das
# frentes das cartas rodam na carga da batalha sobre imagens grandes: sem otimização (Debug)
# a carga leva vários segundos. Esses arquivos são compilados otimizados em qualquer build.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  set_source_files_properties(src/scripts/image/Image.cpp src/scripts/text/Text.cpp
                              src/scripts/cardface/CardFace.cpp
                              PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Debug>:-O2>")
elseif(MSVC)
  set_source_files_properties(src/scripts/image/Image.cpp src/scripts/text/Text.cpp
                              src/scripts/cardface/CardFace.cpp
                              PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Debug>:/O2;/Ob2>")
endif()
target_compile_options(pontifice PRIVATE -Wall -Wextra)

# Testes (ctest --test-dir build): componentes sem SDL, como o monte de cartas.
enable_testing()
add_executable(deck_test tests/deck_test.cpp src/scripts/deck/Deck.cpp)
target_include_directories(deck_test PRIVATE src)
target_compile_options(deck_test PRIVATE -Wall -Wextra)
add_test(NAME deck_test COMMAND deck_test)
add_executable(rules_test tests/rules_test.cpp src/scripts/rules/Rules.cpp src/scripts/ai/Ai.cpp)
target_include_directories(rules_test PRIVATE src)
target_compile_options(rules_test PRIVATE -Wall -Wextra)
add_test(NAME rules_test COMMAND rules_test)

# macOS: alguns Command Line Tools deixam um diretório libc++ antigo e incompleto em
# /Library/Developer/CommandLineTools/usr/include/c++/v1, que esconde o libc++ do SDK.
# Se for o caso, aponta o compilador direto para o libc++ do SDK.
if(APPLE)
  set(_stale_libcxx "/Library/Developer/CommandLineTools/usr/include/c++/v1")
  if(EXISTS "${_stale_libcxx}" AND NOT EXISTS "${_stale_libcxx}/cstdio")
    execute_process(COMMAND xcrun --show-sdk-path
                    OUTPUT_VARIABLE _sdk_path OUTPUT_STRIP_TRAILING_WHITESPACE)
    message(STATUS "libc++ incompleto em ${_stale_libcxx}; usando ${_sdk_path}/usr/include/c++/v1")
    foreach(_target pontifice deck_test rules_test)
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
          ${CMAKE_SOURCE_DIR}/src/sounds $<TARGET_FILE_DIR:pontifice>/sounds
  COMMENT "Copiando assets, sprites, models e sounds")
add_dependencies(copy_assets pontifice)
