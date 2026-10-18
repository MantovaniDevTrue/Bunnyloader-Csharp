# .95.2 — ciclo de vida e combate de summons

Branch `feat/csharp-summon-lifecycle-v95`. Base integrada por merge: `.93` + `.94` (`6a106345`) preservando as correções funcionais da `.90.6` contidas na `.94` (a branch `.90.6` não é ancestral desta linha). Bunny `.90.29.95.2-summon-native-lifecycle`, code272; ExampleMod `0.1.54`; Lights `0.4.2.3` preservado.

## Correção Android .95.2

O relatório do usuário da `.95.1`, gerado em 2026-10-18 19:14, pid4956, demonstrou `Field=identity Expected=Instance:System.Int32 Actual=Missing`. O contrato introduzido na .95 exigia um campo ausente nessa versão Android, recusando o ExampleMod no BindLifecycle. MonoVM foi verificado, mas receitas/tiles/callbacks do mod não terminaram de carregar; os relatórios de hooks e combate pertenciam a outro PID. Isso não valida o ciclo de vida no dispositivo.

A .95.2 elimina toda leitura/resolução de identity. O hook de SetDefaults já instalado no serviço nativo informa a reutilização antes de chamar o original. A identificação usa o slot tipado e validado, a referência do objeto e uma geração própria do runtime. A notificação só acessa o armazenamento na thread que executa a IA, não nas threads de amostras/carregamento. Renovar timeLeft não permite reaplicar o buff. Preserva o dano por origem, os snapshots imutáveis e o protocolo/cache v27. ExampleMod0.1.54 mantém as mesmas fontes C#; não há migração de saves nem mudança de IDs.

Testes novos executam o binder de produção sem identity (e também com um campo identity de outro tipo, que não é utilizado), recusam whoAmI com tipo/storage/offset/limites incorretos e verificam que originalDamage continua obrigatório. Os hooks de produção cobrem a notificação antes do original, reutilização do mesmo objeto/slot/tipo/dono/timeLeft, cancelamento após renovar timeLeft, amostra diferente no mesmo slot e notificação de outra thread. São testes controlados, não execução do Terraria.

## Correções sistêmicas

- Minions verificam o **dono real** e o buff antes da IA nativa: cancelamento, morte ou dono inativo chamam Projectile.Kill e os hooks existentes; buff só é aplicado na primeira observação de uma instância após SetDefaults nativo.
- `timeLeft=2` é renovado com buff ativo; renovar não cria geração nova. SetDefaults invalida o estado anterior antes de o motor sobrescrever o slot; reutilizar o mesmo objeto/tipo/dono permite reinvocação. Amostras de conteúdo em outra thread não acessam o estado de gameplay.
- `BunnyProjectile.MinionMovement` usa BunnyMinionMovement configurável a cada update nativo, corrigindo a inércia originalmente calculada a cada seis updates do worker. Seguir/atacar/retornar, posição relativa ao dono/direção, formação/separação, teleporte, rotação e spriteDirection.
- CanBeChasedBy/CanHitLine, alvo manual quando campo disponível, limite de afastamento do dono e atualização de posições de alvo entre scans. Friendly apenas quando há alvo no controlador nativo.
- ApplyStatsFromSource inicializa originalDamage pelo item real; preserva dano escalado e baseline existente. Eliminado fallback que usava o maior dano entre cajados registrados e mascarava a causa do zero.
- Registro antecipado de BunnyProjectile extrai apenas nomes/assets; defaults calculados e MinionMovement sao executados pelo Roslyn/MonoVM e validados no descritor, sem exigir atribuicoes literais. Teste adicional executa parser e registrador antecipado de producao.
- Contratos dos novos campos, tipos, tamanhos, offsets/limites e assinaturas validados antes de habilitar minions. Nomes de parâmetros podem variar; tipos/retorno/instância continuam obrigatórios. Cache de metadata e descritor opcional de 18 campos; formatos anteriores preservados. Cache compilador v27.

## Publicação concorrente das definições

A build `.95`/code270 foi compilada e entregue experimentalmente pelo run [39164705347](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39164705347), fonte `25ea4f5bc9c18d3eca91d20303e9da6c7e122c3e`. A revisão posterior identificou leitores do jogo acessando mapas que o carregador pode alterar em segundo plano. A `.95.1` publica snapshots imutáveis de armas e definições de projéteis (defaults, offsets validados, movimento e chave de diagnóstico). O jogo conserva uma revisão enquanto a lê; a publicação do lote ocorre depois da validação. Não há cópia de mapas nem alocação de configurações a cada frame.

O teste do publicador de produção executa 2.000 revisões com três leitores concorrentes e verifica consistência entre chave e valores. O ExampleMod permanece0.1.54. Os demais stores legados não recebem uma certificação global de concorrência por este teste. Validar no Android a `.95.1` ou posterior, não promover a `.95` a referência estável por ter CI verde.

