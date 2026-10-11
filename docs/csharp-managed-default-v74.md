# Bunny Loader C# .74 — MonoVM por padrão; NativeAOT como legado opcional

**Estado:** **APK .74 compilada, assinada e publicada no GitHub (Actions #38149174534, success)**; **validação de gameplay no Android ainda pendente**. [Release APK + ExampleModCSharp 0.1.9](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.74-managed-default).

## Por que esta limpeza existe

O caminho ativo de novos mods C# agora é: pacote .bmod com manifest.json e arquivos .cs, compilação Roslyn no Android e execução de callbacks pelo worker MonoVM. A .73 comprovou dois tipos com eventos gerenciados independentes no mesmo mod (985/985 eventos agregados, 879/879 secundários, zero falhas/descartes).

O protótipo NativeAOT interno (libbunnycs.so) fazia uma publicação AOT complexa em todo build e executava um benchmark no startup do jogo. Isso não faz parte da compilação de mods-fonte.

## Mudanças da .73 para a .74

| Sistema | .73 | .74 |
| --- | --- | --- |
| Runtime principal de mods .cs | Roslyn + MonoVM | Roslyn + MonoVM, explicitamente o padrão |
| Protótipo libbunnycs.so | Compilado, embutido e inicializado no startup | Não compilado/embutido e não inicializado por padrão |
| Carregador legado libmod.so | Disponível | **Preservado**, sem apagar o código ou os formatos antigos |
| Hook C++ para Terraria e registro nativo | Disponível | Preservado |
| Managed Ingress e callbacks por tipo | FIFO64, dois tipos testados | Mesma implementação funcional da .73 |
| ExampleModCSharp | 0.1.9 | 0.1.9, fontes funcionais mantidos; guias atualizados |
| Diagnóstico | hostInfo confundia disponibilidade do protótipo com C# | hostInfo detalhado + managedAvailable() |
| VersionCode | 98 | 99 |

Não é correto chamar libbunnycs.so de dependência do MonoVM: o MonoVM usa libmonosgen-2.0.so, suas assemblies e o código nativo de integração do Bunny.

**Compatibilidade:** mods NativeAOT externos com libmod.so continuam pelo carregador antigo CSharpModLoader. Sem garantir suporte para qualquer ABI externa não testada. A prova de compatibilidade no Android deve ser executada em uma versão posterior. Workflows e fontes históricos NativeAOT continuam no repositório, disponíveis como laboratório em vez de serem apagados.

## Compilação verificada

- GitHub Actions: [#38149174534](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38149174534) **success**, incluindo verificação da assinatura Android, versionCode 99, nomes das bibliotecas MonoVM, hashes SHA-256 das assemblies e ausência de libbunnycs.so no APK.
- APK publicado: BunnyLoader-CSharp-Test-v0.1.10.16.74-ManagedDefault.apk, 223.287.037 bytes.
- Mod de teste publicado: ExampleModCSharp-v0.1.9.bmod (funcionalmente igual ao usado na .73).
- O arquivo de 0,47 MB menor que o APK .73 não é demonstração de ganho de FPS ou RAM.

## Teste da .74 em Android

1. Instalar sobre a .73 **sem desinstalar**; se a assinatura for diferente, interromper e reportar.
2. Usar o ExampleModCSharp 0.1.9 de release; não habilitar duas versões simultâneas.
3. Verificar as duas armas, duas curvas, efeitos OnFire da principal e movimento próprio da Wave.
4. Atacar inimigos com as duas armas e observar FPS por 1–2 min. Entrar e sair do mundo ao menos uma vez para checar a inicialização limpa.
5. Executar no editor:

~~~javascript
JSON.stringify({
  runtime: bl.csharp.hostInfo(),
  mono: bl.csharp.managedAvailable(),
  auto: bl.csharp.sourceAutoStatus(),
  eventos: bl.csharp.sourceLiveHitStatus(),
  projeteis: bl.csharp.sourceLiveProjectileStatus()
}, null, 2)
~~~

**Esperado:** mono=true, runtimeMode=Roslyn+MonoVM, monoVmReady=true, nativeAotPrototypeAvailable=false, auto phase=ready e curves/enabledCurves=2, projeteis[0].projectile=ExampleWaveBolt e queued=processed depois de drenar, erros/drop/lockDrops/failures zerados, e jogo estável perto de 60 FPS. Não assumir resultados antes do teste no dispositivo.

**Importante:** a função antiga bl.csharp.available() ainda existe para compatibilidade com scripts antigos do protótipo e retornará false sem libbunnycs.so. Não confundir com indisponibilidade do compilador ou do MonoVM.

## Regressão e rollback

- Manter release .73 e sua APK 223,8 MB disponíveis.
- Preservar todos os efeitos e parâmetros funcionais do ExampleModCSharp 0.1.9.
- Se houver erro de carregamento, crash ou perda de callbacks, retornar à .73 e coletar relatório.
- Promover a .74 à referência somente após validado o gameplay em Android, incluindo medição visual de FPS relatada separadamente.

Links: [Release .73](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.73-multi-live-projectiles) · [Validação .73](multi-live-projectiles-v73.md).
