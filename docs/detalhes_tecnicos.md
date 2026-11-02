# Detalhes técnicos

## Stack

- **Linguagem**: C++20.
- **Build**: CMake + Ninja. Scripts `build.sh` (compila) e `run.sh` (executa). Passo a passo no README.
- **Formatação**: clang-format, estilo Google, indentação 2 espaços, limite de 100 colunas (`.clang-format`).
- **Bibliotecas**: SDL3 (`release-3.4.18`), cgltf (`v1.15`, leitura de glTF/GLB), stb_image (PNG/JPEG), stb_truetype (fonte TTF/OTF) e dr_mp3 (decodificação de MP3). Todas baixadas via `FetchContent`; cgltf, stb e dr_libs são header-only.
- **Áudio**: `SDL_AudioStream` com callback; a trilha de `assets/ost.json` é decodificada sob demanda na thread de áudio e toca em loop. As faixas ficam em **categorias**: `"ost"` (menu e loading: HowlsOfTheNight, RuinsOfYore, DecayTemples, ForgottenDungeons) e `"battle"` (batalha: FastBattle, ShoutSword, StartedFight, GloryAndVictory). Na troca de cena o `SceneManager` chama `Music::SetCategory` e `Music::SetScene`: a faixa atual sai em **fade out** de 1,5 s e a primeira da nova categoria entra em **fade in** de 2 s; o volume é **por cena** (`"volume": {"menu": 0.15, "battle": 0.25}`; loading usa o do menu) e caminha em rampa de 1 s. Cada faixa tem um **ganho de normalização** em ost.json (`{"file", "gain"}`, medido com `PONTIFICE_AUDIO_ANALYZE=1`, que decodifica cada MP3 e sugere o ganho para RMS de referência −20 dBFS), então o volume por cena significa o mesmo em todas. O **botão de som** (`scripts/soundbutton`, menu e batalha) chama `Music::ToggleMuted`: volume a zero e de volta em rampa. Tudo na thread de áudio. Cada faixa tem fade in e fade out de 2 s e há 3 s de silêncio entre faixas.
- **Renderização**: `SDL_GPU` (Metal no macOS, Vulkan no Linux, D3D12 no Windows). O menu e o HUD usam o renderer 2D do SDL criado sobre o mesmo device (`SDL_CreateGPURenderer`). O 3D usa o device diretamente, com shaders próprios, e desenha em uma textura alvo que o renderer 2D exibe como `SDL_Texture`.
- **Shaders**: fonte em `src/shaders/`. `card.metal` é embutido no binário e compilado pelo Metal em tempo de execução. `card.vert.glsl` e `card.frag.glsl` são o equivalente para Vulkan; o CMake compila para SPIR-V quando `glslc` está instalado e o jogo carrega `shaders/card.*.spv` ao lado do binário.
- **Janela**: tela cheia sem borda na resolução do monitor por padrão (`SDL_WINDOW_FULLSCREEN` + modo desktop), sem `SDL_WINDOW_RESIZABLE`. `PONTIFICE_WINDOWED=1` abre em janela 1280x720 (depuração).
- **Resolução lógica**: 1920x1080 com letterbox (`SDL_SetRenderLogicalPresentation`). Eventos de mouse são convertidos para essa resolução. A interface 2D fica numa **grade de 16 px** (`ui::Grid(n)` em `scripts/ui/Ui.h`: 1920 = 120 células, 1080 = 67,5): margens de 1 célula, painéis do HUD 24x6 e 34x6, contadores com 3 de altura e largura arredondada à grade (`ui::SnapUp`), botões do menu 22x5 com 2 de vão, botão da batalha 21x5, botão de som 3x3, tooltip com imagem de 14 células. Tamanhos de texto seguem `ui::kScale` (1,5; medidos em 1280x720).
- **Moldura da interface**: duas placas de pedra com moldura de metal e estrelas nos cantos (de `art/ui.jpg`; 768x768), cada uma dividida em **9 tiles iguais** (terços): `tl t tr / ml m mr / bl b br`. `sprites/UiFrameButton.png` (azulada, como na arte) é o fundo dos botões (`frame::Kind::kButton`: Jogar, Nível, Sair, Passar turno, som); `sprites/UiFrameWindow.png` (dessaturada 75%, escurecida a 68%, leve tom de aço) é o fundo de janelas, painéis do HUD e tooltip (`frame::Kind::kWindow`). `scripts/frame/Frame.*` monta cada elemento com tiles: linha de cima `tl` + N vezes `t` + `tr`, de baixo `bl` + N vezes `b` + `br` e, em painéis altos, linhas `ml` + N vezes `m` + `mr` no meio. A escala vem da altura (tile = altura / linhas; linhas = altura / 40 px, mínimo 2) e N cobre a largura com tiles quase quadrados. Estados de botão escurecem a placa (normal 80%, pressionado 60%, destaque 100%). `tools/make_ui_frame.py` gera os dois PNG: converte o JPG (`sips`), torna o fundo branco transparente, tira a margem, refaz o centro em espelho 2x2 (terços simétricos, tiles sem emenda) e deriva a variante Window.
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
    language/
      LanguageScene.* escolha de idioma (docs/idioma.md), primeira cena
    menu/
      MenuScene.*     menu (docs/menu.md)
    loading/
      LoadingScene.*  tela de loading (docs/loading.md): portão 3D abrindo no preto, pré-carrega a próxima cena
    battle/
      BattleScene.*   batalha: render 3D e mão de cartas
  scripts/            componentes reutilizáveis, uma pasta por componente
    ui/
      Ui.h            escala de texto (ui::Px) e grade de 16 px da interface (ui::Grid, ui::SnapUp)
    frame/
      Frame.*         moldura da interface: UiFrameButton.png (botões), UiFrameWindow.png (janelas, painéis, tooltip) e UiFrameButtonDanger.png (botões de ação destrutiva, pedra vermelha) em 9 tiles iguais montados por elemento
    text/
      Text.*          texto com a fonte do jogo (assets/PirataOne.ttf) via stb_truetype: atlas 16/32/64 px, ASCII e Latin-1; sem a fonte, cai na de depuração do SDL
    button/
      Button.*        botão base (hover, foco, clique)
      ButtonPlay.*    botão Jogar
      ButtonExit.*    botão Sair
      ButtonAction.*  botão genérico com rótulo e ação (Passar turno, Nível do menu)
      ButtonLanguage.* botão da cena de idioma: bandeira + nome, define o idioma
    dialog/
      ConfirmDialog.* janela modal de confirmação com [Não] e [Sim] (pausa da batalha)
    flag/
      Flag.*          bandeiras (EUA, Espanha, Brasil) com primitivas do renderer
    i18n/
      I18n.*          textos em pt/en/es (assets/translate.json): Get, GetOr, Format
    math/
      Math.h          Vec2/3/4, Mat4 (perspectiva, lookAt, rotação, escala)
    image/
      Image.*         decodifica PNG/JPEG para RGBA8 (stb_image)
    gpu/
      Gpu.*           pipeline SDL_GPU: malhas, texturas, passe com profundidade; InitDevice serve em qualquer thread
    model/
      Model.*         carrega .glb (cgltf): Parse (CPU, qualquer thread) e Upload (GPU); separa frente, verso e borda; lê baseColor e normalTexture
    hand/
      Hand.*          mão em dock: layout em arco, hover, animação, seleção e inspeção
    board/
      Board.*         um lado do campo (jogador ou oponente): 10 slots alinhados aos ladrilhos, destaque, cartas em 3D, investida e descarte
    soundbutton/
      SoundButton.*   botão com ícone de alto-falante (primitivas) que liga/desliga a música
    sword/
      Sword.*         efeito de golpe de espada nos ataques: espada procedural (caixas coloridas) que desce sobre o alvo e rastro de corte emissivo
    config/
      Config.*        ajustes de cena em JSON (assets/Light.json, Fog.json, Shadow.json) por caminho com pontos, com padrão no código
    rules/
      Rules.*         regras de combate sem SDL: carta de campo ativa, ataque/defesa efetivos, cálculo de dano
    ai/
      Ai.*            oponente (Vs PC): níveis (peso do monte por hierarquia) e decisões por heurística
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
    deck/
      Deck.*          monte de cartas: embaralhar (Fisher-Yates, semente opcional), comprar do topo; sem SDL
    cards/
      Cards.*         dados das cartas (assets/Cards*.json) e montagem do monte por peso `odds`
    cardface/
      CardFace.*      frente da carta: moldura da categoria + nome, ATK/DEF e descrição desenhados (CPU)
    music/
      Music.*         trilha sonora: lê assets/ost.json, decodifica MP3 (dr_mp3), toca em loop com fade e intervalo
  shaders/
    card.metal        shader Metal (macOS), embutido no binário
    card.*.glsl       shader GLSL para SPIR-V (Vulkan)
    card_shadow.frag.glsl  fragmento vazio do passe de sombra (Vulkan)
  assets/             dados das cartas em JSON, paleta de cores e ícone do jogo
    colors.json       paleta extraída do ícone e cores por papel (texto, fundo, borda, HUD, menu)
    ost.json          trilha sonora: volume por cena (menu 0,15; batalha 0,25) e categorias "ost" (menu/loading) e "battle" de MP3 em sounds/
    PirataOne.ttf     fonte do jogo (tem acentos Latin-1)
    icon.png          ícone (1024x1024); vira ícone da janela/Dock em tempo de execução
    CardsGood.json
    CardsEvil.json
    CardsNeutral.json
    CardsField.json
  sounds/             músicas de fundo (MP3) listadas em assets/ost.json
  sprites/            texturas das cartas (PNG), molduras da interface (UiFrameButton.png, UiFrameWindow.png), logotipo (Logo.png) e fontes de arte (PSD)
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
    ui.jpg            ilustração de referência da moldura da interface (placa de pedra); vira sprites/UiFrameButton.png e UiFrameWindow.png
    logo.png          logotipo PONTIFICE em pedra, com margens brancas; vira sprites/Logo.png (menu)
    Door.jpg          ilustração de referência do portão da tela de loading
    DoorAtlas.png     atlas gerado para o portão (1024x1024): pedra procedural, madeira e ferro da arena
    torch.webp        ilustração de referência da tocha
    TorchAtlas.png    atlas gerado para a tocha (512x512, RGBA): madeira e ferro da arena, couro, brasas e chama
