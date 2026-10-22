# Componentes nativos de superfície — 22/10/2026

Checkpoint inicial sobre `f2d98fcd`, na mesma branch da PR130. A continuação
sobre `eb6c44b4` acrescenta o [serviço nativo dos seis hooks](csharp-surface-native-runtime.md),
registro, seleção/menu, callbacks C++ e retenção durante fades. Nenhum sistema
anterior foi substituído. O SurfaceBackgroundLoader **ainda está parcial**:
o serviço nativo executa nos testes, mas faltam os adaptadores de MonoVM/QuickJS
e seu carregamento automático pelas linguagens. Não existe ainda uma
API pública C# de superfície nem um exemplo C# falsamente apresentado como port.
Os quinze alvos continuam contabilizados: **2 implementados nos fontes, 1 parcial,
12 pendentes; 0 validados no Android**. APK e `.bmod` continuam bloqueados.

## Implementação e responsabilidade

| Componente | Código | Contrato executado |
| --- | --- | --- |
| Propriedades Single e arrays primitivos | `il2cpp/CheckedProperty.h` | Single de quatro bytes, Single[] de elementos de quatro bytes, Boolean[] de elementos de um byte; rank, classe, storage e limites verificados. |
| Escrita direta verificada | `il2cpp/CheckedFieldWrite.h` | Campo mutável, proprietário correto, tamanho exato, referência compatível e barreira GC. Propriedade apenas com getter, readonly/literal, structs sem layout de escrita e ausência de exports são recusados. |
| Valores nullable | `il2cpp/NullableLayout.h` | Tipo fechado System.Nullable com flag Boolean e T exato, layout raw/boxed, tamanhos/offsets/alinhamento, campos sem sobreposição ou referências gerenciadas. Ausente e presente com zero têm codificações diferentes. |
| Geometria e fades | `content/backgrounds/SurfaceGeometry.*` | Projeção Far/Middle/Close, arredondamento JS para negativos, escala ×2, altura a/b, clipping depois do céu, prioridades floresta/selva/neve, multiplicação de cor e fades padrão. Não faz seleção de menu/cena. |
| Ampliação das quatro tabelas | `content/backgrounds/SurfaceTables.*` | Preflight de todas as tabelas, raízes, quatro alocações antes de publicar, preservação de dados e rejeição de troca de geração/array. Precisa de campos diretos da instância real; não presume setter transacional. |
| Desenho engine | `content/backgrounds/SurfaceSpriteBatch.*` | Resolve o overload SpriteBatch.Draw de nove argumentos e SkyManager.DrawToDepth pela metadata real. Monta Vector2, Nullable<Rectangle>, Color e SpriteEffects pelos respectivos layouts; chama runtime_invoke sem QuickJS. |

Os caminhos são relativos a `app/src/main/cpp`. Estes são serviços internos;
não estão expostos como uma API C# sem ligação ao runtime. Não existe ID ou
comportamento exclusivo do ExampleMod nesses componentes.

`CheckedFieldWrite` separa escrita de leitura: encontrar `get_foo` não autoriza
escrever memória de `foo`. Campos de referência estáticos recebem o objeto
diretamente em `field_static_set_value`; campos de instância usam a barreira GC.
Os testes verificam essa diferença, que já causou corrupção em bridges antigas.

`growTables` deve executar na thread do jogo. O caminho de tabelas já ampliadas
não cria arrays nem handles GC. Quando há crescimento, mantém arrays e donos
enraizados até terminar e rejeita uma geração vencida antes da publicação.
Uma falha de alocação ou raiz não publica nenhuma das quatro tabelas. Não usa
callbacks/setters gerenciados entre as escritas. O chamador futuro deve fornecer
o teste da instância `LocalUserGameState` ainda vigente, além dos campos reais.

`SurfaceSpriteBatch` usa o batch já aberto pelo Terraria, preservando Begin/End,
blend e sampler. Resolve a classe Color recebida pelo overload: aceita o
namespace Graphics do Android e o Framework quando ele for o parâmetro real.
As ordens de X/Y, canais e campos de Rectangle não são presumidas. Erros de
desenho preservam a exceção/stack fornecida pelo IL2CPP e a etapa DrawLayer ou
DrawToDepth. O chamador ainda precisa acrescentar UID/classe/slot e limitar logs.

