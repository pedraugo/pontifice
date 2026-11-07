# Menu

O menu abre depois da escolha de idioma (docs/idioma.md) ou, quando já existe um save, direto na abertura do jogo. É o único caminho para a batalha, para o grimório e para sair. Os textos vêm de `assets/translate.json` no idioma escolhido; abaixo estão em português.

O menu tem três páginas, todas na mesma cena, com o título `Pontifice` no topo e o rodapé `Criado por pedraugo`.

## Página principal

```
+------------------------------------------------------------+
|                        Pontifice                           |
|                                                            |
|                          Jogar                             |
|                                                            |
|                          Sair                              |
|                                                            |
|                   Criado por pedraugo                      |
+------------------------------------------------------------+
```

- **Jogar**: sem save, abre a página de novo save; com save, abre a página do save.
- **Sair**: encerra o jogo.

## Página de novo save (Jogar sem save)

```
+------------------------------------------------------------+
|                        Pontifice                           |
|                                                            |
|               +------------------------------+             |
|               | Seu nome|                     |             |
|               +------------------------------+             |
|               [Menu]                  [Salvar]             |
|                                                            |
|                   Criado por pedraugo                      |
+------------------------------------------------------------+
```

| Elemento | Conteúdo |
|---|---|
| Campo de nome | Placa de janela (`frame::Kind::kWindow`) com 30x4 células, centrada; texto em `large`, placeholder `Seu nome` em `steelLight` e caret piscando (1 s) no fim do texto. Até 16 caracteres, só `a-z`, `A-Z`, `0-9`, ponto (`.`) e underline (`_`): espaço, acentos, outros símbolos e emoji não entram |
| Menu | Botão com placa sob a borda esquerda do campo; volta à página principal |
| Salvar | Botão com placa sob a borda direita; com o nome vazio fica com 75% de opacidade e não faz nada |

- Digitar escreve no campo (`SDL_StartTextInput` só nesta página); `Backspace` apaga o último caractere.
- `Enter` salva; `Esc` volta à página principal.
- **Salvar** cria o save (ver abaixo) e abre a página do save.

## Página do save

```
+------------------------------------------------------------+
|                        Pontifice                           |
|                                                            |
|                         Batalha                            |
|                       Nível: Médio                         |
|                         Grimório                           |
|                          Sair                              |
|                                                            |
|                   Criado por pedraugo                      |
+------------------------------------------------------------+
```

- **Batalha**: o menu escurece (fade out de 0,5 s) e abre a tela de loading (docs/loading.md), que carrega a batalha com o seu deck do save. Só funciona com o deck completo (40 cartas, `deckSize`); fora disso o item aparece como `Batalha (deck 38/40)`, com 75% de opacidade, e não faz nada. O deck é montado no grimório.
- **Nível**: escolhe a força do oponente (Vs PC) para a próxima batalha, relativa ao seu deck: o monte dele espelha as hierarquias das suas cartas, uma mais fraca no Fácil, iguais no Médio, metade uma mais forte no Difícil e todas uma mais forte no Impossível. Fica salvo no save; o save novo começa em Médio (`defaultDifficulty`). Cada clique passa ao próximo: Fácil, Médio, Difícil, Impossível. Detalhes em docs/detalhesTecnicos.md ("Níveis").
- **Grimório**: fade out e tela de loading, que abre o grimório (docs/grimorio.md), onde você vê as cartas liberadas e monta o deck.
- **Sair**: encerra o jogo.
- `Esc` volta à página principal.
- Ao voltar da batalha ou do grimório, o menu reabre nesta página.

## Save

Arquivo `pontifice.json` na pasta do usuário logado (`SDL_GetUserFolder(SDL_FOLDER_HOME)`: `/Users/<usuário>` no macOS, `/home/<usuário>` no Linux, `C:\Users\<usuário>` no Windows). `PONTIFICE_SAVE=caminho` usa outro arquivo (depuração).

```json
{
  "version": 1,
  "name": "Pedro",
  "language": "pt",
  "difficulty": 1,
  "unlocked": [
    {"code": "G401", "date": "2026-11-07 12:53:01"},
    {"code": "N403", "date": "2026-11-07 12:53:01"},
    {"code": "G401", "date": "2026-11-07 14:10:22"}
  ],
  "deck": [
    "G401",
    "G401",
    "N403"
  ]
}
```

