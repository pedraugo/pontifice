#pragma once

#include <SDL3/SDL.h>

#include <string>

#include "scripts/button/ButtonAction.h"

// Janela modal de confirmação (docs/batalha.md, "Pausa"): véu escuro sobre a tela, placa de
// janela centralizada com a pergunta e dois botões, [Não] à esquerda e [Sim] à direita.
// Enquanto aberta, a cena repassa todos os eventos a HandleEvent e não processa mais nada.
// Teclado: setas esquerda/direita (ou Tab) trocam o foco, Enter/Espaço confirma o botão
// focado, Esc responde Não. O foco começa no Não (a resposta segura).
class ConfirmDialog {
 public:
  enum class Result { kNone, kNo, kYes };

  ConfirmDialog();

  // Abre com os textos já traduzidos e calcula o layout para a tela width x height.
  // yes_danger: o Sim confirma uma ação destrutiva (ex.: abandonar a batalha) e usa a placa
  // vermelha (frame::Kind::kButtonDanger).
  void Open(const std::string& question, const std::string& no_label,
            const std::string& yes_label, int width, int height, bool yes_danger = false);
  void Close() { open_ = false; }
  bool IsOpen() const { return open_; }

  // Resposta do evento (kNone se ainda não respondeu). Não fecha sozinha: a cena decide.
  Result HandleEvent(const SDL_Event& event);
  void Render(SDL_Renderer* r, int width, int height) const;

 private:
  void SetFocus(int index);

  std::string question_;
  ButtonAction no_;
  ButtonAction yes_;
  SDL_FRect panel_{};
  int focused_ = 0;  // 0 = Não, 1 = Sim
  bool open_ = false;
};
