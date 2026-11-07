# Detalhes técnicos

## Stack

- **Linguagem**: C++20.
- **Build**: CMake + Ninja. Scripts `build.sh` (compila) e `run.sh` (executa). Passo a passo no README.
- **Formatação**: clang-format, estilo Google, indentação 2 espaços, limite de 100 colunas (`.clang-format`).
- **Bibliotecas**: SDL3 (`release-3.4.18`), cgltf (`v1.15`, leitura de glTF/GLB), stb_image (PNG/JPEG), stb_truetype (fonte TTF/OTF) e stb_vorbis (decodificação de Ogg Vorbis). Todas baixadas via `FetchContent`; cgltf e stb são header-only.
- **Áudio**: `SDL_AudioStream` com callback; a trilha da seção `"music"` de `assets/audio.json` é decodificada sob demanda na thread de áudio e toca em loop. As faixas ficam em **categorias**: `"menu"` (menu e loading: HowlsOfTheNight, RuinsOfYore, DecayTemples, ForgottenDungeons) e `"battle"` (batalha: FastBattle, ShoutSword, StartedFight, GloryAndVictory). Na troca de cena o `SceneManager` chama `Music::SetCategory` e `Music::SetScene`: a faixa atual sai em **fade out** de 1,5 s e a nova categoria entra em **fade in** de 2 s; o menu toca as faixas **em sequência** e a batalha **em ordem aleatória** (`"random": true` na categoria em audio.json: cada faixa sorteada ao entrar na batalha e a cada troca, sem repetir a que acabou de tocar; a retomada depois da pausa continua a mesma faixa); o volume é **por categoria** (`"volume"`: menu 0,10; batalha 0,20; loading usa o do menu) e caminha em rampa de 1 s. Cada faixa de `"sources"` tem um **ganho de normalização** em audio.json (`{"name", "file", "gain"}`, medido com `PONTIFICE_AUDIO_ANALYZE=1`, que decodifica cada faixa e sugere o ganho para RMS de referência −20 dBFS), então o volume por cena significa o mesmo em todas. O **botão de som** (`ButtonIcon`, menu e batalha) chama `Music::ToggleMuted`: volume a zero e de volta em rampa. Tudo na thread de áudio. Cada faixa tem fade in e fade out de 2 s e há 3 s de silêncio entre faixas. Os **efeitos** (`Sfx`) vêm do array `"sfx"` do mesmo arquivo: `{"name", "file", "gain", "volume"}` por clipe (cartas 0,3 (chegada 0,45); espada e escudo 0,45; giro da arena 0,5; abertura 1,0).
- **Renderização**: `SDL_GPU` (Metal no macOS, Vulkan no Linux, D3D12 no Windows). O menu e o HUD usam o renderer 2D do SDL criado sobre o mesmo device (`SDL_CreateGPURenderer`). O 3D usa o device diretamente, com shaders próprios, e desenha em uma textura alvo que o renderer 2D exibe como `SDL_Texture`.
- **Shaders**: fonte em `src/shaders/`. `card.metal` é embutido no binário e compilado pelo Metal em tempo de execução. `card.vert.glsl` e `card.frag.glsl` são o equivalente para Vulkan; o CMake compila para SPIR-V quando `glslc` está instalado e o jogo carrega `shaders/card.*.spv` ao lado do binário.
- **Janela**: tela cheia sem borda na resolução do monitor por padrão (`SDL_WINDOW_FULLSCREEN` + modo desktop), sem `SDL_WINDOW_RESIZABLE`. `PONTIFICE_WINDOWED=1` abre em janela 1280x720 (depuração).
- **Resolução lógica**: 1920x1080 com letterbox (`SDL_SetRenderLogicalPresentation`), de `ui.json` `screen` (`ui::Width`/`ui::Height`; por isso os JSON são lidos antes de criar a janela). Eventos de mouse são convertidos para essa resolução. A interface 2D fica numa **grade de 16 px** (`ui::Grid(n)` em `scripts/ui/ui.h`: 1920 = 120 células, 1080 = 67,5): margens de 1 célula, painéis do HUD 24x6 e 34x6, contadores com 3 de altura e largura arredondada à grade (`ui::SnapUp`), botões do menu 22x5 com 2 de vão, botão da batalha 21x5, botões de som, de dicas e de sair 3x3 (canto inferior esquerdo, empilhados, nessa ordem de baixo para cima), tooltip com imagem de 14 células. Grade e escala vêm de `ui.json` `screen` (`grid`, `textScale`). Tamanhos de texto seguem `ui::TextScale()` (1,5; medidos em 1280x720).
- **Placa da interface**: `scripts/frame/Frame.*` desenha o fundo de botões (`frame::Kind::kButton`: Passar turno, som, dica, Não/Sim do diálogo), janelas, painéis do HUD e tooltip (`frame::Kind::kWindow`) só com retângulos, sem textura: fundo `RGBA(0,0,0,0,75)`, borda clara de **2 px** `RGBA(255,255,255,0,40)` e, por fora dela, outline escura de **2 px** `RGBA(0,0,0,0,25)` (a "borda da borda"). Tudo cabe no retângulo do elemento: outline nos 2 px de fora, borda nos 2 px seguintes, fundo no resto (`frame::Inner`). Botão e janela são iguais; `frame::Kind::kButtonDanger` (Sair do menu, botão de sair da batalha, confirmar abandono) tem fundo vermelho escuro. Estados de botão (`frame::State`): hover/foco clareia a borda para 60%, pressionado escurece o fundo para 90%; foco de teclado ganha ainda um traço fino (`silver`) em volta do fundo. O fundo também fica sob a borda. Nas **quatro quinas**, um nó entrelaçado na cor da borda, só com retângulos, com as medidas tiradas de `art/corner.png` (**17x17 px** para linha de 3 px; escala com a espessura da linha, então 2 px dá nó de cerca de 11 px) (referência; a imagem não é carregada): as linhas da borda param a 17 px da quina (na escala 1:1); o nó tem barra de cima e de baixo (10 px), haste central (3 px, a 7 px da linha), rabicho da esquerda (10 px) e rabicho da direita (a 14 px), com vãos de 4 px; retângulos sem sobreposição (o alpha não soma); espelhado por quina. Escala = espessura/3, menor ainda se a placa não couber dois nós por lado. Cores em `assets/colors.json` (`black`, `white`, `redDark` no perigo) e opacidades em `assets/opacity.json` (`frameBackground`, `frameBackgroundClicked`, `frameBorder`, `frameBorderHover`, `frameOutline`).
- **Plataforma**: computadores (macOS, Windows, Linux). Sem versão mobile ou console.

## Estrutura do projeto