- `name`: nome digitado.
- `language`: idioma da sessão; na próxima abertura o jogo usa esse idioma e pula a cena de idioma. Escolher outro idioma na cena de idioma (`PONTIFICE_SCENE=language`) atualiza o save.
- `difficulty`: nível do oponente escolhido em **Nível** (0 Fácil, 1 Médio, 2 Difícil, 3 Impossível). Cada clique em Nível grava o save; ao abrir o jogo o menu já mostra o nível salvo. Sem a chave, vale `defaultDifficulty` de `assets/rules.json`.
- `unlocked`: **histórico** das cartas liberadas, uma entrada por cópia, na ordem em que você ganhou, com a data e hora local (`date`). As 40 do deck inicial entram com a data da criação do save; cada recompensa acrescenta uma. `save::Data::Available()` agrupa o histórico em cópias disponíveis por código (ex.: `G401` → 2).
- `deck`: as cartas que você usa na batalha, um código por cópia, com repetições; até `deckSize` (40) entradas e nunca mais cópias de uma carta do que as disponíveis. Ao ler o save, entradas além disso são descartadas.
- **Deck inicial** (`cards::StarterDeck`), sorteio uniforme com no máximo 2 cópias por carta: 26 cartas de hierarquia 4 (benévolas, malignas e neutras), 10 de hierarquia 3 (benévolas, malignas e neutras) e o resto até `deckSize` (40) em cartas de campo, ou seja, 4. As mesmas 40 vão para `unlocked` e para `deck`. As quantidades ficam em `assets/rules.json` (`starterHierarchy4`, `starterHierarchy3`, `deckSize`).
- **Recompensa**: cada vitória libera uma carta, nova ou repetida, sorteada pelo `odds` da carta (chance proporcional; ver docs/batalha.md). Ela entra em `unlocked`; o `deck` só muda no grimório.
- A gravação escreve `pontifice.json.tmp` e renomeia por cima do save, então uma falha no meio mantém o save anterior. Um save ilegível é ignorado (fica no log) e o próximo Salvar o substitui.

## Estados do item

| Estado | Visual |
|---|---|
| Normal | Texto apagado (`steelLight`) |
| Hover / Foco / Pressionado | Texto claro (`silverLight`) |
| Sair (variante `buttonText.danger`) | Texto avermelhado (`red`, `redLight` com hover/foco) |

## Teclado e mouse

- Páginas principal e do save: setas para cima e para baixo movem o foco; nenhum item começa focado (`↓` foca o primeiro, `↑` o último); `Enter` ou `Espaço` ativa o item focado.
- Mouse: hover clareia o texto; clique esquerdo ativa.
- **Som** (ícone de alto-falante no canto inferior esquerdo): liga/desliga a música em todas as páginas.
- Fechar a janela pelo sistema operacional encerra o jogo.

## Medidas em JSON

- `ui.json` `menu`: `titleCenterY`, `footerBottom`, `nameWidth`, `nameHeight`, `namePadding`, `nameGap` (campo até os botões) e `nameMaxLength`.
- `layout.json` `menu`: `fadeOut` e `loadingFadeIn`.
- `button.json`: itens de texto em `buttonText.regular`/`buttonText.danger` (linhas de 4 células, 1 de vão, o mesmo da cena de idioma, bloco centrado em y); Menu e Salvar em `button.regular`.

## Implementação

- Cena: `src/scenes/menu/menuScene.*`; a página atual fica em `SceneContext::menu_page` para o menu reabrir nela.
- Campo de texto: `src/scripts/input/textInput.*`.
- Save: `src/scripts/save/save.*` (`Data` com `Available`, `InDeck`, `UnlockCard`, `SetInDeck`; `Path`, `Load`, `Write`, `ToJson`, `FromJson`); carregado em `main.cpp` antes da primeira cena e guardado em `SceneContext::save`. `PreloadInfo::save` leva uma cópia para o `Preload` da batalha e do grimório (outra thread).
- Textos: `menu.play`, `menu.exit`, `menu.battle`, `menu.battleIncomplete`, `menu.level`, `menu.grimoire`, `menu.namePlaceholder`, `menu.save`, `general.menu`.
- Testes: `tests/saveTest.cpp` (deck inicial, sorteio por `odds`, ida e volta do JSON, edição do deck com limites).
