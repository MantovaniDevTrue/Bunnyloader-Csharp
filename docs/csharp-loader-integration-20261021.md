# Continuidade dos loaders C# — 2026-10-21

## Continuidade sobre 753bb31a — segurança nativa dos laços de água

Implementados dois contratos internos de WaterStyleLoader: publicação conjunta
dos dois limites de DrawWaters e recuperação nativa dos arrays de transparência
de LocalUserGameState. O SDK de água e a cachoeira completos ainda estão
pendentes; a contagem continua **3/15** e o gate de APK permanece fechado.

O patch agora verifica ambos os padrões, limites reais do método, páginas RX,
permissões e conflitos antes de escrever. Retém a contagem efetivamente aplicada
mesmo se o SO recusar restaurar RX; retry repara permissões e hook separadamente.
O guard nativo prepara os dois arrays juntos antes do original, sem entrar em
VM. O JS não marca uma recusa como instalação concluída e limita os retries.
A transação de arrays foi extraída do código já testado de superfície para uma
implementação compartilhada; o wrapper e seus contratos foram preservados.

Código, testes e continuidade: [csharp-water-native-safety.md](csharp-water-native-safety.md).
O código `0496da6b15d8f850b02d67182e0820723ad0815b` passou nos onze workflows;
os quatro jobs do [CI 39774698537](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39774698537)
aprovaram regressões, sintaxe ARM64 e link completo de libbunny.so sem APK.
São 191 verificações nativas novas (90 + 101), também com ASan/UBSan; 65
regressões existentes aprovadas. [Evidência](validation/csharp-water-native-safety-2026-10-22.json).
CI e Android continuam separados: não houve APK, bmod, nova versão de ExampleMod
ou alteração de ABI pública gerenciada nesta etapa. Validação Android pendente.

## Continuidade sobre 8173aaac — superfície com C#/JS reais e exemplo original

Conectados os dois providers ao registro nativo: C# → Mono worker → estágio 13 →
ManagedSurfaceBackgrounds → serviço nativo; JS → QuickJS → bridge própria → mesmo
serviço. Preservados os índices anteriores e o caminho C# sem VM JavaScript.
O SDK público oferece todos os callbacks originais, seleção por UID/classe/menu,
contexto de desenho limitado ao callback e lookup de textura por caminho completo.
O exemplo preserva seis imagens, as três camadas e a animação estática de quatro
frames a cada treze chamadas, com comentários de uso e razão dos mecanismos.

Testes novos executam C# via P/Invoke no worker separado e QuickJS real, depois
ambos no mesmo processo: intercalam quatro registros, validam prioridade/peso,
fades, desenho, exceções, troca de scope e as duas ordens de unload. Os fixtures
continuam usando metadata IL2CPP controlada. Não são teste de Android/FPS.

**3/15 implementados nos fontes; 12 pendentes; zero Android.** ExampleMod 0.1.66,
cache v57, 108 fontes, inventário esperado 77 conteúdos + 12 módulos = 89 tipos.
Guia/API/QA e continuidade em [csharp-surface-backgrounds.md](csharp-surface-backgrounds.md).
A revisão final encontrou uma lacuna de ciclo de vida: a pausa anterior descarregava
todos os módulos ao sair do mundo e impediria callbacks do fundo C# no menu. Foi
corrigida com scope de renderização persistente, PauseWorld no worker e pausa
nativa que conserva somente o estágio 13. Session.HostTests carrega o DLL real,
confirma a mesma instância/animação no menu, reentra três vezes com estáticos de
gameplay reiniciados e descarrega também o renderizador no unload verdadeiro.
A troca de dono dos hooks compartilhados de superfície/subsolo agora transfere
o handle entre scopes, sem reinstalação proibida no contexto de unload. Um teste
recusa toda nova instalação nesse momento e confirma a continuidade do estilo.
O código final `32d6ffa2f3a513bec5aadae0a86b7f0ea33c70df` passou nos **onze
workflows**. [CI combinado 39769307079](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39769307079)
aprovou os quatro jobs, incluindo os testes novos, 65 regressões, sintaxe ARM64
e link completo sem APK. [Evidência](validation/csharp-surface-managed-bridges-2026-10-22.json).
As exceções originais do C# também chegam ao relatório nativo, com sessão revisada,
sem depender de Console.Error; falhas tardias/duplicadas não alteram o novo pedido.
As seis imagens foram conferidas byte a byte contra o arquivo original.
A próxima dupla foi auditada integralmente em [água/cachoeira](csharp-water-rendering-audit.md),
com contratos internos antes ausentes acrescentados à matriz, todos ainda pendentes.
Não confundir essa auditoria com implementação ou limitação comprovada do Android.
O APK e o `.bmod` continuam bloqueados. Abaixo, registros históricos preservados.

