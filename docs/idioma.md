# Idioma

A escolha de idioma é a **primeira cena** do jogo: abre assim que o executável inicia e segue para o menu (docs/menu.md). O idioma escolhido vale para toda a sessão: menu, loading, batalha, HUD, tooltips e os textos impressos nas cartas.

## Layout

Fundo liso quase preto (`background.language`, `#091119`), sem ícone ou logotipo. Um item por idioma: apenas texto clicável com a bandeira à esquerda do nome escrito no próprio idioma, sem placa nem fundo de botão. Os itens formam uma coluna alinhada à esquerda (largura do item mais largo), centrada na tela; linhas de 4 células com 2 células entre elas.

```
+------------------------------------------------------------+
|                                                            |
|                                                            |
|                   [EUA]     English                        |
|                                                            |
|                   [Espanha] Español                        |
|                                                            |
|                   [Brasil]  Português                      |
|                                                            |
+------------------------------------------------------------+
```

Sem hover/foco, a bandeira fica em preto e branco (`flag::Draw` com saturação 0) e o nome usa `buttonText.text`; com hover, foco ou pressionado, a bandeira ganha as cores reais e o nome usa `buttonText.highlight` (mais claro). Mesmo estilo dos itens do menu (docs/menu.md). A área clicável é a linha inteira (bandeira + nome).

| Item | Bandeira | Idioma (`code`) |
|---|---|---|
| English | Estados Unidos | `en` |
| Español | Espanha | `es` |
| Português | Brasil | `pt` (padrão) |

As bandeiras são desenhadas com primitivas do renderer (`scripts/flag`), sem sprite: listras e cantão com pontos (EUA), três faixas (Espanha) e losango com círculo (Brasil). O parâmetro `saturation` mistura cada cor com seu cinza de luminância (0 = preto e branco, 1 = cor cheia).

## Comportamento

- **Clique** (ou `Enter`/`Espaço` no item focado) define o idioma e abre o menu.
- **Setas** movem o foco. Nenhum idioma começa selecionado: `↓` foca o primeiro, `↑` o último; `Enter`/`Espaço` sem foco não faz nada.
- `Esc` sai do jogo.
- Não há persistência: a cada abertura o jogo pergunta de novo.

## Textos (assets/translate.json)

Todos os textos exibidos vêm de `assets/translate.json`. Cada chave tem um texto por idioma:

```json
"menu.play": {"pt": "Jogar", "en": "Play", "es": "Jugar"}
```

- `languages`: lista na ordem dos itens, com `code`, `name` (nome no próprio idioma) e `flag` (`us`, `es`, `br`).
- `strings`: as chaves. `{0}`, `{1}`... são trocados em tempo de execução (`i18n::Format`), ex.: `"menu.level": "Nível: {0}"`.
- **Cartas**: `card.<code>.name` e `card.<code>.description`, um par por carta de `assets/Cards*.json` (ex.: `card.G101.name`). Os nomes das hierarquias ficam em `hierarchy.<categoryName>.<hierarchy>` (ex.: `hierarchy.good.2` = Arcanjos). Esses textos são aplicados ao carregar as cartas (`cards::LoadAll`), então frente da carta, tooltip, HUD e log já saem traduzidos. Sem a chave, fica o texto do JSON da carta, que está em inglês (`name`, `description` e `hierarchyName`).
- Chave ausente em um idioma cai no português; ausente em todos, aparece a própria chave (nunca some da tela).
- As fontes (`assets/pirataOne.ttf` e `assets/newsreader.ttf`) cobrem ASCII e Latin-1 (0xA0..0xFF): acentos, `ñ`, `ç`, `¿` e `¡` funcionam.

Grupos de chaves: `language.*`, `menu.*`, `difficulty.*`, `loading.*`, `battle.*` (status, dicas, tooltip), `hud.*`, `cardface.*` (ATK/DEF/BUFF/NERF, "Favorece"), `category.*`, `hierarchy.*`, `card.*`.

## Implementação

- Cena: `src/scenes/language/LanguageScene.*`.
- Item com bandeira: `src/scripts/button/ButtonText.*` com `SetFlag` (desenha a bandeira à esquerda do texto); a ação (definir o idioma e abrir o menu) fica em `LanguageScene`; bandeiras em `src/scripts/flag/Flag.*`.
- Textos: `src/scripts/i18n/I18n.*` (`Load`, `SetLanguage`, `Get`, `GetOr`, `Format`). Carregado em `main.cpp` antes da primeira cena; só leitura depois disso, então é seguro no `Preload` da batalha (outra thread).
- Depuração: `PONTIFICE_LANG=en|es|pt` já define o idioma e abre direto no menu; `PONTIFICE_SCENE=language` força a cena de idioma.
