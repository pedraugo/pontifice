# HUD

O HUD (Heads-Up Display) é a camada de informação fixa na tela durante a batalha. Ele fica por cima do campo e não ocupa slots.

```
+----------------+----------------------------+----------------------+
| [img] Nome     |                            | Oponente  4200 / 5000|
| +300 Benévolas |                            | Jogador   5000 / 5000|
| -300 Malignas  |                            +----------------------+
+----------------+                                                    |
|                              CAMPO                                   |
|                                                                      |
|                                                                      |
| [🔊]           [MÃO DE CARTAS]        [Monte 28/40     Cemitério 7]  |
+----------------------------------------------------------------------+
```

## Canto superior esquerdo: carta de campo ativa

- Retângulo **compacto** (384 x 96 em 1080p, mais largo se o nome pedir): a **ilustração** da carta em miniatura à esquerda (só a arte, sem a moldura; sem ilustração, a frente inteira) e, à direita, o nome e uma linha para cada efeito: `+300 Benévolas` (buff, verde) e `-300 Malignas` (nerf, vermelho).
- Com o **mouse sobre a ilustração**, ela cresce da miniatura até o tamanho nativo (668 x 448) logo abaixo do painel, numa placa de janela (0,15 s), para ver a arte em detalhe; tirando o mouse, volta.
- Se não há carta de campo ativa, exibe "Sem campo".
- Ao ser substituída, a nova carta de campo aparece neste espaço imediatamente.
- O painel mostra o **efeito em vigor**: a carta em si já foi consumida e contada no cemitério do dono ao ser jogada.

## Canto superior direito: pontos de vida

- Ao **perder vida**, o painel inteiro **treme** (0,4 s, forte no golpe e sumindo), o valor de quem levou o dano **pisca em vermelho** e um **número de dano** (`-2750`, vermelho com sombra, grande) **salta** do valor: estoura, sobe um pouco, cai em arco para dentro da tela e some em 1,2 s. Danos seguidos saem para lados alternados. O número é o dano do golpe, mesmo que a vida pare em 0.

- Mostra os **pontos de vida do oponente** e os **seus**, rotulados `Oponente` e `Jogador`, nesta ordem, de cima para baixo.
- Formato: `vida atual / vida máxima`. Ambos começam em `5000 / 5000`.
- O valor é atualizado logo após cada cálculo de dano.
- Ao chegar a `0`, a batalha termina e o resultado é exibido.

## Tela de resultado

- Com a vida de um lado em `0`, a tela escurece e uma **janela central** (`Hud::RenderResult`, medidas em `ui.json` `hud.result`) mostra **VITÓRIA** ou **DERROTA** (verde/vermelho, variante `heading` de `text.json`), a linha `Tempo jogado mm:ss   Turnos n` e uma tabela com as colunas **Você** e **Oponente**: cartas compradas (monte usado), cartas usadas (invocações e cartas de campo), cartas no cemitério, ataques, ataques diretos, cartas destruídas do outro lado, dano causado ao outro lado e maior golpe. Por fim, o botão **Menu** (`general.menu`, `button.regular`, centrado na base da janela; `Hud::ResultButtonArea` dá o lugar e a `BattleScene` desenha e trata o clique), que volta ao menu; `Esc` e o botão de sair do canto fazem o mesmo. Na vitória, a carta ganha (docs/batalha.md, "Recompensa") aparece ao lado da janela, da mesma altura, com `Nova carta` ou `Carta repetida` acima; janela e carta ficam centradas juntas (`ui.json` `hud.result.cardGap` entre elas).
- Os números vêm de `MatchStats` (`scripts/hud/hud.h`), preenchido pela `BattleScene` a cada ataque resolvido (`ResolveAttack`), invocação, carta de campo e descarte; o tempo conta do início da partida (depois da abertura) até o fim. Carta destruída ao atacar uma carta mais forte conta para quem defendeu. As teclas de depuração de dano não entram na conta.

## Canto inferior esquerdo: som, dica e sair

- **Botão de som**: quadrado com ícone de alto-falante (desenhado com primitivas, sem sprite). Clique liga/desliga a música (volume vai a zero e volta em rampa de 1 s). Desligada, o ícone fica esmaecido, sem ondas e com um X vermelho. O mesmo botão existe no menu.
- **Botão de dica** (acima do som): `?`; liga/desliga a dica do topo da tela.
- **Botão de sair** (acima da dica): porta com seta, placa vermelha de perigo. Faz o mesmo que Esc: abre o diálogo de abandono da batalha; com a partida encerrada, volta ao menu.

## Canto inferior direito: monte e cemitério

Um único painel, em uma linha: `[Monte 35/40      Cemitério 0]`. O monte fica à esquerda e o cemitério alinhado à direita. A largura é medida com os maiores valores (`40/40` e dois dígitos), então o painel não muda de tamanho a cada compra ou descarte.

### Monte

- `Monte restantes/40`, por exemplo `Monte 35/40` logo após a compra da mão inicial.
- Descrição antiga (duas linhas), mantida como referência do que o número resume:
  - **Cartas usadas**: quantas cartas já saíram do monte (compradas para a mão ou retiradas por efeito).
  - **Cartas restantes**: quantas cartas ainda estão no monte.
- A soma das duas é sempre o tamanho do monte: `40` (ver [Batalha](batalha.md)).
- Começa em `usadas 5 / restantes 35`, logo após a compra da mão inicial.
- O valor é atualizado logo após cada compra.
- Quando `restantes` chega a `0`, o número fica em destaque e o jogador não compra mais na Fase de Compra.

### Cemitério

- `Cemitério N`, a **quantidade de cartas no seu cemitério**: cartas destruídas ou descartadas.
- Começa em `0` e é atualizado logo após cada destruição ou descarte.
- Mostra apenas a quantidade. O conteúdo do cemitério **não pode ser consultado** por nenhum jogador (ver [Campo](campo.md)).

## Espaço central inferior

- Reservado para a **mão de cartas** (dock 3D). Com a mão recolhida, os painéis dos cantos não a tocam; com a mão levantada e muitas cartas, as cartas das pontas podem passar por baixo dos painéis, que ficam sempre por cima.
