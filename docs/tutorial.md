# Tutorial

A primeira batalha de um save é sempre um tutorial guiado: uma sequência de dicas, passo a passo, que ensina a jogar uma partida enquanto você joga. Em cada passo só a ação pedida fica liberada, e o que importa naquele momento ganha destaque. O tutorial só acaba quando a partida termina, e essa partida o jogador sempre vence.

## Quando acontece

- O save tem o campo `tutorial` (docs/menu.md, "Save"). Save novo começa com `"tutorial": true`; save sem o campo também vale `true`.
- Com `tutorial` verdadeiro, **Batalhar** no menu abre a batalha em modo tutorial.
- Quando a partida termina, `tutorial` vira `false` e o save é gravado. As próximas batalhas são normais.
- Abandonar a batalha (`Esc` > Sim) não conclui o tutorial: a próxima batalha começa o tutorial de novo.

## Vitória garantida

- Você sempre começa (sem sorteio).
- O monte do oponente só tem as cartas de batalha mais fracas do jogo: as `tutorialOpponentCards` (10) de menor ATK, repetidas até `deckSize`, sem cartas de campo. O nível escolhido no menu não vale nessa partida.
- O oponente invoca, mas **nunca ataca**.
- Sua vida nunca cai abaixo de **1**: mesmo atacando uma carta mais forte, você não perde.
- A mão inicial traz uma carta de campo, se o seu deck tiver alguma, e cartas de batalha no resto, para o passo das cartas de campo.
- A vitória dá a recompensa normal (docs/batalha.md, "Recompensa").

## Painel

Cada passo abre uma janela (`frame::Kind::kWindow`) com `Dica N de 16`, um título e o texto da dica.

- **Explicação**: janela no centro, sobre um véu escuro, com o botão **Próximo** (ou **Jogar** no último passo). `Enter` ou `Espaço` também avançam. Enquanto ela está aberta, a mesa não recebe cliques; `Esc` pausa a batalha como sempre.
- **Spotlight**: nas explicações sobre algo da tela, o véu deixa um recorte sem escurecer em volta do elemento, com um contorno dourado pulsando: o painel de vida (passo 2), a mão (passos 3, 5, 6 e 7, com a mão levantada) e o painel de campo (passo 11 sem carta de campo na mão).
- **Ação**: janela encostada na borda esquerda, sem botão e sem véu. A dica avança sozinha quando você faz o que ela pede.

## Narração

Cada passo tem uma fala gravada, tocada quando o painel do passo abre (`assets/audio.json` `voices.tutorial`: `audios/voices/tutorial{language}{step}.ogg`, ex.: `tutorialPt01.ogg` para o passo 1 em português; o idioma é o código com a primeira letra maiúscula e o passo tem dois dígitos).

- Enquanto a fala toca, o botão **Próximo** (ou **Jogar**) fica com 75% de opacidade e não responde a clique, `Enter` ou `Espaço`; o botão **Passar turno** do passo 12 também.
- Num passo de ação, fazer o que a dica pede antes do fim da fala não avança: o passo muda quando a fala termina.
- Passo sem arquivo de fala no idioma escolhido (hoje o 14 em português e todos em inglês, espanhol e francês) não espera nada.
- Com o som desligado (botão de som da batalha), a fala fica muda e não segura o botão.
- Volume em `voices.volume` (`audio.json`). As falas do idioma são decodificadas no `Preload` da batalha.

## Só a ação pedida

Durante o tutorial (até o passo 16), o resto fica travado:

- **Cartas da mão**: as que não servem para o passo ficam escurecidas e sem cor e não reagem ao mouse (sem destaque, arraste ou inspeção). Arrastar cartas só vale nos passos 8 (cartas de batalha) e 11 (a carta de campo).
- **Trocar posição** (clique na carta da arena): só nos passos 9 e 10.
- **Atacar**: só no passo 15.
- **Passar turno**: o botão só aparece no passo 12, destacado.
- As dicas do topo da tela somem; ficam só as prévias durante um arraste (resultado do ataque, onde soltar) e os controles da inspeção.

## Destaques

- **Passo 8**: os slots livres do seu lado pulsam, e só as cartas de batalha da mão ficam acesas.
- **Passos 9 e 10**: a sua carta que deve trocar de posição ganha uma moldura dourada pulsando no slot.
- **Passo 11**: só a carta de campo da mão fica acesa; ao arrastá-la, a arena já brilha como sempre.
- **Passo 12**: o botão **Passar turno** aparece com foco.
- **Passo 15**: moldura dourada nas suas cartas que podem atacar e nas cartas do oponente; com o lado dele vazio, os slots dele pulsam para o ataque direto.

