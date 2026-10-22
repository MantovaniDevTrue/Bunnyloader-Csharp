# Exemplos para modders C#

Comece pelo [primeiro mod](../docs/PRIMEIRO-MOD.md). Os fontes editáveis ficam
em `Split/Bunny-Loader/samples`; esta pasta oferece navegação.

| Exemplo | Aprender | Evidência e limites |
| --- | --- | --- |
| [BiomeConversion](../Split/Bunny-Loader/samples/BiomeConversionCSharp/README.md) | Conversão nativa de tiles/paredes acionada por projétil C# | Implementado em fonte e testado em host; QA Android pendente |
| [ContentLoader](../Split/Bunny-Loader/samples/ContentLoaderCSharp/README.md) | Loader<T>, aliases, singletons, embedding e mod-fonte com bootstrap automático | Host testado; QA Android pendente |
| [StarterMod](../Split/Bunny-Loader/samples/StarterModCSharp/README.md) | Um material, uma receita, UID e projeto de editor | Compilação/empacotamento em host; não atribuir teste Android independente sem registro |
| [DeclaredContent](../Split/Bunny-Loader/samples/DeclaredContentCSharp/README.md) | Base genérica e identidade por atributo | Parser e Roslyn; restrições sintáticas no SDK |
| [NativeAccessory](../Split/Bunny-Loader/samples/NativeAccessoryCSharp/README.md) | Acessório independente e efeitos nativos | Host sem QuickJS; validação varia por efeito |
| [ArmorPolicies](../Split/Bunny-Loader/samples/ArmorPoliciesCSharp/README.md) | Peças, imunidades, conjunto e receitas vanilla | Integrante dos testes Android da linha .57; não generalizar a todos os cenários |
| [ExampleModCSharp](../Split/Bunny-Loader/samples/ExampleModCSharp/README.md) | Armas, equipamento, jogador, prefixos, bancada, NPC/IA/eventos | Pacote `0.1.40` correspondente à .62 validada; cobertura por recurso continua na matriz |
| [NativePlacementProbe](../Split/Bunny-Loader/samples/NativePlacementProbe/) | Diagnóstico de colocação e bancada | Probe de desenvolvimento; não habilitar junto do exemplo unificado se duplicar conteúdo |

## Encontrar código no ExampleMod

| Recurso | Diretório |
| --- | --- |
| Armas e ferramentas | [Content/Items](../Split/Bunny-Loader/samples/ExampleModCSharp/Playable/content/Content/Items/) |
| Projéteis e callbacks por instância | [Content/Projectiles](../Split/Bunny-Loader/samples/ExampleModCSharp/Playable/content/Content/Projectiles/) |
| NPC, IA e combate | [ExampleSlimeNPC.cs](../Split/Bunny-Loader/samples/ExampleModCSharp/Playable/content/Content/NPCs/ExampleSlimeNPC.cs) |
| Hooks/policies do jogador | [Common/Players](../Split/Bunny-Loader/samples/ExampleModCSharp/Playable/content/Common/Players/) |
| Eventos estáticos legados | [Common/Live](../Split/Bunny-Loader/samples/ExampleModCSharp/Playable/content/Common/Live/) |
| Receitas | [Common/Recipes](../Split/Bunny-Loader/samples/ExampleModCSharp/Playable/content/Common/Recipes/) |
| Conteúdo e assets importáveis | [Playable](../Split/Bunny-Loader/samples/ExampleModCSharp/Playable/) |

Use o ExampleMod da mesma release do APK: pacotes antigos também se chamam
`0.1.40`, mas têm hashes diferentes. [Checkpoint/hashes](../docs/STATUS.md).

Os exemplos seguem o ExampleModBunny e os contratos reais do loader. Não
exigem JS nem APIs de TLPro/tModLoader. Quantidade de fontes não é quantidade
de ports completos: a [matriz dos exemplos](../Split/Bunny-Loader/docs/generated/example_completion/matrix.md)
preserva lacunas e critérios. A referência JS serve como especificação de
comportamento, sem virar dependência no caminho C#.
