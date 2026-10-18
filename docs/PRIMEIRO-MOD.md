# Primeiro mod C# (MonoVM/Roslyn)

O Bunny Loader C# compila fontes `.cs` no Android com Roslyn e executa callbacks compatíveis por MonoVM. Ele **não** carrega automaticamente mods feitos para tModLoader.

## Exemplo real para estudar

O pacote [SourceCSharpMod](../Split/Bunny-Loader/samples/SourceCSharpMod/) é uma referência com `manifest.json`, `content/`, armas e projéteis. O manifesto usa `entry: "main.cs"` e um UID próprio. O [SecondExampleModCSharp/Playable](../Split/Bunny-Loader/samples/SecondExampleModCSharp/Playable/) demonstra outro UID, separação de código e isolamento entre mods.

Para criar um mod novo, mantenha uma pasta própria com `manifest.json`, `content/main.cs` e demais `.cs` ou sprites necessários. Use um `uid` exclusivo, um `id` exclusivo, versão, autor, `gameVersion` compatível e a entrada que realmente existe. Não reaproveite o UID dos exemplos.

Consulte [catálogo](../examples/README.md), [SDK/contratos](SDK-CSharp.md), [referência gerada](../Split/Bunny-Loader/docs/generated/sdk_public_reference.md) e [validação Android](SDK-VALIDACAO.md) para saber quais APIs do checkpoint são suportadas. Os READMEs de exemplos antigos podem mencionar comandos de diagnóstico ou etapas manuais de versões anteriores: tome o SDK e os testes atuais como referência para a versão que está usando.

Um port para o **Bunny original** requer primeiro incorporar o runtime ao app; siga o [guia de integração para o Potato](INTEGRACAO-BUNNY-ORIGINAL.md). Apenas copiar um mod C# para um Bunny que só possui QuickJS não cria suporte à linguagem.