tools/
  texture_arena.py    gera Arena.glb e ArenaAtlas.png a partir de ArenaRaw.glb e Arena.png
  make_logo.py        gera sprites/Logo.png (logotipo do menu) a partir de art/logo.png: fundo branco transparente, recorte com margem
  make_ui_frame.py    gera sprites/UiFrameButton.png e UiFrameWindow.png (placas da interface) a partir de art/ui.jpg: fundo transparente, placa sem a margem, centro em tile; Window mais escura e cinza
  make_door.py        gera Door.glb e DoorAtlas.png (geometria procedural; madeira e ferro de ArenaAtlas.png)
  make_torch.py       gera Torch.glb e TorchAtlas.png (cabo, couro, cesta de ferro, brasas e chama)
  normalmap.py        gera normal map (luminância -> Sobel) da textura embutida e grava como normalTexture no GLB; chamado pelos três geradores
  make_icons.py       gera src/icons/icon.ico a partir de src/assets/icon.png (usa sips)
cmake/
  embed.cmake         converte arquivo texto em array de bytes C
tests/                testes automatizados (ctest --test-dir build)
  deck_test.cpp       monte: embaralhar preserva cartas, é determinístico por semente, compra tira do topo
  rules_test.cpp      regras de dano, carta de campo, decisões da IA e pesos dos níveis
