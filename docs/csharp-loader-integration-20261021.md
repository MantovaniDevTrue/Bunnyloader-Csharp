# Continuidade dos loaders C# — 2026-10-21

Branch: `feat/csharp-loader-completion-15-20261021`.
Base: `5a18a67899c1f6ee88a34097be3adb6e7877edd4`.
O trabalho preserva as correções da integração anterior, inclusive contagem
de classes, referências Android, tooltip, Dust, ItemUse e receitas.

## Regra de entrega

**Não gerar APK.** A lista de quinze contratos originais está em
`csharp-loader-completion-15.json`. O gate bloqueia tarefas de empacotamento e
publicação enquanto houver pendências. A integração do SystemUpdate abaixo
conclui trabalho existente da PR #126; não transforma seus dezenove callbacks
em dezenove loaders e não altera artificialmente a contagem da lista.

## Integração de SystemUpdateLoader

Foi incorporada a PR #126, commit `3451b149`, mantendo seu histórico.
O SDK oferece os dezenove callbacks do `SystemUpdateHooks.js` original.
Os estágios nativos usam âncoras verificadas por quatro opcodes, preservam
registradores gerais, SIMD e flags e mantêm a rotina original do jogo.

Correções adicionais da integração:

- Um assembly contendo apenas `BunnyWorldUpdateLoader`, inclusive com uma
  classe-base intermediária própria, agora é reconhecido sem item auxiliar.
- O verificador gerenciado publica inventário `HOOK_MODULES`; o status do
  worker transporta a quantidade e o host exige correspondência exata entre
  conteúdo nativo e módulos gerenciados. Não foi removida a validação de IDs.
- A descoberta antecipada não interpreta métodos C#: literais de caractere,
  strings verbatim/interpoladas/raw e pontuação são preservados para Roslyn.
  Recursos que precisam de reserva antecipada ainda exigem caminhos literais
  ou `BunnyContent`; isso permanece uma limitação explícita do bootstrap.
- O cache passa a `roslyn-net8-source-v54-managed-module-inventory`.
- O autoload de imagens de fundo também ocorre em mods sem itens.
- `gameMenu` exige Boolean estático de um byte. Campos de taxas exigem
  Single/4, Double/8 ou Int32/4 estáticos e mutáveis. Métodos confirmam o
  armazenamento estático/instância antes de instalar os respectivos hooks.
- A publicação das três taxas é transacional por sessão. Timeout, descarte
  ou troca de quadro impedem uma resposta antiga de sobrescrever a nova.
- `ExampleWorldUpdateCounter` fica ativo e demonstra dois callbacks reais;
  os exemplos que alteram taxas/atualizações globais continuam com opt-in.

## Evidência local

| Verificação executada | Resultado |
| --- | --- |
| `tools/ci/test_ready_prs.py` | 65 verificações aprovadas |
| `tests/world_update_loader/WorldUpdateLoader.csproj` | 115 verificações, incluindo Roslyn real, descoberta de módulo independente, execução e unload |
| `native_contract_test.cpp` | Rejeição de tipo, tamanho, storage, ABI e metadata incompatíveis sem ler/escrever valores |
| `time_rate_mailbox_test.cpp` | Sessões antigas, valores inválidos e 20.000 ciclos com worker concorrente |
| `FullExampleRuntime` | 84 tipos verificados nos modos CLR completo, referências compactas Android, fallback e CLR Android empacotado |
| `tests/ready_prs/run.sh` | Bootstrap de 103 fontes, 52 registros nativos, inventário compatível, Dust e HUD de invasão |
| `tests/backgrounds/run.sh` | Serviço nativo de produção, P/Invoke C# real e loader JS |
| `tests/loader_delivery` | Quatro testes; escopo incompleto permanece bloqueado |
| C++ `SourceLoader`, `AutoSourceRuntime`, `ManagedRuntime`, `MonoWorkerJni`, estágios | Sintaxe host aprovada; ARM64/linkagem serão verificados no workflow sem APK |

Esses resultados não comprovam hooks, renderização, estabilidade ou FPS no
Android. A montagem das instruções ARM64 e a linkagem NDK têm passos próprios
no workflow source-only; registrar o SHA e o resultado do CI quando terminar.

## Ponto de retomada

1. Confirmar o CI source-only desta branch; corrigir a integração antes de
   anunciar qualquer grupo como pronto.
2. Revisar a PR #121 (Gore) e adaptar/testar seu serviço nativo contra a base
   atual, sem substituir DLLs do SDK por binários antigos.
3. Continuar estilos de fundos sobre o registro compartilhado já implementado.
4. Preservar os 206 arquivos JS do inventário do ExampleMod 1.9.0 enviado.
5. Atualizar contratos individualmente com evidências. Os outros quatorze
   loaders da lista continuam pendentes; não existe entrega dos quinze.

Nenhum APK foi gerado e nenhuma validação no aparelho foi atribuída a este
trabalho. As versões publicadas permanecem inalteradas.
