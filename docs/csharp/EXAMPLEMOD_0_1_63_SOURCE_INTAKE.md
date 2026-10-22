# ExampleMod CSharp 0.1.63 — incorporação dos arquivos-fonte

**Base:** `ExampleMod-CSharp-0.1.63 (3).bl` enviada pelo autor do port.  
**SHA-256 do arquivo:** `c8a469f7f45e92349d5a16978cac9a62127a5f1fe0bd46bdee7e4c7700e8ef15`.  
**Conteúdo encontrado:** 174 entradas, 98 fontes C#.  
**Destino:** branch de porta C#, sem geração de APK, sem substituir a base estável.

## Critério de aproveitamento

Os 79 caminhos C# já presentes em `Playable/content` permanecem preservados; não se
substitui código atualizado pelo upload automaticamente. Os **19 arquivos C# adicionais**
foram copiados integralmente para `Porting/Archive-0.1.63/content`, com **SHA Git igual**
ao conteúdo do .bl original. Isso inclui bytes/acentos e identidades originais das
classes, não uma reescrita baseada em tModLoader.

A pasta `Porting` não faz parte do autoload do ExampleMod. É deliberado: compilar
uma classe que cita `BunnyCommand` ou `BunnyDust` sem sua ponte C# + C++ efetiva
não cria um exemplo funcional.

## APIs a incorporar ou compatibilizar

| Lote | Arquivos preservados | Base de implementação atual | Condição para ativar |
|---|---|---|---|
| Comandos | `ExampleEchoCommand`, `ExampleHealCommand`, `ExampleTimeCommand` | Bunny `BunnyCommand` / PR #117; integração parcial #140 | Registrar comando, alias, permissões, ajuda e envio real ao chat |
| Som | `ExampleSoundEffects` | Bunny `BunnySounds` / PR #118 | Estilo carregado uma vez, execução na thread do Terraria, limpeza por UID |
| Localização | `ExampleLocalizationModule` | Bunny `BunnyLocalizationModule` / PR #101 | Resolução UID, callbacks e troca de idioma |
| Pesca | `ExampleQuestFish`, `ExampleFishingPlayer` | Bunny `AnglerQuestLoader` / PR #114 | Missão válida, hardmode, gravidade invertida e UI do Pescador |
| Partículas | `ExampleSparkDust`, `ExampleDustCharm` | Bunny `BunnyDust` / PRs #115, #127, #132 | Registrar no MonoVM antes do OnUse, confirmar ABI e emissão nativa |
| Interface | `ExampleTooltipItem` | Bunny tooltip nativo / PR #123 | Linha colorida e onda arco-íris sem ModItem JS |
| Disparo | `ExampleShotgun` | Bunny `BunnyShootContext`, `BunnyShot` / integração #119 | Oito projéteis, spread original, direção sincronizada com a mira, munição consumida uma vez |
| Raridade | `ExampleModRarity`, `ExampleHigherTierModRarity` | Bunny `BunnyRarity` / PR #102 | Prefixos e cor Disco realmente atualizados no jogo |
| Parede | item e `BunnyWall` `ExampleWallAdvanced` | Fontes Bunny WallLoader | Placement, housing, dust, animação e luz noturna |
| Invasão | `ExampleInvasion`, `ExampleInvasionSystem`, `ExampleInvasionBeacon`, `ExampleInvasionEliteSlimeNPC` | Bunny InvasionLoader / PR #116 e correções posteriores | Ondas, spawn, restrições diurnas, world save e recompensa nativa |

Não basta que os nomes da API existam no SDK: o host, registrador e runtime do
Android precisam executar os hooks genuínos. Cada arquivo muda de `Porting` para
`Playable` **apenas** com a respectiva API funcional e a categoria completa de
comportamento do original.

## SoundLoader — fonte incorporada (22/10)

A implementação de `BunnySounds` foi integrada da PR #118 para esta branch,
incluindo o roteador nativo, oito chamadas de ABI C, fila limitada, descarregamento
por mod e manutenção pelo loop do Terraria. O arquivo original
`Common/Audio/ExampleSoundEffects.cs` foi promovido a `Playable` **sem alterar
um byte**, enquanto a versão de arquivo fornecida continua preservada em
`Porting/Archive-0.1.63`. O `ExampleWaveBolt` usa o som quando acerta um NPC;
não há decodificação de áudio por frame nem passagem pelo runtime JavaScript.

Os testes Linux C++, testes C# / PInvoke e compilação do exemplo passaram no
workflow **ExampleMod CSharp SoundLoader source-only (NO APK)**. Há também
checagem isolada de compilação dos objetos C++ ARM64, sem montar pacote Android.
Isso comprova o código e as interfaces de fonte, não o funcionamento no aparelho.

**Pendente:** integração final do SDK gerenciado ao runtime de distribuição,
verificação real de áudio posicional e unload no Terraria Android e QA de FPS.
Não marcar como Validado no Android antes desses testes. O source gate integral
do ExampleMod continua bloqueando APK até todos os 232 JS estarem completos.

## Conferência reproduzível (apenas fontes)

Dentro da raiz do repositório:

```bash
python3 tools/example_completion/verify_uploaded_csharp.py
python3 tools/example_completion/verify_uploaded_csharp.py --archive "/caminho/ExampleMod-CSharp-0.1.63 (3).bl"
python3 tools/example_completion/check_assets.py
```

A segunda chamada exige o **arquivo exato** fornecido; não usa versões
parecidas como prova. Mostra quais dos 79 caminhos existentes são idênticos
e quais mudaram. Diferenças não são apagadas ou classificadas como correção
sem comparação com a versão JS do Jonata.

**Regra de liberação:** nenhum APK de teste até a tradução de todos os
232 fontes JS e a aprovação do source gate. Android/release exige depois
prova de gameplay, ABI e FPS, separadamente. O pacote .bl enviado é um
snapshot de código, não uma autorização para chamar as APIs incompletas
de "validadas".