## Passos

| # | Passo | Tipo | Avança quando |
|---|---|---|---|
| 1 | Bem-vindo ao Pontifice!: objetivo de levar a vida do oponente de 5000 a 0 | Explicação | Próximo |
| 2 | Pontos de vida: painel no canto superior direito e a cor que muda conforme a vida cai | Explicação | Próximo |
| 3 | Sua mão: destacar uma carta com o mouse | Explicação | Próximo |
| 4 | Inspecionando: duplo clique abre; arrastar gira, roda dá zoom, clique fora sai | Ação | Você abre uma carta e fecha a inspeção |
| 5 | Categorias: Benévola, Maligna ou Neutra, o lado de cada carta na batalha | Explicação | Próximo |
| 6 | Hierarquia: raridade e poder; a hierarquia 1 é a mais forte e a 4 a mais comum | Explicação | Próximo |
| 7 | Ataque e defesa: ATK em modo de ataque, DEF em modo de defesa | Explicação | Próximo |
| 8 | Em prática: uma invocação por turno; arrastar da mão até o campo, entra em ataque | Ação | Você invoca uma carta |
| 9 | Modo de defesa: clicar na carta para virar defesa | Ação | Uma carta sua fica em defesa |
| 10 | Voltar ao ataque: clicar de novo | Ação | Nenhuma carta sua em defesa |
| 11 | Cartas de campo: arrastar a carta de campo até a arena | Ação | Você joga a carta de campo |
| 12 | Passar o turno: quem começa não ataca no primeiro turno | Ação | Você clica em Passar turno |
| 13 | Vez do oponente: as jogadas dele aparecem no centro | Ação | Seu turno começa |
| 14 | Como o ataque funciona: ataque contra ataque, contra defesa e direto | Explicação | Próximo |
| 15 | Atacar o oponente: arrastar sua carta até a carta dele ou ao lado vazio | Ação | Você ataca |
| 16 | Agora é com você: vencer a partida e o botão de dica | Explicação | Jogar |

- No passo 11, sem carta de campo na mão, a dica só explica e tem o botão **Próximo**.
- Uma explicação nunca abre no meio de um arraste: ela espera você soltar.
- Depois do passo 16 o painel some, tudo volta a ser liberado e a partida segue até a vitória, ainda com o oponente fraco que não ataca.

## Implementação

- Save: `save::Data::tutorial` em `src/scripts/save/save.*`; `FromJson` usa `true` quando o campo falta.
- Painel: `src/scripts/tutorial/tutorialPanel.*` (texto quebrado na largura do painel; `SetSpotlight` recorta o véu).
- Batalha: `BattleScene::TutorialStep`, `UpdateTutorial`, `AdvanceTutorial`, `UpdateTutorialFocus`, `TutorialSpotlight`, `TutorialLocksCard`, `TutorialAllowsToggle`, `TutorialAllowsAttack`, `EndTurnLocked`, `ArrangeTutorialHand` e `TutorialOpponentDeck` em `src/scenes/battle/battleScene.*`; a tabela `kTutorialSteps` dá a chave de texto e o tipo de cada passo.
- Mão: `Hand::SetLock` (cartas travadas e escurecidas), `Hand::SetDraggable`, `Hand::SetRaised` e `Hand::ScreenBounds` em `src/scripts/hand/hand.*`; tom e dessaturação das travadas em `layout.json` `battle.hand.lockedTint` e `lockedDesaturate`.
- Arena: `Board::SetFocus` (moldura dourada pulsando) em `src/scripts/board/board.*`.
- HUD: `Hud::LifePanelRect` e `Hud::FieldPanelRect` para o spotlight.
- Regras: `rules.json` `tutorialOpponentCards`.
- Narração: `Voices` (`Init`, `File`, `Preload`, `Say`) em `src/scripts/audios/voices.*`; `BattleScene::TutorialVoice` e `TutorialNarrating`; `TutorialPanel::SetEnabled` desabilita o botão.
- Medidas: `ui.json` `tutorial` (`width`, `padding`, `margin`, `counterGap`, `titleGap`, `buttonGap`, `spotlightMargin`, `spotlightLinePx`, `spotlightPulseSpeed`).
- Textos: `battle.tutorial.*` em `assets/translate.json` (`counter`, `next`, `start` e `title`/`text` de cada passo; `fieldInfo` é o passo 11 sem carta de campo na mão).
