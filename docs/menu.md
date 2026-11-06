# Menu

O menu abre logo depois da escolha de idioma (docs/idioma.md), a primeira cena do jogo, e é o único caminho para a batalha e para sair. Os textos vêm de `assets/translate.json` no idioma escolhido; abaixo estão em português.

## Layout

Tela simples, centralizada horizontalmente, fundo liso quase preto (`background.menu`, `#091119`, a mesma da cena de idioma), sem nuvens, ícone ou imagem de fundo. Os itens são só texto, sem placa nem borda, como na cena de idioma.

```
+------------------------------------------------------------+
|                                                            |
|                        PONTIFICE                           |
|                                                            |
|                          Jogar                             |
|                                                            |
|                       Nível: Médio                         |
|                                                            |
|                          Sair                              |
|                                                            |
|                                                            |
|                                                            |
|                   Criado por pedraugo                      |
+------------------------------------------------------------+
```

| Elemento | Posição | Conteúdo |
|---|---|---|
| Título | Topo, centrado em y = 12 células | Texto `PONTIFICE` na fonte de título (`assets/pirataOne.ttf`), linha de 96 px, cor `text.title` |
| Jogar | Centro, em cima | Texto `Jogar` |
| Nível | Centro, no meio | Texto `Nível: Fácil`, `Médio`, `Difícil` ou `Impossível`; cada clique passa ao próximo |
| Sair | Centro, embaixo | Texto `Sair` |
| Rodapé | Base, centralizado | Texto `Criado por pedraugo`, fonte pequena |

Os três itens ficam em linhas de 4 células, com 2 células entre elas, o bloco centrado em y; cada item é centrado em x e a área clicável tem a largura do próprio texto. Não há mais nada na tela.

## Comportamento

- **Jogar**: o menu escurece (fade out de 0,5 s) e abre a tela de loading (docs/loading.md) com fade in de 0,5 s; ela carrega a batalha e entra nela.
- **Nível**: escolhe a força do oponente (Vs PC) para a próxima batalha; começa em Médio. Fácil: cartas dele inferiores às suas; Médio: equivalentes; Difícil: na maioria das vezes mais fortes; Impossível: nitidamente mais fortes. Detalhes em docs/detalhesTecnicos.md ("Níveis").
- **Sair**: encerra o jogo.
- **Som** (ícone de alto-falante no canto inferior esquerdo): liga/desliga a música; vale para todas as cenas.
- **Mouse**: passar o cursor sobre um item clareia o texto. Clicar com o botão esquerdo ativa.
- **Teclado**: setas para cima e para baixo movem o foco entre os itens. Nenhum item começa focado: `↓` foca o primeiro, `↑` o último; `Enter` ou `Espaço` ativa o item focado (sem foco, não faz nada). `Esc` sai do jogo.
- O item focado pelo teclado recebe o mesmo destaque do hover.
- Fechar a janela pelo sistema operacional também encerra o jogo.

## Estados do item

| Estado | Visual |
|---|---|
| Normal | Texto apagado (`buttonText.text`) |
| Hover / Foco / Pressionado | Texto claro (`buttonText.highlight`) |

## Cores

Papéis de `assets/colors.json`: `background.menu` (fundo, `#091119`), `text.title` (título), `buttonText.text` / `buttonText.highlight` (itens), `text.secondary` (rodapé).

## Implementação

- Cena: `src/scenes/menu/menuScene.h` e `src/scenes/menu/menuScene.cpp`.
- Itens: `src/scripts/button/ButtonText.*` (botão só de texto, com rótulo e ação; Jogar, Nível e Sair são instâncias dele criadas em `MenuScene`). A base `Button` (com placa) continua nos diálogos e na batalha; `ButtonIcon` (placa com ícone) é o botão de som.
- Texto renderizado com a fonte do jogo (`assets/pirataOne.ttf`, via stb_truetype, ver `scripts/text/Text.*`), com acentos.
- Tela cheia por padrão (sem redimensionar) e resolução lógica `1920x1080` com letterbox, para o layout não depender do monitor.