## Continuidade sobre eb6c44b4 — serviço nativo dos seis hooks de superfície

`SurfaceBackgrounds` agora registra UID/classe, consulta o Count real, valida
nullable não-HFA antes de detours e liga os seis métodos originais aos serviços
de tabelas/desenho. Há prioridade/peso, menu independente, fades após original,
comandos personalizados, snapshot enraizado e retenção da textura anterior
durante unload. `scAdj` preserva frações; um callback de mundo vencido não publica
fades nem continua desenhando. Binding inválido não instala hooks; falha parcial
de instalação fica em wrappers somente-original até retry.

Código, testes e pendências: [csharp-surface-native-runtime.md](csharp-surface-native-runtime.md).
O fixture executa o núcleo/detours reais com providers C++ controlados, incluindo
outra thread alterando o registro durante callback. São 336 verificações desse
executável, mais 470 dos componentes e o oráculo JS, aprovadas localmente com
ASan/UBSan/divisão por zero/leaks. As 65 regressões, fundos e subsolo anteriores
passaram. O código final `bc5ed296da12621132687df0be24b4c9a880063c`
passou nos **onze workflows**; [CI combinado 39726493557](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39726493557)
aprovou os quatro jobs, inclusive sintaxe e linkagem ARM64 sem APK.
[Evidência](validation/csharp-surface-native-hooks-2026-10-22.json).
O ajuste final preserva também a velocidade Double no cálculo de fades e evita
apagar ativações de OnEnterWorld num segundo reset tardio. O commit que registra
esta evidência modifica somente documentação.

**Continua 2/15 implementados, 1 parcial e 12 pendentes; zero Android.**
As bridges MonoVM/QuickJS de superfície, SDK e exemplo C# não foram criados nesta
etapa. O loader JS anterior foi preservado. Próxima tarefa: conectar ambas as
linguagens a este registro antes de expor C#, com revisão/deadline, rebinding de
scopes e port integral da animação/imagens. ExampleMod0.1.65, cache v55 e 106 fontes
permanecem iguais. Nenhum APK ou `.bmod` gerado.

## Continuidade de 22/10 — componentes de SurfaceBackgroundLoader

Sobre `f2d98fcd`, preservados os commits anteriores e todos os exemplos existentes.
Implementados três serviços C++ de geometria/fades, ampliação transacional das
quatro tabelas e desenho SpriteBatch/SkyManager com metadata verificada. Novos
contratos internos de escrita e Nullable validam tipos, offsets, tamanhos,
alinhamento e referências antes de montar argumentos ou escrever memória.
Detalhes, testes e continuação em
[csharp-surface-native-components.md](csharp-surface-native-components.md).

O fixture invoca os `.cpp` de produção, incluindo o overload real de desenho
pela API IL2CPP controlada. O original JS imutável executa em Node/V8 para
comparar projeções, prioridades e fades. Não é prova de loader integrado ao
Terraria: ainda faltam hooks, registro compartilhado, SDK/worker e exemplo C#.
Não foi criada uma classe pública parcialmente conectada ao runtime.

**Escopo: 2/15 implementados nos fontes, 1 parcial, 12 pendentes; 0 Android.**
A matriz global mantém 206 referências e o exemplo de superfície como não
portado. ExampleMod permanece 0.1.65/106 fontes; SDK público e cache não mudaram.
Nenhum APK ou `.bmod` gerado. O código final
`261ec3f15304356e01fc14419bb8e79938ea518e` passou nos **onze workflows**.
[CI combinado 39720850163](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39720850163):
quatro jobs aprovados, com 470 verificações nativas novas, comparação com o JS
original, sanitizadores, 65 regressões e linkagem completa ARM64 sem APK.
[Evidência deste checkpoint](validation/csharp-surface-components-2026-10-22.json).
O commit que registra essa evidência modifica somente documentação.

