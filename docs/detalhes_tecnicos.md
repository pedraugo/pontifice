# Detalhes técnicos

## Stack

- **Linguagem**: C++20.
- **Build**: CMake + Ninja. Scripts `build.sh` (compila) e `run.sh` (executa). Passo a passo no README.
- **Formatação**: clang-format, estilo Google, indentação 2 espaços, limite de 100 colunas (`.clang-format`).
- **Bibliotecas**: SDL3 (`release-3.4.18`), cgltf (`v1.15`, leitura de glTF/GLB) e stb_image (PNG/JPEG). Todas baixadas via `FetchContent`; cgltf e stb são header-only.
- **Renderização**: `SDL_GPU` (Metal no macOS, Vulkan no Linux, D3D12 no Windows). O menu e o HUD usam o renderer 2D do SDL criado sobre o mesmo device (`SDL_CreateGPURenderer`). O 3D usa o device diretamente, com shaders próprios, e desenha em uma textura alvo que o renderer 2D exibe como `SDL_Texture`.
- **Shaders**: fonte em `src/shaders/`. `card.metal` é embutido no binário e compilado pelo Metal em tempo de execução. `card.vert.glsl` e `card.frag.glsl` são o equivalente para Vulkan; o CMake compila para SPIR-V quando `glslc` está instalado e o jogo carrega `shaders/card.*.spv` ao lado do binário.
- **Resolução lógica**: 1280x720 com letterbox (`SDL_SetRenderLogicalPresentation`). Eventos de mouse são convertidos para essa resolução.
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
    battle/
      BattleScene.*   batalha: render 3D e mão de cartas
  scripts/            componentes reutilizáveis, uma pasta por componente
    text/
      Text.*          texto com a fonte de depuração do SDL
    button/
      Button.*        botão base (hover, foco, clique)
      ButtonPlay.*    botão Jogar
      ButtonExit.*    botão Sair
    math/
      Math.h          Vec2/3/4, Mat4 (perspectiva, lookAt, rotação, escala)
    image/
      Image.*         decodifica PNG/JPEG para RGBA8 (stb_image)
    gpu/
      Gpu.*           pipeline SDL_GPU: malhas, texturas, passe com profundidade
    model/
      Model.*         carrega .glb (cgltf), separa frente, verso e borda
    hand/
      Hand.*          mão em dock: layout em arco, hover, animação
  shaders/
    card.metal        shader Metal (macOS), embutido no binário
    card.*.glsl       shader GLSL para SPIR-V (Vulkan)
  assets/             dados das cartas em JSON
    CardsGood.json
    CardsEvil.json
    CardsNeutral.json
    CardsField.json
  sprites/            texturas das cartas (PNG) e fontes de arte (PSD)
    CardBase*.png     molduras por categoria (Good, Evil, Field)
    CardBackBase.png  verso da carta
  models/
    Card.glb          carta: frente, verso e borda metálica (2,51 x 3,5 x 0,075)
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

## Mão em 3D (dock)

As cartas na mão são **elementos 3D**, dispostos como um **dock**. Implementado em `src/scripts/hand/Hand.*`.

- Cada carta é uma instância de `models/Card.glb`. A frente recebe o sprite da categoria (`CardFrontGood.png`, `CardFrontEvil.png`, `CardFrontField.png`) e o verso recebe `CardBack.png`; a borda usa a cor metálica do próprio modelo.
- As cartas ficam na parte inferior da tela, em arco: cada uma desce com o quadrado da distância ao centro e gira levemente em Z. O espaçamento diminui quando a mão passa da largura máxima.
- **Hover**: a carta sob o cursor sobe, aproxima-se da câmera e aumenta de escala. As vizinhas se afastam com força decrescente (1, 1/2, 1/3...). Tudo animado com interpolação exponencial, independente do frame rate.
- A carta destacada é desenhada por último, por cima das demais.
- **Detecção**: os quatro cantos da carta são projetados para a tela com a matriz do quadro anterior e testados contra o cursor (ponto em quadrilátero convexo). Entre cartas sobrepostas vence a de centro mais próximo do cursor.
- Parâmetros (altura, espaçamento, arco, escala, elevação, velocidade) ficam em `Hand::Layout`.

Pendente: arrastar carta para um slot, animação ao comprar e descartar, mão do oponente com verso para a câmera.

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

- `Scene`: interface com `OnEnter`, `OnExit`, `HandleEvent`, `Update` e `Render`.
- `SceneContext`: estado compartilhado. A cena pede troca escrevendo em `next_scene` e pede saída com `quit`.
- `SceneManager`: guarda a cena ativa e aplica as trocas pedidas depois de cada evento ou atualização.
- Ordem: `MenuScene` (inicial) e `BattleScene`. O botão Jogar troca para a batalha, `Esc` na batalha volta ao menu.

## Passe 3D e composição

Ordem de um quadro na cena de batalha:

1. `SDL_FlushRenderer` envia o que o renderer 2D já enfileirou.
2. `Gpu::BeginFrame` abre um passe de render na textura alvo (1280x720, RGBA8) com buffer de profundidade (D24 ou D32), limpa ambos.
3. Cada `Gpu::Draw` envia uniforms (MVP e modelo no vértice; cor e flag de textura no fragmento), liga textura e sampler, e desenha a malha indexada.
4. `Gpu::EndFrame` fecha o passe e submete o command buffer.
5. A cena desenha a textura alvo com `SDL_RenderTexture` e o HUD por cima com o renderer 2D.

O fragment shader calcula a normal pela derivada da posição, então funciona em malhas sem normais e ilumina as duas faces. Texturas recebem mipmaps gerados na GPU e filtro anisotrópico.

## Depuração

Variáveis de ambiente lidas em `main.cpp`:

| Variável | Efeito |
|---|---|
| `PONTIFICE_SCENE=battle` | Abre direto na cena de batalha |
| `PONTIFICE_MOUSE=x,y` | Injeta um movimento de mouse em coordenadas lógicas a cada quadro |
| `PONTIFICE_SCREENSHOT=arquivo.bmp` | Salva o quadro 60 em BMP e encerra |

Exemplo: `PONTIFICE_SCENE=battle PONTIFICE_MOUSE=640,560 PONTIFICE_SCREENSHOT=/tmp/hand.bmp ./build/pontifice`

## HUD

O HUD é renderizado em **2D, por cima da cena 3D**, em uma passada separada. Não é afetado pela câmera nem pela virada de campo. Veja a seção [HUD](hud.md).
