# Testes e verificação

A revisão automática valida contratos, sintaxe, build e mock. **Somente jogar no Android confirma estabilidade real.**

## Automatizados

- [Testes nativos do runtime e parser](../Split/Bunny-Loader/tests/)
- [Verificação da thread Mono e transições de GC](../Split/Bunny-Loader/tests/run_managed_pinned_gc.py)
- [Verificação da propriedade dos sinais ARM64](../Split/Bunny-Loader/tests/run_managed_signal_ownership.py)
- [Teste dos exemplos C#](../Split/Bunny-Loader/samples/ExampleModCSharp/Tests/check_examples.py)
- [Workflows de build e checagens](../.github/workflows/)

## Base experimental .81

- [Fila FIFO64 com npcIndex e guardas de pausa/reentrada](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/blob/csharp-managed-event-context-v81-experiment/Split/Bunny-Loader/tests/test_managed_hit_context_v81.cpp). [CI host](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38174850848) aprovado; o callback V2 ainda não existe, não há APK .81 testada.

## Teste real

- [Validação Android v0.1.10.16.80](../docs/csharp-v80-android-validation.md) — 6 verificações concluídas e baseline funcional.
- [Roteiro Android v0.1.10.16.60](../docs/TESTE-ANDROID-v60.md).
- Enviar estado `mono`, `live`, `auto`, FPS observado e relatórios de encerramento.
- Não marcar sucesso Android apenas porque a APK foi criada ou os testes mock passaram.

Os resultados devem alimentar o quadro [STATUS.md](../docs/STATUS.md).
