# Montarias — próximo lote ExampleMod C# (paridade pendente)

Referência: arquivos fontes de `ExampleModBunny(1).zip` enviados pelo usuário, conferidos diretamente. Não usar API de tModLoader nem JavaScript como dependência do porte C#.

## ExampleMount e ExampleMountItem

- Montaria de carro com três balões; texturas `ExampleMount_Back.png`, `ExampleMount_Front.png` e `SimpleAccessory_Balloon` originais.
- Buff `ExampleMountBuff`: NoSave e NoTimeDisplay; invoca `mount.SetMount` do Terraria com a montaria registrada e renova o tempo para 10 ticks.
- JumpHeight=5, acceleration=0.19, jumpSpeed=4, blockExtraJumps=false, constantJump=true, heightBoost=20, fallDamage=0.5, runSpeed=11, dashSpeed=8, flightTimeMax=0, fatigueMax=0.
- Buff próprio e `spawnDust=GemDiamond`; 4 frames, `playerYOffsets` todos 20, xOffset=13, yOffset=-12, playerHeadOffset=22, bodyFrame=3.
- Ciclos de quadro originais: standing/running/idle 4 frames com delay 12, inAir 1 frame delay 12, swimming igual a inAir; flying 0.
- Por jogador: três balões, rotações atualizadas pelo vento e velocidade horizontal; poeira ao correr acima de 4.
- Na montagem: anel de 16 partículas a raio 80, suprimindo poeira padrão. No desenho: três sprites de balão antes da textura traseira; preservar direções, quadro e rotação individuais.
- Item: 20x30, useTime/useAnimation 20, Swing, valor sellPrice de 3 ouro, raridade Green, som Item79, noMelee, montaria vinculada e receita ExampleItem + ExampleWorkbench.

## ExampleMinecartMount e ExampleMinecart

- Montaria de carrinho: `MountID.Sets.Cart` real; preparar metadados via `Mount.SetAsMinecart` compatível com Terraria Android 301720.
- Partículas de spawn tipo 21; delegados nativos originais reutilizam `MeowmereMinecart.MinecartDust` e `FartMinecart.MinecartLandingSound` / `MinecartBumperSound`.
- Em `UpdateEffects`, chance 1/10 de `GemDiamond`: sem gravidade, fadeIn 0.6, scale 0.4, velocidade x/y vezes 0.25; shader secundário conforme `cMinecart`.
- Item: 34x22, valor sellPrice de 1 ouro, raridade Blue e receita Minecart vanilla + 15 ExampleItem na bigorna.

## Contratos C# inexistentes, não presumir implementação

O SDK agora expõe `BunnyMount`, `BunnyMountFrames` e `BunnyMountKind` como contratos tipados de autoria. O `NativeMountPolicy.h` interpreta o wire M1 com validação de limites, strings próprias e rejeição sem estado parcial. O `NativeMountVisualPolicy.h` implementa a matemática dos três balões, deslocamentos por direção, quadros por tempo, anel de 16 partículas e decisão de poeira do carrinho com chance 1/10, sem JavaScript e sem alocações por quadro.

Os dois modelos dos exemplos estão em `tests/examplemod_mount_qa/ExampleMountModels.cs`, propositalmente fora do pacote Playable: são contratos verificados, ainda não conteúdo registrável. O `CompiledAssemblyProbe` rejeita explicitamente classes `BunnyMount` em um mod até existir o registro nativo; não pode ocorrer uma aprovação enganosa com montarias invisíveis. A presença de `BunnyBuff`, `BunnyItem` e de `Player.mount` não cria automaticamente `mountType`, `Mount.mounts` nem os slots das texturas. Portanto, **nenhuma das duas montarias está funcional no Terraria ainda**.

O workflow `check-examplemod-mount-qa.yml` compila os descritores C# via SDK real, valida os 36 campos M1 no C++ sem QuickJS, executa os testes de geometria e rejeita dados inválidos. A primeira execução completa de host foi aprovada em 25/10/2026; os testes posteriores de propriedade dos caminhos precisam de nova execução. Nem esse teste nem uma compilação C++ no host constituem evidência Android.

Próximo trabalho: ligar o contrato M1 ao registro nativo com IDs por UID sem QuickJS; publicar MountID.Count, Mount.mounts e texture tables de modo transacional; vincular buff, itens e mountType por chave local; instalar hooks de montagem, animação e desenho com estado por jogador; e implementar descarregamento seguro e políticas de multiplayer. Reutilizar os algoritmos nativos de balões/poeira já testados sem omitir partículas, som ou shaders. Reaproveitar o `MountLoader` do Bunny apenas como documentação de contrato original, nunca como ponte JavaScript em runtime.

Antes de publicar: compilação ARM64 sem APK, testes de contrato e roundtrip de registro e unload, validação em Terraria Android 1.4.5.8.6 de ambas as montarias, multiplayer, animação, colisão, montaria/desmontaria, som, sprites, 60 FPS e estabilidade. O trabalho do Blocky permanece separado: não bloquear as verificações já aprovadas com uma montaria apenas parcialmente funcional.
