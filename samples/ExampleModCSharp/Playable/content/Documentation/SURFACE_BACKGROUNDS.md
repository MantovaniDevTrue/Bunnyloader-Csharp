# SurfaceBackgroundLoader C# — serviço compartilhado e exemplo original

Continuidade sobre `8173aaac`, sem APK. SDK e exemplo editável 0.1.66, cache
`roslyn-net8-source-v57-surface-session-lifecycle`. Este é o terceiro dos quinze
loaders implementado nos fontes. Os doze restantes continuam bloqueando o APK.
Android, desempenho e o deadline real no MonoVM continuam sem validação nova.

## APIs e execução

| API | Uso | Ligação real |
| --- | --- | --- |
| `BunnySurfaceBackgroundStyle` | Herança, estado e algoritmos próprios; `SetStaticDefaults`, `ModifyFarFades`, `ChooseFarTexture`, `ChooseMiddleTexture`, `PreDrawCloseBackground`, `ChooseCloseTexture` | `BunnySurfaceRegistry` → estágio público 13 → worker MonoVM → `ManagedSurfaceBackgrounds` → seis hooks em `content/backgrounds/SurfaceBackgrounds` |
| `BunnySurfaceBackgrounds` | `VanillaCount` real, `Get`, `GetSlot<T>`, `GetSlot(uid,class)`, `Activate<T>`, `Deactivate<T>`, `Set` com prioridade/peso e seleção de menu | Registro nativo UID/classe compartilhado com JS; nenhum ID é persistido |
| `BunnySurfaceDrawContext` | Snapshot de câmera/altura/fade/cor; `Draw` com textura, posição, source opcional, rotação, origem, escala, efeitos e profundidade | Comandos limitados ao callback, aplicados pelo SpriteBatch já aberto após resposta completa |
| `BunnyBackgroundTextures.GetSlot(fullPath)` | UID/caminho completo, além do overload UID/caminho relativo existente | Mesmo registro nativo de assets, inclusive dependências entre mods |

Exemplo completo:
`samples/ExampleModCSharp/Playable/content/Content/Biomes/ExampleSurfaceBackgroundStyle.cs`.
As seis imagens originais estão em `content/Assets/Textures/Backgrounds`.
Os comentários explicam o cache de slots, Double antes do store Single, chamadas
efetivas da animação e a origem dinâmica do UID. O frame é estático como no JS,
avança após treze chamadas e percorre as quatro imagens, incluindo o caminho
completo do frame 3. Far/Close e todos os parâmetros originais são preservados.

`PreDrawCloseBackground` pode adicionar até 128 sprites e devolver false para
substituir a frente. Não recebe ponteiros IL2CPP nem abre/fecha o batch. O contexto
é `readonly ref struct`: não pode ser guardado em objetos, tarefas ou campos.
Exceção/retorno inválido não publica comandos parcialmente preparados.
Esses recursos são do SDK público e funcionam em classes de mods independentes.

O estágio foi acrescentado no índice 13. Os índices existentes 0..12, incluindo
11 interno e 12 subterrâneo, foram preservados. Em produção o mailbox limita a
espera a 3 ms; uma revisão vencida não escreve em um novo desenho. O teste CLR
usa orçamento de 2 s para absorver JIT e escalonamento do host. **Esse orçamento
de teste não mede nem comprova o atendimento de 3 ms no Android.**

O protocolo v1 usa apenas Int32/Int64/Single/Double/UInt32 de larguras verificadas,
treze doubles de snapshot e treze de comando, sem compartilhar layouts de objetos
entre CLR e IL2CPP. Sessão, slot, operação, comprimento, finitude, flags, source e
capacidade são conferidos antes da resposta. Late/duplicado, ocupado, dispatcher
removido e fades incompletos são recusados. `ArrayPool<float>` e snapshots de
registro evitam arrays gerenciados novos por fade. Logs de falha têm contexto do
mod/classe/hook e exceção original, com emissão na primeira ocorrência e potências
de dois. Não existe log por frame saudável.

## JavaScript e ciclo de vida

`SurfaceBackgroundLoader.js` agora reserva IDs e instala outro provider para o
mesmo serviço. Mantém `List`, `Get`, `VanillaCount`, `Wake`, instâncias e métodos
dos mods JS. Um Map aceita slots intercalados entre C#/JS, sem subtrair Count.
SceneEffect conserva o peso já calculado no ranking. Menu continua utilizando
`MenuLoader.BackgroundSlot`; o loader não instala novamente os seis hooks em JS.
`PreDrawCloseBackground(spriteBatch)` dos mods JS continua recebendo o batch real.

