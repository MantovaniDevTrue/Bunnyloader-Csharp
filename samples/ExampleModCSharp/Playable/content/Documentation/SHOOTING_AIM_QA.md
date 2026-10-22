# Direção e origem do tiro — 1.0.11 Dust/Invasion QA

Este lote parte de `fcdc3fd2c30d1e7432fddbafc4af9f6d65ca473a` (PR #132), preservando os exemplos e assets do pacote C# 0.1.63 fornecido. A compilação dos 98 fontes e os testes de host não comprovam funcionamento no aparelho.

A ponte C++ agora sincroniza `Player.direction` com o X de `Main.MouseWorld` relativo ao centro do jogador **antes** de executar `ItemCheck_Shoot`. O método original continua escolhendo munição, source, owner, modificador, dano e velocidade. O `NativeHoldout` existente aplica o deslocamento visual do exemplo; este lote não substitui a física original por uma origem inventada.

## API e exemplos reais

`BunnyItem.NativeDefaults` declara os campos nativos. Ao declarar `shoot`, a arma entra automaticamente no caminho de sincronização, mesmo sem `OnShoot`. A API legada `BunnyWeapon.Shoot` recebe o mesmo tratamento. `CanShoot`, `ModifyShootStats` e `OnShoot` mantêm suas assinaturas.

Exemplo existente, `ExampleGun.SetDefaults`:

```csharp
// O tipo de munição do inventário determina o projétil efetivamente lançado.
NativeDefaults.SetId("shoot", "ProjectileID", "PurificationPowder")
    .SetFloat("shootSpeed", 10)
    .SetId("useAmmo", "AmmoID", "Bullet");
// Offset visual usado pelo desenho original, depois da direção de tiro correta.
Holdout = new BunnyHoldout(-18, 0);
```

`ExampleRocketLauncher` usa o mesmo contrato com munição de foguete. `ExampleShotgun.OnShoot` é o exemplo com callback: continua gerando os oito projéteis com dispersão, preservando a munição original. Não é necessário adicionar callbacks vazios para acertar a direção.

No protocolo interno, `SHOOT <classe> 0` significa apenas preparação nativa de direção; bits 1/2/4 continuam representando os três callbacks. Máscara zero não inicializa nem aguarda o Mono. O cache de compilação foi versionado para regenerar descritores antigos. As políticas são removidas por mod no reset, recarregamento e falha de carregamento.

A resolução valida tipos, armazenamento, limites dos offsets e layout X/Y de Vector2 uma vez. `MouseWorld` pode ser campo estático ou propriedade estática; o getter é chamado pelo runtime IL2CPP, sem assumir o ABI de retorno de structs. A leitura e escrita de campos ocorrem na thread original do jogo. Não há busca no mundo nem reflexão a cada frame.

Jogadores remotos mantêm a direção recebida pela rede. Mira perfeitamente vertical mantém a direção atual. Coordenadas não finitas, largura inválida ou exceção no getter preservam o estado original e produzem diagnóstico; isso exige investigação no aparelho, não conta como sucesso do recurso.

## Reprodução automática

Na raiz, com .NET 8 e g++ no PATH:

```sh
dotnet run --project tests/basic_items/ItemBatch.csproj -c Release
python3 tests/basic_items/check_full_wire.py build/basic-items/full-source-runtime-values.txt
DOTNET=dotnet bash tests/shooting/run.sh
dotnet run --project tests/ready_prs/FullExampleRuntime/FullExampleRuntime.csproj -c Release -- samples/ExampleModCSharp/Playable
python3 tools/examplemod_coverage.py --check
```

O teste compila os exemplos reais com Roslyn e executa a implementação C++ de produção contra metadados IL2CPP de host. Confere direção dentro do original, lados/vertical, ownership remoto, getter de Vector2 e exceção, coordenadas inválidas, classes sem callback, warmup, remoção seletiva, assinaturas/layouts incorretos e preservação do disparo. Os testes gerenciados exercitam o algoritmo real da shotgun. Os metadados do host não são evidência de compatibilidade com a build Android.

## Roteiro Android pendente

Alvo: Xiaomi Mi 11 Lite 5G NE, Android 14, Terraria 1.4.5.8.6, build 301720, arm64. Importar somente o pacote C# deste lote, reiniciar e entrar num mundo de teste. Obter ExampleGun, ExampleRocketLauncher e ExampleShotgun e munição compatível pelo menu de conteúdo existente.

1. Parado, mirar e atirar à esquerda e à direita sem primeiro mover o personagem. Registrar direção, posição do cano e trajetória, com vídeo e logs.
2. Mirar verticalmente, mudar de lado durante o uso automático e repetir com gravidade invertida. Verificar holdout e origem; este lote não declara essas situações validadas em jogo.
3. Alternar tipos de bala/foguete. Conferir o projétil, consumo, dano e owner reais; confirmar oito projéteis da shotgun com a dispersão esperada.
4. Repetir com um mod JS comum junto e com dois mods C# independentes; recarregar/desativar um sem quebrar o outro.
5. Reentrar no mundo e reiniciar o app; testar inventário salvo. Em multiplayer, confirmar que a mira local não gira o jogador remoto.
6. Registrar boot, carga de mods, FPS, memória, logs e hash do APK/mod. Campos não medidos permanecem `null`.

Problemas pendentes do objetivo global estão em `EXAMPLEMOD_COVERAGE.md`/`.json`. Em particular, o tooltip C# ainda depende de execução JS. Não declarar que todos os loaders estão completos.