```
CMakeLists.txt        alvo pontifice, FetchContent do SDL3
build.sh / run.sh     compilar / compilar e executar
src/
  main.cpp            janela, renderer, loop principal
  scenes/             uma pasta por cena
    scene.h           interface Scene e SceneContext
    sceneManager.*    cena ativa e troca de cena
    language/
      languageScene.* escolha de idioma (docs/idioma.md), primeira cena
    menu/
      menuScene.*     menu (docs/menu.md)
    loading/
      loadingScene.*  tela de loading (docs/loading.md): tocha 3D cintilando no preto, pré-carrega a próxima cena
    battle/
      battleScene.*   batalha: render 3D e mão de cartas
    grimoire/
      grimoireScene.* grimório (docs/grimorio.md): galeria de cartas com filtro por categoria e páginas 4x2
  scripts/            componentes reutilizáveis, uma pasta por componente
    ui/
      ui.*            interface 2D de assets/ui.json: resolução, escala de texto (ui::Px), grade (ui::Grid, ui::SnapUp), ui::Cells e ui::Number
    layout/
      layout.*        objetos 3D, movimento e tempos de assets/layout.json: layout::Number, layout::Vec3, layout::Node
    frame/
      frame.*         placa da interface (botões, janelas, painéis, tooltip): fundo escuro, borda clara e outline escura de 2 px, nó entrelaçado em retângulos nas quinas; variante de perigo com fundo vermelho
    text/
      text.*          texto com as fontes do jogo via stb_truetype (text::Face: kTitle = fonts/pirataOne.ttf, kBody = fonts/newsreader.ttf): atlas 16/32/64 px por fonte, ASCII e Latin-1; sem a de corpo, usa a de título; sem a de título, cai na de depuração do SDL
    button/
      button.*        botão com placa, rótulo e ação (Passar turno, diálogos); categorias normal e perigo; base dos outros dois (hover, foco, clique)
      buttonText.*    botão só de texto, sem placa (menu: Jogar, Nível, Sair); com bandeira (SetFlag) é o item da cena de idioma
      buttonIcon.*    botão quadrado com placa e ícone em primitivas: alto-falante (liga/desliga a música), "?" (liga/desliga a dica do topo, na batalha) e porta com seta (sair da batalha, placa de perigo: abre o diálogo de abandono, como Esc)
    draw/
      draw.*          primitivas 2D com espessura via SDL_RenderGeometry: Line, Arc, FillPolygon (ícones de ButtonIcon, nó das quinas do Frame)
    dialog/
      confirmDialog.* janela modal de confirmação: título em PirataOne, pergunta em Newsreader, [Não] e [Sim] (pausa da batalha)
    flag/
      flag.*          bandeiras (EUA, Espanha, Brasil) com primitivas do renderer
    i18n/
      i18n.*          textos em pt/en/es (assets/translate.json): Get, GetOr, Format
    math/
      math.h          Vec2/3/4, Mat4 (perspectiva, lookAt, rotação, escala)
    image/
      image.*         decodifica PNG/JPEG para RGBA8 (stb_image)
    gpu/
      gpu.*           pipeline SDL_GPU: malhas, texturas, passe com profundidade; InitDevice serve em qualquer thread
    model/
      model.*         carrega .glb (cgltf): Parse (CPU, qualquer thread) e Upload (GPU); separa frente, verso e borda; lê baseColor e normalTexture
    hand/
      hand.*          mão em dock: layout em arco, hover, animação, seleção e inspeção
    board/
      board.*         um lado do campo (jogador ou oponente): 10 slots alinhados aos ladrilhos, destaque, cartas em 3D, investida e descarte
    sword/
      sword.*         efeito de golpe de espada nos ataques: espada procedural (caixas coloridas) que desce sobre o alvo e rastro de corte emissivo
    shield/
      shield.*        escudo sobre a carta em defesa atacada: procedural, recua e brilha no golpe, onda de choque; quebra se a carta é destruída
    config/
      config.*        ajustes em JSON (assets/light.json, fog.json, shadow.json, button.json, ui.json, layout.json, text.json, rules.json) por caminho com pontos, com padrão no código
    rules/
      rules.*         regras sem SDL: ajustes da partida (rules::Get, de assets/rules.json), carta de campo ativa, ataque/defesa efetivos, cálculo de dano
    ai/
      ai.*            oponente (Vs PC): níveis (peso do monte por hierarquia) e decisões por heurística
    hud/
      hud.*           HUD da batalha (docs/hud.md): carta de campo ativa, pontos de vida, monte/cemitério e janela de resultado com as estatísticas da partida (MatchStats)
    json/
      json.*          leitor JSON mínimo (objetos, arrays, strings, números)
      log.*           log padrão do jogo: `LOG(fmt, ...)` escreve `[YYYY-MM-DD HH:MM:SS] [arquivo] - mensagem` em stderr
    colors/
      colors.*        cores lidas de assets/colors.json, pedidas por nome ("silver", "stoneBlack")
    opacity/
      opacity.*       opacidades fixas lidas de assets/opacity.json, pedidas por nome; Apply multiplica o alpha de uma cor
    torch/
      torch.*         tocha 3D: corpo iluminado, chama emissiva tremulando; FlamePosition dá onde pôr a luz
    deck/
      deck.*          monte de cartas: embaralhar (Fisher-Yates, semente opcional), comprar do topo; sem SDL
    cards/
      cards.*         dados das cartas (assets/Cards*.json) e montagem do monte por peso `odds`
    cardface/
      cardFace.*      frente da carta: moldura da categoria + nome, ATK/DEF e descrição desenhados (CPU)
    audios/
      music.*         trilha sonora: lê a seção "music" de assets/audio.json, decodifica Ogg Vorbis (stb_vorbis), toca em loop (sequência ou sorteio) com fade e intervalo
      sfx.*           efeitos curtos (espada, escudo, cartas, arena): clipes do array "sfx" de assets/audio.json em cache, vários tocam ao mesmo tempo
      voices.*        narrações e falas: uma voz por vez, em fila; Interrupt corta e esvazia
      clip.*          clipe Ogg Vorbis decodificado em memória e playback one-shot por SDL_AudioStream
  shaders/
    card.metal        shader Metal (macOS), embutido no binário
    card.*.glsl       shader GLSL para SPIR-V (Vulkan)
    cardShadow.frag.glsl  fragmento vazio do passe de sombra (Vulkan)
  assets/             dados das cartas e ajustes em JSON, paleta de cores
    colors.json       cores por nome, sem camadas: tons do ícone (stone, slate, steel, silver), cores sólidas (sem alpha), metais e couro da espada e do escudo (goldDark, brass, brown), texto da carta (ink, inkDark) e bandeiras (flagUs*, flagEs*, flagBr*)
    ui.json           interface 2D por cena: posições e tamanhos de textos, botões, HUD e diálogo; resolução e grade
    layout.json       3D por cena: posições, dimensões e movimento dos modelos, câmeras, transições e tempos
    rules.json        regras da partida: vida, tamanho do monte, mão inicial, limite da mão, cópias por carta e nível inicial do oponente
    opacity.json      opacidades fixas por nome (0..1): véu, placa da interface, slot, dica, carta gasta, sombra do texto da carta, seta de ataque, brilhos, halo da tocha
    audio.json          áudio: "music" (categorias "menu" (menu/loading) e "battle", cada uma com "volume" (0,10 e 0,20), "random" (batalha) e "sources" de Ogg Vorbis em audios/music/), "sfx" (array de clipes de audios/sfx/ com name, file, gain e volume) e "voices" (vazio por enquanto)
    cardsGood.json
    cardsEvil.json
    cardsNeutral.json
    cardsField.json
  fonts/              fontes TTF do jogo (copiadas para fonts/ junto do binário)
    pirataOne.ttf     fonte de título do jogo: títulos, botões, painéis, rótulos (tem acentos Latin-1)
    newsreader.ttf    fonte de corpo: linha de situação e dica da batalha, descrição da carta (frente) e modo/categoria/hierarquia do tooltip
  audios/             áudio em Ogg Vorbis: music/menu e music/battle (listadas em assets/audio.json), sfx/ (efeitos, também em audio.json) e voices/ (falas)
  sprites/            texturas das cartas (WebP)
    CardBase*.png     molduras por categoria (Good, Evil, Field)
    CardBackBase.png  verso da carta
    arenaAtlas.png    atlas da arena (2048x2048), gerado por tools/textureArena.py e embutido em arena.glb
    torchAtlas.png    atlas da tocha (512x512, RGBA), gerado por tools/makeTorch.py e embutido em torch.glb: madeira e ferro da arena, couro, brasas e chama
  icons/
    icon.png          ícone (1024x1024); vira ícone da janela/Dock em tempo de execução (copiado para icons/ junto do binário)
    icon.ico          ícone do executável no Windows, gerado por tools/makeIcons.py
    pontifice.rc      recurso do Windows que embute o icon.ico
  models/
    card.glb          carta: frente, verso e borda metálica (2,51 x 3,5 x 0,075)
    torch.glb         tocha (nós Torch e Flame), gerada por tools/makeTorch.py; luz do loading e da arena
    ArenaRaw.glb      arena bruta de imagem-para-3D (referência; usada só com --raw)
    arena.glb         arena procedural texturizada, gerada por tools/textureArena.py
  art/
    Arena.png         ilustração de referência da arena (perspectiva)
    ArenaAtlas2.png   sprite sheet: tabuleiro completo, painel de pedra e tiras
    ArenaAtlas3.png   sprite sheet plano usado pela ferramenta: ladrilhos, painel de
                      madeira, bandas de metal, tábuas, cruz e placa com flor-de-lis
    card.png          ilustração de referência da carta
    cardFront.psd     fonte de arte da frente da carta (molduras cardFront*.webp)
    cardBack.psd      fonte de arte do verso da carta (cardBack.webp)
    corner.psd        fonte de arte do nó da quina (corner.png)
    corner.png        referência do nó da quina da interface (128x128, quina inferior esquerda, magenta = fora); as medidas estão em código (frame.cpp)
    torch.webp        ilustração de referência da tocha
tools/
  textureArena.py    gera arena.glb e arenaAtlas.png a partir de ArenaRaw.glb e Arena.png
  makeTorch.py       gera torch.glb e torchAtlas.png (cabo, couro, cesta de ferro, brasas e chama)
  normalMap.py        gera normal map (luminância -> Sobel) da textura embutida e grava como normalTexture no GLB; chamado pelos três geradores
  makeIcons.py       gera src/icons/icon.ico a partir de src/icons/icon.png (usa sips)
  pngToWebp.py        converte os PNG de uma pasta (recursivo) para WebP sem perda (cwebp -lossless -exact) e apaga os PNG
  mp3ToOgg.py         converte os MP3 de uma pasta (recursivo) para Ogg Vorbis q3 (afconvert + oggenc) e apaga os MP3
cmake/
  embed.cmake         converte arquivo texto em array de bytes C
tests/                testes automatizados (ctest --test-dir build)
  deckTest.cpp       monte: embaralhar preserva cartas, é determinístico por semente, compra tira do topo
  rulesTest.cpp      regras de dano, carta de campo, decisões da IA e pesos dos níveis
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
| `image`, `imageFile`, `imageUrl` | string | Nome, arquivo e caminho da ilustração (`imageUrl` relativo ao binário, ex.: `sprites/neutral/n301.webp`). O jogo usa `imageUrl`; se o arquivo não existe, a janela da imagem fica em branco. |
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

A arena é `models/arena.glb`, desenhada na cena de batalha com câmera própria (acima e à frente do lado do jogador), antes da mão. Fundo da cena em `#444`.

