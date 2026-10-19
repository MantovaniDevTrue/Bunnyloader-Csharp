# DustLoader C# — desenvolvimento isolado, sem APK

## Origem

Branch `feat/csharp-dust-loader-20261019`, base `v1.0`. O motor `content/dusts/ModDusts.h/.cpp` veio da fonte do Bunny Loader original `JonataOliveiraa/Bunny-Loader`. O caminho C# não usa QuickJS nem código do tModLoader.

## Implementado e validado em CI

- Registro nativo antecipado `BunnyDust` no `SourceParser` e `SourceLoader`, propriedade `BunnyContentKind.Dust`, domínio `DUST`, IDs por mod e verificação de caminhos e existência do PNG.
- Motor Dust do Bunny original importado, `tickModDusts` habilitado e incluído na barreira `contentSettled` antes do mundo.
- Compilador Roslyn reconhece classes `BunnyDust`, chama `SetStaticDefaults` e exporta `DUST\tClassName\tUpdateType\tHookMask\tTexture` para o host, com validação de identidade e bounds.
- `BunnyDust.cs`: estados de 64 bytes, registro, máscaras de métodos sobrescritos, lifecycle por tipo/slot/generation, rollback, isolamento de falhas e reset.
- `BunnyDustSourceDispatcher.cs`: registra assemblies gerenciadas, faz bind dos tipos e executa callbacks a partir do ponteiro de estado nativo, sem JavaScript. O dispatcher funciona nos smoke tests isolados; **ainda não foi conectado ao worker do jogo**.
- `ManagedDustMailbox.h`: fila limitada a 128 registros, sem espera na thread do jogo, rejeição de respostas antigas e limitação de idade a 3 frames. Ainda precisa ser ligada ao ciclo real do MonoVM.
- Exemplo `ExampleSparkDust` no ExampleMod C#, com `TrailblazerSpark.png` reaproveitado da fonte do sample.
- Testes de contrato C#/C++ e smoke real de Roslyn e exportação do descritor foram aprovados; compilação ARM64 C++ do projeto Android passou sem gerar pacote instalável.

## Bloqueadores reais antes de release

1. Vincular `BunnyDustSourceDispatcher` à thread MonoVM residente, registrando as DLLs e os IDs dos tipos no worker. O mecanismo nativo deve rejeitar chamadas a mods não vinculados.
2. Integrar `ManagedDustMailbox` ao hook Dust com capturas em lote, geração de spawn, retornos assíncronos e aplicação dos resultados na thread do jogo. Não transferir ponteiros de objetos IL2CPP para o worker.
3. Aplicar sem perda os retornos de `Update`, `MidUpdate`, `GetAlpha`, `PreDraw`, `OnSpawn`, inclusive kill / `SetNotActive` sem quebrar a pilha de 6000 slots.
4. Validar concorrência, descarte de lotes e deadlines, respostas em atraso, proteção de geração, coexistência de mods, menus e troca de mundo.
5. Validar texturas personalizadas, frames, alpha e render no Android/Terraria; corrigir qualquer diferença do ExampleModBunny.
6. Verificar desempenho, estabilidade, integração completa com QuickJS existente e todas as fontes nativas/gerenciadas em CI.
7. Só depois de **todos** os gates, considerar uma build de teste. Nunca gerar APK de um lote incompleto.

## Segurança de desenvolvimento

`managedDustCallbacksConnected()` retorna `false` enquanto a ponte com o worker não existe. O host **rejeita** mod C# de Dust que exija hooks personalizados nesse estágio, com `MonoDustBridgeNotConnected`. Dust sem callbacks pode seguir o caminho nativo. Isso impede declarar falsamente que todos os efeitos funcionam.

A PR fica em draft e não deve ser mesclada como feature funcional enquanto os bloqueadores persistirem.