```

## Dados das cartas

Cada arquivo em `src/assets` é um array JSON. Campos comuns a toda carta:

| Campo | Tipo | Descrição |
|---|---|---|
| `code` | string | Identificador único. Prefixo pela categoria: `G`, `E`, `N`, `F`. |
| `name` | string | Nome em inglês (ex.: `Saint Michael`). O nome exibido vem de `assets/translate.json` (`card.<code>.name`) no idioma escolhido; este campo é o padrão quando falta a chave. |
| `description` | string | Texto curto da carta, em inglês. O exibido vem de `card.<code>.description` no `translate.json`. |
| `category` | string | `G`, `E`, `N` ou `F`. |
| `categoryName` | string | `good`, `evil`, `neutral` ou `field`. |
| `hierarchy` | int | 1 a 4 para cartas de batalha, 0 para cartas de campo. |
| `hierarchyName` | string | Nome da hierarquia em inglês (ex.: `Archangels`). O exibido vem de `hierarchy.<categoryName>.<hierarchy>` no `translate.json`. Ausente em cartas de campo. |
| `image`, `imageFile`, `imageUrl` | string | Nome, arquivo e caminho da ilustração (`imageUrl` relativo ao binário, ex.: `sprites/neutral/N301.png`). O jogo usa `imageUrl`; se o arquivo não existe, a janela da imagem fica em branco. |
| `attributes` | objeto | Atributos específicos do tipo de carta. |

`attributes` de carta de batalha: `attack`, `defense`, `odds`, `combinable`.

`attributes` de carta de campo: `odds`, `category` (`good` ou `evil`, lado favorecido), `buff`, `nerf`.

Os JSON são carregados uma vez na inicialização e mantidos em memória como structs. As ilustrações (`imageUrl`) são lidas no carregamento da batalha e entram na frente composta de cada carta.

## Campo em 3D

O campo é um **elemento 3D**, não uma imagem plana.

- É uma malha (mesa ou arena) posicionada no plano horizontal, com os **22 slots** modelados como regiões demarcadas na superfície: 10 slots de carta e 1 slot de campo para cada lado.
- Cada slot tem uma **posição e rotação no espaço 3D**. Uma carta colocada no slot é um objeto 3D posicionado nessas coordenadas.
- As cartas em campo são **quads texturizados com dupla face**: frente com o sprite da carta, verso com `CardBackBase.png`. Toda carta em campo fica virada para cima.
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

- Arena, portão e tocha têm **normal map** gerado de suas texturas (`tools/normalmap.py`, embutido no GLB como `normalTexture`; a arena com força menor). Cada carta é uma instância de `models/Card.glb`, com **normal map** na frente (`CardFrontNormalMap.png`, 1600x2230 com cantos transparentes: ao carregar, a transparência vira normal plana e o mapa é reduzido para 800x1115, a resolução da frente composta) e no verso (`CardBackNormalMap.png`): relevo da moldura e dos entalhes, reagindo às luzes e ao hover/inspeção. A frente é uma textura **composta por carta** (`cardface`: moldura da categoria com os textos por cima, ver "Frente da carta"); o verso recebe `CardBack.png`; a borda usa a cor metálica do próprio modelo.
- **Monte real**: no `Preload` da batalha as cartas de `assets/Cards*.json` são carregadas, o monte de 40 é montado por peso `odds` (no máximo 2 cópias por carta, 1 das de hierarquia 1) e **embaralhado** (`deck::Deck`); as 5 da mão inicial são compradas e compostas ali, antes de qualquer animação. A tecla D compra a próxima do monte.
- As cartas ficam na parte inferior da tela, em arco: cada uma desce com o quadrado da distância ao centro e gira levemente em Z. O espaçamento diminui quando a mão passa da largura máxima.
- **Hover**: a carta sob o cursor sobe, aproxima-se da câmera e aumenta de escala. As vizinhas se afastam com força decrescente (1, 1/2, 1/3...). Tudo animado com interpolação exponencial, independente do frame rate.
- **Entrada**: cada carta adicionada à mão (`Hand::Add`) desliza de fora da tela, pela esquerda, até o seu lugar, a velocidade constante (13 unidades/s, suavizado nas pontas), então quem vai mais longe demora mais e não ultrapassa a anterior; cartas adicionadas em sequência entram uma a uma, com 0,12 s entre elas. `Hand::DelayDeal` adia a próxima entrada (a batalha espera o fade in).
- **Gestos** (clicar numa carta e arrastar 24 px na vertical): para **cima** seleciona ou desseleciona a carta (várias podem ficar selecionadas; ficam erguidas e aproximadas, e não descem com a mão recolhida; `Hand::Selected()` lista os índices). Para **baixo** inspeciona (uma por vez): a carta vai animada para o centro da tela, ampliada (a z = 4,2, câmera em z = 10), e responde ao arrasto do mouse: horizontal **gira em Y** (360°, mostra o verso; fica onde parou) e vertical **inclina um pouco em X** (±0,35 rad, volta sozinha ao soltar); a **roda do mouse** dá zoom entre 75% e 100% da altura da tela (começa em 80%, passos de 5%); enquanto isso a mão não reage ao mouse. **Clique na carta** (ou Espaço) a **vira** com meia-volta animada, mostrando o verso (`CardBack.png`); clique fora dela, botão direito ou Esc encerram a inspeção e a carta volta animada para a mão.
- A carta destacada é desenhada por último, por cima das demais; a inspecionada, depois dela.
- **Detecção**: os quatro cantos da carta são projetados para a tela com a matriz do quadro anterior e testados contra o cursor (ponto em quadrilátero convexo). Entre cartas sobrepostas vence a de centro mais próximo do cursor.
- Parâmetros (altura, espaçamento, arco, escala, elevação, velocidade) ficam em `Hand::Layout`.

Pendente: mão do oponente com verso para a câmera.

## Partida contra o computador

Implementada em `BattleScene` com `scripts/board/Board.*` (um `Board` por lado), `scripts/rules/Rules.*` (regras puras) e `scripts/ai/Ai.*` (oponente). Segue docs/batalha.md e docs/campo.md.

**Início**: cada lado monta um monte de 40 (o do computador conforme o nível, ver abaixo) e compra 5. Quando as tochas acendem (3,2 s), um sorteio decide quem começa (`PONTIFICE_STARTER=player|enemy` força). Quem começa não compra nem ataca no primeiro turno.

**Seu turno** (botão **Passar turno** no centro superior da tela, logo abaixo da dica, visível sempre que você pode agir):

1. **Selecionar a carta**: arrastar uma carta da mão para cima a seleciona (uma por vez). Os slots vazios do seu lado ficam levemente destacados (quad translúcido pulsando) e aparece a dica do que fazer. Toda carta entra **virada para cima, em modo de ataque**; não existe carta virada para baixo.
2. **Escolher o slot**: o slot sob o mouse recebe uma **moldura quadrada amarela** (`board.slotHover`). A detecção projeta os quatro cantos do ladrilho na tela com a câmera da arena (`math::PointInConvexQuad`). Um clique sobre uma carta da mão nunca conta como clique no slot atrás dela.
3. **Carta vai para a arena**: `Hand::Remove` tira a carta da mão (as outras deslizam para fechar o espaço) e `Board::Place` a coloca no slot: desce animada (0,45 s) de cima e à frente do slot até deitar sobre o ladrilho, topo para o lado do oponente. Uma invocação normal por turno.
4. **Slot ocupado**: invocar sobre um slot com carta descarta a anterior (`Board::Discard`: sobe e encolhe até sumir; cemitério +1).
5. **Carta de campo** selecionada: clicar em qualquer ladrilho da arena a ativa (`PlayField`): a carta é consumida e conta no seu cemitério na hora, sem ocupar slot; o efeito vai para o painel do HUD (frente composta pelo `cardface`), substitui o anterior e passa a valer o buff/nerf em todo cálculo (`rules::EffectiveAttack/Defense`). Não gasta a invocação do turno.
6. **Trocar posição**: sem carta selecionada, clicar numa carta sua no campo alterna **ataque <-> defesa** (`Board::TogglePosition`, giro animado de 90° no plano da mesa). (A regra "não troca de posição no turno em que entrou" vale para a IA; para o jogador a troca é livre no seu turno, por decisão de design.)
7. **Atacar**: **arrastar** uma carta sua em ataque (que ainda não atacou neste turno) até uma carta do oponente a ataca; se o lado dele está vazio, soltar sobre qualquer ladrilho dele é **ataque direto**. Durante o arrasto uma linha amarela liga a carta ao mouse e o alvo ganha a moldura. A carta faz uma **investida** em arco (`Board::Lunge`, 0,5 s) e no impacto (0,25 s) `rules::Resolve` aplica as tabelas de dano: destruição vai para o cemitério, dano para o HUD. No impacto toca o **golpe de espada** (`scripts/sword/Sword.*`): uma espada procedural (lâmina de aço, guarda e pomo dourados, cabo de couro; caixas sem textura, iluminadas pelas tochas) surge acima do alvo vinda do lado do atacante, desce num golpe de 0,19 s até a carta e some; no impacto um **rastro de corte** branco emissivo risca o plano da carta na diagonal e se apaga em 0,3 s, e a carta atingida **pisca em vermelho** (`Board::Flash`). Vale para os dois lados, inclusive no ataque direto (golpe no meio do lado vazio). Uma carta que **já atacou** no turno fica **menor (82%), em preto e branco (dessaturação no shader, `Gpu::SetDesaturate`) e com 30% de opacidade** até o próximo turno do dono (`Board::Card::spent`, `spent_scale`/`spent_alpha` em `Board::Layout`); sob o mouse volta a opaca para leitura.
8. **Ver uma carta no campo**: passar o mouse sobre qualquer carta em campo (sua ou do oponente, em qualquer momento) a **amplia** (1,45x, sobe um pouco, desenhada por cima das vizinhas; `Board::SetHoverCard`) e abre um **tooltip** ao lado do cursor (`RenderTooltip`): frente composta da carta à esquerda (mesma textura do HUD, em cache por código), e à direita nome, categoria e hierarquia (número e nome, ex.: "Maligna - Hierarquia 3 (Criaturas)"), **ATK** e **DEF** efetivos (verde/vermelho e "(base n)" quando a carta de campo altera), descrição com quebra de linha, dono e posição. Na mão, a carta sob o mouse cresce 1,64x (`Hand::Layout::hover_scale`).
9. **Passar turno**: com mais de 6 cartas na mão, as de menor ataque são descartadas. A câmera orbita 180° (1 s), a mão se recolhe e começa o turno do computador.

**Turno do computador** (`RunEnemyTurn`, passos agendados com 0,7 s de pausa para o jogador acompanhar; a entrada fica bloqueada): compra; trocas de posição (`ai::ChoosePositionChanges`); carta de campo se favorece sua categoria dominante (`ai::ChooseFieldCard`); invocação (`ai::ChooseSummon`: a de maior ataque efetivo, em ataque se consegue destruir alguma carta do jogador ou se o campo dele está vazio, senão a de maior defesa em modo de defesa; slot pela linha de trás e centro; campo cheio substitui a mais fraca); ataques um a um, reavaliando o campo a cada vez (`ai::ChooseAttack`: prefere destruir com dano, depois tirar escudos, aceita troca igual; nunca ataca quem ganharia dela; ataque direto com campo vazio); fim de turno com descarte acima de 6; a câmera volta e seu turno começa com a compra. Pontos de vida em 0 encerram a batalha (HUD mostra o resultado).

**Revelação da carta do oponente**: toda carta que o computador joga (invocação ou carta de campo) sai da mão dele e aparece **grande no centro da tela por 2 s** (`StartReveal`/`RenderReveal`, `kRevealSeconds`) antes de ir à arena: véu escuro sobre a mesa e a carta **em 3D** na camada da mão, com a mesma luz, foil e aura das cartas da sua mão, balançando de leve (560 px de altura; entra crescendo de 85% em 0,2 s, sai em 0,25 s); sem o render 3D cai na frente composta em 2D, a legenda acima ("O oponente invoca" ou "O oponente ativa a carta de campo") e, na invocação, a posição abaixo ("Em ataque" ou "Em defesa"). A colocação na arena é agendada com `ScheduleNext` (frente da fila), então acontece logo depois da revelação e antes dos passos seguintes. A pausa (Esc) congela a revelação junto com o resto.

**Níveis** (`ai::Difficulty`, escolhidos no menu, `PONTIFICE_DIFFICULTY=easy|medium|hard|impossible` força): mudam só o **monte do computador**, por peso de sorteio por hierarquia (`ai::DeckWeight`; os limites de 2 cópias e 1 de hierarquia 1 continuam):

| Nível | Hierarquia 1 | 2 | 3 | 4 | Efeito |
|---|---|---|---|---|---|
| Fácil | 0 | 0 | 40% | 100% | cartas inferiores às suas |
| Médio | 100% | 100% | 100% | 100% | igual ao seu monte |
| Difícil | 6x | 15x | 4x | 30% | na maioria das vezes mais fortes |
| Impossível | todas | todas | completa | 0 | nitidamente mais fortes |

A tomada de decisão da IA é a mesma em todos os níveis.

Os slots ficam em `Board::SlotPosition`: colunas em x = (col − 2) × ladrilho, linha A em z = meia canaleta + meio ladrilho e linha B um ladrilho além, no topo da plataforma (constantes `ARENA_*` de `tools/texture_arena.py` vezes `kArenaScale`); o lado do oponente (`SetSide(-1)`) espelha x e z e vira as cartas para ele. `Board::Layout` guarda escala da carta, durações e trajetos das animações.

## Virada de campo no turno do computador

Quando o turno passa para o computador, a **câmera orbita 180°** em torno do centro do campo (`UpdateCamera`, 1 s com smoothstep), de modo que o jogador passa a ver a arena pelo ponto de vista do oponente.

- A rotação é uma **animação contínua** (cerca de 1 segundo, com easing), não um corte.
- Durante a animação, a entrada do jogador fica **bloqueada**.
- Ao final, o lado do computador aparece na parte de baixo da tela e o lado do jogador na parte de cima. As cartas viradas para baixo do jogador continuam mostrando o verso.
- A mão do jogador se recolhe para fora da tela (`Hand::SetAway`). O dock do computador com o verso das cartas ainda não é desenhado; a quantidade de cartas na mão dele aparece na linha de situação sob o título.
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
- As tochas emergem **depois** da arena: começam a subir do chão (4,5 unidades abaixo) em 1,1 s e levam 1,2 s, apagadas. Em 2,3 s o fogo acende (0,6 s: a chama cresce da base e ganha opacidade) e a luz das tochas vem junto com ele.
- Enquanto isso, as 5 cartas da mão inicial entram pela esquerda da tela, uma a uma, a partir de 0,8 s.
- `PONTIFICE_ARENA_EYE` (depuração) fixa a câmera e pula a abertura.

## Passe 3D e composição

Ordem de um quadro na cena de batalha:

1. `SDL_FlushRenderer` envia o que o renderer 2D já enfileirou.
2. `Gpu::BeginFrame` abre um passe de render na textura alvo (1920x1080, RGBA8) com buffer de profundidade (D24 ou D32), limpa ambos.
3. Cada `Gpu::Draw` envia uniforms (MVP e modelo no vértice; cor e flag de textura no fragmento), liga textura e sampler, e desenha a malha indexada. Malhas opacas (`kMesh`) usam o pipeline com escrita de profundidade; névoa, chama e halo usam um segundo pipeline igual mas **sem escrita de profundidade**, então testam contra o que é opaco sem esconder o que for desenhado atrás depois (uma camada de névoa à frente não corta a chama).
4. `Gpu::NewLayer` fecha o passe e abre outro mantendo a cor e limpando só a profundidade. É assim que a mão (câmera fixa) fica sempre por cima da arena (câmera do jogador).
5. `Gpu::EndFrame` fecha o passe e submete o command buffer.
6. A cena desenha a textura alvo com `SDL_RenderTexture` e o HUD por cima com o renderer 2D.

O fragment shader calcula a normal pela derivada da posição, então funciona em malhas sem normais e ilumina as duas faces. Com um **normal map** (segundo sampler, `Gpu::Draw(..., normal_map)`), o referencial tangente também vem das derivadas de posição e UV (cotangent frame), sem tangentes nos vértices; o mapa é lido na convenção OpenGL com o verde invertido porque o V das texturas cresce para baixo. Texturas recebem mipmaps gerados na GPU e filtro anisotrópico.

Modos de desenho (`Gpu::DrawMode`): `kMesh` (iluminado: luz direcional opcional, até **duas luzes pontuais** quentes com tremular, névoa, vinheta), `kMist` (quad de névoa com alpha de ruído animado), `kEmissive` (cor da textura direto, sem luz nem névoa), `kFlame` (chama procedural animada a partir do UV do quad), `kFlameGlow` (a mesma chama como halo oval difuso) e `kFoil` (como `kMesh`, mais arco-íris holográfico que cresce com a inclinação, reflexo e estrelas) e `kAura` (brilho procedural transparente em volta da carta foil).

## Frente da carta

`cardface::Compose` recebe a moldura da categoria reduzida 2x (800x1115; `CardFrontGood`, `CardFrontEvil`, `CardFrontNeutral` ou `CardFrontField`, uma por categoria, todas com as mesmas faixas) e desenha os textos com a fonte do jogo na CPU (`text::DrawOnImage`), antes de a imagem virar textura. Áreas, em px da imagem de 800x1115:

| Área | Posição | Conteúdo |
|---|---|---|
| Faixa do topo | y 58–114 | `name` à esquerda e `hierarchy` (1 a 4) à direita (56 px); carta de campo não tem hierarquia |
| Janela da imagem | y 129–576, x 66–733 (668x448) | Ilustração de `imageUrl` (ex.: `sprites/neutral/N301.png`, 1340x900), ajustada uma vez no carregamento da batalha (`cardface::FitArt`: preenche a janela cortando o excesso, centrada) e desenhada antes dos textos; sem o arquivo, fica o branco da moldura |
| Faixa abaixo da imagem | y 592–688 | `attributes.attack` à esquerda (`ATK 2800`) e `attributes.defense` à direita (`DEF 2900`), 76 px; carta de campo mostra `BUFF +n` e `NERF -n` |
| Área branca de baixo | y 712–1056, x 70–730 | `description`, 40 px, com quebra por palavras e bloco centralizado; carta de campo acrescenta "Favorece: Bondosas/Malignas" |

Cartas de **hierarquia 1** são **foil**: a frente é desenhada em `Gpu::DrawMode::kFoil` (holográfica) e a carta ganha uma **aura** de luz em volta, na mão, no campo, na inspeção e na revelação do oponente.

- **Arco-íris pela inclinação**: o shader mede o olhar projetado no plano da carta (eixos da carta pelas derivadas de posição e UV, sem o relevo). De frente o arco-íris é discreto; inclinando (hover, inspeção, carta deitada no campo) ele domina a carta e corre para o lado da inclinação. A cor é tingida (multiplicada) mais um brilho próprio, então o branco vira cor viva e o texto escuro continua legível. Riscas finas, granulado e o relevo do normal map variam a cor localmente.
- **Reflexo**: uma faixa clara varre a carta em diagonal a cada ~6 s.
- **Estrelas**: brilhos em cruz que piscam em pontos sorteados, mais fortes inclinando.
- **Aura** (`scripts/aura/CardAura.*`, `Gpu::DrawMode::kAura`): grade 4x4 no espaço local da carta, no plano do meio da espessura, desenhada logo antes da carta; UV = distância além da borda. Energia que flui (ruído animado) e pulsa, branco-dourada junto à borda e em arco-íris para fora, com a borda externa irregular. Mais fraca em carta que já atacou e some junto no descarte.

Na revelação da carta do oponente a carta é desenhada em 3D na camada da mão (`Hand::CenterMatrix`/`DrawLoose`), com a mesma pele, foil e aura das suas cartas, balançando para o arco-íris correr. Nome, hierarquia e ATK/DEF são brancos com contorno escuro (8 direções, ~5% do tamanho) e sombra, legíveis sobre as faixas coloridas; a descrição é preta lisa, sem contorno nem sombra, sobre a área branca. Cartas neutras (`sprites/neutral/`) e malignas (`sprites/evil/`) já têm ilustração; bondosas e de campo ficam com a janela em branco até terem os arquivos.

## Ajustes em JSON

Iluminação, névoa/nuvens e sombras ficam em três arquivos de `assets/`, lidos em `main.cpp` antes da primeira cena por `scripts/config` e consultados por caminho com pontos (`config::Float("fog", "battle.fog.density", 0.05f)`):

| Arquivo | Nome | Conteúdo |
|---|---|---|
| `Light.json` | `light` | Cor, halo e intensidade das tochas (`torch`); luz direcional, ambiente e vinheta da arena (`battle.arena`), da mão (`battle.hand`) e do loading (`loading`) |
| `Fog.json` | `fog` | Névoa por distância e de altura (`battle.fog`, `loading.fog`); camadas de névoa de chão (`battle.mist`, `loading.mist`); manto da abertura (`battle.introMist`); nuvens do menu e da cena de idioma (`menu.clouds`: texturas de ruído e camadas atrás e à frente) |
| `Shadow.json` | `shadow` | Tamanho do mapa (`mapSize`), `bias` e, por cena, força, fov, near/far e alvo da câmera de sombra (`battle`, `loading`) |

- Cores aceitam `#rrggbb`, papel de `colors.json` (`"background.fog"`) ou `[r, g, b]` em 0..1.
- Valor ausente ou de tipo errado usa o padrão do código (os mesmos números de antes dos arquivos) e avisa no log uma vez.
- Os valores são lidos ao entrar em cada cena; mudar o arquivo vale na próxima execução.
- Cada arquivo traz um bloco `notes` explicando os campos.

