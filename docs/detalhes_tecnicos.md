# Detalhes técnicos

## Stack

- **Linguagem**: C++20.
- **Build**: CMake + Ninja. Scripts `build.sh` (compila) e `run.sh` (executa). Passo a passo no README.
- **Formatação**: clang-format, estilo Google, indentação 2 espaços, limite de 100 colunas (`.clang-format`).
- **Bibliotecas**: SDL3 (`release-3.4.18`), cgltf (`v1.15`, leitura de glTF/GLB), stb_image (PNG/JPEG), stb_truetype (fonte TTF/OTF) e dr_mp3 (decodificação de MP3). Todas baixadas via `FetchContent`; cgltf, stb e dr_libs são header-only.
- **Áudio**: `SDL_AudioStream` com callback; a trilha de `assets/ost.json` é decodificada sob demanda na thread de áudio e toca em loop, em todas as cenas, com ganho do próprio stream (padrão 30%). Cada faixa tem fade in e fade out de 2 s e há 3 s de silêncio entre faixas.
- **Renderização**: `SDL_GPU` (Metal no macOS, Vulkan no Linux, D3D12 no Windows). O menu e o HUD usam o renderer 2D do SDL criado sobre o mesmo device (`SDL_CreateGPURenderer`). O 3D usa o device diretamente, com shaders próprios, e desenha em uma textura alvo que o renderer 2D exibe como `SDL_Texture`.
- **Shaders**: fonte em `src/shaders/`. `card.metal` é embutido no binário e compilado pelo Metal em tempo de execução. `card.vert.glsl` e `card.frag.glsl` são o equivalente para Vulkan; o CMake compila para SPIR-V quando `glslc` está instalado e o jogo carrega `shaders/card.*.spv` ao lado do binário.
- **Janela**: 1280x720, tamanho fixo (sem `SDL_WINDOW_RESIZABLE`): não redimensiona, não maximiza e não entra em tela cheia.
- **Resolução lógica**: 1280x720 com letterbox (`SDL_SetRenderLogicalPresentation`), que continua cobrindo telas de alta densidade. Eventos de mouse são convertidos para essa resolução.
- **Plataforma**: computadores (macOS, Windows, Linux). Sem versão mobile ou console.

## Estrutura do projeto

