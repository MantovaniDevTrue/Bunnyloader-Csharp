# Áudio C# — desenvolvimento do port integral

Este fonte demonstra a API de áudio nativa; o conjunto completo de exemplos ainda está em desenvolvimento e não há nova entrega parcial.

```csharp
// Em SetDefaults de BunnyItem ou BunnyWeapon. O arquivo pertence ao próprio mod.
CustomUseSound = new BunnySoundStyle("Assets/Sounds/Items/Guns/ExampleGun.ogg",
    volume: 0.9f, pitchVariance: 0.2f, maxInstances: 3);
```

O host confere o arquivo dentro de content/, registra o estilo no serviço Android compartilhado e cria o LegacySoundStyle do item. O marker permanece protegido pelo GC. Volume0..4, pitch-1..1, variação0..2, máximo0..256 (0 sem limite por estilo; a fila global continua limitada). ReplaceOldest substitui o mais antigo; IgnoreNew preserva as instâncias existentes. As definições são snapshots de SetDefaults, não ações por frame.

Use ExampleGun com ExampleBullet para conferir munição, dano, som, variação e offset(-18,0). Alterne lados e mire para cima/baixo; pause/retorne e teste junto de mod JS de áudio. Compare o asset original, limite de sobreposição e relatórios. O teste de host verifica contratos e transporte; não prova reprodução/decodificação no Android. Preservar os outros exemplos e os206 arquivos da matriz.

Configurações de som registradas são imutáveis durante a sessão: repetir a mesma definição reutiliza o registro; mudar asset/opções requer reiniciar o jogo. A recusa explícita evita reutilizar um style ID ainda referenciado por itens antigos. Não é suporte a hot reload de áudio.

O cache nativo reutiliza o asset por caminho canônico e o marker por arquivo/opções; conteúdos que compartilham a mesma configuração compartilham também o limite de instâncias, como no Bunny original. O cache de markers é fraco, sem reter objetos de lotes recusados. Falha assíncrona de decode é registrada no logcat com ID, caminho quando disponível e status real do SoundPool.


## SoundLoader independente: BunnySounds (C# puro)

Os exemplos de áudio de uso de item continuam funcionando. Para sons de impacto,
ambiente e interfaces, use a API C# abaixo. Ela chama diretamente a ponte nativa
do Bunny Loader; nenhum runtime ou API JavaScript é envolvido.

O exemplo jogável está em
`Common/Audio/ExampleSoundEffects.cs` e é acionado de
`Content/Projectiles/ExampleWaveBolt.cs` no callback `OnHitNPC`.

```csharp
using BunnyLoader.Source;

// Use o UID do manifest.json do mod que contém o arquivo.
const string modUid = "68c3115f-62f4-49f2-a56b-388256781d5f";
BunnySoundClip clip = BunnySounds.Register(modUid,
    new BunnySoundStyle("Assets/Sounds/Items/Guns/ExampleGun.ogg",
        volume: 0.7f, pitchVariance: 0.12f,
        maxInstances: 3, limitBehavior: BunnySoundLimitBehavior.IgnoreNew));

// Registro é idempotente: mantenha o clip em cache, não recrie por frame.
int state = clip.State; // 0 aguardando decodificação, 1 pronto, -1 falhou.
BunnySoundPlayback playback = clip.Play(500, 320); // Pixels do mundo.
int token = playback.Token; // 0 aguardando, >0 aceito, -1 recusado.
int current = clip.FindActive(); // Última reprodução ativa conhecida.
bool stopped = playback.Stop(); // Cancela mesmo se ainda estiver na fila.

// Som de interface, sem posição, usa clip.Play().
// Em descarregamento EXPLÍCITO do mod, não a cada saída do mundo:
BunnySounds.UnloadMod(modUid);
```

As chamadas de registro, reprodução e stop originadas do MonoVM apenas
enfileiram pedidos; o processamento do Android, os campos IL2CPP, a atenuação
e o controle de instâncias executam na thread do Terraria. Não há varredura
de todos os NPCs nem alocação de uma nova thread por reprodução.

Contratos: caminho relativo a `content/`, com extensão `.ogg`, `.wav` ou
`.mp3`; arquivo regular existente, dentro da pasta do mod (links simbólicos
externos são rejeitados); tamanho de 1 até 16 MiB. Até 32 estilos por mod,
256 estilos globais, 128 pedidos pendentes e 512 resultados retidos. O pump
limita trabalho por quadro e libera estilos de forma gradual. Registro repetido
com o mesmo arquivo e os mesmos parâmetros retorna o mesmo handle.

`FindActive()` e `Token` são leituras de estados assíncronos, não prova de que
o SoundPool efetivamente emitiu áudio. A saída do mundo cancela pedidos e
interrompe reproduções; mantém os estilos para a próxima entrada. `UnloadMod`
remove os estilos do UID e invalida seus handles; não modifica outros mods.

### Gate obrigatório antes de gerar qualquer APK

1. CI sem APK: SDK real `BunnySourceSdk` (net8), SDK autoral (net10),
   host test da ponte C++ e regressão `sound_router_test.cpp`.
2. A DLL atualizada e o `framework.sha256` devem acompanhar os fontes no
   pacote integrado; só ter `BunnySounds.cs` não habilita a API no runtime.
3. Validar integração Android ARM64 sem publicar APK, incluindo símbolos
   `bunny_csharp_sound_*` no `libbunny.so` e a ausência de dependência QuickJS.
4. Antes de liberar um APK, ainda será necessário demonstrar no jogo real:
   áudio posicional/esquerda-direita, limite IgnoreNew/ReplaceOldest,
   pause/retorno, troca de mundo, descarga do mod, coexistência com JS,
   ausência de erros IL2CPP, duração/stop e FPS sem regressões.

Até concluir os gates, o SoundLoader permanece desenvolvimento: não criar
APK de teste, distribuição ou release. Os testes em host não substituem
a validação em dispositivo.