`tools/textureArena.py` (só biblioteca padrão do Python) gera o modelo e o atlas. A geometria é **procedural e low poly** (196 triângulos): um **prisma trapezoidal** com base retangular maior embaixo, topo retangular menor (onde ficam os ladrilhos e a canaleta) e laterais planas inclinadas com altura de **um ladrilho** e base de **2.5 x 2.5** (topo 1.9 x 1.9; ≈40° da vertical), seguindo a especificação do tabuleiro (visão de cima: b, a, canaleta, a, b; ladrilhos quadrados, canaleta com a largura de um ladrilho e rebaixada 0.2 ladrilho, moldura metálica em volta):

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

Para regenerar depois de mudar a ilustração ou o modelo bruto: `python3 tools/textureArena.py`. O passo seguinte é alinhar os 10 slots de cada lado com os ladrilhos do tabuleiro.

## Mão em 3D (dock)

As cartas na mão são **elementos 3D**, dispostos como um **dock**. Implementado em `src/scripts/hand/Hand.*`.

- Arena e tocha têm **normal map** gerado de suas texturas (`tools/normalMap.py`, embutido no GLB como `normalTexture`; a arena com força menor). Cada carta é uma instância de `models/card.glb`, com **normal map** na frente (`cardFrontNormalMap.webp`, 1600x2230 com cantos transparentes: ao carregar, a transparência vira normal plana e o mapa é reduzido para 800x1115, a resolução da frente composta) e no verso (`cardBackNormalMap.webp`): relevo da moldura e dos entalhes, reagindo às luzes e ao hover/inspeção. A frente é uma textura **composta por carta** (`cardface`: moldura da categoria com os textos por cima, ver "Frente da carta"); o verso recebe `cardBack.webp`; a borda usa a cor metálica do próprio modelo.
- **Monte real**: no `Preload` da batalha as cartas de `assets/Cards*.json` são carregadas, o monte de 40 é montado por peso `odds` (no máximo 2 cópias por carta, 1 das de hierarquia 1) e **embaralhado** (`deck::Deck`); as 5 da mão inicial são compradas e compostas ali, antes de qualquer animação. A tecla D compra a próxima do monte.
- As cartas ficam na parte inferior da tela, em arco: cada uma desce com o quadrado da distância ao centro e gira levemente em Z. O espaçamento diminui quando a mão passa da largura máxima.
- **Hover**: a carta sob o cursor sobe, aproxima-se da câmera e aumenta de escala. As vizinhas se afastam com força decrescente (1, 1/2, 1/3...). Tudo animado com interpolação exponencial, independente do frame rate.
- **Entrada**: cada carta adicionada à mão (`Hand::Add`) desliza de fora da tela, pela esquerda, até o seu lugar, a velocidade constante (13 unidades/s, suavizado nas pontas), então quem vai mais longe demora mais e não ultrapassa a anterior; cartas adicionadas em sequência entram uma a uma, com 0,12 s entre elas. `Hand::DelayDeal` adia a próxima entrada (a batalha espera o fade in).
- **Gestos**: **clicar e arrastar** uma carta (12 px) a tira da mão: ela segue o mouse no plano z = 1 da câmera da mão (`Hand::Dragging()` dá o índice), pequena (escala 0,32, menor que na mão) para o slot embaixo ficar visível, e ao soltar fora de um slot volta animada ao lugar. **Duplo clique** inspeciona (uma por vez): a carta vai animada para o centro da tela, ampliada (a z = 4,2, câmera em z = 10), e responde ao arrasto do mouse: horizontal **gira em Y** (360°, mostra o verso; fica onde parou) e vertical **inclina um pouco em X** (±0,35 rad, volta sozinha ao soltar); a **roda do mouse** dá zoom entre 75% e 100% da altura da tela (começa em 80%, passos de 5%); enquanto isso a mão não reage ao mouse. **Clique na carta** (ou Espaço) a **vira** com meia-volta animada, mostrando o verso (`cardBack.webp`); clique fora dela, botão direito ou Esc encerram a inspeção e a carta volta animada para a mão.
- A carta destacada é desenhada por último, por cima das demais; a arrastada e a inspecionada, depois dela.
- **Detecção**: os quatro cantos da carta são projetados para a tela com a matriz do quadro anterior e testados contra o cursor (ponto em quadrilátero convexo). Entre cartas sobrepostas vence a de centro mais próximo do cursor.
- Parâmetros (altura, espaçamento, arco, escala, elevação, velocidade) ficam em `Hand::Layout`.

