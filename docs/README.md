# Documentacao atual do SDK C#

- [Comecar e entender a execucao](SDK-CSharp.md)
- [Plano completo e criterios de conclusao](SDK-ROADMAP.md)
- [Testes desta etapa e roteiro Android](SDK-VALIDACAO.md)
- [Referencia publica gerada do SDK compilado](../Split/Bunny-Loader/docs/generated/sdk_public_reference.md)

Os documentos abaixo preservam o historico por versao. Menus, limites e
catalogos antigos podem descrever estados anteriores; para esta branch, consulte
primeiro os contratos atuais acima. Nao promover uma validacao antiga a todas
as funcionalidades de um SDK mais recente.

---

# Documentação — Bunny Loader C#

Esta pasta reúne a documentação **da extensão experimental C#**, sem substituir os [documentos do Bunny Loader base](../Split/Bunny-Loader/docs/README.md).

| Documento | Finalidade |
| --- | --- |
| [STATUS.md](STATUS.md) | O que realmente foi validado, por versão, e o que está pendente |
| [Relatório de callbacks por projétil .73](multi-live-projectiles-v73.md) | Android: 985/985 eventos, 879/879 secundários, 0 perdas/falhas; FPS não informado |
| [Managed Default .74](csharp-managed-default-v74.md) | Roslyn/MonoVM como runtime primário, sem protótipo NativeAOT no boot, legado externo preservado; teste Android pendente |
| [ExampleModCSharp 0.1.9](../Split/Bunny-Loader/samples/ExampleModCSharp/README.md) | Exemplos reais de 2 armas, 2 projéteis e 2 classes de callbacks; guia atualizado |
| [managed-live-benchmark-v64.md](managed-live-benchmark-v64.md) | Teste opt-in de 25 mil callbacks reais do MonoVM (.64) |
| [gameplay-perf-v65.md](gameplay-perf-v65.md) | Medir latência, contagem de callbacks e ritmo de atualização durante gameplay (.65) |
| [vm-parity-v66.md](vm-parity-v66.md) | Histórico: .66 compilou, mas a chamada do método MonoVM falhou no Android |
| [vm-parity-v67.md](vm-parity-v67.md) | .67: DLL específica validada no build, mas ausente na pasta do processo Android |
| [vm-parity-v68.md](vm-parity-v68.md) | .68: paridade matemática confirmada, incluindo repetições no Android |
| [vm-ai-parity-v69.md](vm-ai-parity-v69.md) | .69: simulações equivalentes de IA de 64 projéteis/NPCs, teste de CPU QuickJS × MonoVM |
| [gameplay-perf-v70.md](gameplay-perf-v70.md) | .70: percentis de callbacks C# reais, CPU da thread e curvas de projéteis no jogo |
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