## Sombras

Shadow mapping **por tocha**: cada luz pontual tem um mapa de profundidade de 2048x2048 (D32 float, `Shadow.json` `mapSize`) desenhado a partir da própria chama, com projeção perspectiva ampla (130° na batalha, 125° no loading, `Shadow.json`) apontada para o centro da cena. Fluxo em um quadro:

1. Para cada luz, `Gpu::BeginShadowPass(k, light_vp)` abre um passe só de profundidade (pipeline sem alvo de cor, `fs_shadow` vazio), a cena desenha os ocultadores com `Model::DrawDepth` / `Torch::DrawBodyDepth` / `Gpu::DrawShadow` e fecha com `EndShadowPass`. Tudo no mesmo command buffer do quadro.
2. No passe principal o vertex shader projeta cada vértice nos dois espaços de luz; o fragment shader amostra os mapas com sampler de comparação e **PCF 3x3** (bordas suaves) e multiplica a contribuição da luz correspondente. Bias com inclinação (cresce onde a luz chega de raspão). Fora do frustum da luz não há sombra.
3. A névoa de chão também recebe sombra (metade da força), então o muro e as tochas projetam sombra no nevoeiro.

`Atmosphere::shadow_strength` (0,85 na arena, 0,9 no loading, 0 na mão) e `shadow_bias`, ambos de `Shadow.json`. Ocultadores: arena e corpos das tochas na batalha; muro, porta (que ao abrir sombreia a passagem) e tochas no loading. Se o backend não suportar D32 amostrável, `Gpu::ShadowsSupported()` é falso e tudo segue sem sombra.

