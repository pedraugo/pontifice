# Tela de loading

Entre o menu e a batalha há uma tela de loading no estilo de *Resident Evil Survivor*: fundo todo preto e um único objeto 3D em ação. No PONTIFICE o objeto é um **portão medieval** (parede de pedra com arco duplo e porta de tábuas com ferragens, inspirado em `src/art/Door.jpg`) que se **abre para dentro**, revelando o escuro, com névoa rasteira no chão.

## Linha do tempo

| Instante | O que acontece |
|---|---|
| 0,0 s | Tela preta. A próxima cena é criada e seu `Preload` (leitura, decodificação e envio à GPU de sprites e modelos) começa em outra thread; o portão anima sem travar |
| quando o `Preload` termina | `OnEnter` da cena alvo na thread principal: só o que depende do `SDL_Renderer` (textura do HUD), instantâneo |
| 0,0 a 0,5 s | Fade in do preto para o portão fechado |
| 0,6 a 3,4 s | Porta gira 100° em torno da dobradiça esquerda, com aceleração e desaceleração suaves; a câmera se aproxima devagar |
| 3,4 a 3,9 s | Pausa com a porta aberta |
| 3,9 a 4,6 s | Fade out para o preto |
| 4,6 s | Troca para a cena carregada, que entra com **fade in** de 1 s |

A troca só acontece quando a animação terminou **e** a carga acabou. Se a carga demorar mais que a animação, a porta fica aberta esperando.

## Elementos

- **Portão** (`models/Door.glb`, gerado por `tools/make_door.py`): dois nós. `Wall` (parede, aduelas, arco recuado e passagem) fica parado; `Door` (folha com bandas rebitadas, guarnição, dobradiças e argola) é desenhada com a matriz da dobradiça. A madeira e o ferro vêm do atlas da arena para manter a mesma direção de arte; a pedra é procedural.
- **Luz**: ambiente frio fraco, luz principal e pontual quentes (tocha) à frente e à direita, névoa por distância preta (a passagem some no escuro) e vinheta forte.
- **Névoa de chão**: densa e espessa. Sete camadas `kMist` de y = 0,02 a 1,30, cor `background.loadingMist`. De baixo para cima cada camada perde opacidade (0,95 → 0,20), densidade (`Atmosphere::mist_density` 0,20 → 0,02: manto contínuo embaixo, fiapos em cima) e alcance (`mist_fade_*` de 4 a 12 unidades embaixo até 1,6 a 4,6 em cima), formando um volume largo na base e estreito no topo. A névoa por distância preta apaga o resto antes do horizonte.
- **Texto**: `Carregando` com pontos que avançam, no canto inferior direito, cor `text.muted`.
- **Fundo**: `background.loading` (preto).

## Implementação

- `LoadingScene` lê `ctx.loading_target` (quem pediu a troca define a cena alvo; o botão Jogar define a batalha), cria a cena com `SceneManager::Create`, roda `Scene::Preload` dela em outra thread (`std::async`) e, quando termina, chama `OnEnter` na thread principal.
- `Scene::Preload(const PreloadInfo&)` é a parte pesada da carga: arquivos, PNG, glTF e criação de recursos de GPU pelo `SDL_GPUDevice` (criação de recursos e command buffers do SDL_GPU são seguros em qualquer thread). Não toca o `SDL_Renderer`, que é só da thread principal. `OnEnter` precisa funcionar também sem `Preload` antes (entrada direta com `PONTIFICE_SCENE=battle`).
- Para isso `Gpu` separa `InitDevice` (pipeline e recursos, qualquer thread) de `SetRenderer` + textura alvo (criada no primeiro `BeginFrame`, thread principal), e `Model` separa `Parse` (CPU) de `Upload` (GPU).
- No fim escreve em `ctx.preloaded` a instância pronta, `ctx.fade_in_seconds = 1` e `ctx.next_scene`. O `SceneManager` usa a instância sem chamar `OnEnter` de novo e desenha o véu preto do fade in sobre a cena nova.
- Depuração: `PONTIFICE_SCENE=loading` abre direto nela; `PONTIFICE_LOADING_TIME=s` começa a linha do tempo em `s` segundos (útil com `PONTIFICE_SCREENSHOT`).
