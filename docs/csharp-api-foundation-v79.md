# Base para a API C# e exemplos de modders

Revisão da branch `csharp-managed-heap-v79-experiment`, commit `d7ffddfb7dd5add8374f778bbc8fddd0f631b2a7`.

O projeto já compila fontes com Roslyn e executa callbacks no MonoVM. A documentação registra testes Android de dois mods (.75), pausa/retomada (.77) e cinco entradas em mundo (.78). A troca dos sinais Mono/IL2CPP já está implementada; não é necessário recomeçar essa investigação. Os testes da .79 acrescentam telemetria, não uma API geral de acesso ao Terraria.

## Correções nesta revisão

1. **Produtor atrasado atravessando pausa/retomada.** Comparar apenas o tipo armado permite que um produtor que leu o tipo antes da pausa enfileire um evento antigo depois da retomada. A fila agora valida uma identificação de sessão e uma geração por vínculo, capturadas antes de resolver o tipo. Pausar um vínculo invalida apenas suas admissões antigas; limpar/reiniciar a sessão invalida todas. Eventos de outro vínculo continuam aceitos. Um callback já iniciado não é cancelado por essa barreira.
2. **Orçamento de falhas contaminado por outro vínculo.** O limite do callback principal usava o total agregado de falhas. Três erros no vínculo secundário mais um no principal podiam encerrar a sessão inteira. O contador do principal agora é próprio e reinicia com a sessão. Os totais de diagnóstico continuam agregados. A política existente de encerrar a sessão após três falhas do próprio principal permanece; isto ainda não é isolamento simétrico completo entre mods.
3. **Documentação desatualizada.** A página SupportedAPI ainda dizia que dois mods não haviam sido validados e que a .74 aguardava gameplay. Ela agora referencia os resultados posteriores e separa callbacks no worker, políticas nativas e comandos de diagnóstico.

## Validação reproduzível

```sh
python3 Split/Bunny-Loader/tests/run_managed_lifecycle_regressions.py
```

A suíte cobre FIFO64, 25.000 eventos, múltiplos produtores, pausa por vínculo, produtor bloqueado durante pausa/retomada, reuso de tipo em outro mundo, invalidação por clear/reset, parser de memória e execução do dispatcher real contra uma biblioteca Mono simulada que injeta exceções. Reverter separadamente a validação da admissão ou o contador de falhas faz os respectivos testes falharem.

São testes host: não simulam Unity, o coletor Mono real ou os custos ARM64. Nenhuma validação nova no aparelho é alegada.

## Próxima API pública recomendada

Manter os exemplos existentes e estabilizar os seguintes contratos antes de prometer suporte geral a mods:

| Prioridade | Entrega | Critério verificável |
| --- | --- | --- |
| 1 | Registro de callbacks por identidade do mod e tipo, com lifecycle explícito | Testar load/world enter/world exit/pause/resume sem exigir nomes ou arquivos mágicos no tutorial |
| 2 | Contextos tipados e imutáveis para hit/kill | Expor identidade estável do evento, sessão e entidades; documentar worker versus thread do jogo |
| 3 | Comandos de efeitos aplicados na thread do jogo | Validar sessão e identidade da entidade antes de aplicar; rejeitar comandos de mundo anterior |
| 4 | Limites apropriados para sessões longas | Substituir o teto experimental por política explícita de capacidade, tempo e falhas por mod, com testes de sobrecarga |
| 5 | Exemplos pequenos com testes automáticos | Cada exemplo demonstra uma API efetivamente implementada e compila contra a versão declarada do SDK |

Não passar ponteiros IL2CPP ao C# do worker nem tratar o retorno `int` dos callbacks atuais como uma alteração direta de gameplay. Não renomear a API atual como compatível com tModLoader: os contratos e a base de execução são diferentes.
