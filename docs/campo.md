# Campo

O campo é a arena onde a batalha acontece. Ele é dividido em dois lados espelhados: o **seu lado** e o **lado do oponente**. Cada lado tem:

- **10 slots de carta**, em uma grade de **5 colunas por 2 linhas** (5x2), todos iguais entre si;
- **1 slot de campo**, exclusivo para carta de campo;
- **1 monte** (deck), fora da grade.

O cemitério existe como pilha de cartas destruídas ou descartadas, mas **não é exibido no campo** e não pode ser consultado por nenhum jogador.

## O que é um slot

Um **slot** é um espaço fixo do campo onde uma única carta pode ser colocada. Cada slot:

- comporta **no máximo uma carta** por vez;
- aceita **apenas um tipo de carta**: slots de carta aceitam cartas Bondosas, Malignas e Neutras; o slot de campo aceita apenas cartas de campo;
- pertence a **um único jogador**. Você só coloca cartas nos seus slots, nunca nos do oponente;
- é identificado por **linha e coluna**. Exemplo: slot `A3` é a linha A, coluna 3.

Se um slot está ocupado, nenhuma outra carta pode ser colocada nele até que a carta atual seja destruída ou movida.

## Layout de cada lado

```
                    LADO DO OPONENTE
          +-------+-------+-------+-------+-------+
 [MONTE]  |  B1   |  B2   |  B3   |  B4   |  B5   |  [CAMPO]
          +-------+-------+-------+-------+-------+
          |  A1   |  A2   |  A3   |  A4   |  A5   |
          +-------+-------+-------+-------+-------+
          =========================================  centro
          +-------+-------+-------+-------+-------+
          |  A1   |  A2   |  A3   |  A4   |  A5   |
          +-------+-------+-------+-------+-------+
 [CAMPO]  |  B1   |  B2   |  B3   |  B4   |  B5   |  [MONTE]
          +-------+-------+-------+-------+-------+
                        SEU LADO
              1       2       3       4       5
```

**Slots de carta (A1 a B5)**: 10 slots iguais, tanto no seu lado quanto no do oponente. Qualquer um recebe cartas Bondosas, Malignas e Neutras, em modo de ataque ou defesa. Não há diferença de regra entre a linha A e a linha B: toda carta em qualquer slot pode **atacar e ser atacada**. As letras e números servem apenas para identificar a posição.

**Slot de campo**: 1 slot exclusivo para carta de campo. Nenhuma outra carta pode ser colocada nele, e uma carta de campo não pode ser colocada em nenhum outro slot.

**Monte**: fica fora da grade, virado para baixo.

## Regras dos slots

- No máximo **10 cartas de batalha** por jogador em campo. Com os 10 slots ocupados, é preciso perder uma carta antes de invocar outra.
- O jogador escolhe em **qual dos 10 slots** invoca a carta. A posição não altera ataque, defesa nem quem pode atacar quem.
- Uma carta não muda de slot depois de colocada, exceto por efeito de carta.
- Cada lado tem um slot de campo, mas só existe **uma carta de campo ativa na arena**. Ao colocar uma carta de campo no seu slot, a carta de campo do oponente (se houver) vai para o cemitério dele.
- Ataque direto só acontece quando **todos os 10 slots de carta do oponente estão vazios**.
