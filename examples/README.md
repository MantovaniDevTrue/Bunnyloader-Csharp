# Exemplos para modders C#

| Exemplo | Uso | Evidencia desta etapa |
| --- | --- | --- |
| [StarterMod](../Split/Bunny-Loader/samples/StarterModCSharp/README.md) | Um material, uma receita, projeto de editor e gerador de UID | Compilacao e pacote em host; Android pendente |
| [DeclaredContent](../Split/Bunny-Loader/samples/DeclaredContentCSharp/README.md) | Base abstrata generica e identidade declarada por atributo | Parser nativo e Roslyn; exige o novo SDK; Android pendente |
| [ExampleModCSharp](../Split/Bunny-Loader/samples/ExampleModCSharp/) | Conjunto existente de armas, equipamentos, jogador, NPC e bancada | Validacao varia por recurso; consultar as evidencias |

O manifest do ExampleMod deste checkout identifica a versao 0.1.37. Nao
confundir quantidade de fontes C# com quantidade de ports integrais concluidos.
A [matriz dos 201 arquivos de referencia](../Split/Bunny-Loader/docs/generated/example_completion/matrix.md)
e o backlog existente continuam preservados; o historico de cada entrada pode
precisar ser conciliado com os commits recentes antes de promover seu estado.

Use os [contratos do SDK](../docs/SDK-CSharp.md). Os callbacks estaticos legados
sao diferentes das policies de instancia avaliadas na compilacao. Os exemplos
seguem o ExampleModBunny e os fontes do loader; nao importam APIs de outros loaders.
