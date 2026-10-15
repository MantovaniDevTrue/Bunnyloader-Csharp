# Fontes C#

Os diretorios canonicos navegaveis estao em `Split/Bunny-Loader`:

- [SDK gerenciado e classes publicas](../Split/Bunny-Loader/experiments/csharp/BunnyCsSdk/)
- [Projeto BunnySourceSdk usado pelos mods-fonte](../Split/Bunny-Loader/experiments/csharp/BunnySourceSdk/)
- [Compilador Roslyn](../Split/Bunny-Loader/experiments/csharp/BunnySourceCompiler/)
- [Host MonoVM e ponte nativa](../Split/Bunny-Loader/app/src/main/cpp/experiments/csharp/)
- [App Android](../Split/Bunny-Loader/app/)
- [Exemplos e StarterMod](../examples/README.md)

Os atalhos legados dentro de `src/` nao definem a versao atual nem o conjunto
compilado do SDK. Em particular, a ponte C++ `script/api` e distinta das classes
C# de `BunnyCsSdk`. Use os caminhos acima para consultar ou alterar os fontes.
