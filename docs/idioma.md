# Idioma

A escolha de idioma é a **primeira cena** do jogo: abre assim que o executável inicia e segue para o menu (docs/menu.md). Depois, o item **Idioma** da página principal do menu abre a cena de novo para trocar o idioma. O idioma escolhido vale para toda a sessão: menu, loading, batalha, HUD, tooltips e os textos impressos nas cartas.

## Layout

Fundo liso quase preto (`stoneDarker`, `#091119`), sem ícone ou logotipo. Um item por idioma: apenas texto clicável com a bandeira à esquerda do nome escrito no próprio idioma, sem placa nem fundo de botão. Os itens formam uma coluna alinhada à esquerda (largura do item mais largo), centrada na tela; linhas de 4 células com 1 célula entre elas (`button.json` `buttonText.flag.rowGap`), o mesmo vão do menu.

```
+------------------------------------------------------------+
|                                                            |
|                                                            |
|                   [EUA]     English                        |
|                                                            |
|                   [Espanha] Español                        |
|                                                            |
|                   [França]  Français                       |
|                                                            |
|                   [Brasil]  Português                      |
|                                                            |
+------------------------------------------------------------+
```

Sem hover/foco, a bandeira fica em preto e branco (`flag::Draw` com saturação 0) e o nome usa `steelLight`; com hover, foco ou pressionado, a bandeira ganha as cores reais e o nome usa `silverLight` (mais claro). Mesmo estilo dos itens do menu (docs/menu.md). A área clicável é a linha inteira (bandeira + nome).

| Item | Bandeira | Idioma (`code`) |
|---|---|---|
| English | Estados Unidos | `en` |
| Español | Espanha | `es` |
| Français | França | `fr` |
| Português | Brasil | `pt` (padrão) |

As bandeiras são desenhadas com primitivas do renderer (`scripts/flag`), sem sprite: listras e cantão com pontos (EUA), três faixas (Espanha), três faixas verticais (França) e losango com círculo (Brasil). O parâmetro `saturation` mistura cada cor com seu cinza de luminância (0 = preto e branco, 1 = cor cheia).

## Comportamento

- **Clique** (ou `Enter`/`Espaço` no item focado) define o idioma e abre o menu.
- **Setas** movem o foco. Nenhum idioma começa selecionado: `↓` foca o primeiro, `↑` o último; `Enter`/`Espaço` sem foco não faz nada.
- `Esc` sai do jogo; aberta pelo item **Idioma** do menu (`SceneContext::language_from_menu`), volta ao menu sem trocar o idioma.
- Persistência: o idioma é gravado no save (docs/menu.md). Com save, o jogo abre direto no menu com o idioma salvo; a cena de idioma só aparece de novo pelo item **Idioma** do menu ou com `PONTIFICE_SCENE=language`, e escolher ali atualiza o save.

## Textos (assets/translate.json)

Todos os textos exibidos vêm de `assets/translate.json`. As chaves ficam aninhadas por grupo e cada folha tem um texto por idioma; no código o caminho vira a chave pontuada (`i18n::Get("menu.play")`):

```json
"menu": {
  "play": {"pt": "Jogar", "en": "Play", "es": "Jugar", "fr": "Jouer"}
}
```

- `languages`: lista na ordem dos itens, com `code`, `name` (nome no próprio idioma) e `flag` (`us`, `es`, `fr`, `br`).
- Demais chaves da raiz: os grupos de textos. `{0}`, `{1}`... são trocados em tempo de execução (`i18n::Format`), ex.: `menu.level` = "Nível: {0}".
- **Cartas**: `card.<code>.name` e `card.<code>.description`, um par por carta de `assets/Cards*.json` (ex.: `card.G101.name`). Os nomes das hierarquias ficam em `hierarchy.<categoryName>.<hierarchy>` (ex.: `hierarchy.good.2` = Arcanjos). Esses textos são aplicados ao carregar as cartas (`cards::LoadAll`), então frente da carta, tooltip, HUD e log já saem traduzidos. Sem a chave, fica o texto do JSON da carta, que está em inglês (`name`, `description` e `hierarchyName`).
- Chave ausente em um idioma cai no português; ausente em todos, aparece a própria chave (nunca some da tela).
- As fontes (`fonts/pirataOne.ttf` e `fonts/newsreader.ttf`) cobrem ASCII e Latin-1 (0xA0..0xFF): acentos, `ñ`, `ç`, `¿`, `¡` e `«»` funcionam. `œ` (U+0153) e aspas curvas ficam fora e não aparecem: os textos em francês evitam palavras com `œ` e usam apóstrofo reto. Antes de `:`, `;`, `!` e `?` o francês usa espaço não separável (U+00A0), que não quebra a linha.

Grupos de textos, por cena e por elemento:

| Grupo | Conteúdo |
|---|---|
| `general` | Termos genéricos repetidos em várias telas: `opponent`, `player`, `you`, `back`, `forward`, `menu`, `attack`, `defense` |
| `ui` | Componentes de interface: `confirm.yes` e `confirm.no` do diálogo de confirmação |
| `loading` | Tela de carregamento (`label`) |
| `menu` | Menu: `play`, `battle`, `grimoire`, `exit`, `level`, `credits`, `namePlaceholder`, `save` e `difficulty.*` (easy/medium/hard/impossible) |
| `grimoire` | Grimório: `title`, `page`, `empty` |
| `battle` | Batalha: `endTurn`, `noRender`, `status.*`, `hint.*`, `tooltip.*`, `abandon.*` (título e pergunta), `reveal.*` |
| `hud` | HUD da batalha: `noField`, `deck`, `graveyard`, `victory`, `defeat`, `stats.*`, `reward.new` e `reward.repeated` |
| `cardface` | Frente da carta: `atk`, `def`, `buff`, `nerf`, `favors` |
| `category` | Nomes das categorias (`good`, `evil`, `neutral`, `field`) e plurais (`plural.good`, `plural.evil`) |
| `hierarchy` | `hierarchy.<categoryName>.<hierarchy>` |
| `card` | `card.<code>.name` e `card.<code>.description` |

A tela de idioma não tem textos próprios: os nomes vêm da lista `languages`.

## Implementação

- Cena: `src/scenes/language/LanguageScene.*`.
- Item com bandeira: `src/scripts/button/ButtonText.*` com `SetFlag` (desenha a bandeira à esquerda do texto); a ação (definir o idioma e abrir o menu) fica em `LanguageScene`; bandeiras em `src/scripts/flag/Flag.*`.
- Textos: `src/scripts/i18n/I18n.*` (`Load`, `SetLanguage`, `Get`, `GetOr`, `Format`). Carregado em `main.cpp` antes da primeira cena; só leitura depois disso, então é seguro no `Preload` da batalha (outra thread).
- Depuração: `PONTIFICE_LANG=en|es|fr|pt` já define o idioma (no lugar do salvo) e abre direto no menu; `PONTIFICE_SCENE=language` força a cena de idioma.