Pendente: mão do oponente com verso para a câmera.

## Partida contra o computador

Implementada em `BattleScene` com `scripts/board/Board.*` (um `Board` por lado), `scripts/rules/Rules.*` (regras puras) e `scripts/ai/Ai.*` (oponente). Segue docs/batalha.md e docs/campo.md.

**Carga**: as 65 ilustrações são decodificadas e ajustadas em paralelo (até 8 threads) no `Preload`, e `image.cpp`, `text.cpp` e `cardFace.cpp` são compilados com `-O2` mesmo em Debug (stb_image, stb_truetype e composição das frentes); o log mostra o tempo (`Ilustracoes de cartas: 65 (… ms)`).

**Início**: cada lado monta um monte de 40 (o do computador conforme o nível, ver abaixo) e compra 5. Um sorteio na entrada da batalha decide quem começa (`PONTIFICE_STARTER=player|opponent` força); se é o oponente, a mão dele (oculta) é distribuída na entrada no lugar da sua, que fica recolhida desde o início, e a câmera da abertura já parte do lado dele (`orbit_ = 1` na entrada; `UpdateIntro` aplica o mesmo giro da virada), então o turno dele começa sem girar a arena. Quando as tochas acendem (3,2 s) o primeiro turno começa. Quem começa não compra nem ataca no primeiro turno.

**Seu turno** (botão **Passar turno** no centro superior da tela, logo abaixo da dica, visível sempre que você pode agir):

1. **Arrastar a carta**: clicar e arrastar uma carta da mão a leva até a arena. Durante o arrasto os slots vazios do seu lado ficam levemente destacados (quad translúcido pulsando) e aparece a dica do que fazer; soltar sobre um slot seu invoca ali, soltar fora devolve a carta à mão. Toda carta entra **virada para cima, em modo de ataque**; não existe carta virada para baixo.
2. **Escolher o slot**: o slot sob o mouse recebe uma **moldura quadrada amarela** (`yellow`). A detecção projeta os quatro cantos do ladrilho na tela com a câmera da arena (`math::PointInConvexQuad`). Um clique sobre uma carta da mão nunca conta como clique no slot atrás dela.
3. **Carta vai para a arena**: `Hand::Remove` tira a carta da mão (as outras deslizam para fechar o espaço) e `Board::Place` a coloca no slot: desce animada (0,45 s) de cima e à frente do slot até deitar sobre o ladrilho, topo para o lado do oponente. Uma invocação normal por turno.
4. **Slot ocupado**: invocar sobre um slot com carta descarta a anterior (`Board::Discard`: sobe e encolhe até sumir; cemitério +1).
5. **Carta de campo** arrastada: soltar em qualquer ladrilho da arena a ativa (`PlayField`): a carta é consumida e conta no seu cemitério na hora, sem ocupar slot; o efeito vai para o painel do HUD (com a ilustração da carta), substitui o anterior e passa a valer o buff/nerf em todo cálculo (`rules::EffectiveAttack/Defense`). Não gasta a invocação do turno. Enquanto há **carta de campo ativa** (de qualquer jogador), a **arena fica dourada** (tint `gold` reforçado 1,5x) a **névoa de chão** toma a cor da categoria que ela favorece (`blue` azul, `purple` roxa; sem campo ela é cinza, `grayLight`) e o **fundo do ambiente** (cor de limpeza e névoa por distância) vai para o tom escuro dela (`blueMarine`, `purpleDark`), em transição suave (`field_glow_`). Uma carta de campo sendo arrastada já mostra esse visual, com a cor dela, como prévia.
6. **Trocar posição**: clicar numa carta sua no campo alterna **ataque <-> defesa** (`Board::TogglePosition`, giro animado de 90° no plano da mesa). (A regra "não troca de posição no turno em que entrou" vale para a IA; para o jogador a troca é livre no seu turno, por decisão de design.)
7. **Atacar**: **arrastar** uma carta sua em ataque (que ainda não atacou neste turno) até uma carta do oponente a ataca; se o lado dele está vazio, soltar sobre qualquer ladrilho dele é **ataque direto**. Durante o arrasto um **arco amarelo com seta** liga a carta ao mouse (`DrawAttackArc`: Bézier quadrática curvada para cima, altura proporcional à distância; engrossa até a ponta, com contorno escuro, brilho e um pulso de luz correndo para o alvo) e o alvo ganha a moldura. A carta faz uma **investida** em arco (`Board::Lunge`, 0,5 s) e no impacto (0,25 s) `rules::Resolve` aplica as tabelas de dano: destruição vai para o cemitério, dano para o HUD. No impacto toca o **golpe de espada** (`scripts/sword/Sword.*`): uma espada procedural (lâmina de aço, guarda e pomo dourados, cabo de couro; caixas sem textura, iluminadas pelas tochas) surge acima do alvo vinda do lado do atacante, desce num golpe de 0,19 s até a carta e some; no impacto um **rastro de corte** branco emissivo risca o plano da carta na diagonal e se apaga em 0,3 s, e a carta atingida **pisca em vermelho** (`Board::Flash`). Carta atingida **em defesa**: surge um **escudo** sobre ela (`scripts/shield/Shield.*`: escudo heráldico procedural, borda e cruz douradas e corpo prateado, polígono extrudado; em pé, virado para o atacante, inclinado para a câmera; cresce com peso em 0,15 s, recua e brilha no golpe da espada com uma onda de choque em anel emissivo; segurando, some em 0,9 s; se a carta é destruída, fica vermelho e cai girando). Carta **abatida** (destruída no golpe): a **arena treme** (`BattleScene::Destroy` arma `shake_left_`; `UpdateCamera` sacode câmera e alvo da arena em três senos, 0,16 de amplitude caindo com o quadrado do tempo em 0,45 s; mão e HUD não tremem). Vale para os dois lados, inclusive no ataque direto (golpe no meio do lado vazio). Uma carta que **já atacou** no turno fica **menor (82%), meio dessaturada (50%, no shader via `Gpu::SetDesaturate`) e com 75% de opacidade** até o próximo turno do dono (`Board::Card::spent`, `spent_scale`/`spent_alpha`/`spent_desaturate` em `Board::Layout`); sob o mouse volta a opaca para leitura.
8. **Ver uma carta no campo**: passar o mouse sobre qualquer carta em campo (sua ou do oponente, em qualquer momento) a **amplia** (1,45x, sobe um pouco, desenhada por cima das vizinhas; `Board::SetHoverCard`) e abre um **tooltip** ao lado do cursor (`RenderTooltip`): frente composta da carta à esquerda (mesma textura do HUD, em cache por código), e à direita nome, **ATK** e **DEF** efetivos (Pirata One, 24 px; verde/vermelho quando a carta de campo altera) e, na fonte de corpo (Newsreader, 18 px), **MODO** (ataque/defesa), **CATEGORIA** e **HIERARQUIA** (nome e número, ex.: "Hierarquia Criaturas (3)"). Na mão, a carta sob o mouse cresce 1,64x (`Hand::Layout::hover_scale`).
9. **Passar turno**: com mais de 6 cartas na mão, as de menor ataque são descartadas. A câmera orbita 180° (1 s), a mão se recolhe e começa o turno do computador.

