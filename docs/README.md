# Documentação — Bunny Loader C#

Esta pasta reúne a documentação **da extensão experimental C#**, sem substituir os [documentos do Bunny Loader base](../Split/Bunny-Loader/docs/README.md).

| Documento | Finalidade |
| --- | --- |
| [STATUS.md](STATUS.md) | O que realmente foi validado, por versão, e o que está pendente |
| [Verificação Android .73](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/blob/csharp-managed-default-v74-cleanup/docs/multi-live-projectiles-v73.md) | 985/985 eventos, 879/879 do segundo projétil; falhas e descartes zero, FPS não informado |
| [Arquitetura .74](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/blob/csharp-managed-default-v74-cleanup/docs/csharp-managed-default-v74.md) | MonoVM padrão, NativeAOT legado preservado fora do build principal, teste Android pendente |
| [ExampleModCSharp 0.1.8: dois projéteis no Android](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/blob/csharp-examplemod-dual-projectile-0.1.8/docs/examplemod-csharp-dual-projectile-018.md) | 2 curvas ativas, 2.393 atualizações da curva nova, 161/161 callbacks antigos, 0 perdas e 60 FPS relatados |
| [managed-ingress-v72-android.md](managed-ingress-v72-android.md) | .72: perfil de 15 s e combate de ~7–8 min (2.897 eventos, 0 perdas, 60 FPS relatados) |
| [managed-live-benchmark-v64.md](managed-live-benchmark-v64.md) | Teste opt-in de 25 mil callbacks reais do MonoVM (.64) |
| [gameplay-perf-v65.md](gameplay-perf-v65.md) | Medir latência, contagem de callbacks e ritmo de atualização durante gameplay (.65) |
| [vm-parity-v66.md](vm-parity-v66.md) | Histórico: .66 compilou, mas a chamada do método MonoVM falhou no Android |
| [vm-parity-v67.md](vm-parity-v67.md) | .67: DLL específica validada no build, mas ausente na pasta do processo Android |
| [vm-parity-v68.md](vm-parity-v68.md) | .68: instalação com SHA-256 no processo do jogo, teste do comparador |
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