```
CMakeLists.txt        alvo pontifice, FetchContent do SDL3
build.sh / run.sh     compilar / compilar e executar
src/
  main.cpp            janela, renderer, loop principal
  scenes/             uma pasta por cena
    Scene.h           interface Scene e SceneContext
    SceneManager.*    cena ativa e troca de cena
    menu/
      MenuScene.*     menu (docs/menu.md), primeira cena
    loading/
      LoadingScene.*  tela de loading (docs/loading.md): portão 3D abrindo no preto, pré-carrega a próxima cena
    battle/
      BattleScene.*   batalha: render 3D e mão de cartas
  scripts/            componentes reutilizáveis, uma pasta por componente
    text/
      Text.*          texto com a fonte do jogo (assets/PirataOne.ttf) via stb_truetype: atlas 16/32/64 px, ASCII e Latin-1; sem a fonte, cai na de depuração do SDL
    button/
      Button.*        botão base (hover, foco, clique)
      ButtonPlay.*    botão Jogar
      ButtonExit.*    botão Sair
    math/
      Math.h          Vec2/3/4, Mat4 (perspectiva, lookAt, rotação, escala)
    image/
      Image.*         decodifica PNG/JPEG para RGBA8 (stb_image)
    gpu/
      Gpu.*           pipeline SDL_GPU: malhas, texturas, passe com profundidade; InitDevice serve em qualquer thread
    model/
      Model.*         carrega .glb (cgltf): Parse (CPU, qualquer thread) e Upload (GPU); separa frente, verso e borda
    hand/
      Hand.*          mão em dock: layout em arco, hover, animação
    hud/
      Hud.*           HUD da batalha (docs/hud.md): carta de campo ativa e pontos de vida
    json/
      Json.*          leitor JSON mínimo (objetos, arrays, strings, números)
    colors/
      Colors.*        paleta lida de assets/colors.json; cores pedidas por papel ("panel.fill")
    clouds/
      Clouds.*        nuvens e névoa 2D em camadas (ruído gerado na CPU): 4 atrás e 3 na frente do texto do menu
    torch/
      Torch.*         tocha 3D: corpo iluminado, chama emissiva tremulando; FlamePosition dá onde pôr a luz
    music/
      Music.*         trilha sonora: lê assets/ost.json, decodifica MP3 (dr_mp3), toca em loop com fade e intervalo
  shaders/
    card.metal        shader Metal (macOS), embutido no binário
    card.*.glsl       shader GLSL para SPIR-V (Vulkan)
  assets/             dados das cartas em JSON, paleta de cores e ícone do jogo
    colors.json       paleta extraída do ícone e cores por papel (texto, fundo, borda, HUD, menu)
    ost.json          trilha sonora: volume (0 a 1) e lista de MP3 em sounds/, tocados em sequência
    PirataOne.ttf     fonte do jogo (tem acentos Latin-1)
    icon.png          ícone (1024x1024); vira ícone da janela/Dock em tempo de execução
    CardsGood.json
    CardsEvil.json
    CardsNeutral.json
    CardsField.json
  sounds/             músicas de fundo (MP3) listadas em assets/ost.json
  sprites/            texturas das cartas (PNG) e fontes de arte (PSD)
    CardBase*.png     molduras por categoria (Good, Evil, Field)
    CardBackBase.png  verso da carta
  icons/
    icon.ico          ícone do executável no Windows, gerado por tools/make_icons.py
    pontifice.rc      recurso do Windows que embute o icon.ico
  models/
    Card.glb          carta: frente, verso e borda metálica (2,51 x 3,5 x 0,075)
    Door.glb          portão da tela de loading (nós Wall e Door), gerado por tools/make_door.py
    Torch.glb         tocha (nós Torch e Flame), gerada por tools/make_torch.py; luz do loading e da arena
    ArenaRaw.glb      arena bruta de imagem-para-3D (referência; usada só com --raw)
    Arena.glb         arena procedural texturizada, gerada por tools/texture_arena.py
  art/
    Arena.png         ilustração de referência da arena (perspectiva)
    ArenaAtlas2.png   sprite sheet: tabuleiro completo, painel de pedra e tiras
    ArenaAtlas3.png   sprite sheet plano usado pela ferramenta: ladrilhos, painel de
                      madeira, bandas de metal, tábuas, cruz e placa com flor-de-lis
    ArenaAtlas.png    atlas gerado para a arena (2048x2048)
    Card.png          ilustração de referência da carta
    Door.jpg          ilustração de referência do portão da tela de loading
    DoorAtlas.png     atlas gerado para o portão (1024x1024): pedra procedural, madeira e ferro da arena
    torch.webp        ilustração de referência da tocha
    TorchAtlas.png    atlas gerado para a tocha (512x512, RGBA): madeira e ferro da arena, couro, brasas e chama
tools/
  texture_arena.py    gera Arena.glb e ArenaAtlas.png a partir de ArenaRaw.glb e Arena.png
  make_door.py        gera Door.glb e DoorAtlas.png (geometria procedural; madeira e ferro de ArenaAtlas.png)
  make_torch.py       gera Torch.glb e TorchAtlas.png (cabo, couro, cesta de ferro, brasas e chama)
  make_icons.py       gera src/icons/icon.ico a partir de src/assets/icon.png (usa sips)
cmake/
  embed.cmake         converte arquivo texto em array de bytes C
tests/                testes automatizados
```

## Dados das cartas

Cada arquivo em `src/assets` é um array JSON. Campos comuns a toda carta:

| Campo | Tipo | Descrição |
|---|---|---|
| `code` | string | Identificador único. Prefixo pela categoria: `G`, `E`, `N`, `F`. |
| `name` | string | Nome exibido. |
| `description` | string | Texto curto da carta. |
| `category` | string | `G`, `E`, `N` ou `F`. |
| `categoryName` | string | `good`, `evil`, `neutral` ou `field`. |
| `hierarchy` | int | 1 a 4 para cartas de batalha, 0 para cartas de campo. |
| `hierarchyName` | string | Nome da hierarquia (ex.: `Arcanjos`). Ausente em cartas de campo. |
| `image`, `imageFile`, `imageUrl` | string | Nome, arquivo e caminho da textura. |
| `attributes` | objeto | Atributos específicos do tipo de carta. |

`attributes` de carta de batalha: `attack`, `defense`, `odds`, `combinable`.

`attributes` de carta de campo: `odds`, `category` (`good` ou `evil`, lado favorecido), `buff`, `nerf`.

Os JSON são carregados uma vez na inicialização e mantidos em memória como structs. A textura de cada carta é carregada sob demanda a partir de `imageUrl`.

## Campo em 3D

O campo é um **elemento 3D**, não uma imagem plana.

- É uma malha (mesa ou arena) posicionada no plano horizontal, com os **22 slots** modelados como regiões demarcadas na superfície: 10 slots de carta e 1 slot de campo para cada lado.
- Cada slot tem uma **posição e rotação no espaço 3D**. Uma carta colocada no slot é um objeto 3D posicionado nessas coordenadas.
- As cartas em campo são **quads texturizados com dupla face**: frente com o sprite da carta, verso com `CardBackBase.png`. Carta virada para baixo é renderizada com rotação de 180° no eixo longitudinal.
- Carta em **modo de defesa** fica girada 90° no plano da mesa (deitada), carta em **modo de ataque** fica em pé em relação ao jogador.
- A câmera fica em **perspectiva**, atrás e acima do lado do jogador, olhando para o centro do campo. Toda a arena é visível sem rolagem.
- O monte é renderizado como uma **pilha 3D** cuja altura diminui conforme as cartas são compradas.

## Arena em 3D

A arena é `models/Arena.glb`, desenhada na cena de batalha com câmera própria (acima e à frente do lado do jogador), antes da mão. Fundo da cena em `#444`.

`tools/texture_arena.py` (só biblioteca padrão do Python) gera o modelo e o atlas. A geometria é **procedural e low poly** (196 triângulos): um **prisma trapezoidal** com base retangular maior embaixo, topo retangular menor (onde ficam os ladrilhos e a canaleta) e laterais planas inclinadas com altura de **um ladrilho** e base de **2.5 x 2.5** (topo 1.9 x 1.9; ≈40° da vertical), seguindo a especificação do tabuleiro (visão de cima: b, a, canaleta, a, b; ladrilhos quadrados, canaleta com a largura de um ladrilho e rebaixada 0.2 ladrilho, moldura metálica em volta):

- **duas plataformas** elevadas, fundo (lado do oponente, z < 0) e frente (lado do jogador, z > 0), cada uma com tabuleiro de **5 colunas x 2 linhas** e moldura metálica;
- **canaleta central** com a largura de um ladrilho e rebaixada 0.2 ladrilho (paredes com ladrilhos escuros e lábio de metal, piso com ladrilhos escuros e cruz), fechada nas pontas por triângulos finos nos planos das paredes;
- oito laterais inclinadas (frente, fundo e três trechos em cada lado: plataforma, canaleta baixa, plataforma), indo direto do topo até a base maior, cada uma com **painel de madeira rebaixado** dentro da moldura de metal (relevo); frente e fundo levam cruz ao centro, os trechos laterais são lisos;
- **cantoneiras**: pirâmides triangulares com placa de flor-de-lis nas pontas dos painéis das plataformas (quinas externas e bordas da canaleta);
- base retangular com tábuas.