**Dica** (linha abaixo da situação do turno, `BattleScene::Hint`, chaves `battle.hint.*`): uma frase curta com o que fazer agora. Vale a primeira situação da lista:

1. Carta da mão em inspeção: os controles (girar, virar, zoom, Esc).
2. Turno do computador: aguardar e passar o mouse nas cartas; some durante a revelação da carta dele, que já tem legenda.
3. Animação em curso no seu turno (investida, golpe, câmera): sem dica.
4. Arrasto de ataque: sobre uma carta do oponente, o **resultado** se soltar ali, pelas mesmas regras de `rules::Resolve` com os valores efetivos (`preview.win`, `winDefense`, `lose`, `tie`, `blocked`, `even`, com o dano); fora de um alvo, "solte sobre uma carta"; com o lado dele vazio, o dano do ataque direto.
5. Carta da mão arrastada: carta de campo, onde ativar (ou que substitui a atual); invocação já usada, que soltar devolve a carta; mouse sobre um slot seu ocupado, que a carta dele vai para o cemitério; seu lado cheio, como trocar; senão, onde invocar.
6. Mouse sobre uma carta da mão: os gestos (arrastar joga, duplo clique inspeciona).
7. Mouse sobre uma carta sua no campo: em defesa, como voltar ao ataque; no 1º turno de quem começa, que não ataca; já atacou; senão, arrastar para atacar ou clicar para defesa.
8. Próxima jogada: atacar (ou ataque direto, com o lado dele vazio); invocar, se ainda não invocou e tem carta de batalha; aviso de mais de 6 cartas na mão; carta de campo na mão (não gasta a invocação); por fim, passar o turno (no 1º turno de quem começa, lembrando que não há ataque).

A linha de situação sai em branco opaco (`white`, linha de 24 px) e a dica, menor, com 75% de opacidade (`silver` com a opacidade `boardHint`, linha de 18 px), ambas na Newsreader; as frases da dica cabem entre os painéis do topo nos três idiomas.

**Turno do computador** (`RunOpponentTurn`, passos agendados com 0,7 s de pausa para o jogador acompanhar; a entrada fica bloqueada): compra; trocas de posição (`ai::ChoosePositionChanges`); carta de campo se favorece sua categoria dominante (`ai::ChooseFieldCard`); invocação (`ai::ChooseSummon`: a de maior ataque efetivo, em ataque se consegue destruir alguma carta do jogador ou se o campo dele está vazio, senão a de maior defesa em modo de defesa; slot pela linha de trás e centro; campo cheio substitui a mais fraca); ataques um a um, reavaliando o campo a cada vez (`ai::ChooseAttack`: prefere destruir com dano, depois tirar escudos, aceita troca igual; nunca ataca quem ganharia dela; ataque direto com campo vazio); fim de turno com descarte acima de 6; a câmera volta e seu turno começa com a compra. Pontos de vida em 0 encerram a batalha (HUD mostra o resultado com as estatísticas da partida, ver docs/hud.md).

**Mão do oponente**: no turno dele a mão aparece no dock de baixo (`opponent_hand_view_`, um segundo `Hand`, só visual, sem mouse), no lugar da sua, que se recolhe. Todas as cartas usam a mesma frente oculta, composta uma vez no Preload (`cardface::ComposeHidden` sobre `sprites/cardFrontOpponent.webp`: nome "?", "ATK ?", "DEF ?", descrição "?", sem hierarquia). `OpponentHandAdd`/`OpponentHandRemove` mantêm a mão visível igual a `opponent_hand_` (compras, cartas jogadas, descarte acima de 6). A carta jogada sai da pose dela no dock (`Hand::Remove` devolve a matriz) e voa 0,5 s até o centro (`Hand::CenterPose`/`PoseMatrix`), crescendo, endireitando o leque e virando em Y: frente oculta até 90°, carta real depois; a revelação de 2 s começa ao chegar. No fim do turno dele a mão se recolhe (0,5 s) antes de a câmera orbitar de volta.

