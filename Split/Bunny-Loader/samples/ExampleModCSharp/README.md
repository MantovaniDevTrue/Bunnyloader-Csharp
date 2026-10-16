# ExampleMod C#

Exemplos editáveis do SDK Bunny, com `.cs` e assets no pacote.
**Versão do manifest: `0.1.40`; checkpoint atual: Bunny `0.1.10.16.90.29.62`.**
Use o pacote da mesma release do APK; versões `0.1.40` anteriores têm bytes
diferentes. [Validação e hashes](../../../../docs/STATUS.md).

## Onde começar

Para um mod novo, prefira o [StarterMod](../StarterModCSharp/README.md) e o
[guia rápido](../../../../docs/PRIMEIRO-MOD.md). O ExampleMod reúne vários
recursos para consulta; não é preciso copiar o conjunto inteiro.

| Pasta | Conteúdo |
| --- | --- |
| [Playable](Playable/) | Manifest, fontes e assets que entram no pacote |
| [Content/Items](Playable/content/Content/Items/) | Armas, ferramentas, acessórios e armaduras |
| [Content/Projectiles](Playable/content/Content/Projectiles/) | Defaults, IA e eventos de projéteis |
| [ExampleSlimeNPC](Playable/content/Content/NPCs/ExampleSlimeNPC.cs) | Defaults, perseguição no worker e hit/kill |
| [Common/Players](Playable/content/Common/Players/) | Jogador, policies, dano e hooks |
| [Common/Recipes](Playable/content/Common/Recipes/) | Receitas comentadas |
| [Common/Live](Playable/content/Common/Live/) | Callbacks estáticos legados por projétil |
| [Tests](Tests/) | Guardas dos fontes e exemplos |

## Executar e verificar

Importe e habilite o `.bmod` correspondente ao APK; o loader prepara C# antes
do mundo. Não precisa executar comandos JS de inicialização. Encontre conteúdo
no menu de mods ou pelas receitas; teste itens, equipamento, NPC e Wave Wand.
[Protocolo Android](../../../../docs/SDK-VALIDACAO.md).

Equipamento, textura Back, asas, animação/luz e armaduras/conjuntos usam o
host nativo sem adaptadores JS. IA e eventos por entidade passam pelo MonoVM;
curvas nativas têm prioridade de movimento. Consulte os [contratos](../../../../docs/SDK-CSharp.md)
antes de confundir policies amostradas com callbacks reais.

A [referência pública](../../docs/generated/sdk_public_reference.md) é gerada
do SDK compilado. A [matriz](../../docs/generated/example_completion/matrix.md)
preserva o progresso de cada recurso; validação geral do pacote não promove
todos os ports a completos. Multiplayer/hot-unload universais seguem pendentes.

[Notas e resultados históricos](Docs/HISTORY.md) · [Catálogo de exemplos](../../../../examples/README.md)