As dimensões ficam em constantes `ARENA_*` no início da seção de malha (`ARENA_TILE` é o lado do ladrilho quadrado; largura, profundidade das plataformas e largura da canaleta derivam dele, `ARENA_LOW_Y` fica `ARENA_CHANNEL_DEPTH_FRACTION` ladrilho abaixo de `ARENA_HIGH_Y`, a altura da lateral `ARENA_SIDE_H` é um ladrilho, e `ARENA_BASE_W` fixa a largura da base; `ARENA_SIDE_RUN` e `ARENA_SIDE_ANGLE_DEG` são derivados). A malha bruta `ArenaRaw.glb` (imagem-para-3D, irregular) continua disponível com `--raw`, que classifica as faces pela normal.

Etapas da texturização:

1. Recorta os elementos do sprite sheet `art/ArenaAtlas3.png` em coordenadas fixas (medidas por detecção de componentes): 25 ladrilhos do bloco xadrez, painel de madeira emoldurado, banda de metal rebitada, bloco de tábuas, cruz e placa com flor-de-lis. Pixels quase pretos dos ornamentos são tratados como transparentes.
2. Monta um atlas 2048x2048 com três regiões. Topo: tabuleiro 5 colunas x 2 linhas por nível com ladrilhos sorteados do bloco (variedade real), faixa central da canaleta (paredes com ladrilhos escuros pequenos e lábio de metal, piso com ladrilhos escuros e cruz), moldura rebitada e placas nos quatro cantos. Laterais: duas faixas com o painel de madeira emoldurado, uma com cruz ao centro (frente e fundo) e outra lisa (metades laterais). Fundo: tábuas.
3. Atribui UV por face: plataformas, paredes e piso da canaleta em projeção planar "desdobrada" ao longo de Z sobre a região do topo (plataforma, parede, piso, parede, plataforma), cada lateral recebe o painel de madeira com a moldura geométrica casada à moldura do sprite (espessura constante, independente da altura do painel), pirâmides usam a placa triangular guardada no canto inferior direito do atlas, e o fundo recebe as tábuas.
4. Grava o GLB com o atlas PNG embutido.

Para regenerar depois de mudar a ilustração ou o modelo bruto: `python3 tools/texture_arena.py`. O passo seguinte é alinhar os 10 slots de cada lado com os ladrilhos do tabuleiro.

## Mão em 3D (dock)

As cartas na mão são **elementos 3D**, dispostos como um **dock**. Implementado em `src/scripts/hand/Hand.*`.

- Cada carta é uma instância de `models/Card.glb`. A frente recebe o sprite da categoria (`CardFrontGood.png`, `CardFrontEvil.png`, `CardFrontField.png`) e o verso recebe `CardBack.png`; a borda usa a cor metálica do próprio modelo.
- As cartas ficam na parte inferior da tela, em arco: cada uma desce com o quadrado da distância ao centro e gira levemente em Z. O espaçamento diminui quando a mão passa da largura máxima.
- **Hover**: a carta sob o cursor sobe, aproxima-se da câmera e aumenta de escala. As vizinhas se afastam com força decrescente (1, 1/2, 1/3...). Tudo animado com interpolação exponencial, independente do frame rate.
- **Entrada**: cada carta adicionada à mão (`Hand::Add`) desliza de fora da tela, pela esquerda, até o seu lugar, a velocidade constante (13 unidades/s, suavizado nas pontas), então quem vai mais longe demora mais e não ultrapassa a anterior; cartas adicionadas em sequência entram uma a uma, com 0,12 s entre elas. `Hand::DelayDeal` adia a próxima entrada (a batalha espera o fade in).
- A carta destacada é desenhada por último, por cima das demais.
- **Detecção**: os quatro cantos da carta são projetados para a tela com a matriz do quadro anterior e testados contra o cursor (ponto em quadrilátero convexo). Entre cartas sobrepostas vence a de centro mais próximo do cursor.
- Parâmetros (altura, espaçamento, arco, escala, elevação, velocidade) ficam em `Hand::Layout`.

