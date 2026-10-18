# Native Minion CSharp Probe 0.1.1 — Lote .90.5

Teste nativo da API C#/MonoVM, sem JavaScript. Preserva o ExampleMod CSharp 0.1.49.

1. Instale o Bunny Loader .90.5 e o NativeMinionCSharpProbe 0.1.1. Desative o probe 0.1.0.
2. Em mundo de teste, invoque usando Native C# Summon Staff. Confira o ícone de buff Native C# Minion.
3. Verifique que o minion mostra a textura original ExampleMinion e alterna entre quatro frames.
4. Invoque um slime próximo: o minion deve persegui-lo e acertá-lo, não apenas seguir o jogador.
5. Mova-se, passe por blocos e reinvoque com um minion ativo. Confira uso de slots, ausência de duplicação e buff persistente.
6. Salve, saia, reentre e verifique se o minion/buff é limpo sem estado vazando. Compare FPS e latência com .90.4.
7. Teste ExampleWorkbench, Lights And Shadows e Weapon Out Lite para regressões.
8. Envie o relatório completo se o minion não causar dano, não mostrar ícone/buff ou se houver travamento.

Regressões de compilação NÃO significam confirmação no Android. A .90.4 assinada continua como fallback.
