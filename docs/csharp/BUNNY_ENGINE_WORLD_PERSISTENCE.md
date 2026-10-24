# Bunny Engine — persistência de mundo C# (lote 2)

Referência de comportamento: `ExampleModBunny/ExampleMod/content/Common/Systems/DownedBossSystem.js`. O exemplo original define `ClearWorld`, `SaveWorldData`, `LoadWorldData`, `NetSend` e `NetReceive`. Este lote implementa os três primeiros para mods C# nativos, sem reutilizar APIs do tModLoader.

## Código para autores

```csharp
using BunnyLoader.Source;

public sealed class MyWorldSystem : BunnyWorldSystem
{
    public static bool BossDefeated { get; private set; }

    public override void ClearWorld() => BossDefeated = false;

    public override void SaveWorldData(BunnyWorldData tag)
    {
        if (BossDefeated)
            tag.SetBool("bossDefeated", true);
    }

    public override void LoadWorldData(BunnyWorldData tag)
    {
        BossDefeated = tag.ContainsKey("bossDefeated");
    }
}
```

O `BunnyWorldData` suporta `SetBool/GetBool`, `SetInt/GetInt`, `SetString/GetString`, `ContainsKey`. Os dados são codificados deterministicamente e validados. Limites: 64 chaves por sistema, 64 caracteres por chave, 2 KiB por string, 16 KiB por sistema, máximo de 128 registros por arquivo de mundo.

## Execução

- `ClearWorld` é executado na entrada de uma sessão.
- `LoadWorldData` é executado depois de ler o arquivo, se o armazenamento estiver disponível, antes de `OnWorldEnter`.
- `OnWorldTick` mantém a periodicidade do executor C# existente: atualmente aproximadamente 30 frames por callback, não execução síncrona por frame.
- `SaveWorldData` compara o estado serializado com a última versão persistida e só grava se houver mudança.
- Na saída de sessão, há tentativa adicional de salvamento antes de `OnWorldExit`.
- Um erro do callback é isolado ao sistema da sessão atual.
- Somente mundo local/servidor autoritativo pode salvar. Cliente multiplayer não grava este estado.

A gravação é feita pelo código C++ nativo em um arquivo auxiliar `<mundo>.wld.worldsystems.bl`, junto do caminho do mundo local. Arquivos existentes de invasões não são alterados. A gravação usa arquivo temporário, flush, fsync e rename; recusa dados malformados, nomes inválidos e arquivos acima dos limites. A busca e a gravação separam dados por mod UID e nome do sistema. Não há gravação em mundos salvos na nuvem nesta etapa.

## Testes

```sh
dotnet run --project experiments/csharp/BunnyWorldSystemsSmoke/BunnyWorldSystemsSmoke.csproj -c Release

c++ -std=c++20 -O2 -Iapp/src/main/cpp experiments/csharp/BunnyWorldSystemsSmoke/WorldPersistenceSmoke.cpp -o /tmp/bunny-world-save
/tmp/bunny-world-save
```

CI também compila `NativeWorldSystemPersistence.cpp` e o `BunnySourceSdk` principal.

## Pendências

O formato é um sidecar, não dados embutidos na estrutura `.wld`. O salvamento na nuvem, a migração automática entre arquivos de mundo, a rede `NetSend/NetReceive`, acesso seguro a todos os objetos IL2CPP, hot unload e validação no Terraria Android não estão concluídos. Se a API nativa não estiver disponível no momento da primeira entrada, o mundo continua funcional sem persistência para a sessão: o motor não sobrescreve dados que não conseguiu carregar. Ainda não existe APK de teste para este lote.
