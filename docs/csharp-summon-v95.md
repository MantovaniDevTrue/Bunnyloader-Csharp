# .95.1 — ciclo de vida e combate de summons

Branch `feat/csharp-summon-lifecycle-v95`. Base integrada por merge: `.93` + `.94` (`6a106345`) preservando as correções funcionais da `.90.6` contidas na `.94` (a branch `.90.6` não é ancestral desta linha). Bunny `.90.29.95.1-summon-snapshots`, code271; ExampleMod `0.1.54`; Lights `0.4.2.3` preservado.

## Correções sistêmicas

- Minions verificam o **dono real** e o buff antes da IA nativa: cancelamento, morte ou dono inativo chamam Projectile.Kill e os hooks existentes; buff só é aplicado na primeira observação de uma nova identity.
- `timeLeft=2` é renovado com buff ativo; renovar não cria geração nova. Reutilizar slot/objeto com nova identity permite reinvocação.
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
