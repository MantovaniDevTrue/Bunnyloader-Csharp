> Histórico: para a .95, use SUMMON_LOADER_V95.md; as instruções e limitações abaixo descrevem a entrega anterior.

# Bunny C# .94 — ExampleMod CSharp 0.1.53

O cajado, buff e minion fazem parte do ExampleMod CSharp. Não é necessário instalar o Native Minion Probe separado.

1. Instale o APK .94 e o ExampleMod CSharp 0.1.53; desative o Native Minion Probe antigo.
2. Use **Example Minion Staff**. Confirme que a descrição indica dano de lacaio e que o buff **Example Minion** aparece ao invocar.
3. Confira a animação em quatro frames, a perseguição a um slime e a perda real de vida do inimigo por contato.
4. Invoque outro minion, teste os slots, remova o buff, salve o mundo e volte a entrar.
5. Teste receitas da ExampleWorkbench, equipamentos, efeitos Lights And Shadows e FPS.
6. Caso o dano continue zero, envie o relatório incluindo as linhas `csharp summon:` e contadores Projectile.OnHitNPC.

Este lote traz os consertos da .90.6 para a arquitetura .93. Os bônus finais de dano por acessórios continuam fora da recuperação de dano zero; o jogo preserva dano positivo. Os testes do CI não substituem a confirmação no Android.
