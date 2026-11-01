# PONTIFICE

PONTIFICE é um jogo eletrônico brasileiro de cartas lançado exclusivamente para computadores inspirado na temática biblica.

## Configurações

`brew install cmake ninja`

`git init`

```bash
cat > .gitignore <<'EOF'
build/

.cache/
.DS_Store

.vscode/
.claude/
EOF
```

## Documentação

- [Regras do jogo](docs/regra_jogo.md): objetivo, componentes, fluxo da partida, fim da batalha e regras gerais.
- [Cartas](docs/cartas.md): categorias, hierarquias e cartas de campo.
- [Campo](docs/campo.md): slots, layout 5x2 e regras de posicionamento.
- [Batalha](docs/batalha.md): fases do turno, invocação, posições, ataque e cálculo de dano.
- [HUD](docs/hud.md): carta de campo ativa e pontos de vida na tela.
- [Detalhes técnicos](docs/detalhes_tecnicos.md): stack, estrutura do projeto, formato das cartas e renderização 3D.