Pendente: arrastar carta para um slot, animação ao descartar, mão do oponente com verso para a câmera.

## Virada de campo no turno do computador

Quando o turno passa para o computador, a **câmera orbita 180°** em torno do centro do campo, de modo que o jogador passa a ver a arena pelo ponto de vista do oponente.

- A rotação é uma **animação contínua** (cerca de 1 segundo, com easing), não um corte.
- Durante a animação, a entrada do jogador fica **bloqueada**.
- Ao final, o lado do computador aparece na parte de baixo da tela e o lado do jogador na parte de cima. As cartas viradas para baixo do jogador continuam mostrando o verso.
- O dock do computador aparece na parte inferior, com o verso das cartas. O dock do jogador fica no topo, também com verso.
- O **HUD não gira**: carta de campo ativa segue no canto superior esquerdo e os pontos de vida no canto superior direito, com os nomes "Você" e "Oponente" inalterados.
- Quando o turno volta ao jogador, a câmera orbita os 180° de volta, com a mesma animação.

## Cenas

O jogo é organizado em cenas. Só uma cena fica ativa por vez, e o loop principal repassa a ela eventos, atualização e desenho.

- `Scene`: interface com `Preload` (carga pesada, inclusive envio à GPU, chamada pela tela de loading em outra thread), `OnEnter`, `OnExit`, `HandleEvent`, `Update` e `Render`.
- `SceneContext`: estado compartilhado. A cena pede troca escrevendo em `next_scene` e pede saída com `quit`. `loading_target` diz à tela de loading qual cena preparar; `preloaded` entrega uma instância já carregada (o `SceneManager` a usa no lugar de criar outra); `fade_in_seconds` faz a nova cena entrar do preto.
- `SceneManager`: guarda a cena ativa e aplica as trocas pedidas depois de cada evento ou atualização. Desenha o véu do fade in por cima da cena nova. `Create` é a fábrica de cenas.
- Ordem: `MenuScene` (inicial), `LoadingScene` e `BattleScene`. O botão Jogar abre a tela de loading, que carrega a batalha enquanto o portão abre e então troca com fade in (docs/loading.md). `Esc` na batalha volta direto ao menu.

## Abertura da batalha

Ao entrar na batalha (depois do fade in da tela de loading), por 1,6 s a arena **emerge de uma névoa densa** enquanto a **câmera se aproxima**, com a mesma curva suave (smoothstep) para as duas coisas:

- A arena começa 3,2 unidades abaixo da posição final e sobe. A névoa de altura (`fog_full_y` na base) já a esconde enquanto está embaixo.
- Cinco camadas extras de névoa (`kIntroMistLayers`) ficam em alturas fixas do mundo, da base ao topo da arena, com `mist_density` alto no início (manto fechado) e caindo a zero; a opacidade cai com a raiz do tempo restante. A arena atravessa camada por camada ao subir; no fim restam só as duas camadas normais da base.
- A câmera parte de `(0, 16, 22)` e chega à posição do jogador `(0, 10,5, 12,2)`, sempre olhando o centro da arena.
- Enquanto isso, as 5 cartas da mão inicial entram pela esquerda da tela, uma a uma, a partir de 0,8 s.
- `PONTIFICE_ARENA_EYE` (depuração) fixa a câmera e pula a abertura.

## Passe 3D e composição

Ordem de um quadro na cena de batalha:

