# Detalhes técnicos

## Stack

- **Linguagem**: C++ (padrão C++17 ou superior).
- **Build**: CMake + Ninja. Scripts `build.sh` (compila) e `run.sh` (executa).
- **Formatação**: clang-format, estilo Google, indentação 2 espaços, limite de 100 colunas (`.clang-format`).
- **Renderização**: 3D em tempo real. Biblioteca gráfica a definir (candidatas: raylib, OpenGL puro, bgfx).
- **Plataforma**: computadores (macOS, Windows, Linux). Sem versão mobile ou console.

## Estrutura do projeto

```
src/
  main.cpp            ponto de entrada
  assets/             dados das cartas em JSON
    CardsGood.json
    CardsEvil.json
    CardsNeutral.json
    CardsField.json
  sprites/            texturas das cartas (PNG) e fontes de arte (PSD)
    CardBase*.png     molduras por categoria (Good, Evil, Field)
    CardBackBase.png  verso da carta
  models/             malhas 3D (campo, slots, mesa)
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

As cartas na mão também são **elementos 3D**, dispostos como um **dock**:

- Ficam na parte inferior da tela, em arco, levemente inclinadas em direção à câmera, próximas o suficiente para serem lidas.
- Ao passar o cursor sobre uma carta, ela **sobe e aumenta de escala**, e as vizinhas se afastam para abrir espaço (efeito de magnificação, como no dock do macOS).
- A carta sob o cursor é renderizada **por cima** das demais.
- Ao clicar e arrastar, a carta sai do dock e segue o cursor em 3D. Soltar sobre um slot válido a invoca; soltar fora devolve ao dock com animação.
- O dock se **reorganiza com animação** ao ganhar ou perder cartas, mantendo o arco centralizado.
- A mão do oponente é renderizada do lado oposto, sempre com o verso para a câmera.

## Virada de campo no turno do computador

Quando o turno passa para o computador, a **câmera orbita 180°** em torno do centro do campo, de modo que o jogador passa a ver a arena pelo ponto de vista do oponente.

- A rotação é uma **animação contínua** (cerca de 1 segundo, com easing), não um corte.
- Durante a animação, a entrada do jogador fica **bloqueada**.
- Ao final, o lado do computador aparece na parte de baixo da tela e o lado do jogador na parte de cima. As cartas viradas para baixo do jogador continuam mostrando o verso.
- O dock do computador aparece na parte inferior, com o verso das cartas. O dock do jogador fica no topo, também com verso.
- O **HUD não gira**: carta de campo ativa segue no canto superior esquerdo e os pontos de vida no canto superior direito, com os nomes "Você" e "Oponente" inalterados.
- Quando o turno volta ao jogador, a câmera orbita os 180° de volta, com a mesma animação.

## HUD

O HUD é renderizado em **2D, por cima da cena 3D**, em uma passada separada. Não é afetado pela câmera nem pela virada de campo. Veja a seção [HUD](hud.md).