**Revelação da carta do oponente**: toda carta que o computador joga (invocação ou carta de campo) sai da mão dele e aparece **grande no centro da tela por 2 s** (`StartReveal`/`RenderReveal`, `battle.reveal.seconds` do `layout.json`) antes de ir à arena: véu escuro sobre a mesa e a carta **em 3D** na camada da mão, com a mesma luz, foil e aura das cartas da sua mão, balançando de leve (560 px de altura; entra crescendo de 85% em 0,2 s, sai em 0,25 s); sem o render 3D cai na frente composta em 2D, a legenda acima ("O oponente invoca" ou "O oponente ativa a carta de campo") e, na invocação, a posição abaixo ("Em ataque" ou "Em defesa"). A colocação na arena é agendada com `ScheduleNext` (frente da fila), então acontece logo depois da revelação e antes dos passos seguintes. A pausa (Esc) congela a revelação junto com o resto.

**Níveis** (`ai::Difficulty`, escolhidos no menu, `PONTIFICE_DIFFICULTY=easy|medium|hard|impossible` força): mudam só o **monte do computador**, por peso de sorteio por hierarquia (`ai::DeckWeight`; os limites de 2 cópias e 1 de hierarquia 1 continuam):

| Nível | Hierarquia 1 | 2 | 3 | 4 | Efeito |
|---|---|---|---|---|---|
| Fácil | 0 | 0 | 40% | 100% | cartas inferiores às suas |
| Médio | 100% | 100% | 100% | 100% | igual ao seu monte |
| Difícil | 6x | 15x | 4x | 30% | na maioria das vezes mais fortes |
| Impossível | todas | todas | completa | 0 | nitidamente mais fortes |

A tomada de decisão da IA é a mesma em todos os níveis.

Os slots ficam em `Board::SlotPosition`: colunas em x = (col − 2) × ladrilho, linha A em z = meia canaleta + meio ladrilho e linha B um ladrilho além, no topo da plataforma (constantes `ARENA_*` de `tools/textureArena.py` vezes `battle.arena.scale` do `layout.json`); o lado do oponente (`SetSide(-1)`) espelha x e z e vira as cartas para ele. `Board::Layout` guarda escala da carta, durações e trajetos das animações.

## Virada de campo no turno do computador

Quando o turno passa para o computador, a **câmera orbita 180°** em torno do centro do campo (`UpdateCamera`, 1 s com smoothstep), de modo que o jogador passa a ver a arena pelo ponto de vista do oponente.

- A rotação é uma **animação contínua** (cerca de 1 segundo, com easing), não um corte.
- Durante a animação, a entrada do jogador fica **bloqueada**.
- Ao final, o lado do computador aparece na parte de baixo da tela e o lado do jogador na parte de cima. As cartas viradas para baixo do jogador continuam mostrando o verso.
- A mão do jogador se recolhe para fora da tela (`Hand::SetAway`). A linha de situação no topo mostra só o turno e de quem é a vez (`battle.status.turn`: "Turno {0} - {1}").
- O **HUD não gira**: carta de campo ativa segue no canto superior esquerdo e os pontos de vida no canto superior direito, com os nomes "Você" e "Oponente" inalterados.
- Quando o turno volta ao jogador, a câmera orbita os 180° de volta, com a mesma animação.

## Cenas

O jogo é organizado em cenas. Só uma cena fica ativa por vez, e o loop principal repassa a ela eventos, atualização e desenho.

- `Scene`: interface com `Preload` (carga pesada, inclusive envio à GPU, chamada pela tela de loading em outra thread), `OnEnter`, `OnExit`, `HandleEvent`, `Update` e `Render`.
- `SceneContext`: estado compartilhado. A cena pede troca escrevendo em `next_scene` e pede saída com `quit`. `loading_target` diz à tela de loading qual cena preparar; `preloaded` entrega uma instância já carregada (o `SceneManager` a usa no lugar de criar outra); `fade_out_seconds` escurece a cena atual até o preto antes da troca (sem eventos nesse tempo) e `fade_in_seconds` faz a nova cena entrar do preto.
- `SceneManager`: guarda a cena ativa e aplica as trocas pedidas depois de cada evento ou atualização. Desenha o véu do fade in por cima da cena nova. `Create` é a fábrica de cenas.
- Ordem: `MenuScene` (inicial), `LoadingScene` e `BattleScene`. O botão Jogar abre a tela de loading, que carrega a batalha enquanto a tocha cintila e então troca com fade in (docs/loading.md). `Esc` na batalha volta direto ao menu. O item Grimório usa a mesma tela de loading com `loading_target = kGrimoire` para compor as frentes das cartas (`GrimoireScene`, docs/grimorio.md); Sair ou `Esc` voltam ao menu.

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

`cardface::Compose` recebe a moldura da categoria reduzida 2x (800x1115; `CardFrontGood`, `CardFrontEvil`, `CardFrontNeutral` ou `CardFrontField`, uma por categoria, todas com as mesmas faixas) e desenha os textos na CPU (`text::DrawOnImage`): nome, hierarquia e ATK/DEF na PirataOne, descrição na Newsreader (`text::Face::kBody`), antes de a imagem virar textura. Áreas, em px da imagem de 800x1115:

| Área | Posição | Conteúdo |
|---|---|---|
| Faixa do topo | y 58–114 | `name` à esquerda e `hierarchy` (1 a 4) à direita (56 px); carta de campo não tem hierarquia |
| Janela da imagem | y 129–576, x 66–733 (668x448) | Ilustração de `imageUrl` (ex.: `sprites/neutral/n301.webp`, 1340x900), ajustada uma vez no carregamento da batalha (`cardface::FitArt`: preenche a janela cortando o excesso, centrada) e desenhada antes dos textos; sem o arquivo, fica o branco da moldura |
| Faixa abaixo da imagem | y 592–688 | `attributes.attack` à esquerda (`ATK 2800`) e `attributes.defense` à direita (`DEF 2900`), 76 px; carta de campo mostra `BUFF +n` e `NERF -n` |
| Área branca de baixo | y 712–1056, x 70–730 | `description`, 40 px, com quebra por palavras e bloco centralizado; carta de campo acrescenta "Favorece: Benévolas/Malignas" |

Cartas de **hierarquia 1** são **foil**: a frente é desenhada em `Gpu::DrawMode::kFoil` (holográfica) e a carta ganha uma **aura** de luz em volta, na mão, no campo, na inspeção e na revelação do oponente.