O desenho enraíza batch, textura e céu durante a chamada. Isso **não substitui**
os leases de assets que a integração do loader deverá manter durante fades e
unload: uma raiz GC sozinha não impede `Texture2D.Dispose`. Ainda falta essa
integração. As coordenadas ficam em Double até montar o Vector2 nativo, evitando
arredondamento antecipado em posições grandes.

## Evidência executável

Executar `bash tests/surface_backgrounds/run.sh`. Para acrescentar ASan, UBSan,
verificação de divisão por zero e leak detector:
`BUNNY_SURFACE_SANITIZE=1 bash tests/surface_backgrounds/run.sh`.

- O fixture nativo reutiliza a metadata/heap controlados dos testes subterrâneos
  e executa os novos `.cpp` de produção. Cobre falha de cada alocação/raiz,
  recriação de arrays, referências erradas, rank/tamanhos incorretos, campos
  estáticos/read-only/fora do objeto, publicação estática correta e liberação
  de todos os handles. O overload de desenho é realmente invocado pela API
  IL2CPP controlada; não se trata de procurar strings no fonte.
- Valores nullable testam ausência versus zero, flag inválida, T incorreto,
  byref, sobreposição, alinhamento incompatível e ambas as convenções de offsets.
- Os testes de desenho usam X/Y invertidos, Rectangle reordenado, Color BGRA,
  namespace Graphics e flag nullable depois do valor. Testam também argumentos
  incorretos, overload ambíguo, exceções nativas e falta de exports.
- O JavaScript original imutável está em `tests/surface_backgrounds/reference`,
  com hashes conferidos. O original executa no Node/V8 e seus hooks geram os
  desenhos comparados com 45 projeções nativas, 144 decisões de prioridade e
  cinco atualizações de fades. A referência também fixa a animação de 53 chamadas
  e o lookup pelo caminho completo. Essa última verificação é um **oráculo do
  original**, não prova de animação C# já portada.

Nenhum teste novo executa Terraria, MonoVM Android, GPU real ou QuickJS real
de superfície. A independência destes componentes C++ é demonstrada pelo link
sem QuickJS; não certifica um loader C# completo. Não há nova medição de FPS.

## Continuidade exata

1. O núcleo agora registra UID/classe, consulta Count real, arbitra prioridade,
   cena/menu e mantém Wake/Sleep. Adaptar as duas bridges para esse registro:
   o loader JS atual continua preservado e **ainda não o utiliza**.
2. Os seis detours e os contratos de Main/LocalUserGameState/Sets estão no núcleo,
   com nullable não-HFA de oito bytes validado para AAPCS64. Os testes executam
   os detours no host; confirmar a metadata real e a renderização no Android.
3. Manter as transações de quatro tabelas, cores, geometria fracionária e leases
   já implementadas ao ligar o SDK. Não voltar a presumir scAdj inteiro nem
   descartar a verificação de setters e layouts de cores.
4. Criar transação nativa/worker por revisão para ModifyFarFades, escolhas e
   PreDrawCloseBackground com comandos reais de desenho personalizado. Não
   substituir callback por flag fixa nem permitir resposta vencida.
   O estágio 11 é o relógio interno e 12 é UndergroundBackground; preservar os
   índices e ampliar Count/assinaturas/remoção ao acrescentar um estágio.
5. Adaptar a bridge JS ao mesmo registro/serviço sem C# invocar QuickJS; ligar
   SDK natural/reutilizável e portar ExampleSurfaceBackgroundStyle com as seis
   texturas já existentes, incluindo quatro frames e avanço a cada 13 chamadas.
6. Testar C# por P/Invoke, JS original no QuickJS real e ambos juntos; descarte
   durante fade, menu/mundo, exceções e dependências ausentes. Só então promover
   o contrato para implementado, atualizar cache se mudar SDK/protocolo e criar
   o comando/exemplo de QA. Android continua uma etapa distinta.

O CI source-only inclui estes testes e os três novos `.cpp` na checagem ARM64
e linkagem nativa completa. Nenhuma tarefa de empacotamento foi acrescentada.

Código `261ec3f1`: onze workflows aprovados; quatro jobs do CI combinado
[39720850163](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39720850163),
incluindo 470 verificações nativas, sanitizadores e linkagem ARM64 sem APK.
Registro completo em [validation/csharp-surface-components-2026-10-22.json](validation/csharp-surface-components-2026-10-22.json).
