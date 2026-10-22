# Fundos subterrâneos C# — implementação e validação

Fontes do ExampleMod **0.1.65**, cache
`roslyn-net8-source-v55-native-underground-backgrounds`. Nenhum APK ou `.bmod`
é gerado enquanto os quinze loaders do escopo estiverem incompletos.

## API pública

`BunnyUndergroundBackgroundStyle` é um módulo gerenciado com `ModId` e `Slot`
atribuídos pelo host. A herança e os métodos auxiliares são C# normal, executado
no MonoVM. `SetStaticDefaults()` prepara o estado; `FillTextureArray(Span<int>)`
preenche cinco slots: borda céu/terra, terra, borda terra/pedra, pedra e passagem
para o inferno. O valor inicial da quinta camada é 6, como no original. `-1`
preserva a camada do jogo. Nenhuma textura é criada dentro do callback.

`BunnyUndergroundBackgrounds` oferece `VanillaCount`, `Get`, `GetSlot<T>`,
`GetSlot(modUid,className)`, `Activate<T>`, `Deactivate<T>` e `Set`. O registro
usa UID e nome completo da classe; nunca salve o slot numérico. `Get` devolve
instâncias C#; estilos JS podem ser referenciados por UID/nome/slot no serviço
compartilhado. Dois mods podem usar a mesma classe sem colisão.

`BunnyBackgroundPriority` aceita None, BiomeLow, BiomeMedium, BiomeHigh,
Environment, Event e os três níveis de Boss. BiomeLow substitui a caverna
comum daquele X; BiomeMedium também neve/selva; BiomeHigh e superiores podem
substituir todos os estilos vanilla. Peso 0..1 compõe a classificação, com
desempate determinístico por UID/classe. Ativação termina ao mudar de mundo.

O serviço não implementa o **BiomeLoader** nem o **SceneEffectLoader** C#;
esses contratos permanecem pendentes. Um controlador C# pode usar `Set` para
publicar suas condições. O comando do exemplo só ativa o estilo para QA e
não se apresenta como port do bioma, contagem de blocos ou música original.

## Caminho nativo e contratos

- `content/backgrounds/UndergroundBackgrounds` registra estilos, escolhe a
  prioridade e conecta o picker e os getters de texturas atual/anterior.
  Preserva a execução original; somente as primeiras cinco entradas mudam,
  mantendo as entradas 5/6 do Terraria e reparando buffers recriados.
- `il2cpp/CheckedProperty` resolve campos e getters reais. Valida tipo,
  tamanho, armazenamento, hierarquia, referências, ranks e offsets antes de
  ler/escrever. O picker usa o **MethodInfo da assinatura validada**, mesmo
  se outro overload tiver o mesmo nome e quantidade de argumentos.
- `ManagedUndergroundBackgrounds` e o estágio `UndergroundBackground` trocam
  cinco inteiros por revisão. O código C# roda no worker Mono, inclusive se
  o pedido partir de uma thread de desenho. A espera de produção é limitada
  a 3 ms; timeout/falha conserva o último preenchimento válido, ou as texturas
  originais se ainda não houver preenchimento. Isso é fallback diagnosticado,
  não prova de que o callback executou. Respostas antigas/duplicadas são
  recusadas e uma chamada simultânea não cancela a solicitação em andamento.
- O preenchimento ocorre uma vez por estilo **solicitado pelos getters** em
  cada desenho, incluindo o estilo anterior durante a transição. Não é feito
  para candidatos que perderam a seleção. Travas do registro são liberadas
  antes de chamar qualquer linguagem. Mudança de mundo durante o callback
  impede aplicar a resposta naquele desenho.
- `BackgroundAssetLifetime` mantém leases dos assets usados pelas camadas.
  Descarregar um mod limpa suas tabelas, mas a imagem anterior só é destruída
  quando a transição deixa de referenciá-la. Liberação numa thread de desenho
  fica pendente para destruição na thread Unity. Slots não são reutilizados.
- `script/api/UndergroundBackgrounds` adapta o loader JS ao mesmo serviço;
  C# não inicializa QuickJS. Os fontes JS originais estão preservados em
  `tests/underground_backgrounds/reference/` para comparar comportamento.

Offsets/métodos e texturas resolvidas ficam em cache. O exemplo resolve os
quatro caminhos uma vez em `SetStaticDefaults`, evitando alocações de strings
por desenho. Raízes GC/leases são renovadas apenas quando a imagem muda.
Erros incluem estágio, motivo, UID/classe/slot quando conhecidos; repetições
são limitadas. Logs não inventam validação no dispositivo.

## Evidência automatizada

`bash tests/underground_backgrounds/run.sh` executa o núcleo e a bridge de
produção com metadata IL2CPP controlada, um teste com threads de desenho e
worker separados usando `UniversalHookMailbox` real, e C# por P/Invoke usando
a classe real do ExampleMod. Cobre prioridades, fronteiras de cavernas, cinco
camadas, buffers atual/anterior, múltiplos UIDs, unload do primeiro proprietário,
realocação/rollback de raízes GC, mundo trocado durante callback, dependência
ausente, pedidos simultâneos e publicação atrasada/duplicada.

`bash tests/underground_backgrounds/run_quickjs.sh` executa o motor QuickJS
real, o loader adaptado e o exemplo JS original. Cobre IDs JS/C# intercalados,
seleção, transição, isolamento de exceções e unload JS preservando C#.
Também executa `MixedProgram.cs`: o exemplo C# real e o exemplo JS original
no mesmo processo, usando a bridge de produção, incluindo prioridade/peso,
transição entre linguagens e as duas ordens de unload/reload. Esse host usa
CLR .NET e metadata controlada; não é execução MonoVM no Android.
`tests/backgrounds/run.sh` também testa leases entre threads e destruição.
O fixture do núcleo passa em ASan/UBSan com detecção de leaks habilitada.

Esses testes não executam o IL2CPP do celular. O CI faz sintaxe e linkagem
ARM64 da biblioteca, sem APK. Renderização, tempo dos callbacks, estabilidade
e FPS no Xiaomi permanecem **aguardando validação Android**.

## Roteiro Android do ExampleMod (para quando o gate liberar APK)

1. Habilitar o ExampleMod C# e a opção de fundos do Terraria. Confirmar carga
   completa e MonoVM. Com apenas C#, QuickJS deve continuar não inicializado.
2. Usar `/examplebackground Assets/Textures/Backgrounds/ExampleBiomeUnderground0`
   para consultar a textura do próprio ExampleMod. Executar
   `/exampleunderground on` e descer da superfície até terra/pedra: verificar
   as quatro imagens originais e a passagem para o inferno.
3. Caminhar horizontalmente e alternar `/exampleunderground off`/`on` durante
   a transição. Conferir fundo atual e anterior, sem camada preta ou textura
   trocada. Desligar/ligar os fundos na configuração e repetir.
4. Salvar/sair e voltar: a ativação de QA deve estar desligada, sem persistir
   IDs temporários. Reativar e repetir; desabilitar o mod e conferir o mundo.
5. Quando o pacote de QA entre mods estiver disponível, usar dois UIDs com a
   mesma classe, variar prioridade/peso, inverter a ordem de carga e carregar
   o ExampleMod JS junto. Remover uma dependência de textura: exigir erro
   identificado e fallback, sem interromper estilos independentes.
6. Medir quadros/tempo de callbacks durante deslocamento e transições. Não
   atribuir 60 FPS ao novo loader com base apenas em compilação ou CI.