## Continuidade de 22/10 — UndergroundBackgroundLoader

Implementação nova sobre `7ff39991`, sem reiniciar a branch nem substituir
os sistemas estáveis. API e contratos: [fundos subterrâneos](csharp-underground-backgrounds.md).
O ExampleMod passa a **0.1.65 nos fontes**, 106 arquivos C#, 76 entradas de
conteúdo e 11 módulos de hooks (87 tipos verificados). O SDK público tem
158 tipos. Cache atualizado para `roslyn-net8-source-v55-native-underground-backgrounds`.

O núcleo compartilha seleção, cinco camadas, getters atual/anterior, prioridade
e leases de assets entre as duas linguagens. C# usa Mono e mensagem nativa por
revisão; não inicializa QuickJS. O exemplo original JS também executa contra o
mesmo serviço. `BunnyHookModule.ModId` fornece a identidade real para módulos
de qualquer mod, sem classe/UID fixos. O dispatcher migra para outro scope
antes de descarregar o proprietário anterior.

Evidência local desta alteração:

- 65 regressões combinadas aprovadas; bootstrap com 106 fontes/53 registros,
  inventário exato de 76 conteúdos e teste de HUD real preservados.
- Núcleo subterrâneo, C# real por P/Invoke e exemplo original JS no QuickJS
  real aprovados. Teste separado usa threads de desenho e worker com o
  `UniversalHookMailbox` de produção; resposta vencida, duplicada ou concorrente
  não troca o snapshot vigente.
- Transação de cinco raízes GC, rollback e unload com transição testados;
  ASan/UBSan com leak detector habilitado aprovados no fixture do núcleo.
- Registro de imagens de fundo, P/Invoke e compatibilidade JS aprovados após
  a introdução dos leases. Liberação fora da thread Unity só agenda destruição.
- ExampleMod completo aprovado nos quatro modos de compilação/verificação
  gerenciada; referência pública regenerada e conferida; seis testes do gate
  e da integridade das evidências aprovados.
- Contratos nativos SystemUpdate/taxas/HookManager e sintaxe host dos caminhos
  ARM64 aprovados. A linkagem NDK completa exige CI no novo SHA.

A auditoria encontrou cinco chaves JS duplicadas no JSON histórico de
evidências. As cópias antigas ocultavam os registros anteriores com os nomes
Example e o relato geral do usuário sobre a .96. Foram mantidos os registros
mais completos e o auditor agora rejeita duplicatas, com teste de regressão.
Nenhum exemplo foi promovido a validado Android por esse relato geral.
A matriz global foi regenerada preservando os **206 JS**, agora com o estilo
subterrâneo mapeado e validação de host separada da execução no celular.

**Estado do escopo: 2/15 implementados nos fontes (13,33%); 0/15 validados
Android; treze pendentes.** SystemUpdate/Gore são integrações adicionais.
Nenhum APK ou `.bmod` foi gerado. Não existe promessa de FPS da nova bridge.

Próxima implementação: SurfaceBackgroundLoader sobre o serviço de assets e
contratos de propriedade agora compartilhados; depois SceneEffect/Biome e
os estilos água/cachoeiras/mapa. Não substituir o port de bioma/música pelo
comando `/exampleunderground`: ele serve exclusivamente para QA do estilo.

As seções abaixo preservam o histórico; suas contagens/versões se referem
aos respectivos checkpoints.

### CI do checkpoint e complemento de isolamento

O commit `23101aad6b8fd3b3cc2c8628cdfffaef3dedc9fc` passou nos onze workflows.
[CI combinado 39713260819](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39713260819)
aprovou quatro jobs, incluindo sintaxe ARM64 e linkagem completa sem APK.

O complemento seguinte limita erros por estilo/operação, evitando que um mod
suprima o primeiro diagnóstico de outro. A recuperação de textura aposentada
só tenta Fill depois de ganhar a arbitragem vanilla. Os diagnósticos de resposta
ausente/cancelada/sessão invalidada agora descrevem o fato observado. O teste
de timeout usa o mailbox de produção; uma exceção C# real preserva mensagem,
classe, UID e stack sem afetar o estilo independente.

