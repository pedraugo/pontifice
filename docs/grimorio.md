# Grimório

Galeria com todas as cartas do jogo, aberta pelo item **Grimório** do menu (docs/menu.md). Passa pela tela de loading (docs/loading.md), que compõe as frentes das cartas enquanto a tocha cintila, e volta ao menu com **Sair** ou `Esc`.

## Layout

```
+------------------------------------------------------------+
|                         Grimório                           |
|                                                            |
|        [Benévola]  [Maligna]  [Neutra]  [Campo]            |
|                                                            |
|    +------+  +------+  +------+  +------+                  |
|    | card |  | card |  | card |  | card |                  |
|    +------+  +------+  +------+  +------+                  |
|    +------+  +------+  +------+  +------+                  |
|    | card |  | card |  | card |  | card |                  |
|    +------+  +------+  +------+  +------+                  |
|                                                            |
|  [Voltar]           Página 1 de 9           [Avançar]      |
|                                                            |
|                                                   [Sair]   |
+------------------------------------------------------------+
```

| Elemento | Conteúdo |
|---|---|
| Título | `Grimório`, fonte de título, centrado em y = 4 células |
| Filtros | Quatro botões com placa, um por categoria (`category.*`), centrados em y = 8; **vários podem ficar selecionados**; todos começam selecionados. Selecionado = destaque de foco (borda clara e traço fino); desmarcado fica com 75% de opacidade; clique alterna |
| Grade | 4 colunas x 2 linhas de frentes de carta (a mesma arte composta da batalha, `cardface::Compose`, em meia resolução), centradas entre y = 13 e y = 54, 1 célula de vão. As cartas são **modelos 3D** (`Hand::DrawLoose`, com a luz da mão da batalha) posicionados no plano de inspeção da câmera da mão para cobrir exatamente o retângulo 2D (`PoseForRect`); cartas de **hierarquia 1 têm o efeito foil** (aura e reflexo do shader), como na batalha. Sob o mouse a carta cresce 6%, fica um pouco mais perto da câmera, por cima das vizinhas, e **inclina** na direção do cursor (até 0,3 rad nas quinas, `Hand::Layout::tilt_max`, suavizado com `tilt_speed`), como na mão da batalha; ao tirar o mouse, crescimento, aproximação e inclinação voltam suavemente ao repouso (`anim_speed`), e a carta continua por cima das vizinhas até assentar. Sem GPU 3D a grade cai para texturas 2D sem foil |
| Voltar / Avançar | Nos cantos da grade, em y = 56; `Página x de y` entre eles; na primeira/última página o botão correspondente fica com 75% de opacidade. Setas `←` `→` fazem o mesmo. A troca é animada (`slideSeconds`, 0,8 s, smoothstep): ao avançar a página atual sai pela esquerda sumindo (opacidade 1 → 0) com a grade inteira encolhendo até 75% em torno do seu centro (`slideShrink` 0,25, as cartas se aproximam entre si) e a nova entra pela direita aparecendo (0 → 1) com a grade crescendo de 75% a 100%; ao voltar, o contrário. Durante o deslize não há hover nem nova troca. Sem categoria marcada, a grade mostra `Nenhuma categoria selecionada` |
| Sair | Canto inferior direito; volta ao menu com fade. `Esc` idem |
| Inspecionar | **Duplo clique** numa carta: a tela escurece e a carta **em 3D** (o mesmo `Hand` da batalha, com uma só carta, câmera em z = 10, fov 35°) cresce do lugar da carta na grade até o centro (`Hand::SetInspectOrigin` projeta o retângulo 2D no plano de inspeção da câmera da mão) e volta para lá ao fechar, com os mesmos gestos da batalha (docs/batalha.md): arrastar horizontal **gira em Y** (mostra o verso), vertical inclina um pouco, **clique na carta** ou Espaço **vira**, roda do mouse dá zoom (75% a 100%). Cartas de hierarquia 1 têm a **aura foil**. A frente começa em meia resolução e é trocada pela de resolução cheia composta em outra thread. `Esc`, botão direito ou clique fora da carta a devolvem animada ao seu lugar na grade; enquanto inspeciona, nada mais reage. Sem GPU 3D o duplo clique não faz nada |

Ordem das cartas: a de `cards::LoadAll` (benévolas, malignas, neutras, campo), filtrada pelas categorias marcadas; a página é mantida ao filtrar, limitada ao total.

## Medidas em JSON

- `ui.json` `grimoire`: `titleCenterY`, `filterY`, `filterGap`, `gridTop`, `gridBottom`, `cardGap`, `navY`, `margin` (laterais da grade), `exitMargin`, `hoverGrow` (fração), `slideSeconds`, `slideShrink`.
- `button.json` `grimoire`: `filterHeight`, `filterPadding` (dos lados do rótulo), `navWidth`, `navHeight`, `exitWidth`, `exitHeight`.
- `text.json` `grimoire`: `title`, `page`.

## Implementação

- Cena: `src/scenes/grimoire/grimoireScene.*`; `SceneId::kGrimoire`; `PONTIFICE_SCENE=grimoire` abre direto nela.
- `Preload` (na thread da tela de loading) carrega `cards::LoadAll`, as quatro molduras e compõe as 65 frentes em paralelo (até 8 threads, ~2,5 s), guardadas como `Image` em meia resolução (400x557). As texturas SDL são criadas na primeira vez que a carta aparece e liberadas em `OnExit`. Para a grade 3D e a inspeção cria um `Gpu` próprio, o modelo `card.glb`, a aura, o verso e os normal maps (`cardface::NormalMap`, compartilhado com a batalha). As frentes viram texturas de GPU na primeira vez que aparecem (liberadas em `OnExit`); a frente em resolução cheia da carta inspecionada vive só enquanto ela está na tela. Um único passe 3D por quadro desenha as páginas, depois (`NewLayer`) o véu escuro e a carta inspecionada, composto por cima do título, filtros e botões.
- Textos em `assets/translate.json`: `menu.grimoire`, `grimoire.title`, `grimoire.back`, `grimoire.forward`, `grimoire.exit`, `grimoire.page`, `grimoire.empty`; filtros usam `category.good/evil/neutral/field`.