- **Arco-íris pela inclinação**: o shader mede o olhar projetado no plano da carta (eixos da carta pelas derivadas de posição e UV, sem o relevo). De frente o arco-íris é discreto; inclinando (hover, inspeção, carta deitada no campo) ele domina a carta e corre para o lado da inclinação. A cor é tingida (multiplicada) mais um brilho próprio, então o branco vira cor viva e o texto escuro continua legível. Riscas finas, granulado e o relevo do normal map variam a cor localmente.
- **Reflexo**: uma faixa clara varre a carta em diagonal a cada ~6 s.
- **Estrelas**: brilhos em cruz que piscam em pontos sorteados, mais fortes inclinando.
- **Aura** (`scripts/aura/CardAura.*`, `Gpu::DrawMode::kAura`): grade 4x4 no espaço local da carta, no plano do meio da espessura, desenhada logo antes da carta; UV = distância além da borda. Energia que flui (ruído animado) e pulsa, branco-dourada junto à borda e em arco-íris para fora, com a borda externa irregular. Mais fraca em carta que já atacou e some junto no descarte.

Na revelação da carta do oponente a carta é desenhada em 3D na camada da mão (`Hand::CenterPose`/`PoseMatrix`/`DrawLoose`), com a mesma pele, foil e aura das suas cartas, balançando para o arco-íris correr. Nome, hierarquia e ATK/DEF são brancos com contorno escuro (8 direções, ~5% do tamanho) e sombra, legíveis sobre as faixas coloridas; a descrição é preta lisa, sem contorno nem sombra, sobre a área branca. Todas as cartas têm ilustração: neutras (`sprites/neutral/`), malignas (`sprites/evil/`), benévolas (`sprites/good/`) e de campo (`sprites/field/`). O painel de campo do HUD mostra só a ilustração da carta ativa (`BattleScene::HudArt`).

## Ajustes em JSON

Iluminação, névoa/nuvens, sombras, medidas dos botões, medidas da interface 2D, objetos 3D com movimento e tempos, tamanhos de texto e regras da partida ficam em oito arquivos de `assets/`, lidos em `main.cpp` antes da primeira cena por `scripts/config` e consultados por caminho com pontos (`config::Float("fog", "battle.fog.density", 0.05f)`):

| Arquivo | Nome | Conteúdo |
|---|---|---|
| `light.json` | `light` | Cor, halo e intensidade das tochas (`torch`); luz direcional, ambiente e vinheta da arena (`battle.arena`) e da mão (`battle.hand`); ambiente do loading (`loading`) |
| `fog.json` | `fog` | Névoa por distância e de altura (`battle.fog`); camadas de névoa de chão (`battle.mist`); manto da abertura (`battle.introMist`) |
| `shadow.json` | `shadow` | Tamanho do mapa (`mapSize`), `bias` e força, fov, near/far e alvo da câmera de sombra da batalha (`battle`) |
| `button.json` | `button` | Variações por categoria de botão, em células da grade de 16 px (`Button::Measure`), escolhidas por `Button::VariantKey()`: `button.{small,regular,large,dangerSmall,dangerRegular,dangerLarge}` (`height`, `padding` dos lados do rótulo, `minWidth`, `label` = variante de `text.json`) para placas com texto (filtros/Voltar/Avançar/Menu do grimório, Passar turno em `large`, Não/Sim do diálogo); `buttonText.{regular,danger,flag}` (`rowHeight`, `rowGap`, `label`; `danger` é o Sair do menu em vermelho; `flag` acrescenta `flagHeight` e `gap`) para o menu e a escolha de idioma; `buttonIcon.{small,regular,large,danger}` (`size`, `margin`, `padding` em fração do lado) para som/dica e o sair da batalha (`danger`) |
| `ui.json` | `ui` | Só a interface 2D parada, por cena: `screen` (resolução lógica `width`/`height`, janela de depuração `windowedWidth`/`windowedHeight`, `grid` em px, `textScale`), `language` (vazia: a lista de idiomas só centraliza e usa as medidas de `button.json`), `menu` (título, rodapé), `loading` (margem do rótulo), `grimoire` (título, filtros, grade com `columns`/`rows`, `hoverGrow`, navegação), `battle` (status, dica, Passar turno, tooltip `tip`, tamanho da revelação 2D e do brilho `reveal`, limites do mouse `input`, desenho da seta `attack.arrow`), `hud` (painéis `life`, `field` com zoom, `piles`, `result`, posição dos números de dano `popup`) e `dialog`. Medidas em células da grade (`ui::Cells`); chaves terminadas em `Px` em px lógicos (`ui::Number`) |
| `layout.json` | `layout` | Objetos 3D, movimento, transições e tempos, por cena (`layout::Number`, `layout::Vec3`, `layout::Node`): `menu` (fades para o loading), `loading` (câmera, tocha e ondas de `torch.flicker`, pontos por segundo, tempo mínimo, fades), `grimoire` (`hoverLift`, deslize de página, fades), `battle` (tempos e balanço da revelação `reveal`, `arena`, `torch`, câmera `camera` com órbita e tremor, abertura `intro`, ritmo do oponente `opponent`, `handHideSeconds`, impacto `attack.hitSeconds` e as `Layout` de `board`, `hand` (câmera da mão também usada pelo grimório), `sword` e `shield`), `hud` (tremor e piscada da vida, tempo do zoom, física e tempos dos números de dano). Unidades: `seconds`/`...Seconds` em segundos; posições e escalas 3D em unidades do mundo; `fov` em graus; ângulos 3D (`tilt`, `yaw`, `angle*`) em radianos; `...Speed` são taxas por segundo; `...Ratio`/`...Fraction` frações; `...Px` em px lógicos |
| `text.json` | `text` | Variantes de texto combináveis (`text::Styles({"large", "bold"})` ou `text::StyleOf("large+bold")`, campos mesclados da esquerda para a direita): `size` na escala da fonte de depuração (vezes `ui::TextScale()` = `screen.textScale` 1,5; altura da linha = 8 px × size), `face` (`title` PirataOne / `body` Newsreader), `bold`, `italic` (falsos: segunda passada deslocada / cisalhamento), `letterSpacing` (fração do em), `lineGap` (fração da linha em `text::LineHeight`). Variantes: `xsmall`, `small`, `regular`, `description` (body), `large`, `xlarge`, `subtitle`, `heading`, `title`, `display`, `body`, `bold`, `italic`, `spaced`, `loose`. Quem usa o quê fica no código (`Layout` de cada cena) |
| `rules.json` | `rules` | Regras da partida, lidas uma vez em `main.cpp` para `rules::Settings` (`rules::Get()`), com limites mínimos em `rules::Set`: `maxLife` (5000), `deckSize` (40), `initialHand` (5), `handLimit` (6, descarta no fim do turno), `maxCopies` (2 por carta), `maxCopiesRarest` (1 para hierarquia 1), `defaultDifficulty` (0 fácil a 3 impossível; 1 médio) |