`Mixed.HostTests` também executa o exemplo C# real e o original JS no mesmo
processo com QuickJS real, prioridade/peso, transição e ambas as ordens de
unload/reload. Mantém o teste separado sem QuickJS para provar independência.
Essas alterações precisam de CI próprio, sem herdar aprovação do SHA anterior.

O próximo loader tem [auditoria detalhada](csharp-surface-background-audit.md)
e contratos adicionais na matriz. Não basta Add/Get/Wake: nullable, quatro
arrays, menu, fades, três fases de desenho e animação original continuam
pendentes. Nenhuma API de superfície incompleta foi exposta.

CI do complemento `8c1512e2`: os onze workflows passaram; o combinado
[39714561760](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39714561760)
inclui a execução mista real C#/QuickJS e a linkagem ARM64 sem APK.
O reforço posterior também valida os tamanhos Int32/Double do picker, enumeração
de métodos e resolução de campos herdados antes de instalar hooks; a ausência
de exports é identificada pelo nome. O fixture recusa Double de quatro bytes
sem instalar hooks nem escrever. Esses guardas não ampliam a contagem de loaders.

O commit `f3c92d59` passou no CI combinado
[39715577521](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39715577521),
incluindo sintaxe ARM64 e linkagem completa. A revisão JS seguinte preserva
`Number.isInteger` do original: null/string/Symbol não viram textura 0, arrays
encurtados omitem camadas e entradas extras são ignoradas pelo desenho.
Getters que lançam exceção são testados sem perder a mensagem original.

### Evidência final do código desta execução

O código `46e6d545f8b267e2c7e2bb94ab813f780e277a3f` passou nos **onze workflows**.
[CI combinado 39716948102](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39716948102):
quatro jobs aprovados, incluindo os testes mistos C#/JS, sintaxe ARM64 e
linkagem completa com os sete exports da nova bridge. Registro reproduzível:
`docs/validation/csharp-backgrounds-2026-10-22.json`. O commit posterior só
conserva esta evidência documental; não altera o código validado.

Continuar pelo contrato completo da superfície em `csharp-surface-background-audit.md`.
Manter 2/15 implementados nos fontes, treze pendentes e zero validados Android.
Não gerar APK ou `.bmod` e não converter a aprovação do CI em gameplay validado.

Branch: `feat/csharp-loader-completion-15-20261021`.
Base: `5a18a67899c1f6ee88a34097be3adb6e7877edd4`.
O trabalho preserva as correções da integração anterior, inclusive contagem
de classes, referências Android, tooltip, Dust, ItemUse e receitas.

## Regra de entrega

**Não gerar APK.** A lista de quinze contratos originais está em
`csharp-loader-completion-15.json`. O gate bloqueia tarefas de empacotamento e
publicação enquanto houver pendências. A integração do SystemUpdate abaixo
conclui trabalho existente da PR #126; não transforma seus dezenove callbacks
em dezenove loaders e não altera artificialmente a contagem da lista.

## Integração de SystemUpdateLoader

Foi incorporada a PR #126, commit `3451b149`, mantendo seu histórico.
O SDK oferece os dezenove callbacks do `SystemUpdateHooks.js` original.
Os estágios nativos usam âncoras verificadas por quatro opcodes, preservam
registradores gerais, SIMD e flags e mantêm a rotina original do jogo.

Correções adicionais da integração:

- Um assembly contendo apenas `BunnyWorldUpdateLoader`, inclusive com uma
  classe-base intermediária própria, agora é reconhecido sem item auxiliar.
- O verificador gerenciado publica inventário `HOOK_MODULES`; o status do
  worker transporta a quantidade e o host exige correspondência exata entre
  conteúdo nativo e módulos gerenciados. Não foi removida a validação de IDs.
- A descoberta antecipada não interpreta métodos C#: literais de caractere,
  strings verbatim/interpoladas/raw e pontuação são preservados para Roslyn.
  Recursos que precisam de reserva antecipada ainda exigem caminhos literais
  ou `BunnyContent`; isso permanece uma limitação explícita do bootstrap.
- O cache passa a `roslyn-net8-source-v54-managed-module-inventory`.
- O autoload de imagens de fundo também ocorre em mods sem itens.
- `gameMenu` exige Boolean estático de um byte. Campos de taxas exigem
  Single/4, Double/8 ou Int32/4 estáticos e mutáveis. Métodos confirmam o
  armazenamento estático/instância antes de instalar os respectivos hooks.
