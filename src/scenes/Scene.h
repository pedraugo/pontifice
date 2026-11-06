#pragma once

#include <SDL3/SDL.h>

#include <memory>
#include <optional>

class Music;

enum class SceneId { kLanguage, kMenu, kLoading, kBattle };
// Nível do oponente (ver scripts/ai/Ai.h, ai::Difficulty): 0 fácil, 1 médio, 2 difícil,
// 3 impossível. Inteiro aqui para o cabeçalho das cenas não depender da IA.
constexpr int kDefaultDifficulty = 1;

class Scene;

// O que uma cena pode usar em Preload (fora da thread principal): o device SDL_GPU, cujas
// funções de criação de recursos e command buffers são seguras em qualquer thread, e o
// tamanho lógico. O SDL_Renderer fica de fora de propósito: só a thread principal o usa.
struct PreloadInfo {
  SDL_GPUDevice* gpu_device = nullptr;  // nulo se o renderer não é GPU
  int logical_width = 0;
  int logical_height = 0;
  int difficulty = kDefaultDifficulty;
};

// Estado compartilhado entre o loop principal e a cena ativa.
// A cena pede troca de cena ou saída do jogo escrevendo aqui.
struct SceneContext {
  SDL_Renderer* renderer = nullptr;
  Music* music = nullptr;  // trilha sonora: o SceneManager ajusta o volume por cena
  SDL_GPUDevice* gpu_device = nullptr;  // SDL_GetGPURendererDevice(renderer), para Preload
  int logical_width = 0;
  int logical_height = 0;

  int difficulty = kDefaultDifficulty;  // escolhido no menu, lido pela batalha
  PreloadInfo Preload() const {
    return {gpu_device, logical_width, logical_height, difficulty};
  }
  bool quit = false;
  std::optional<SceneId> next_scene;
  // Cena que a tela de loading (SceneId::kLoading) prepara e abre em seguida.
  SceneId loading_target = SceneId::kBattle;
  // Instância já carregada (OnEnter feito) para a troca pedida em next_scene; o SceneManager
  // a usa no lugar de criar uma nova. Preenchida pela tela de loading.
  std::unique_ptr<Scene> preloaded;
  // Se > 0, a próxima cena entra do preto com um fade in desta duração (segundos).
  float fade_in_seconds = 0.0f;
  // Se > 0, a cena atual escurece até o preto por esta duração (segundos) antes da troca
  // pedida em next_scene; enquanto escurece, ela continua animando mas não recebe eventos.
  float fade_out_seconds = 0.0f;
};

class Scene {
 public:
  virtual ~Scene() = default;

  // Parte pesada da carga: ler arquivos, decodificar imagens e modelos e enviá-los à GPU
  // (via info.gpu_device). A tela de loading a chama em outra thread antes de OnEnter; não
  // pode tocar o SDL_Renderer. OnEnter deve funcionar com ou sem Preload antes.
  virtual void Preload(const PreloadInfo& /*info*/) {}
  virtual void OnEnter(SceneContext& /*ctx*/) {}
  virtual void OnExit(SceneContext& /*ctx*/) {}
  virtual void HandleEvent(SceneContext& ctx, const SDL_Event& event) = 0;
  virtual void Update(SceneContext& /*ctx*/, float /*dt_seconds*/) {}
  virtual void Render(SceneContext& ctx) = 0;
};
