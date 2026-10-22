# Estado do Bunny Loader C#

Este documento reúne o estado de referência da extensão C# e aponta para as evidências específicas. Compilar um componente não equivale a validá-lo no Terraria Android.

## Fontes e checkpoints

- [Código de integração e primeiros passos](../README.md): a referência de código integrada está na branch `v1.0`, enquanto `main` mantém o histórico e recebe as propostas.
- [Roadmap C#](SDK-ROADMAP.md): trata o checkpoint `.62` como uma base de testes anterior, não como aprovação automática de todos os recursos futuros.
- [Arquitetura e SDK](SDK-CSharp.md): Roslyn/MonoVM executa C# e o host nativo C++ integra os recursos ao Terraria IL2CPP.
- [Validação e Android](SDK-VALIDACAO.md): deve ser consultado para separar testes em host, relatórios de aparelho e funcionalidades ainda pendentes.
- [Matriz dos exemplos](../Split/Bunny-Loader/docs/generated/example_completion/matrix.md): cobertura por exemplo, sem declarar um exemplo completo apenas por possuir classe C#.
- [Histórico](HISTORICO.md): relatórios e regressões anteriores.

## Áreas que exigem validação contínua

| Área | Condição documentada |
| --- | --- |
| Compilação e carregamento | Fontes C# com Roslyn e MonoVM, integração via C++/IL2CPP; comportamento e custo de início dependem da sessão Android |
| Conteúdo jogável | Itens, receitas, acessórios, armaduras, NPCs e projéteis possuem implementações específicas; nem todos os exemplos tModLoader são suportados |
| UI, mundo e multiplayer | Contratos gerais incompletos; não equiparar APIs disponíveis a compatibilidade total com o tModLoader |
| Desempenho | FPS, memória, filas e latência devem ser aferidos com logs e sessões reais no aparelho |
| Descarregamento e persistência | Requerem testes por mod, mundo, personagem e carga repetida |

## HairLoader C# — PR #134

A [PR #134](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/134) é experimental e **permanece em rascunho**, com [documentação e requisitos próprios](csharp-hair-loader.md).

Os testes atuais verificam registro C#, preparação de manifesto, tipos estáveis, contratos C++ de UI/textura/rede e salvamento apenas no nível de fonte/host. O `SourceLoader` recusa conteúdo `ModHair` enquanto faltam instalação/recarga IL2CPP, hooks de save, envio/recebimento multiplayer e QA Android.

**Nenhuma build Android ou exemplo jogável deve ser publicado a partir dessa PR até que os requisitos estejam completos e validados.**

## Política de atualização

Ao validar um novo lote, registrar o commit, os testes automatizados, o cenário Android e eventuais limitações no documento específico. Não substituir resultados antigos por uma inferência de compatibilidade. Mudanças experimentais não alteram automaticamente o checkpoint geral.
