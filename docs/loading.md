# Tela de loading

Entre o menu e a batalha há uma tela de loading: fundo liso (`#091119`, o mesmo do menu) e uma única **tocha 3D** (`models/torch.glb`) cintilando no canto inferior direito, acima do texto `Carregando...`. Enquanto ela queima, a batalha é carregada em outra thread.

```
+------------------------------------------------------------+
|                                                            |
|                                                            |
|                                                            |
|                                                            |
|                                                            |
|                                                            |
|                                                  (chama)   |
|                                                    |       |
|                                              Carregando... |
+------------------------------------------------------------+
```

## Linha do tempo

| Instante | O que acontece |
|---|---|
| antes | No menu, "Jogar" faz o menu **escurecer** (fade out de 0,5 s) antes da troca |
| 0,0 s | Tela preta. A próxima cena é criada e seu `Preload` (leitura, decodificação e envio à GPU de sprites e modelos) começa em outra thread; a tocha anima sem travar |
| quando o `Preload` termina | `OnEnter` da cena alvo na thread principal: só o que depende do `SDL_Renderer` (textura do HUD), instantâneo |
| 0,0 a 0,5 s | **Fade in** do preto |
| até 2,0 s | Tempo mínimo na tela, mesmo com carga rápida, para não piscar |
| carga pronta e ≥ 2,0 s | **Fade out** de 0,7 s para o preto (a tocha segue cintilando) |
| fim do fade out | Troca para a cena carregada, que entra com **fade in** de 1 s |

Se a carga demorar mais que o mínimo, a tocha fica queimando esperando. Os véus (fade in/out) são do `SceneManager`: a cena que pede a troca escreve `fade_out_seconds` (escurece a cena atual antes de sair; sem eventos nesse tempo) e `fade_in_seconds` (a nova cena entra do preto).

## Elementos

- **Tocha** (`models/torch.glb`, gerada por `tools/makeTorch.py`): corpo iluminado e chama procedural no shader, que tremula em tamanho e encara a câmera. Câmera fixa (fov 35°, a 11,4 unidades) olhando para a origem; a tocha fica em y = −2,8 (canto inferior, ocupando ~20% da altura da tela) e seu x é calculado para ficar **centralizada com o texto** `Carregando...` (centro do texto convertido para o plano z = 0).
- **Luz**: só a chama (luz pontual, cor de `light.json` `torch.color`, intensidade `torch.loading.intensity`), **cintilando** (soma de senos fora de fase, ±20%); sem luz direcional, ambiente mínimo (`loading.ambient`), sem névoa, vinheta ou sombra.
- **Texto**: `Carregando` com pontos que avançam, no canto inferior direito, cor `slate`.
- **Fundo**: `stoneDarker` (`#091119`, a mesma cor do menu e da cena de idioma).

## Implementação

- `LoadingScene` lê `ctx.loading_target` (quem pediu a troca define a cena alvo; o botão Jogar define a batalha), cria a cena com `SceneManager::Create`, roda `Scene::Preload` dela em outra thread (`std::async`) e, quando termina, chama `OnEnter` na thread principal.
- `Scene::Preload(const PreloadInfo&)` é a parte pesada da carga: arquivos, PNG, glTF e criação de recursos de GPU pelo `SDL_GPUDevice` (criação de recursos e command buffers do SDL_GPU são seguros em qualquer thread). Não toca o `SDL_Renderer`, que é só da thread principal. `OnEnter` precisa funcionar também sem `Preload` antes (entrada direta com `PONTIFICE_SCENE=battle`).
- Para isso `Gpu` separa `InitDevice` (pipeline e recursos, qualquer thread) de `SetRenderer` + textura alvo (criada no primeiro `BeginFrame`, thread principal), e `Model` separa `Parse` (CPU) de `Upload` (GPU).
- No fim escreve em `ctx.preloaded` a instância pronta, `ctx.fade_in_seconds = 1` e `ctx.next_scene`. O `SceneManager` usa a instância sem chamar `OnEnter` de novo e desenha o véu preto do fade in sobre a cena nova.
- Depuração: `PONTIFICE_SCENE=loading` abre direto nela; `PONTIFICE_LOADING_TIME=s` começa a linha do tempo em `s` segundos (útil com `PONTIFICE_SCREENSHOT`).
