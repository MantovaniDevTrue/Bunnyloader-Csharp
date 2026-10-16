# Histórico técnico — linha experimental C#

As branches de versão funcionam como snapshots de regressão. O acompanhamento atual está na branch de documentação **`csharp-development`**, derivada da **.60**, enquanto a branch de teste **.60 permanece intacta**.

| Versão | Mudança principal | Evidência |
| --- | --- | --- |
| `0.1.10.16.34–.36` | Ponte de conteúdo, IA de exemplos, compilação por identidade de mod e carregamento automático | Testes de exemplo em etapas; cobertura específica |
| `0.1.10.16.45–.48` | Investigações de execução gerenciada ao vivo/IA NPC e threads | Experimental; parte dos caminhos exigiu correções de ciclo de vida |
| `0.1.10.16.49–.57` | Diagnósticos de Mono, threads, sinais ARM64 e travamentos | Investigação incremental; nem toda versão é estável |
| **`0.1.10.16.58`** | Espera na thread proprietária do Mono compatível com GC | `DOMAIN_ONLY` e `PINNED_MONO` relatados funcionando no Android, retorno 42 |
| **`0.1.10.16.59`** | Callback **C# gerenciado OnHitNPC** sob ativação explícita | **10/10 eventos reais**, sem perda/falha no relato registrado |
| **`0.1.10.16.60`** | Registro automático de `OnHitNPC` + `OnKill` via convenção, ExampleMod 0.1.7 | CI aprovado; **teste real Android pendente** |

### Links essenciais

- [Release .58](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.58)
- [Release .59](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.59)
- [Release .60](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/releases/tag/bunny-csharp-managed-runtime-0.1.10.16.60)
- [Branch congelada de teste .60](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/tree/bunny-csharp-source-0.1.10.16.60-auto-live-events)
- [Origem upstream de Jonata](https://github.com/JonataOliveiraa/Bunny-Loader)

Não tratar `result=42` como prova de compatibilidade de todo o gameplay; ele confirma uma execução gerenciada específica. Da mesma forma, um workflow verde não comprova FPS no aparelho.
