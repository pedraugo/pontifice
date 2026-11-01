# HUD

O HUD (Heads-Up Display) é a camada de informação fixa na tela durante a batalha. Ele fica por cima do campo e não ocupa slots.

```
+----------------------+------------------------+----------------------+
| [CARTA DE CAMPO]     |                        | OPONENTE  4200 / 5000|
|  ativa               |                        | VOCÊ      5000 / 5000|
+----------------------+                        +----------------------+
|                                                                      |
|                              CAMPO                                   |
|                                                                      |
+----------------------+------------------------+----------------------+
| MONTE                |                        | CEMITÉRIO            |
|  usadas      12      |     [MÃO DE CARTAS]    |  cartas       7      |
|  restantes   28      |                        |                      |
+----------------------+------------------------+----------------------+
```

## Canto superior esquerdo: carta de campo ativa

- Mostra a **carta de campo ativa na arena**, virada para cima, com nome e imagem.
- Indica qual categoria está recebendo **buff** e qual está recebendo **nerf**, com os valores.
- Se não há carta de campo ativa, o espaço fica vazio ou exibe "Sem campo".
- Ao ser substituída, a nova carta de campo aparece neste espaço imediatamente.

## Canto superior direito: pontos de vida

- Mostra os **pontos de vida do oponente** e os **seus**, nesta ordem, de cima para baixo.
- Formato: `vida atual / vida máxima`. Ambos começam em `5000 / 5000`.
- O valor é atualizado logo após cada cálculo de dano.
- Ao chegar a `0`, a batalha termina e o resultado é exibido.

## Canto inferior esquerdo: monte

- Mostra a situação do **seu monte**, em duas linhas, de cima para baixo:
  - **Cartas usadas**: quantas cartas já saíram do monte (compradas para a mão ou retiradas por efeito).
  - **Cartas restantes**: quantas cartas ainda estão no monte.
- A soma das duas é sempre o tamanho do monte: `40` (ver [Batalha](batalha.md)).
- Começa em `usadas 5 / restantes 35`, logo após a compra da mão inicial.
- O valor é atualizado logo após cada compra.
- Quando `restantes` chega a `0`, o jogador não compra mais na Fase de Compra.

## Canto inferior direito: cemitério

- Mostra a **quantidade de cartas no seu cemitério**: cartas destruídas ou descartadas.
- Começa em `0` e é atualizado logo após cada destruição ou descarte.
- Mostra apenas a quantidade. O conteúdo do cemitério **não pode ser consultado** por nenhum jogador (ver [Campo](campo.md)).

## Espaço central inferior

- Reservado para a **mão de cartas** (dock 3D). Com a mão recolhida, os painéis dos cantos não a tocam; com a mão levantada e muitas cartas, as cartas das pontas podem passar por baixo dos painéis, que ficam sempre por cima.