- A publicação das três taxas é transacional por sessão. Timeout, descarte
  ou troca de quadro impedem uma resposta antiga de sobrescrever a nova.
- `ExampleWorldUpdateCounter` fica ativo e demonstra dois callbacks reais;
  os exemplos que alteram taxas/atualizações globais continuam com opt-in.

## Evidência local

| Verificação executada | Resultado |
| --- | --- |
| `tools/ci/test_ready_prs.py` | 65 verificações aprovadas |
| `tests/world_update_loader/WorldUpdateLoader.csproj` | 115 verificações, incluindo Roslyn real, descoberta de módulo independente, execução e unload |
| `native_contract_test.cpp` | Rejeição de tipo, tamanho, storage, ABI e metadata incompatíveis sem ler/escrever valores |
| `time_rate_mailbox_test.cpp` | Sessões antigas, valores inválidos e 20.000 ciclos com worker concorrente |
| `FullExampleRuntime` | 84 tipos verificados nos modos CLR completo, referências compactas Android, fallback e CLR Android empacotado |
| `tests/ready_prs/run.sh` | Bootstrap de 103 fontes, 52 registros nativos, inventário compatível, Dust e HUD de invasão |
| `tests/backgrounds/run.sh` | Serviço nativo de produção, P/Invoke C# real e loader JS |
| `tests/loader_delivery` | Quatro testes; escopo incompleto permanece bloqueado |
| C++ `SourceLoader`, `AutoSourceRuntime`, `ManagedRuntime`, `MonoWorkerJni`, estágios | Sintaxe host aprovada; ARM64/linkagem serão verificados no workflow sem APK |

Esses resultados não comprovam hooks, renderização, estabilidade ou FPS no
Android. A montagem das instruções ARM64 e a linkagem NDK têm passos próprios
no workflow source-only; registrar o SHA e o resultado do CI quando terminar.

## Ponto de retomada do primeiro checkpoint

1. Confirmar o CI source-only desta branch; corrigir a integração antes de
   anunciar qualquer grupo como pronto.
2. Revisar a PR #121 (Gore) e adaptar/testar seu serviço nativo contra a base
   atual, sem substituir DLLs do SDK por binários antigos.
3. Continuar estilos de fundos sobre o registro compartilhado já implementado.
4. Preservar os 206 arquivos JS do inventário do ExampleMod 1.9.0 enviado.
5. Atualizar contratos individualmente com evidências. Os outros quatorze
   loaders da lista continuam pendentes; não existe entrega dos quinze.

Nenhum APK foi gerado e nenhuma validação no aparelho foi atribuída a este
trabalho. As versões publicadas permanecem inalteradas.

## Integração adicional de GoreLoader

A PR #121 foi integrada sobre a base atual, com o código explicado em
`docs/csharp/GORE_LOADER_IMPLEMENTATION.md`. Um NPC próprio, **Example Gore Slime**,
exercita a API; não foi necessário alterar os exemplos de invasões/armas.
O serviço de produção recebeu testes de transação, contratos IL2CPP, GC,
layout de Vector2, configuração de floats e P/Invoke com dois UIDs reais.
O inventário do ExampleMod passa a 85 tipos gerenciados (75 entradas do
contrato de conteúdo + 10 módulos de hooks) e 104 arquivos C# (53 registros de bootstrap verificados).

A revisão com `script/bridge/Bindings.cpp` também corrigiu o contrato de
`field_static_set_value` do novo serviço de fundos: referências passam o
objeto diretamente; valores numéricos passam endereço. O fixture anterior
reproduzia a chamada incorreta e foi corrigido. A aprovação anterior do CI
não era prova desse contrato no aparelho. Nenhum APK com esta mudança foi gerado.

Os testes de host agora têm diretório `obj` por projeto. Vários executáveis
na mesma pasta podiam reaproveitar o apphost de outro teste. Os arquivos de
persistência das invasões também são isolados por processo, conservando o
save/reload real dentro de cada execução.

CI anterior à integração Gore: [39673760844](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39673760844),
commit `34247fd5`, quatro jobs aprovados, incluindo ARM64/linkagem sem APK.
As mudanças posteriores exigem CI próprio; não herdam essa aprovação.

