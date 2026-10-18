# Native Minion — CSharp Probe 0.1.2 (Bunny .90.6)

Este mod testa o runtime nativo Bunny C#/MonoVM sem dependências de tModLoader ou JavaScript.

1. Instale o Bunny Loader C# .90.6 e substitua o NativeMinionCSharpProbe 0.1.1 pelo 0.1.2.
2. Invoque Native C# Summon Staff; confira se o buff é visível e renovado enquanto o minion existir.
3. Confirme o texto de dano por lacaio, animação de 4 frames e comportamento ao passar perto de inimigos.
4. Num mundo de teste, deixe um slime se aproximar do minion: confira o dano por contato, barras de vida e eventos Projectile.OnHitNPC.
5. Invoque dois minions, controle slots e remova o buff; confirme a limpeza automática. Salve, saia, reentre.
6. Teste ExampleWorkbench, Lights And Shadows e FPS; registre o menor valor visto e eventos sem crash.
7. Envie o relatório completo especialmente linhas `csharp summon:` caso não surja buff ou não haja dano.

CI verde não valida automaticamente dano e estabilidade em Android. A .90.5 assinada permanece fallback.