## Tochas e iluminação

A luz das cenas 3D vem das **tochas** (`models/Torch.glb`, `src/scripts/torch/Torch.*`): cada chama é uma luz pontual (`Atmosphere::point_pos` e `point2_pos`, posição de `Torch::FlamePosition`), de cor amarelo-branca (`Light.json`, `torch.color`) e com o tremular do shader. A luz direcional fica em zero e o ambiente baixo e frio.

- **Loading**: duas tochas presas nos pilares do portão (x = ±1,72, altura 1,75, inclinadas 0,22 rad para fora), intensidade 0,9 cada.
- **Batalha**: duas tochas fincadas no chão à esquerda e à direita da arena (x = ±6,4, z = −1, escala 2,6), intensidade 2,6 cada; na abertura emergem do chão depois da arena. A mão continua com a iluminação própria, para as cartas ficarem legíveis.
- A chama (nó `Flame`) é um quad sempre na vertical e virado para a câmera, desenhado em `kFlame` (chama procedural no shader: gota deformada por três camadas de ruído que sobem, línguas no topo, núcleo claro na base, fase por posição) mais um halo maior e translúcido um pouco atrás. Corpo da tocha é desenhado antes das camadas de névoa e a chama depois delas.

## Depuração

Variáveis de ambiente lidas em `main.cpp`:

