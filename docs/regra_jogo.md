# Regras do jogo

## Objetivo

PONTIFICE é um jogo de cartas para dois jogadores, disputado em turnos. Cada jogador monta um monte de 40 cartas e tenta reduzir os pontos de vida do oponente de 5000 a 0.

## Componentes

- **Cartas de batalha**: Bondosas, Malignas e Neutras. Têm ataque e defesa. Veja [Cartas](cartas.md).
- **Cartas de campo**: alteram o campo, dando buff a uma categoria e nerf à outra. Veja [Cartas](cartas.md).
- **Campo**: arena 5x2 por lado, com slot de campo e monte. Veja [Campo](campo.md).
- **HUD**: informações fixas na tela. Veja [HUD](hud.md).

## Fluxo de uma partida

1. Cada jogador embaralha seu monte e compra 5 cartas.
2. Sorteia-se quem começa.
3. Os jogadores alternam turnos seguindo as fases descritas em [Batalha](batalha.md).
4. A partida termina quando uma das condições abaixo ocorre.

## Fim da batalha

A batalha termina quando ocorre uma das situações:

- Um jogador chega a **0 pontos de vida**: ele perde.
- Um jogador precisa comprar e o **monte está vazio**: ele perde.
- Um jogador **desiste**.
- Se ambos chegarem a 0 pontos de vida ao mesmo tempo, é **empate**.

## Regras gerais

- O cemitério não é visível nem consultável. Carta destruída ou descartada sai do jogo até o fim da batalha.
- Cartas viradas para baixo em campo não podem ser vistas pelo oponente até serem viradas.
- O número de cartas na mão e no monte de cada jogador é informação pública.
- Qualquer efeito de carta tem prioridade sobre as regras gerais.
