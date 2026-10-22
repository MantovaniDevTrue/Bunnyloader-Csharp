# Água: publicação nativa e tabelas de transparência

Continuação de `753bb31a`; WaterStyleLoader e WaterfallStyleLoader **continuam
pendentes como loaders completos**. Dois contratos internos da água agora têm
implementação nativa e testes executáveis. Não há novo SDK de água exposto,
novo exemplo C# de água, APK ou `.bmod` nesta alteração.

## Serviço independente de VM

`content/tiles/ModWater.cpp` continua atendendo a bridge JS existente, mas agora
usa `hook/CodePatchTransaction` para preparar os dois limites de DrawWaters juntos.
A resolução verifica o overload completo `void DrawWaters(bool isBackground)`,
storage de instância e tamanho real do Boolean. A chamada exige a thread Unity
observada; aceita contagens de 15 a 4087, porque o maior imediato é total + 8.

O serviço exige exatamente um padrão de cada laço, dentro de uma região RX
mapeada e limitada pelo próximo método da classe. Sem esse limite comprovado,
recusa a operação; não varre um intervalo arbitrário de 512 KiB. Antes da primeira
escrita, todas as páginas envolvidas precisam permitir escrita e as instruções
precisam continuar iguais às verificadas. Falha de padrão, conflito ou recusa de
permissão não escreve apenas um laço. A exclusão mútua serializa escritores;
não suspende outras threads. A operação deve ocorrer na thread do motor, fora
da execução do método alvo, antes de chamar o original.

Se o SO recusar a restauração de proteção após a escrita, **ambos** os limites
já foram publicados. O resultado distingue isso de uma recusa anterior à
publicação; preserva a contagem real, relata a consequência e retém as páginas
que precisam de reparo. A próxima tentativa primeiro restaura as permissões.
Também é possível repetir a instalação do guard sem reaplicar os limites se o
hook tiver sido recusado depois da publicação. Nenhuma dessas falhas vira sucesso.

As três tabelas de assets precisam possuir o tipo real Asset<Texture2D>[],
capacidade suficiente e assets compatíveis em todos os slots de mod. O serviço
não registra texturas nem inventa IDs. O registro compartilhado C#/JS, leases,
fallbacks e a publicação conjunta dessas três tabelas continuam pendentes.

## Troca de mundo sem depender do callback JS

`WaterAlphaTables` resolve campos estáticos ou getters Main com backing fields
tipados em LocalUserGameState. Um getter sozinho não é tratado como campo
gravável. Verifica storage, tipo Single[], tamanho dos elementos, offsets,
classe do objeto e limites dos arrays. As referências e campos ficam em cache.

`il2cpp/PrimitiveArrayTransaction.h` prepara e enraíza os dois arrays antes de
publicar qualquer um. Preserva os valores existentes e recusa snapshots cujo
owner ou array mudou durante a alocação. Essa mesma transação passa a atender
os quatro arrays de superfície através do wrapper anterior, sem mudar seu
comportamento. O caminho em que os arrays já têm capacidade não aloca nem cria
handles de GC; isso é verificado pelos testes, não uma medição de FPS no Android.

Depois de publicar a contagem, um guard nativo verifica as transparências antes
de cada DrawWaters. Se LocalUserGameState nasceu novamente, cresce os arrays
para a contagem já publicada. Se isso falhar, não chama o original com arrays
pequenos. Diagnósticos repetidos idênticos são emitidos na primeira ocorrência
e nas potências de dois, com contagem de mensagens suprimidas. O original é
chamado uma vez quando a preparação é válida; o guard relê o ponteiro encadeado
com acquire, acompanhando a publicação atômica feita pelo HookManager. Um
original ausente é relatado e não provoca reinstalações duplicadas nem sucesso.

O loader JS também só considera a instalação concluída quando a bridge retorna
true. Uma recusa ou exceção adia a próxima tentativa por 60 chamadas de desenho,
evitando patch/log a cada quadro, e não chama o desenho inseguro nesse intervalo.

## Validação e continuidade

`BUNNY_WATER_SANITIZE=1 bash tests/water_styles/run.sh` executa:

- 90 verificações do código de transação real: páginas mmap RX, dois padrões,
  três condições de continuação, duplicação, falta de limite, memória inacessível,
  falhas de mprotect, conflito e reparo de permissões.
- 101 verificações do serviço ModWater real com metadata/heap IL2CPP controlados: overloads errados,
  Boolean com largura errada, símbolos IL2CPP ausentes identificados pelo nome,
  cinco tabelas inválidas, assets ausentes com slot identificado, thread
  errada, falha isolada de hook, publicação monotônica, transação dos dois arrays,
  falhas de alocação/roots, owner/array trocado, caminho sem alocações e guard.
- O arquivo JS de produção em execução Node: ordem das cinco tabelas, recusa,
  exceção, espera, retry e recriação das transparências. Esse teste utiliza uma
  bridge controlada; não é execução de Terraria nem teste QuickJS/Mono misto.
- Os dois binários nativos novamente com AddressSanitizer e UndefinedBehaviorSanitizer.

O fixture não inicializa nem vincula QuickJS. Isso demonstra a independência dos
serviços nativos testados, **não** a conclusão do loader C# de água, que ainda
precisa da API pública, bridges de callbacks e exemplos originais completos.
O código `0496da6b15d8f850b02d67182e0820723ad0815b` foi aprovado nos onze
workflows, incluindo os quatro jobs do [CI 39774698537](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39774698537):
compilação ARM64, link completo de libbunny.so sem APK e regressões.
[Evidência com commits, runs e resultados](validation/csharp-water-native-safety-2026-10-22.json).

Continuar pelo registro compartilhado e publicação das três tabelas de assets;
depois ligar os callbacks completos da água/cachoeira listados em
[csharp-water-rendering-audit.md](csharp-water-rendering-audit.md), incluindo o
alias de Update do gore, três overloads de desenho, chuva e tintura. Preservar
os estados pendentes. A validação Android e o orçamento real de 60 FPS permanecem
sem medição nesta integração. A entrega de APK segue bloqueada em 3/15 loaders.
