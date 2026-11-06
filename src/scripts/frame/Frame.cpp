#include "scripts/frame/Frame.h"

#include <algorithm>

#include "scripts/colors/Colors.h"

namespace frame {

namespace {

// Nó da quina, medido em art/corner.png (linha de 3 px). Coordenadas locais em px da quina
// inferior esquerda: u cresce para dentro na horizontal, v para dentro na vertical, origem no
// canto externo da borda. Retângulos sem sobreposição (o alpha não pode somar):
//   A rabicho da esquerda   B barra de baixo    C haste central
//   D barra de cima         E rabicho da direita
// As linhas da borda saem da barra de cima (v >= kKnotSize) e do rabicho da direita
// (u >= kKnotSize), com os vãos de 4 px da referência entre eles.
struct Box {
  float u0, v0, u1, v1;
};
constexpr float kLine = 3.0f;
constexpr float kKnotSize = 17.0f;
constexpr Box kKnot[] = {
    {0.0f, 3.0f, 3.0f, 10.0f},    // A
    {0.0f, 0.0f, 7.0f, 3.0f},     // B
    {7.0f, 0.0f, 10.0f, 17.0f},   // C
    {0.0f, 14.0f, 7.0f, 17.0f},   // D
    {14.0f, 0.0f, 17.0f, 10.0f},  // E
};

SDL_FRect Shrink(const SDL_FRect& rect, float by) {
  return SDL_FRect{rect.x + by, rect.y + by, rect.w - 2.0f * by, rect.h - 2.0f * by};
}

void Fill(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color c, float alpha) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, static_cast<Uint8>(c.a * alpha));
  SDL_RenderFillRect(r, &rect);
}

// Anel de `thickness` px rente à borda de `outer`, em quatro faixas (sem sobrepor).
void Ring(SDL_Renderer* r, const SDL_FRect& outer, float thickness, SDL_Color c, float alpha) {
  const SDL_FRect top{outer.x, outer.y, outer.w, thickness};
  const SDL_FRect bottom{outer.x, outer.y + outer.h - thickness, outer.w, thickness};
  const SDL_FRect left{outer.x, outer.y + thickness, thickness, outer.h - 2.0f * thickness};
  const SDL_FRect right{outer.x + outer.w - thickness, outer.y + thickness, thickness,
                        outer.h - 2.0f * thickness};
  Fill(r, top, c, alpha);
  Fill(r, bottom, c, alpha);
  Fill(r, left, c, alpha);
  Fill(r, right, c, alpha);
}

// Nó numa quina: (cx, cy) é o canto externo da borda; (su, sv) apontam para dentro da placa;
// `s` é a escala (1 = linha de 3 px).
void Knot(SDL_Renderer* r, float cx, float cy, float su, float sv, float s, SDL_Color line) {
  for (const Box& b : kKnot) {
    const float xa = cx + su * b.u0 * s, xb = cx + su * b.u1 * s;
    const float ya = cy + sv * b.v0 * s, yb = cy + sv * b.v1 * s;
    Fill(r, SDL_FRect{std::min(xa, xb), std::min(ya, yb), (b.u1 - b.u0) * s, (b.v1 - b.v0) * s},
         line, 1.0f);
  }
}

// Borda com nós: um nó em cada quina e quatro faixas entre eles. `edge` é o retângulo
// externo da borda (rect sem a outline).
void BorderWithKnots(SDL_Renderer* r, const SDL_FRect& edge, float border, SDL_Color line) {
  // Escala: borda de 3 px = 1; menor em placas pequenas para os dois nós de um lado caberem.
  const float s = std::min(border / kLine, std::min(edge.w, edge.h) / (2.0f * kKnotSize));
  const float knot = kKnotSize * s;
  const float thick = kLine * s;
  const float x0 = edge.x, x1 = edge.x + edge.w, y0 = edge.y, y1 = edge.y + edge.h;
  Knot(r, x0, y1, +1.0f, -1.0f, s, line);
  Knot(r, x0, y0, +1.0f, +1.0f, s, line);
  Knot(r, x1, y1, -1.0f, -1.0f, s, line);
  Knot(r, x1, y0, -1.0f, +1.0f, s, line);
  const float span_w = edge.w - 2.0f * knot;
  const float span_h = edge.h - 2.0f * knot;
  if (span_w > 0.0f) {
    Fill(r, SDL_FRect{x0 + knot, y0, span_w, thick}, line, 1.0f);
    Fill(r, SDL_FRect{x0 + knot, y1 - thick, span_w, thick}, line, 1.0f);
  }
  if (span_h > 0.0f) {
    Fill(r, SDL_FRect{x0, y0 + knot, thick, span_h}, line, 1.0f);
    Fill(r, SDL_FRect{x1 - thick, y0 + knot, thick, span_h}, line, 1.0f);
  }
}

}  // namespace

SDL_FRect Inner(const SDL_FRect& rect) { return Shrink(rect, kInset); }

void Draw(SDL_Renderer* r, const SDL_FRect& rect, Kind kind, State state, float alpha) {
  if (rect.w <= 0.0f || rect.h <= 0.0f) return;
  alpha = std::clamp(alpha, 0.0f, 1.0f);
  // Placas menores que duas vezes a margem encolhem outline e borda na proporção.
  const float shrink = std::min(1.0f, std::min(rect.w, rect.h) / (2.0f * kInset));
  const float outline = kOutline * shrink;
  const float border = kBorder * shrink;

  const bool danger = kind == Kind::kButtonDanger;
  const bool pressed = state == State::kPressed;
  const char* fill = danger ? (pressed ? "frame.dangerFillPressed" : "frame.dangerFill")
                            : (pressed ? "frame.fillPressed" : "frame.fill");
  SDL_Color line = colors::Get(state == State::kHighlight ? "frame.borderHighlight" : "frame.border");
  line.a = static_cast<Uint8>(line.a * alpha);

  // Outline, fundo (tudo dentro da outline, inclusive sob a borda) e, por cima, a borda com
  // os nós nas quinas.
  Ring(r, rect, outline, colors::Get("frame.outline"), alpha);
  const SDL_FRect edge = Shrink(rect, outline);
  Fill(r, edge, colors::Get(fill), alpha);
  BorderWithKnots(r, edge, border, line);
}

}  // namespace frame
