# HUD

O HUD (Heads-Up Display) é a camada de informação fixa na tela durante a batalha. Ele fica por cima do campo e não ocupa slots.

```
+----------------------+------------------------+----------------------+
| [CARTA DE CAMPO]     |                        | OPONENTE  4200 / 5000|
|  ativa               |                        | VOCÊ      5000 / 5000|
+----------------------+                        +----------------------+
|                                                                      |
|                              CAMPO                                   |
|                                                                      |
+----------------------------------------------------------------------+
```

## Canto superior esquerdo: carta de campo ativa

- Mostra a **carta de campo ativa na arena**, virada para cima, com nome e imagem.
- Indica qual categoria está recebendo **buff** e qual está recebendo **nerf**, com os valores.
- Se não há carta de campo ativa, o espaço fica vazio ou exibe "Sem campo".
- Ao ser substituída, a nova carta de campo aparece neste espaço imediatamente.

## Canto superior direito: pontos de vida

- Mostra os **pontos de vida do oponente** e os **seus**, nesta ordem, de cima para baixo.
- Formato: `vida atual / vida máxima`. Ambos começam em `5000 / 5000`.
- O valor é atualizado logo após cada cálculo de dano.
- Ao chegar a `0`, a batalha termina e o resultado é exibido.
