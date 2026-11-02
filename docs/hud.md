# HUD

O HUD (Heads-Up Display) é a camada de informação fixa na tela durante a batalha. Ele fica por cima do campo e não ocupa slots.

```
+----------------+----------------------------+----------------------+
| [img] Nome     |                            | Oponente  4200 / 5000|
|  +300 Bondosas |                            | Jogador   5000 / 5000|
|  -300 Malignas |                            +----------------------+
+----------------+                                                    |
|                              CAMPO                                   |
|                                                                      |
|                                                                      |
| [🔊] [Monte 28/40]    [MÃO DE CARTAS]              [Cemitério 7]     |
+----------------------------------------------------------------------+
```

## Canto superior esquerdo: carta de campo ativa

- Retângulo **compacto** (384 x 96 em 1080p, mais largo se o nome pedir): a **ilustração** da carta em miniatura à esquerda (só a arte, sem a moldura; sem ilustração, a frente inteira) e, à direita, o nome e uma linha para cada efeito: `+300 Bondosas` (buff, verde) e `-300 Malignas` (nerf, vermelho).
- Se não há carta de campo ativa, exibe "Sem campo".
- Ao ser substituída, a nova carta de campo aparece neste espaço imediatamente.
- O painel mostra o **efeito em vigor**: a carta em si já foi consumida e contada no cemitério do dono ao ser jogada.

## Canto superior direito: pontos de vida

- Mostra os **pontos de vida do oponente** e os **seus**, rotulados `Oponente` e `Jogador`, nesta ordem, de cima para baixo.
- Formato: `vida atual / vida máxima`. Ambos começam em `5000 / 5000`.
- O valor é atualizado logo após cada cálculo de dano.
- Ao chegar a `0`, a batalha termina e o resultado é exibido.

## Canto inferior esquerdo: som e monte

- **Botão de som**: quadrado com ícone de alto-falante (desenhado com primitivas, sem sprite). Clique liga/desliga a música (volume vai a zero e volta em rampa de 1 s). Desligada, o ícone fica esmaecido, sem ondas e com um X vermelho. O mesmo botão existe no menu.
- À direita dele, o painel do monte. Uma linha: `Monte restantes/40`, por exemplo `Monte 35/40` logo após a compra da mão inicial.
- Descrição antiga (duas linhas), mantida como referência do que o número resume:
  - **Cartas usadas**: quantas cartas já saíram do monte (compradas para a mão ou retiradas por efeito).
  - **Cartas restantes**: quantas cartas ainda estão no monte.
- A soma das duas é sempre o tamanho do monte: `40` (ver [Batalha](batalha.md)).
- Começa em `usadas 5 / restantes 35`, logo após a compra da mão inicial.
- O valor é atualizado logo após cada compra.
- Quando `restantes` chega a `0`, o número fica em destaque e o jogador não compra mais na Fase de Compra.

## Canto inferior direito: cemitério

- Uma linha: `Cemitério N`, a **quantidade de cartas no seu cemitério**: cartas destruídas ou descartadas.
- Começa em `0` e é atualizado logo após cada destruição ou descarte.
- Mostra apenas a quantidade. O conteúdo do cemitério **não pode ser consultado** por nenhum jogador (ver [Campo](campo.md)).

## Espaço central inferior

- Reservado para a **mão de cartas** (dock 3D). Com a mão recolhida, os painéis dos cantos não a tocam; com a mão levantada e muitas cartas, as cartas das pontas podem passar por baixo dos painéis, que ficam sempre por cima.
