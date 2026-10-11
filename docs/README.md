# Documentação — Bunny Loader C#

Esta pasta reúne a documentação **da extensão experimental C#**, sem substituir os [documentos do Bunny Loader base](../Split/Bunny-Loader/docs/README.md).

| Documento | Finalidade |
| --- | --- |
| [STATUS.md](STATUS.md) | O que realmente foi validado, por versão, e o que está pendente |
| [managed-live-benchmark-v64.md](managed-live-benchmark-v64.md) | Teste opt-in de 25 mil callbacks reais do MonoVM (.64) |
| [ARQUITETURA.md](ARQUITETURA.md) | MonoVM, Roslyn, host nativo e limites de execução |
| [mono-auto-live-world-signal-v62.md](mono-auto-live-world-signal-v62.md) | **Teste Android validado .62**: 512 callbacks, 0 falhas, limite controlado |
| [TESTE-ANDROID-v60.md](TESTE-ANDROID-v60.md) | Protocolo histórico da versão .60 |
| [ROADMAP.md](ROADMAP.md) | Próximas etapas, sem promessas de compatibilidade não comprovada |
| [HISTORICO.md](HISTORICO.md) | Linha do tempo das mudanças importantes e referências dos builds |

### Fontes e documentação técnica já existentes

- [Protótipo de eventos automáticos .60](bunny-csharp-auto-managed-events-v0.1.10.16.60.md)
- [Teste gerenciado OnHitNPC .59](mono-live-hit-onhit-0.1.10.16.59.md)
- [Correção do GC da thread Mono](mono-pinned-gc-wait-fix.md)
- [Compilação Roslyn em worker](bunny-csharp-roslyn-worker-0.1.10.16.md)
- [Guia Android / Mono ARM64](mono-arm64-build-runbook.md)
- [ExampleMod C#: início](../Split/Bunny-Loader/samples/ExampleModCSharp/Docs/GettingStarted.md)

Em toda a documentação, **implementado**, **CI aprovado** e **confirmado no celular** são estados distintos.