## Testes e limites

`tests/summon_lifecycle`: executa os hooks de produção e helpers com providers IL2CPP controlados e QuickJS desativado. Cobre cancelamento/morte/reinvocação/dono/seguimento/teleporte/alvos/LOS/leash, baseline por origem/dano escalado/zero e contratos rejeitados sem publicação parcial. O compilador real valida wire e recusa combinação de IA nativa e override C# ou AiStyle incompatível. Suítes anteriores preservadas. Checagens lexicais antigas do hook foram substituídas por execução real do hook e do agendador: primeiro ticket imediato, cadência de seis ticks, deduplicação, descarte de gerações antigas e pausa de mundo. O teste de registro antecipado prepara NativeContentSpec; não aloca IDs reais nem carrega texturas do Terraria.

Esses testes não executam Terraria Android nem certificam a ABI real ou FPS. Guia incluído em `Documentation/SUMMON_LOADER_V95.md`; todos os passos usam ExampleMod. Não declarar Lote 1 ou ProjectileLoader completo: matriz mantém exemplos parciais (atributos do cajado, spawn na mira, luz, contador de buff, hooks e outras famílias de projéteis pendentes).

Os avisos RecipeCallback de deadline e ScreenFx CPU do relatório `.94` são áreas independentes; este lote não muda seus budgets nem afirma corrigi-los. Prosseguir com medição e otimização específica após validar o ciclo de vida, sem retirar efeitos.

## Continuidade

1. Validar o guia Android e registrar versão/relatório por teste; não promover por aprovação de CI.
2. Completar propriedades públicas do cajado e spawn de origem/mira reutilizando os serviços existentes de itens.
3. Expandir armas/projéteis por mecanismos compartilhados (classes de dano, munição/mana, imunidade/colisão, minions/sentries e IA), não por hotfix de item.
4. Seguir matrizes completas sem perder arquivos; Bunny JS como mecanismo Android, PC/tModLoader como referência comportamental, C# independente como implementação. Sem nova dependência de tModLoader, QuickJS ou shaders neste lote.

## Histórico da entrega .95.1 — falha de vínculo constatada no Android

Fonte testada: `ea35b7b130487480ae8993d4f7ad3189d83e50af`. [CI39165749967](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39165749967) e nove suites independentes no mesmo SHA aprovados; assinatura V3 verificada no pipeline. [Release privada .95.1](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp-Builds/releases/tag/csharp-v3-run-39165749967-a1): APK241518449bytes e ExampleMod0.1.54. Checksum do bmod conferido e todas as71fontes C# comparadas byte a byte com o repositorio. Recibo: [csharp-v95-1-ci-2026-10-18.json](validation/csharp-v95-1-ci-2026-10-18.json).

Para o teste de combate, gere **Example Slime** (`ExampleSlimeNPC`) pelo catalogo do ExampleMod. Use ExampleMinionItem, seu buff e a bancada C#. Android/gameplay/ABI real/FPS continuam aguardando validacao do usuario. A .95 e a .95.1 permanecem históricas; a .95.1 foi recusada no Android pelo contrato de identity descrito acima. Usar a .95.2 ou posterior para a próxima validação. O conjunto completo de armas/projeteis permanece aberto.

## Como validar a correção no ExampleMod

1. Instalar o APK .95.2 por atualização e manter ExampleMod0.1.54; não apagar saves nem dados.
2. Abrir o relatório e confirmar `Stage=BindLifecycle Result=Ready InstanceTracking=NativeSetDefaults`, carregamento completo do ExampleMod e ausência do erro identity. Conferir a bancada C#, sua receita do cajado e os conteúdos do mod.
3. Executar `Documentation/SUMMON_LOADER_V95.md`: ExampleMinionItem, cancelar o buff com uma e várias invocações, reinvocar, acompanhar o jogador, atacar Example Slime, morte/respawn e salvar/sair/reentrar.
4. Registrar relatório da sessão e resultado de cada passo; os relatórios de hooks/combate só são evidência se o PID corresponder ao jogo atual. FPS permanece sem medição nova.

## Entrega .95.2 verificada

Fonte `018b477a5a90115f399aed57ff84811c020d7a3f`; [run39171952411](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39171952411) e nove suítes independentes push no mesmo SHA aprovados. Build Android, assinatura V3 e job de verificação da publicação privada aprovados. [Release experimental .95.2](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp-Builds/releases/tag/csharp-v3-run-39171952411-a1): APK241516501bytes; ExampleMod0.1.54, 190649bytes. Hash do bmod conferido e todas as71fontes comparadas byte a byte. O fixture de runtime passou também com ASan/UBSan no host. [Recibo](validation/csharp-v95-2-ci-2026-10-18.json). Aguardando novo relatório Android, sem certificação de gameplay/FPS.
