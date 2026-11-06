#pragma once

#include <SDL3/SDL.h>

#include <functional>
#include <string>

struct SceneContext;

// Botão com placa da interface (scripts/frame/Frame.*: fundo escuro, borda clara e outline) e
// rótulo centrado; ao clicar, chama a ação da cena (ex.: "Passar turno", Não/Sim do diálogo).
// Categorias: normal e perigo (SetDanger: fundo vermelho, para Sair e confirmar abandono).
// Os outros dois tipos derivam dele: ButtonText (só texto, sem placa: menu e idiomas) e
// ButtonIcon (placa com ícone: som e dica). Estados (hover, foco, pressionado) em docs/menu.md.
class Button {
 public:
  using Action = std::function<void(SceneContext&)>;
  explicit Button(std::string label, Action action = nullptr);
  virtual ~Button() = default;

  void SetRect(const SDL_FRect& rect) { rect_ = rect; }
  void SetLabel(std::string label) { label_ = std::move(label); }
  void SetFocused(bool focused) { focused_ = focused; }
  // Ação destrutiva (Sair, confirmar abandono): placa de fundo vermelho escuro
  // (frame::Kind::kButtonDanger).
  void SetDanger(bool danger) { danger_ = danger; }
  // Largura do rótulo na escala do botão (para dimensionar botões só de texto).
  float LabelWidth() const;

  // Retorna true quando um clique completo (pressionar e soltar dentro) acontece.
  bool HandleEvent(const SDL_Event& event);
  // Placa e, por cima, RenderContent. Subclasses sem placa (ButtonText) sobrescrevem e
  // desenham só o conteúdo.
  virtual void Render(SDL_Renderer* r) const;

  // Chama a ação (se houver). A cena chama depois de HandleEvent ou pelo teclado.
  virtual void OnClick(SceneContext& ctx);

  bool Contains(float x, float y) const;
  const SDL_FRect& Rect() const { return rect_; }

 protected:
  // Conteúdo sobre a placa: por padrão o rótulo centralizado. Subclasses podem desenhar
  // algo a mais.
  virtual void RenderContent(SDL_Renderer* r) const;
  const std::string& Label() const { return label_; }
  bool Highlighted() const { return hovered_ || focused_ || pressed_; }
  static constexpr float LabelScale() { return 3.0f; }

 private:
  std::string label_;
  Action action_;
  SDL_FRect rect_{};
  bool hovered_ = false;
  bool pressed_ = false;
  bool focused_ = false;
  bool danger_ = false;
};
