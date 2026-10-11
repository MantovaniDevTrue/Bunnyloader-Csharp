# Roadmap C# — etapas propostas

Este é um plano de desenvolvimento, **não uma lista de funcionalidades já entregues**.

| Ordem | Etapa | Critério de avanço |
| --- | --- | --- |
| 1 — concluída | Validar **.62 auto live** no Android | **512 callbacks reais**, 0 falhas; jogo continuou funcional após o limite |
| 2 — em desenvolvimento | Avaliar no Android a .63 com fila FIFO64 e teto experimental de 25 mil | Medir FPS/latência/memória e eventos descartados; testar drenagem, sem alterar os efeitos ou bloquear a thread do jogo |
| 3 | Generalizar registro por mod/projétil com isolamento | Identidade correta, vários tipos sem interferência, descarregamento seguro |
| 4 | Ampliar o ExampleModCSharp por APIs confirmadas | Métodos pequenos, compilação automática, exemplos reproduzíveis e testes |
| 5 | Cobrir mais eventos gerenciados com segurança | Comparação entre nativo/precalculado e callback real; política de execução clara |
| 6 | Estudar sincronização multiplayer | Implementação e testes próprios; não ativar por suposição |
| 7 | Comparar desempenho QuickJS × MonoVM | Mesmo comportamento e carga, mesmas cenas, métricas FPS/p90/CPU/memória |
| 8 | Preparar publicação para usuários | API documentada, compatibilidade declarada por recurso, estabilidade demonstrada |

## Princípios

- Respeitar o [Bunny Loader original](https://github.com/JonataOliveiraa/Bunny-Loader) e seu modelo de execução; não apresentar o projeto como oficial.
- No Bunny Loader, usar **ExampleModBunny e os fontes reais do próprio loader como referência**, não código de tModLoader transplantado.
- Preservar fidelidade, recursos e efeitos sem sacrificar desempenho.
- Manter a orientação/mira do jogador sincronizada antes dos disparos quando aplicável.
- Separar rigorosamente resultados simulados, Android confirmado e hipóteses.
- Cada etapa deve ter teste que um desenvolvedor consiga reproduzir.
