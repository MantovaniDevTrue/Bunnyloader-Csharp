# ExampleModCSharp — exemplos reais para Bunny Loader C#

**Versão do pacote de teste:** `0.1.7` (release do Bunny Loader C# **0.1.10.16.60**).

Este exemplo existe para **ensinar e testar somente as APIs já implementadas na integração C# experimental do Bunny Loader**, sem copiar código do tModLoader. Ele não prova compatibilidade geral com mods de PC.

## Onde estão os arquivos

| Local | Finalidade |
| --- | --- |
| [`Playable/content/`](Playable/content/) | Fontes C# utilizados para montar o `.bmod` |
| [`Playable/manifest.json`](Playable/manifest.json) | Manifesto do pacote |
| [`Playable/content/Content/`](Playable/content/Content/) | Arma, consumível, acessório, buffs, NPC de exemplo |
| [`Playable/content/Common/`](Playable/content/Common/) | Player e demonstrações gerenciadas/eventos |
| [`Playable/content/Common/Live/BunnyLiveEvents.cs`](Playable/content/Common/Live/BunnyLiveEvents.cs) | `OnHitNPC` e `OnKill` automáticos da .60 |
| [`Examples/`](Examples/) | Exemplos históricos e didáticos de versões anteriores |
| [`Docs/`](Docs/) | Como testar e APIs documentadas |
| [`Tests/`](Tests/) | Verificações automáticas de pacote/fontes |
| [`compatibility.json`](compatibility.json) | Registro da cobertura declarada |

## Como testar a versão atual

Instale os dois artefatos da [release .60](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.60): APK **.60** e `ExampleModCSharp-v0.1.7.bmod`. Entre em **singleplayer**. Use a ExampleWand para atingir um inimigo e aguarde o projétil se destruir.

A .60 deve detectar e vincular `BunnyLiveEvents.OnHitNPC(int,int)` e `BunnyLiveEvents.OnKill(int,int)` **sem ativação manual no editor**. Consulte [o roteiro e os critérios](../../../../docs/TESTE-ANDROID-v60.md).

**Limite atual:** um tipo de projétil por processo para esses callbacks automáticos; eles contabilizam eventos sem alterar por si só o gameplay. Build aprovado no GitHub Actions; **o teste Android da .60 ainda está pendente**.

O histórico dos testes manuais e comandos de versões anteriores permanece em [`Docs/GettingStarted.md`](Docs/GettingStarted.md), inclusive os modos opt-in que **não** devem ser utilizados para comprovar a detecção automática da .60.