Regressão combinada após a integração Gore: 65/65 verificações locais aprovadas.
O teste JS original também confirma que o registro de Gore preserva os assets e
valores C# anteriores. O fixture nativo confirma a direção inversa: reconstruir
as tabelas C# preserva uma entrada externa adicionada depois.
O ExampleMod está em **0.1.64 nos fontes**, sem pacote de entrega ou APK.

## Correção do CI após `5566d944`

O teste de referência pública detectou documentação gerada desatualizada;
`tools/sdk_reference` regenerou os 155 tipos públicos e o modo `--check` passou.
A compilação NDK detectou um `try/catch` novo incompatível com o contrato
`-fno-exceptions` do núcleo. O catch foi removido; a preparação do índice
continua ocorrendo antes da publicação. Não foi habilitado desenrolamento C++
em nenhum arquivo. As falhas de API/textura/raiz GC continuam retornando erro;
OOM fatal do alocador C++ segue a política existente do processo, sem promessa
de recuperação. Os testes Gore e o preflight agora usam as flags de produção.
O CI completo deve passar novamente no SHA desta correção.

## Revisão do próximo lote existente

PR #120, head `6bbc27f5`, foi revisada e **não integrada** neste checkpoint.
Antes de considerá-la completa, substituir a descoberta por classe fixa
`BunnyLoader.Source.BunnyTouchEntry` por módulo genérico com UID fornecido pelo
host; `GetAssignedKeys` não pode retornar apenas o rótulo do botão como se
fosse o binding real. O exemplo atual registra contadores, mas ainda precisa
executar a ação original e incluir os ícones. Aproveitar a descoberta genérica
já implementada e testar dois módulos/UIDs, sem consumir eventos do segundo
mod ao preparar o snapshot do primeiro. Usar um snapshot por estágio de input,
e liberar registros, texturas e estados no descarregamento.

PR #122, head `bcaa1322`, ainda usa `Split/Bunny-Loader`: adaptar os novos arquivos
e as integrações de WorldSave à raiz atual; não mesclar as remoções de layout.
Não há integração parcial de Keybind/WorldSave nesta branch.

## Evidência de CI da correção de ABI