| Variável | Efeito |
|---|---|
| `PONTIFICE_SCENE=battle` | Abre direto na cena de batalha; `=loading` abre na tela de loading (que segue para a batalha); `=menu` pula a escolha de idioma; `=language` força a cena de idioma |
| `PONTIFICE_LANG=en` | Define o idioma (`en`, `es`, `pt`) sem passar pela cena de idioma e abre no menu |
| `PONTIFICE_MOUSE=x,y` | Injeta um movimento de mouse em coordenadas lógicas a cada quadro |
| `PONTIFICE_WINDOWED=1` | Abre em janela 1280x720 em vez de tela cheia; `=1920x1080` (ou outro LxA) abre nesse tamanho, útil para screenshots em resolução cheia |
| `PONTIFICE_ENEMY_HAND=G101,E101,...` | Força a mão inicial do computador (até 5), para testar a revelação de cartas específicas |
| `PONTIFICE_HAND=G101,E101,...` | Força os códigos da mão inicial (até 5), para testar cartas específicas (ex.: foil de hierarquia 1) |
| `PONTIFICE_DRAG=...` | Sequência de gestos separados por `;`: `x0,y0,x1,y1,t` arrasta o botão esquerdo de (x0,y0) a (x1,y1) a partir de `t` s (mesmo ponto = clique); `w,dy,t` gira a roda. Testa seleção, inspeção, virar e zoom |
| `PONTIFICE_KEYS=Escape,4;Return,6` | Aperta teclas (nomes do SDL) nos instantes dados, em segundos. Testa a pausa e os diálogos |
| `PONTIFICE_SCREENSHOT=arquivo.bmp` | Salva um quadro em BMP e encerra (quadro 60 por padrão) |
| `PONTIFICE_SCREENSHOT_FRAME=n` | Número do quadro salvo por `PONTIFICE_SCREENSHOT` |
| `PONTIFICE_SCREENSHOT_TIME=s` | Em vez do quadro, salva o primeiro quadro depois de `s` segundos (sem vsync a taxa de quadros varia; use isto para animações) |
| `PONTIFICE_LOADING_TIME=s` | Linha do tempo da tela de loading já começa em `s` segundos (porta num ponto fixo da abertura) |
| `PONTIFICE_ARENA_EYE=x,y,z` | Posição da câmera da arena, para inspecionar o modelo por outro ângulo |
| `PONTIFICE_AUDIO_LOG=1` | A cada segundo loga volume aplicado e alvo, pico e RMS do áudio enviado |
| `PONTIFICE_AUDIO_ANALYZE=1` | Na inicialização, mede o RMS de cada faixa de ost.json e sugere o ganho de normalização |
| `PONTIFICE_STARTER=player\|enemy` | Força quem começa a batalha |
| `PONTIFICE_DIFFICULTY=easy\|medium\|hard\|impossible` | Força o nível do oponente |

Exemplo: `PONTIFICE_WINDOWED=1 PONTIFICE_SCENE=battle PONTIFICE_MOUSE=960,840 PONTIFICE_SCREENSHOT=/tmp/hand.bmp ./build/pontifice`

## HUD

O HUD é renderizado em **2D, por cima da cena 3D**, em uma passada separada. Não é afetado pela câmera nem pela virada de campo. Veja a seção [HUD](hud.md).
