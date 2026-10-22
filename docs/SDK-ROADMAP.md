# Roadmap C#

Base integrada: [checkpoint .62](STATUS.md). As confirmações do usuário e
telemetria por sessão são preservadas separadamente dos testes de host.
Objetivo: mods-fonte C# independentes, carregamento automático e APIs
reutilizáveis, com fidelidade e desempenho medidos no Android.

| Área | Entregue | Próximo critério de conclusão |
| --- | --- | --- |
| Roslyn/cache | Integridade, invalidação de referências, parsing adiado, cache externo e recuperação | Comparar cold/warm start e RAM em mods grandes no aparelho |
| Independência JS | Serviços nativos de receitas, equipamentos, armaduras, animação/luz e desenho existentes | Manter guardas sem QuickJS e validar cada API nova sem fallback JS |
| Ferramentas de modder | StarterMod, UID, projeto de editor, pacote determinístico e referência gerada | Validar cada sample independente e melhorar mensagens/diagnóstico |
| Player/dano | Policies, hooks ref/out, isolamento e prazo por golpe | Mais mods simultâneos, percentis/timeout e sessões longas |
| Entidades | Snapshots, geração, IA no worker, hit/kill e primeiro tick antecipado | Ampliar a amostra além dos 280/280 callbacks sem rejeições, medir carga/memória, reuso de slots e troca de mundo |
| Ciclo de vida | Invalidação por sessão e limpeza dos vínculos existentes | Contrato completo de load/unload, persistência e estáticos por mod |
| Exemplos/conteúdo | Armas, equipamentos, jogador, prefixos, NPC e bancada; matriz preservada | Completar categorias inteiras com comportamento, assets, testes e Android |
| Mundo/sistemas | Fundações parciais | Save customizado, worldgen, biomas, UI/config/comandos/localização reutilizáveis |
| Multiplayer | Sem contrato completo demonstrado para esta ponte C# | Autoridade, IDs, host/cliente, entrada tardia e sincronização de estado |
| Publicação | Código integrado, guias atuais, histórico e guarda de organização | SDK versionado, pacotes com versão nova a cada alteração e release reproduzível |
| Runtime/manutenção | Worker e filas limitadas; GC/sinais com regressões | Modularizar sem alterar contratos de thread/GC e medir carga real |

## Próximos lotes

1. Medir a `.62` em sessão prolongada: callbacks, rejeições por motivo, memória,
   latência e FPS; a confirmação e as métricas registradas na PR #35 já constam no status.
2. Ampliar IA/combate por categoria completa de NPC/projétil/boss. Preservar
   curvas, efeitos, mira e direção; manter isolamento de mod/tipo e sessão.
3. Conciliar evidências recentes com a [matriz completa](../Split/Bunny-Loader/docs/generated/example_completion/matrix.md)
   e expandir exemplos para mods independentes, sem patches específicos por exemplo.
4. Formalizar persistência/ciclo de vida e avançar sistemas de mundo/UI.
5. Implementar multiplayer com testes próprios de host/cliente.

Agrupe mudanças por dependência e faça checkpoints amplos; APK só quando houver
alteração que realmente precise de Android. Não adicionar APIs vazias ou usar
scripts JS como substituto de implementação C#. Nenhum item da matriz deve ser
promovido a completo apenas por compilar ou compartilhar nome com o exemplo JS.

[Plano anterior preservado](history/snapshot-SDK-ROADMAP.md) · [Validação](SDK-VALIDACAO.md)

Em desenvolvimento separado: [PR #35 (.63)](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/35),
controle de QuickJS em sessão somente C# e avisos de combinação C#/JS.
A validação Android dessa alteração continua pendente.

## Paridade com loaders do tModLoader

Objetivo: todos os loaders oficiais, com API/código o mais próximos possível do
tModLoader, mantendo as adaptações necessárias ao Terraria Mobile.
[Inventário pinado de 43 arquivos oficiais](TMODLOADER-LOADERS.md).

O [primeiro lote de infraestrutura](csharp-content-loader.md) implementa
`Loader`, `Loader<T>`, `ILoader`, `LoaderManager`, `ModTypeLookup<T>` e o ciclo
gerenciado de registros, bootstrap automático no dono MonoVM e teardown global.
Inclui mod-fonte de serviços e testes da fila e do gate de mundo. Testes de host não validam integração
Android. O lote permanece sem APK e sem novos registros `androidVerified`.
