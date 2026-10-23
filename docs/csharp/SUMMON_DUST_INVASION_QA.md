# QA — cajado, Dust e busca da invasão (APK 313 / ExampleMod 0.1.65)

Base: hotfix 312, commit `d93468e5aae85a6be9af9488431694e6f84f8e52`, sobre 1.0.11 Dust/Invasion QA. O relato do usuário confirma armas funcionando com Lights And Shadows 1.0.0 e ExampleMod 0.1.64. O relatório identifica Xiaomi 2109119DG / Android 14 / Terraria 1.4.5.8.6 build 301720. Não informa versionCode e não mede FPS. Os novos comportamentos abaixo aguardam reteste no aparelho.

## Cajado original

Referência: `content/Content/Items/Weapons/Summon/ExampleMinionItem.js` do pacote JS 1.9.0 fornecido. A classe C# existente foi preservada, com mesmo nome e identidade de save.

Foram recuperados dano 30, knockback 3, mana 10, tamanho 32×32, uso/animação 36, Swing, venda por 30 gold, Cyan, som Item44, noMelee, summon, buff por 2 ticks e receita com um ExampleItem na Work Bench vanilla. O original não configura velocidade de disparo, portanto o default do cajado passa a zero. O controle de movimento do minion existente permanece responsável por mover o projétil após a criação. O minion também recebe o brilho branco 0.78 do original pelo campo nativo light. Esta mudança não declara paridade integral de toda a IA do minion.

API reutilizável em `BunnyShootSource`:

```csharp
// Calcula a posição na thread do jogo e chama o limitador real do Player.
SpawnAtCursor = true;
NativeDefaults.SetVanillaUseSound("Item44")
    .SetItemStaticBool("GamepadWholeScreenUseRange", true)
    .SetItemStaticBool("LockOnIgnoresCollision", true)
    .SetStaffMinionSlotsRequired(1);
```

`SpawnAtCursor` funciona em BunnyItem e BunnyWeapon. O host lê Main.MouseWorld, chama `Player.LimitPointToPlayerReachableArea(ref Vector2)` e aplica a posição validada antes da fábrica de projéteis e dos callbacks C#. Source/owner/dano/modificador continuam os originais. Jogadores remotos preservam a posição do motor; não usam a mira local. O exemplo não exige callbacks vazios nem executa JavaScript.

`SetVanillaUseSound` resolve um SoundID real de tipo LegacySoundStyle, mantendo a raiz estática e aplicando a referência com write barrier. As flags de ItemID.Sets e StaffMinionSlotsRequired usam arrays nativos expandidos e tipos verificados. **StaffMinionSlotsRequired é Int32[] neste Android**, não Single[]. Registros inválidos são rejeitados antes da publicação. CustomUseSound continua tendo precedência.

O protocolo SHOOT mantém linhas de três campos; um quarto campo `1` habilita a posição na mira. O cache Roslyn v54 invalida descritores anteriores.

## Dust

O log real encontrou `NewDustSignatureUnavailable`. A inspeção local do binário/metadata de Terraria 1.4.5.8.6 confirma que NewDust recebe `Microsoft.Xna.Framework.Graphics.Color`. O loader exigia Framework.Color e recusava a assinatura Android.

Agora são aceitas essas duas identidades de Color, sempre verificando que o parâmetro é value type de **quatro bytes**. `default(Color)` é enviado como quatro bytes zero, independente da ordem RGBA/ABGR. Retorno Int32, método estático, posição Vector2 e os outros oito parâmetros continuam estritos. O teste usa o resolvedor Signature.cpp de produção e Graphics.Color como cenário principal, incluindo rejeição de tamanho/namespace incorretos.

## Invasão

O relatório prova que o controlador inicia, mas acumula SpawnFailed com `NoSafeSpawnSite`; não prova ausência de uma API de NPC. A busca anterior recusava qualquer tile ativo na área livre, inclusive plantas e tochas. Agora a pré-seleção recusa sólidos e a validação final continua usando SolidCollision, WetCollision e LavaCollision do motor. A busca em ambos os lados foi ampliada, com limite fixo; preserva margem do mundo, piso válido, rejeição de lava/água e os limites de distância. Não há varredura do mundo.

Quando não houver local, o log passa a registrar coordenadas preferidas, tamanho do mundo, candidatos e rejeições por sólido/água/lava/exceção. Isso distingue um local realmente inadequado de falha de binding. **Sucesso de spawn e progresso ainda não estão confirmados no aparelho.**

## Testar no Android

Atualizar APK e importar ExampleMod 0.1.65; reiniciar. Manter Lights And Shadows ativo para a regressão de convivência.

1. Obter o cajado e ter mana. Invocar apontando para locais distintos, perto e longe: o minion deve nascer na mira limitada pelo alcance. Conferir swing, som, custo, tooltip e receita na bancada vanilla. Repetir com vários minions, remover o buff, reinvocar e salvar/reentrar.
2. Usar ExampleDustCharm: devem aparecer partículas. Conferir ausência de NewDustSignatureUnavailable. O efeito visível é necessário para validar, não apenas Result=Queued.
3. Usar ExampleInvasionBeacon nas condições permitidas, com chão próximo. Confirmar NPCs do ExampleMod, progresso ao derrotá-los e encerramento. Se falhar, enviar as novas linhas Stage=FindSpawnSite com contadores; não considerar Running como prova de spawn.
4. Repetir Gun/Rocket/Shotgun para ambos os lados; conferir que a nova opção do cajado não move os tiros das outras armas para o cursor.

## Encerramentos e memória

O relatório mistura processos e sessões: registra low-memory no mono_probe e SIGKILL no game, mas também save seguido de onUnityPlayerQuitted. Isso não estabelece a causa de um crash do jogo durante combate. Não foram introduzidos ajustes especulativos de GC/memória; é necessário correlacionar PID, horário e a ação que causou o encerramento. O pico isolado de ScreenFx de 18.9 ms também não mede FPS sustentado.
