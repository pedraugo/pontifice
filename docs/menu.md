# Menu

O menu abre depois da escolha de idioma (docs/idioma.md) ou, quando já existe um save, direto na abertura do jogo. É o único caminho para a batalha, para o grimório, para trocar de idioma e para sair. Os textos vêm de `assets/translate.json` no idioma escolhido; abaixo estão em português.

O menu tem três páginas, todas na mesma cena, com o título `Pontifice` no topo e o rodapé `Criado por pedraugo`. Sem save, abre na página principal (Jogar e Sair); com save, abre sempre direto na página do save, inclusive ao voltar da batalha, do grimório ou da cena de idioma.

## Página principal (sem save)

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

- **Jogar**: abre a página de novo save.
- **Sair**: encerra o jogo.
- `Esc` encerra o jogo.

## Página de novo save (Jogar)

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
|                         Batalhar                           |
|                         Grimório                           |
|                          Sair                              |
| [nível]                                                    |
| [bandeira]                                                 |
| [som]                                                      |
|                                                            |
|                   Criado por pedraugo                      |
+------------------------------------------------------------+
```

- **Batalhar**: com `tutorial` verdadeiro no save, a batalha é o tutorial guiado (docs/tutorial.md). O menu escurece (fade out de 0,5 s) e abre a tela de loading (docs/loading.md), que carrega a batalha com o seu deck do save. Só funciona com o deck completo (40 cartas, `deckSize`); fora disso o item aparece como `Batalhar (deck 38/40)`, com 75% de opacidade, e não faz nada. O deck é montado no grimório.
- **Nível** (ícone no canto inferior esquerdo, só nesta página): quatro barras crescentes, preenchidas até o nível (1 Fácil, 2 Médio, 3 Difícil, 4 Impossível, este com as barras em vermelho); escolhe a força do oponente (Vs PC) para a próxima batalha, relativa ao seu deck: o monte dele espelha as hierarquias das suas cartas, uma mais fraca no Fácil, iguais no Médio, metade uma mais forte no Difícil e todas uma mais forte no Impossível. Fica salvo no save; o save novo começa em Médio (`defaultDifficulty`). Cada clique passa ao próximo: Fácil, Médio, Difícil, Impossível. Detalhes em docs/detalhesTecnicos.md ("Níveis").
- **Grimório**: fade out e tela de loading, que abre o grimório (docs/grimorio.md), onde você vê as cartas liberadas e monta o deck.
- **Sair**: encerra o jogo.
- `Esc` encerra o jogo.

## Save

Arquivo `pontifice.json` na pasta do usuário logado (`SDL_GetUserFolder(SDL_FOLDER_HOME)`: `/Users/<usuário>` no macOS, `/home/<usuário>` no Linux, `C:\Users\<usuário>` no Windows). `PONTIFICE_SAVE=caminho` usa outro arquivo (depuração).

```json
{
  "version": 1,
  "name": "Pedro",
  "language": "pt",
  "difficulty": 1,
  "tutorial": true,
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
- `language`: idioma da sessão; na próxima abertura o jogo usa esse idioma e pula a cena de idioma. Escolher outro idioma na cena de idioma (`PONTIFICE_SCENE=language`) ou clicar na bandeira do menu atualiza o save.
- `difficulty`: nível do oponente escolhido em **Nível** (0 Fácil, 1 Médio, 2 Difícil, 3 Impossível). Cada clique em Nível grava o save; ao abrir o jogo o menu já mostra o nível salvo. Sem a chave, vale `defaultDifficulty` de `assets/rules.json`.
- `tutorial`: `true` enquanto a primeira batalha, com o tutorial guiado (docs/tutorial.md), não terminou. Save novo começa em `true`, e save sem a chave também vale `true`; ao fim dessa partida vira `false`.
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
- Canto inferior esquerdo, botões-ícone 3x3 (`ButtonIcon`) empilhados de baixo para cima:
  - **Som** (alto-falante): liga/desliga a música, em todas as páginas.
  - **Idioma** (bandeira quadrada do idioma atual, em cores), em todas as páginas: cada clique passa ao próximo idioma da lista `languages` (inglês > espanhol > francês > português > inglês...). Os textos do menu mudam na hora, sem sair da página (o nome digitado no formulário fica); com save, o idioma é gravado nele.
  - **Nível** (barras): só na página do save; cada clique passa ao próximo nível e grava o save.
- Com o mouse sobre um botão-ícone, um **tooltip** (janela `frame::Kind::kWindow` da altura do botão, à direita dele) diz o que ele é e o estado atual: `Som ligado`/`Som desligado`, `Idioma: Português`, `Nível: Médio`.
- Os botões-ícone respondem só ao mouse; as setas percorrem apenas os itens de texto.
- Fechar a janela pelo sistema operacional encerra o jogo.

## Medidas em JSON

- `ui.json` `menu`: `titleCenterY`, `footerBottom`, `nameWidth`, `nameHeight`, `namePadding`, `nameGap` (campo até os botões) e `nameMaxLength`.
- `layout.json` `menu`: `fadeOut` e `loadingFadeIn`.
- `button.json`: itens de texto em `buttonText.regular`/`buttonText.danger` (linhas de 4 células, 1 de vão, o mesmo da cena de idioma, bloco centrado em y); Menu e Salvar em `button.regular`.

## Implementação

- Cena: `src/scenes/menu/menuScene.*`; a página inicial sai de `SceneContext::save` (com save, a página do save).
- Campo de texto: `src/scripts/input/textInput.*`.
- Save: `src/scripts/save/save.*` (`Data` com `Available`, `InDeck`, `UnlockCard`, `SetInDeck`; `Path`, `Load`, `Write`, `ToJson`, `FromJson`); carregado em `main.cpp` antes da primeira cena e guardado em `SceneContext::save`. `PreloadInfo::save` leva uma cópia para o `Preload` da batalha e do grimório (outra thread).
- Textos: `menu.play`, `menu.exit`, `menu.battle`, `menu.battleIncomplete`, `menu.grimoire`, `menu.namePlaceholder`, `menu.save`, `general.menu`.
- Testes: `tests/saveTest.cpp` (deck inicial, sorteio por `odds`, ida e volta do JSON, edição do deck com limites).