[CI #39680627977](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39680627977)
foi aprovada no commit `c5946f82`: testes gerenciados/contratos, host nativo,
sintaxe ARM64 e linkagem completa de `libbunny.so`, sem APK.
Os workflows de localização, raridades, Dust/ARM64, invasões, comandos,
SystemUpdate e Gore também passaram nessa revisão.

O complemento posterior alinha os buffers Vector2 e limita explicitamente a
terminação do texto de `il2cpp_format_exception`, cujo contrato permite saída
sem NUL. O fixture reproduz esse caso. A suíte Gore passou com `-fno-exceptions`,
`-fno-rtti`, ASan e UBSan (o leak detector fica desligado neste fixture de heap
IL2CPP; a quantidade de raízes residentes é verificada por asserts). O SDK
público gerado continua em 155 tipos; os quatro testes do bloqueio de APK passam.
Os quinze alvos continuam com 14 pendências, além das integrações adicionais.
Não há teste Android nem APK deste trabalho.

## Compatibilidade SystemUpdate C# e JavaScript — complemento

O CI do SHA `d2d901bf` passou em todos os onze workflows, incluindo
[39682216237](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39682216237)
com sintaxe ARM64 e linkagem da biblioteca nativa sem APK.

A revisão seguinte encontrou dois problemas que esses testes ainda não
cobriam: validar opcodes depois que a outra linguagem já instalou seu hook,
e arredondar as taxas duas vezes em dois loaders independentes. Foram criados:

- `HookManager.installVerified`, com prova dos bytes anteriores ao primeiro
  patch, instalação/validação sob a mesma trava e diagnóstico esperado/real.
- `content/systems/SystemTimeRates`, serviço sem Mono/QuickJS que compõe as
  três taxas em uma transação aninhada. Apenas o limite externo grava os
  campos nativos. As frações não se perdem entre linguagens.
- Resolução de metadata em thread de carregamento, sem ler/gravar valores;
  execução e troca de menu/mundo limitadas à thread do jogo. Campos Single,
  Double e Int32 recebem exatamente o número de bytes validado. Int32.MaxValue
  não pode arredondar para cima ao ser representado como Single.
- Reset independente no ciclo nativo menu/mundo; remover apenas o módulo C#
  não interfere no estado JS. O mailbox mantém sua proteção contra respostas
  MonoVM atrasadas e agora preserva o evento independente recebido do núcleo.

Os quatro testes em `tests/world_update_loader/run_native.sh` executam código
nativo de produção e cobrem os dois sentidos de integração, 16 instalações
concorrentes, falhas sem escrita, exceção/cleanup no wrapper JS e os contratos
de tipos/thread. O Node passou 34 testes de comportamento; seu modo explícito
behavior-only **não** afirma validação de assinaturas do dump ausente.
A suíte gerenciada mantém 115 verificações. As 65 verificações combinadas e
o check dos 155 tipos públicos também passaram novamente. A aprovação do SHA anterior não
é atribuída automaticamente a estas mudanças; o CI precisa passar novamente.

Próximos loaders: estilos de fundos continuam pendentes na lista de quinze;
Keybind e WorldSave precisam das adaptações descritas acima. Não há APK,
release, garantia de FPS ou validação Android neste checkpoint.


## Prova de interoperabilidade com QuickJS real

[CI 39684504809](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39684504809)
passou nos quatro jobs do SHA `1ce4c19b`, incluindo ARM64 e linkagem completa.
O complemento seguinte separa a bridge em `script/api/SystemTimeRates.cpp`;
o serviço continua em `content/systems/`, sem referências a QuickJS ou Mono.

`run_quickjs.sh` compila o QuickJS do próprio repositório, executa o
`SystemUpdateHooks.js` real e chama as bindings C++ de produção. O teste
confirma composição nas duas ordens, frações, relógio, fallback de NaN/infinito
e fechamento da transação após exceção no original. O lado C# é representado
pelo protocolo nativo equivalente, pois o trampoline ARM64/MonoVM exige o
aparelho; a suíte C# continua testando o dispatcher gerenciado separadamente.
Esse resultado não é apresentado como execução Android.

O build rigoroso também detectou incremento/leitura sem sequenciamento no
contador de logs. O incremento foi separado antes do cálculo, tanto na bridge
JS quanto no caminho C#. Os testes verificam o conteúdo real do diagnóstico
de opcodes rejeitados e passam com ASan/UBSan. A bridge JS valida ainda o
storage estático/instância do método antes de instalar as âncoras.

Continuam vigentes o bloqueio de APK, as 14 pendências do escopo de quinze e
as adaptações de Keybind/WorldSave descritas acima. Nenhuma aprovação anterior
foi reutilizada como aprovação do novo SHA.


## CI do complemento com QuickJS real

O SHA `7d5c87479ee13f1f75c90b27aa37f54fac82f11d` passou nos onze workflows.
[CI combinado 39685243816](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39685243816):
quatro jobs aprovados, incluindo o teste QuickJS real, as regressões existentes,
compilação ARM64 dos arquivos modificados e linkagem de `libbunny.so` completa.
O workflow confirma ausência de APK. O SHA registrado é o dos fontes testados;
este registro documental posterior não representa outro teste no dispositivo.

O plano oficial `csharp-completion-plan.md` agora apresenta a regra de quinze
no topo. A regra de cinco permanece somente no histórico; não autoriza gerar
APK parcial. A lista atual contabiliza **1/15 contratos implementados (6,67%)**
e **0/15 validados no Android**; as duas integrações adicionais não inflam
esse percentual. O inventário global de exemplos continua sendo uma meta
separada. Os 14 contratos restantes não foram reduzidos a stubs ou removidos.


## Guarda adicional de ABI da UI

O caminho nativo `UpdateUIStates(GameTime)` passa o argumento como ponteiro
opaco ao callback C#. A resolução por nome curto de tipo não basta para
confirmar essa ABI. O binding verifica agora a representação real do parâmetro:
classe por referência aceita; value type, parâmetro ausente ou export de
metadata indisponível são recusados antes de instalar o hook. O teste nativo
exercita a aceitação e essas três recusas, com diagnóstico do tipo encontrado.
Nenhum campo do GameTime é lido por offset e a assinatura pública existente
`UpdateUI(nint)` foi preservada. Os novos testes de host e a sintaxe com flags
de produção passaram; esta guarda exige o CI do próprio commit e QA Android.