- Cores aceitam `#rrggbb`, nome de `colors.json` (`"stoneDark"`) ou `[r, g, b]` em 0..1.
- Valor ausente ou de tipo errado usa o padrão do código (os mesmos números de antes dos arquivos) e avisa no log uma vez.
- Ficam no código: geometria procedural (contorno do escudo, peças da espada, chama da tocha), gabarito em pixels da frente da carta (`cardface`), medidas internas da placa (`frame`) e das bandeiras, fórmulas (suavizações, metades, épsilons) e limites técnicos (passo máximo de tempo, planos near/far).
- Os valores são lidos ao entrar em cada cena (`rules.json` só ao iniciar o jogo); mudar o arquivo vale na próxima execução.
- Cada arquivo traz um bloco `notes` explicando os campos.

## Sombras

Shadow mapping **por tocha**: cada luz pontual tem um mapa de profundidade de 2048x2048 (D32 float, `shadow.json` `mapSize`) desenhado a partir da própria chama, com projeção perspectiva ampla (130°, `shadow.json`) apontada para o centro da cena. Fluxo em um quadro:

1. Para cada luz, `Gpu::BeginShadowPass(k, light_vp)` abre um passe só de profundidade (pipeline sem alvo de cor, `fs_shadow` vazio), a cena desenha os ocultadores com `Model::DrawDepth` / `Torch::DrawBodyDepth` / `Gpu::DrawShadow` e fecha com `EndShadowPass`. Tudo no mesmo command buffer do quadro.
2. No passe principal o vertex shader projeta cada vértice nos dois espaços de luz; o fragment shader amostra os mapas com sampler de comparação e **PCF 3x3** (bordas suaves) e multiplica a contribuição da luz correspondente. Bias com inclinação (cresce onde a luz chega de raspão). Fora do frustum da luz não há sombra.
3. A névoa de chão também recebe sombra (metade da força), então o muro e as tochas projetam sombra no nevoeiro.

`Atmosphere::shadow_strength` (0,85 na arena, 0 na mão e no loading) e `shadow_bias`, ambos de `shadow.json`. Ocultadores: arena e corpos das tochas na batalha. Se o backend não suportar D32 amostrável, o pipeline de sombra não é criado e tudo segue sem sombra.

## Tochas e iluminação

A luz das cenas 3D vem das **tochas** (`models/torch.glb`, `src/scripts/torch/Torch.*`): cada chama é uma luz pontual (`Atmosphere::point_pos` e `point2_pos`, posição de `Torch::FlamePosition`), de cor amarelo-branca (`light.json`, `torch.color`) e com o tremular do shader. A luz direcional fica em zero e o ambiente baixo e frio.

- **Loading**: uma tocha no canto inferior direito (y = −2,8 com a câmera a 11,4; x centralizado com o texto "Carregando..."), intensidade 0,9 cintilando ±20%.
- **Batalha**: duas tochas fincadas no chão à esquerda e à direita da arena (x = ±6,4, z = −1, escala 2,6), intensidade 2,6 cada; na abertura emergem do chão depois da arena. A mão continua com a iluminação própria, para as cartas ficarem legíveis.
- A chama (nó `Flame`) é um quad sempre na vertical e virado para a câmera, desenhado em `kFlame` (chama procedural no shader: gota deformada por três camadas de ruído que sobem, línguas no topo, núcleo claro na base, fase por posição) mais um halo maior e translúcido um pouco atrás. Corpo da tocha é desenhado antes das camadas de névoa e a chama depois delas.

## Depuração

Variáveis de ambiente lidas em `main.cpp`:

| Variável | Efeito |
|---|---|
| `PONTIFICE_SCENE=battle` | Abre direto na cena de batalha; `=loading` abre na tela de loading (que segue para a batalha); `=menu` pula a escolha de idioma; `=language` força a cena de idioma; `=grimoire` abre o grimório |
| `PONTIFICE_LANG=en` | Define o idioma (`en`, `es`, `pt`) sem passar pela cena de idioma e abre no menu |
| `PONTIFICE_MOUSE=x,y` | Injeta um movimento de mouse em coordenadas lógicas a cada quadro |
| `PONTIFICE_WINDOWED=1` | Abre em janela 1280x720 em vez de tela cheia; `=1920x1080` (ou outro LxA) abre nesse tamanho, útil para screenshots em resolução cheia |
| `PONTIFICE_SEED=n` | Fixa o sorteio da batalha (montes, compras e quem começa), para repetir uma partida igual em testes |
| `PONTIFICE_OPPONENT_HAND=G101,E101,...` | Força a mão inicial do computador (até 5), para testar a revelação de cartas específicas |
| `PONTIFICE_HAND=G101,E101,...` | Força os códigos da mão inicial (até 5), para testar cartas específicas (ex.: foil de hierarquia 1) |
| `PONTIFICE_DRAG=...` | Sequência de gestos separados por `;`: `x0,y0,x1,y1,t` arrasta o botão esquerdo de (x0,y0) a (x1,y1) a partir de `t` s (mesmo ponto = clique); `w,dy,t` gira a roda; `d,x,y,t` dá duplo clique. Testa arrasto, inspeção, virar e zoom |
| `PONTIFICE_KEYS=Escape,4;Return,6` | Aperta teclas (nomes do SDL) nos instantes dados, em segundos. Testa a pausa e os diálogos |
| `PONTIFICE_SCREENSHOT=arquivo.bmp` | Salva um quadro em BMP e encerra (quadro 60 por padrão) |
| `PONTIFICE_SCREENSHOT_FRAME=n` | Número do quadro salvo por `PONTIFICE_SCREENSHOT` |
| `PONTIFICE_SCREENSHOT_TIME=s` | Em vez do quadro, salva o primeiro quadro depois de `s` segundos (sem vsync a taxa de quadros varia; use isto para animações) |
| `PONTIFICE_LOADING_TIME=s` | Linha do tempo da tela de loading já começa em `s` segundos (pula o fade in) |
| `PONTIFICE_ARENA_EYE=x,y,z` | Posição da câmera da arena, para inspecionar o modelo por outro ângulo |
| `PONTIFICE_AUDIO_LOG=1` | A cada segundo loga volume aplicado e alvo, pico e RMS do áudio enviado |
| `PONTIFICE_AUDIO_ANALYZE=1` | Na inicialização, mede o RMS de cada faixa de audio.json e sugere o ganho de normalização |
| `PONTIFICE_STARTER=player\|opponent` | Força quem começa a batalha |
| `PONTIFICE_DIFFICULTY=easy\|medium\|hard\|impossible` | Força o nível do oponente |

Exemplo: `PONTIFICE_WINDOWED=1 PONTIFICE_SCENE=battle PONTIFICE_MOUSE=960,840 PONTIFICE_SCREENSHOT=/tmp/hand.bmp ./build/pontifice`

## HUD

O HUD é renderizado em **2D, por cima da cena 3D**, em uma passada separada. Não é afetado pela câmera nem pela virada de campo. Veja a seção [HUD](hud.md).
