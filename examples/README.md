# Exemplos C# — ExampleModCSharp

**Exemplos de teste atuais:** `ExampleModCSharp 0.1.9` e `Second Example CSharp 0.1.0` (dois mods independentes, três armas), distribuídos na [release .80](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.80-projectile-profiler) e validados no Android para combate, pausa, retomada e reentrada.

- [Código-fonte canônico do ExampleModCSharp](../Split/Bunny-Loader/samples/ExampleModCSharp/Playable/content/)
- [Manifest do pacote](../Split/Bunny-Loader/samples/ExampleModCSharp/Playable/manifest.json)
- [Arma ExampleWand](../Split/Bunny-Loader/samples/ExampleModCSharp/Playable/content/Content/Items/Weapons/ExampleWand.cs)
- [Callbacks automáticos OnHitNPC e OnKill](../Split/Bunny-Loader/samples/ExampleModCSharp/Playable/content/Common/Live/BunnyLiveEvents.cs)
- [Guia de começo](../Split/Bunny-Loader/samples/ExampleModCSharp/Docs/GettingStarted.md)
- [Compatibilidade documentada](../Split/Bunny-Loader/samples/ExampleModCSharp/compatibility.json)
- [Validador dos exemplos](../Split/Bunny-Loader/samples/ExampleModCSharp/Tests/check_examples.py)

**Não copie APIs do tModLoader diretamente.** Os exemplos devem refletir exclusivamente o que o compilador, host e APIs do Bunny realmente suportam. `BunnyLiveEvents` é uma convenção **experimental** com escopo restrito, não a API completa de mods C#.