O caminho C# funciona sem contexto/VM QuickJS. Remover um provider aposenta só
os estilos de sua linguagem. O último scope C# pode sair enquanto JS continua;
remover JS preserva o dispatcher C#. Remover o primeiro scope transfere a posse do hook
ao próximo, sem desinstalar/reinstalar o estágio e sem pedir permissão nativa de
instalação durante teardown. Mundo novo apaga ativação de cena e invalida respostas pendentes,
preservando a seleção independente de menu. O host mantém módulos de superfície
e seu scope de renderização ativos ao sair do mundo. `PauseWorld()` remove os
módulos/scopes de gameplay; na reentrada estes usam um contexto coletável novo
para preservar o reset anterior de instâncias e estáticos. O estilo de superfície
continua na instância/contexto original, conservando sua animação de menu. Unload
real do mod fecha também esse scope e sua assembly. O código nativo deixa apenas
o estágio 13 ativo durante essa pausa, sem habilitar hooks de gameplay no menu.
Texturas de saída ficam retidas por
leases durante o fade; dependência ausente tem diagnóstico e fallback seguro.

## Testes executáveis

| Comando | Evidência |
| --- | --- |
| `BUNNY_SURFACE_SANITIZE=1 bash tests/surface_backgrounds/run.sh` | Componentes e seis detours de produção, Nullable/Color/SpriteBatch reais sob metadata controlada, transações de arrays/leases, revisão, deadline, capacidade e comparação com JS original em Node |
| `bash tests/surface_backgrounds/run_managed.sh` | Exemplo C# real, P/Invoke, worker separado e `UniversalHookMailbox` de produção; 53 chamadas de animação, fades, desenho próprio, exceção, menu/mundo e rebinding; Session.HostTests registra o DLL real, executa PauseWorld, desenha no menu, reentra três vezes com estáticos de gameplay reiniciados e só então descarrega o mod |
| `bash tests/surface_backgrounds/run_quickjs.sh` | Exemplo JS original no QuickJS do projeto; depois C# e JS reais juntos no mesmo processo, slots C#/JS/C#/JS, prioridades e ambas as ordens de unload |
| `python3 tools/ci/test_ready_prs.py` | Regressões existentes; o inventário mantém todos os exemplos anteriores e acrescenta um comando e um módulo de estilo |

O CI source-only executa também sintaxe ARM64 e link completo de `libbunny.so`,
conferindo onze novos exports. Não executa `assemble`, assinatura ou publicação.
Fonte IL2CPP controlada e CLR host não equivalem a Terraria ou MonoVM Android.

## QA Android, exclusivamente com ExampleMod

Procedimento para quando o gate autorizar o APK completo; ainda não executado:

1. Habilitar ExampleMod C# 0.1.66 com as seis imagens originais. Verificar registro
   de `ExampleSurfaceBackgroundStyle`, sem erros/avisos e sem GUID fixo no fonte.
2. Na superfície aberta, executar `/examplesurface on`. Conferir montanha distante,
   quatro frames do meio e camada próxima. Andar horizontalmente e subir/descer
   para observar parallax, repetição, altura e transição.
3. Executar `/examplesurface off` durante a animação: todas as camadas retornam ao
   fundo anterior por fade. Repetir várias vezes; não usar um item vanilla como
   substituto desse teste.
4. Salvar/sair e entrar novamente. A ativação de QA termina na fronteira do mundo;
   executar `on` outra vez e confirmar os mesmos assets/animação, sem erro de slot
   antigo. Repetir com outro personagem/mundo.
5. Testar junto do ExampleMod JS original e mods C# independentes. Conferir ausência
   de colisão de IDs e continuidade do outro provider quando um mod for desligado
   e a sessão reconstruída. Não retirar texturas enquanto o mod ainda as necessita.
6. Medir frame time/FPS, estabilidade, memória e tempo de callback com a iluminação
   já existente e durante combate. Registrar versão, PID, logs e resultado real.

O comando de QA não implementa `ExampleSurfaceBiome`, contagem de tiles ou
`SceneEffectLoader`: esses loaders/exemplos permanecem pendentes na matriz.

## Próximas implementações do mesmo lote

WaterStyle e WaterfallStyle compartilham seleção por cena/menu, texturas e vários
pontos de renderização. Auditar juntos seus contratos originais antes do port;
reutilizar registro/leases/mailbox e adicionar as operações nativas específicas,
sem assumir equivalência entre tabelas de água e de backgrounds. Depois integrar
SceneEffect/Biome e os demais doze alvos. Não mudar o gate para liberar este recorte.