1. `SDL_FlushRenderer` envia o que o renderer 2D já enfileirou.
2. `Gpu::BeginFrame` abre um passe de render na textura alvo (1280x720, RGBA8) com buffer de profundidade (D24 ou D32), limpa ambos.
3. Cada `Gpu::Draw` envia uniforms (MVP e modelo no vértice; cor e flag de textura no fragmento), liga textura e sampler, e desenha a malha indexada. Malhas opacas (`kMesh`) usam o pipeline com escrita de profundidade; névoa, chama e halo usam um segundo pipeline igual mas **sem escrita de profundidade**, então testam contra o que é opaco sem esconder o que for desenhado atrás depois (uma camada de névoa à frente não corta a chama).
4. `Gpu::NewLayer` fecha o passe e abre outro mantendo a cor e limpando só a profundidade. É assim que a mão (câmera fixa) fica sempre por cima da arena (câmera do jogador).
5. `Gpu::EndFrame` fecha o passe e submete o command buffer.
6. A cena desenha a textura alvo com `SDL_RenderTexture` e o HUD por cima com o renderer 2D.

O fragment shader calcula a normal pela derivada da posição, então funciona em malhas sem normais e ilumina as duas faces. Texturas recebem mipmaps gerados na GPU e filtro anisotrópico.

Modos de desenho (`Gpu::DrawMode`): `kMesh` (iluminado: luz direcional opcional, até **duas luzes pontuais** quentes com tremular, névoa, vinheta), `kMist` (quad de névoa com alpha de ruído animado), `kEmissive` (cor da textura direto, sem luz nem névoa), `kFlame` (chama procedural animada a partir do UV do quad) e `kFlameGlow` (a mesma chama como halo oval difuso).

## Tochas e iluminação

A luz das cenas 3D vem das **tochas** (`models/Torch.glb`, `src/scripts/torch/Torch.*`): cada chama é uma luz pontual (`Atmosphere::point_pos` e `point2_pos`, posição de `Torch::FlamePosition`), de cor quente e com o tremular do shader. A luz direcional fica em zero e o ambiente baixo e frio.

- **Loading**: duas tochas presas nos pilares do portão (x = ±1,72, altura 1,75, inclinadas 0,22 rad para fora), intensidade 0,9 cada.
- **Batalha**: duas tochas fincadas no chão à esquerda e à direita da arena (x = ±6,4, z = −1, escala 2,6), intensidade 2,6 cada; sobem junto com a arena na abertura. A mão continua com a iluminação própria, para as cartas ficarem legíveis.
- A chama (nó `Flame`) é um quad sempre na vertical e virado para a câmera, desenhado em `kFlame` (chama procedural no shader: gota deformada por três camadas de ruído que sobem, línguas no topo, núcleo claro na base, fase por posição) mais um halo maior e translúcido um pouco atrás. Corpo da tocha é desenhado antes das camadas de névoa e a chama depois delas.

## Depuração

Variáveis de ambiente lidas em `main.cpp`:

| Variável | Efeito |
|---|---|
| `PONTIFICE_SCENE=battle` | Abre direto na cena de batalha; `=loading` abre na tela de loading (que segue para a batalha) |
| `PONTIFICE_MOUSE=x,y` | Injeta um movimento de mouse em coordenadas lógicas a cada quadro |
| `PONTIFICE_SCREENSHOT=arquivo.bmp` | Salva um quadro em BMP e encerra (quadro 60 por padrão) |
| `PONTIFICE_SCREENSHOT_FRAME=n` | Número do quadro salvo por `PONTIFICE_SCREENSHOT` |
| `PONTIFICE_SCREENSHOT_TIME=s` | Em vez do quadro, salva o primeiro quadro depois de `s` segundos (sem vsync a taxa de quadros varia; use isto para animações) |
| `PONTIFICE_LOADING_TIME=s` | Linha do tempo da tela de loading já começa em `s` segundos (porta num ponto fixo da abertura) |
| `PONTIFICE_ARENA_EYE=x,y,z` | Posição da câmera da arena, para inspecionar o modelo por outro ângulo |

Exemplo: `PONTIFICE_SCENE=battle PONTIFICE_MOUSE=640,560 PONTIFICE_SCREENSHOT=/tmp/hand.bmp ./build/pontifice`

## HUD

O HUD é renderizado em **2D, por cima da cena 3D**, em uma passada separada. Não é afetado pela câmera nem pela virada de campo. Veja a seção [HUD](hud.md).
