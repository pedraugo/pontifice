# Menu

O menu é a **primeira cena** do jogo. Abre assim que o executável inicia e é o único caminho para a batalha e para sair.

## Layout

Tela simples, centralizada horizontalmente, fundo escuro sólido.

```
+------------------------------------------------------------+
|                                                            |
|                                                            |
|                        PONTIFICE                           |
|                                                            |
|                                                            |
|                      [   Jogar   ]                         |
|                                                            |
|                      [    Sair   ]                         |
|                                                            |
|                                                            |
|                                                            |
|                   Criado por pedraugo                      |
+------------------------------------------------------------+
```

| Elemento | Posição | Conteúdo |
|---|---|---|
| Título | Topo, centralizado | Texto `PONTIFICE`, fonte grande |
| Botão Jogar | Centro, acima de Sair | Texto `Jogar` |
| Botão Sair | Centro, abaixo de Jogar | Texto `Sair` |
| Rodapé | Base, centralizado | Texto `Criado por pedraugo`, fonte pequena |

Não há mais nada na tela: sem opções, sem créditos extras, sem imagem de fundo nesta versão.

## Comportamento

- **Jogar**: troca para a cena de batalha.
- **Sair**: encerra o jogo.
- **Mouse**: passar o cursor sobre um botão destaca a borda e o texto. Clicar com o botão esquerdo ativa.
- **Teclado**: setas para cima e para baixo movem o foco entre os botões. `Enter` ou `Espaço` ativa o botão focado. `Esc` sai do jogo.
- O botão focado pelo teclado recebe o mesmo destaque do hover.
- Fechar a janela pelo sistema operacional também encerra o jogo.

## Estados do botão

| Estado | Visual |
|---|---|
| Normal | Borda clara fina, texto claro, fundo transparente |
| Hover / Foco | Borda mais grossa, fundo levemente iluminado |
| Pressionado | Fundo iluminado mais forte, enquanto o botão do mouse está pressionado |

## Cores

| Uso | Cor (RGB) |
|---|---|
| Fundo | `13, 10, 20` |
| Texto e borda | `230, 220, 200` |
| Hover (fundo) | `60, 50, 80` |
| Pressionado (fundo) | `90, 75, 120` |

## Implementação

- Cena: `src/scenes/MenuScene.h` e `src/scenes/MenuScene.cpp`.
- Botões: `src/scripts/Button.h` (base), `src/scripts/ButtonPlay.cpp` (Jogar), `src/scripts/ButtonExit.cpp` (Sair).
- Texto renderizado com a fonte de depuração do SDL (`SDL_RenderDebugText`) escalada. Será trocada por fonte própria via SDL_ttf quando a direção de arte definir tipografia.
- Resolução lógica fixa de `1280x720` com letterbox, para o layout não depender do tamanho da janela.
