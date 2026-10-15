# Bunny Loader C#

Suporte experimental a mods C# para Terraria Mobile, baseado no
[Bunny Loader de Jonata Oliveira](https://github.com/JonataOliveiraa/Bunny-Loader).
Os mods-fonte usam `.cs`, Roslyn e MonoVM, com integracao nativa C++/IL2CPP.

## Comecar a desenvolver

- [Guia atual do SDK e contratos de execucao](docs/SDK-CSharp.md)
- [StarterMod minimo e gerador de mods](Split/Bunny-Loader/samples/StarterModCSharp/README.md)
- [Exemplo de heranca e identidade declarada](Split/Bunny-Loader/samples/DeclaredContentCSharp/README.md)
- [Exemplo de acessorio com efeitos nativos](Split/Bunny-Loader/samples/NativeAccessoryCSharp/README.md)
- [Independencia C# de JavaScript](docs/csharp-native-policy-independence.md)
- [Armaduras, imunidades e conjunto com receitas vanilla](Split/Bunny-Loader/samples/ArmorPoliciesCSharp/README.md)
- [Referencia publica gerada do SDK](Split/Bunny-Loader/docs/generated/sdk_public_reference.md)
- [Plano completo de evolucao e pendencias](docs/SDK-ROADMAP.md)
- [Testes e roteiro Android desta etapa](docs/SDK-VALIDACAO.md)

## Fontes canonicos deste checkout

| Diretorio | Conteudo |
| --- | --- |
| [BunnySourceSdk](Split/Bunny-Loader/experiments/csharp/BunnySourceSdk/) | Projeto do SDK gerenciado utilizado pelo runtime |
| [BunnyCsSdk](Split/Bunny-Loader/experiments/csharp/BunnyCsSdk/) | Classes-fonte publicas C#; o csproj acima define quais entram no SDK fonte |
| [BunnySourceCompiler](Split/Bunny-Loader/experiments/csharp/BunnySourceCompiler/) | Roslyn, cache, verificacao e exportacao de descritores |
| [Host C++](Split/Bunny-Loader/app/src/main/cpp/experiments/csharp/) | MonoVM, filas, callbacks e integracao IL2CPP |
| [ExampleMod C#](Split/Bunny-Loader/samples/ExampleModCSharp/) | Exemplos existentes, fontes e assets |
| [Projeto Android](Split/Bunny-Loader/app/) | App, NDK e integracao do loader |

A branch `main` historicamente acompanha sincronizacoes do Bunny original e
nao representa automaticamente o estado C# mais recente. Este conjunto parte de
`codex/sync-jonata-20261015`, commit `7229a61` (.90.29.31), e esta sendo revisado
em `codex/csharp-sdk-runtime-foundation`, com a atualizacao .90.29.32.1
(commit `3dd8c8d`) de defesa nativa e a .90.29.33 (`13e53fe`) de efeitos de
acessorio integradas. Compile o checkout da branch desejada.
Os links acima abrem os diretorios canonicos diretamente, sem depender de espelhos.

## Estado desta etapa

Implementados para verificacao: caches de assembly e descritores com integridade,
parsing adiado, efeitos, armaduras/conjuntos e instalacao de equipamentos/animacao/luz em C++ sem adaptadores JS e samples, declaracao explicita de conteudo para heranca, StarterMod com projeto de
editor, referencia publica gerada e callbacks automaticos continuos com filas
limitadas e 32 vinculos. Benchmarks manuais mantem o teto de 25 mil eventos.
Essas mudancas ainda precisam de build Android e validacao em aparelho.

Ainda ha policies de equipamento executadas pelos carregadores JavaScript.
IA gerenciada geral por frame, multiplayer, hot-unload e todos os exemplos do
Bunny continuam pendentes no plano. Nenhum teste de host implica 60 FPS no jogo.
O projeto e independente e nao e uma distribuicao oficial do Bunny Loader.
Nao executa mods tModLoader automaticamente. Direitos de Terraria permanecem
com seus respectivos titulares; o projeto nao fornece o jogo.
