# Menu

O menu abre logo depois da escolha de idioma (docs/idioma.md), a primeira cena do jogo, e é o único caminho para a batalha e para sair. Os textos vêm de `assets/translate.json` no idioma escolhido; abaixo estão em português.

## Layout

Tela simples, centralizada horizontalmente, fundo escuro sólido.

```
+------------------------------------------------------------+
|                                                            |
|                  [ícone]  PONTIFICE                        |
|                                                            |
|                      [   Jogar   ]                         |
|                                                            |
|                   [ Nível: Médio ]                         |
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
| Ícone | Topo, à esquerda do logotipo | `assets/icon.png` com 128 px (8 células da grade); ícone e logotipo formam uma linha centrada em x, com 2 células entre eles, centro em y = 12 células |
| Título | Topo, à direita do ícone | Logotipo `sprites/Logo.png` (letras de pedra, 640 px de largura); sem o arquivo, texto `PONTIFICE` em fonte grande no mesmo lugar |
| Botão Jogar | Centro, em cima | Texto `Jogar` |
| Botão Nível | Centro, no meio | Texto `Nível: Fácil`, `Médio`, `Difícil` ou `Impossível`; cada clique passa ao próximo |
| Botão Sair | Centro, embaixo | Texto `Sair`, placa vermelha (`sprites/UiFrameButtonDanger.png`), por ser ação destrutiva |
| Rodapé | Base, centralizado | Texto `Criado por pedraugo`, fonte pequena |

Não há mais nada na tela: sem créditos extras, sem imagem de fundo nesta versão.

## Comportamento

- **Jogar**: abre a tela de loading (docs/loading.md), que carrega a batalha e entra nela.
- **Nível**: escolhe a força do oponente (Vs PC) para a próxima batalha; começa em Médio. Fácil: cartas dele inferiores às suas; Médio: equivalentes; Difícil: na maioria das vezes mais fortes; Impossível: nitidamente mais fortes. Detalhes em docs/detalhes_tecnicos.md ("Níveis").
- **Sair**: encerra o jogo.
- **Som** (ícone de alto-falante no canto inferior esquerdo): liga/desliga a música; vale para todas as cenas.
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

- Cena: `src/scenes/menu/MenuScene.h` e `src/scenes/menu/MenuScene.cpp`.
- Botões: `src/scripts/button/Button.h` (base), `src/scripts/button/ButtonPlay.cpp` (Jogar), `src/scripts/button/ButtonExit.cpp` (Sair).
- Texto renderizado com a fonte do jogo (`assets/PirataOne.ttf`, via stb_truetype, ver `scripts/text/Text.*`), com acentos.
- Tela cheia por padrão (sem redimensionar) e resolução lógica `1920x1080` com letterbox, para o layout não depender do monitor.
