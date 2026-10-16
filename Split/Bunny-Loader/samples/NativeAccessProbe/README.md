# Native Access Probe CSharp 0.1.0

O pacote combina o conteúdo mínimo `ProbePlayer` com o módulo canônico [ExampleNativeAccess](../ExampleModCSharp/Experimental/NativeAccess/ExampleNativeAccess.cs). O empacotamento copia esse fonte para o pacote temporário, evitando manter duas implementações no repositório.

Requer a build experimental .69. Desative **Universal Hooks Probe CSharp**, pois ambos usam o mesmo método `Player.ResetEffects`. ExampleMod CSharp e ArmorPolicies podem permanecer ligados. O teste consulta vida, posição, dia/noite e o índice de On Fire; não escreve atributos do personagem.

1. Instale o APK e ative este `.bmod`.
2. Entre em singleplayer e confira no Editor C# a linha `Acesso C#`.
3. `lotes` e `metodos` devem crescer; `escritas` deve permanecer zero. Confira também os contadores dos hooks e FPS.
4. Feche o editor, jogue alguns minutos e confira novamente. Saia e entre no mundo para exercitar descarregamento e novo vínculo.

A consulta manual é `Print(BunnyNativeScope.GetStatus());`. `falhas` registra operações nativas recusadas e exceções; `esperasVencidas`, pedidos cancelados antes da conclusão. O teste ainda requer validação no aparelho.
