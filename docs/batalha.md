# Batalha

A batalha é disputada em turnos entre dois jogadores, cada um com seu monte de cartas, até que um deles fique sem pontos de vida.

## Preparação

- Cada jogador começa com **5000 pontos de vida**.
- Cada jogador usa um **monte (deck) de 40 cartas**, embaralhado antes da batalha.
- Cada jogador compra **5 cartas** para a mão inicial.
- Decide-se por sorteio quem joga primeiro.
- Cada jogador tem **10 slots de carta** (grade 5x2), **1 slot de campo** e o monte no seu lado do campo. Veja a seção [Campo](campo.md).

## Estrutura do turno

Cada turno é dividido em fases, nesta ordem:

1. **Fase de Compra**: o jogador compra 1 carta do monte. O primeiro jogador da batalha não compra no primeiro turno.
2. **Fase de Espera**: resolvem-se efeitos que ocorrem "no início do turno".
3. **Fase Principal 1**: o jogador pode invocar cartas, trocar posição de batalha, jogar cartas de campo e ativar efeitos.
4. **Fase de Batalha**: o jogador declara ataques com suas cartas. Pode ser pulada.
5. **Fase Principal 2**: igual à Fase Principal 1, exceto que não é permitida uma nova invocação normal se ela já foi usada na Fase Principal 1.
6. **Fase Final**: encerra o turno. Se o jogador tiver mais de **6 cartas na mão**, descarta até ficar com 6.

## Invocação de cartas

- O jogador tem direito a **uma invocação normal por turno**.
- **Invocar** coloca a carta virada para cima em **modo de ataque**.
- Qualquer carta pode ser invocada diretamente da mão, independentemente da hierarquia. Não existe sacrifício.
- Uma carta invocada neste turno **não pode trocar de posição** no mesmo turno.

## Posições de batalha

- **Ataque (virada para cima)**: a carta usa seu valor de **ataque**. Só cartas nesta posição podem atacar.
- **Defesa**: a carta usa seu valor de **defesa**. Não pode atacar.
- Cada carta pode trocar de posição **uma vez por turno**, somente na Fase Principal, e somente se não tiver sido invocada neste turno nem tiver atacado neste turno.

## Declaração de ataque

- Só é possível atacar a partir da Fase de Batalha.
- O jogador que começa a batalha **não pode atacar no primeiro turno**.
- Cada carta em modo de ataque pode atacar **uma vez por turno**.
- O atacante escolhe uma carta do oponente como alvo. Se **todos os slots de carta do oponente estão vazios**, o ataque é **direto** e todo o valor de ataque é descontado dos pontos de vida do oponente.
- Uma vez declarado, o ataque não pode ser cancelado, exceto por efeito de carta.

## Cálculo de dano

**Ataque contra carta em modo de ataque:**

| Situação | Resultado |
|---|---|
| Ataque do atacante **maior** que ataque do defensor | Defensor destruído. Oponente perde a **diferença** em pontos de vida. |
| Ataque do atacante **menor** que ataque do defensor | Atacante destruído. Jogador que atacou perde a **diferença** em pontos de vida. |
| Ataques **iguais** | Ambas destruídas. Ninguém perde pontos de vida. |

**Ataque contra carta em modo de defesa:**

| Situação | Resultado |
|---|---|
| Ataque do atacante **maior** que defesa do defensor | Defensor destruído. **Ninguém** perde pontos de vida. |
| Ataque do atacante **menor** que defesa do defensor | Ninguém é destruído. Jogador que atacou perde a **diferença** em pontos de vida. |
| Ataque **igual** à defesa | Ninguém é destruído. Ninguém perde pontos de vida. |

**Ataque direto:** oponente perde pontos de vida igual ao **ataque total** da carta atacante.

Cartas destruídas vão para o cemitério imediatamente após o cálculo.

## Cartas de campo

- A carta de campo **não entra no campo**: é consumida ao ser jogada, some e vai direto para o cemitério do dono. Não ocupa slot.
- O **efeito** dela fica ativo até o fim da batalha ou até outra carta de campo, de qualquer jogador, substituí-lo. Só existe um efeito de campo ativo por vez.
- O painel do HUD mostra o efeito de campo em vigor.
- Cartas da categoria **favorecida** recebem **buff** no ataque e na defesa.
- Cartas da categoria **prejudicada** recebem **nerf** no ataque e na defesa.
- Cartas **Neutras** não são afetadas por cartas de campo.
- O buff e o nerf são aplicados antes de qualquer cálculo de dano.

## Defesa

Quando a carta atacada está em modo de defesa, um escudo dourado e prateado com uma cruz surge em pé sobre ela, virado para o atacante. No golpe da espada ele recua, brilha e solta uma onda de choque. Se a carta aguenta, o escudo fica um instante e some; se ela é destruída, o escudo fica vermelho e cai girando.

## Carta abatida

Quando uma carta é destruída em combate, de qualquer lado, a arena treme por menos de meio segundo, forte no golpe e sumindo em seguida. A mão e o HUD não tremem.

## Jogadas do oponente

Toda carta que o oponente joga, invocação ou carta de campo, aparece grande no centro da tela por 2 segundos antes de ir para a arena. A legenda diz o que ele está fazendo e, na invocação, em que posição a carta entra. Assim o jogador lê nome, ATK, DEF e descrição antes de a carta descer na mesa. A carta aparece em 3D, igual às da sua mão: cartas foil (hierarquia 1) brilham com a aura e mostram o arco-íris enquanto balançam.

## Pausa

`Esc` durante a batalha pausa o jogo e abre uma janela de confirmação:

```
+-----------------------------------------------+
|   Deseja realmente abandonar essa batalha?    |
|                                               |
|   [ Não ]                         [ Sim ]     |
+-----------------------------------------------+
```

- **Pausa**: abertura, câmera, turno do computador e animações congelam. Um véu escuro cobre a batalha e só a janela recebe cliques e teclas.
- **Música**: a trilha de batalha sai em fade out e a do menu entra no volume do menu.
- **Não** (ou `Esc` de novo): fecha a janela, a batalha continua de onde parou e a música de batalha volta em fade in no ponto em que foi pausada.
- **Sim**: abandona a batalha e volta ao menu, que segue com a música que já está tocando. O botão usa a placa vermelha (`sprites/UiFrameButtonDanger.png`), por ser ação destrutiva.
- **Teclado**: setas esquerda e direita (ou `Tab`) trocam o foco, `Enter` ou `Espaço` confirmam. O foco começa no Não.
- Se uma carta estiver em inspeção, o primeiro `Esc` só sai da inspeção.
- Com a partida encerrada (vitória ou derrota), `Esc` volta direto ao menu, sem pergunta.

Implementação: `scripts/dialog/ConfirmDialog.*` (janela reutilizável), `BattleScene::Pause`/`Resume` e `Music::SetCategory(nome, resume)`, que guarda a faixa e a posição de cada categoria ao sair dela. Textos em `assets/translate.json` (`battle.abandon.*`).

