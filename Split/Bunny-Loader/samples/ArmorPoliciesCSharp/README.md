# ArmorPoliciesCSharp

Tres pecas independentes, com os fontes e sprites de armadura do ExampleModBunny
preservados nesta base. Cada receita usa dois Dirt Blocks numa Work Bench
vanilla: o mod nao depende do ExampleMod nem da bancada/material dele.

- Capacete: defesa 5 e conjunto completo com +20% de dano melee, ranged, magic
  e minion.
- Peitoral: defesa 6, +20 de mana maxima, +1 lacaio e imunidade a OnFire.
- Pernas: defesa 5 e +5% de velocidade de movimento.

Os atributos, imunidades, combinacao das pecas e registro de tooltip em
ArmorSetBonuses sao executados pelo host nativo. As texturas Head/Body/Legs
sao registrados por NativeItemPolicies, sem o carregador visual JS. O pacote do mod contem somente C# e PNG, sem main.js ou DLL.
Nao e uma promessa de novos callbacks C# gerais por frame.

Requer SDK e host da branch `codex/csharp-native-item-policies` com o campo
opcional 26 de ITEM; a APK .33 anterior nao entende esse contrato. Validado em
host com IL2CPP simulado; gameplay e desenho ainda precisam de teste no Android.

Para criar outro mod, gere UID novo com `tools/create_csharp_mod.py`, conserve
esse UID e copie os fontes/assets deste exemplo ajustando seu namespace.
Compile opcionalmente `dotnet build samples/ArmorPoliciesCSharp/ArmorPolicies.csproj -c Release`.
Empacote somente `Playable/` usando `tools/mods/pack.py`.

Use `Behavior.Armor.Add`, `Multiply` e `ImmuneTo`. `BunnyArmorSet` recebe as
chaves das tres pecas (nomes locais ou UID/Nome) e o texto do conjunto;
`set.Effects` usa as mesmas operacoes. Declare o conjunto em uma de suas pecas.
O registro espera todas as dependencias, preserva as linhas vanilla e refaz as
linhas se a tabela SetsContaining for substituida. Campos e larguras IL2CPP sao
validados antes da instalacao; imunidades e inteiros sao conferidos antes de
publicar qualquer escrita da policy naquele ciclo.

Teste as pecas separadamente, conjunto completo/incompleto, vaidade, loadout,
morte/respawn, save/reabertura e junto de outro mod. O teste nativo sem QuickJS
exercita os efeitos e outro teste consome os descritores reais e registra os
assets em tabelas IL2CPP simuladas. Isso ainda nao valida renderizacao Android.
