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

## Integração adicional de GoreLoader

A PR #121 foi integrada sobre a base atual, com o código explicado em
`docs/csharp/GORE_LOADER_IMPLEMENTATION.md`. Um NPC próprio, **Example Gore Slime**,
exercita a API; não foi necessário alterar os exemplos de invasões/armas.
O serviço de produção recebeu testes de transação, contratos IL2CPP, GC,
layout de Vector2, configuração de floats e P/Invoke com dois UIDs reais.
O inventário do ExampleMod passa a 85 tipos gerenciados (75 entradas do
contrato de conteúdo + 10 módulos de hooks) e 104 arquivos C# (53 registros de bootstrap verificados).

A revisão com `script/bridge/Bindings.cpp` também corrigiu o contrato de
`field_static_set_value` do novo serviço de fundos: referências passam o
objeto diretamente; valores numéricos passam endereço. O fixture anterior
reproduzia a chamada incorreta e foi corrigido. A aprovação anterior do CI
não era prova desse contrato no aparelho. Nenhum APK com esta mudança foi gerado.

Os testes de host agora têm diretório `obj` por projeto. Vários executáveis
na mesma pasta podiam reaproveitar o apphost de outro teste. Os arquivos de
persistência das invasões também são isolados por processo, conservando o
save/reload real dentro de cada execução.

CI anterior à integração Gore: [39673760844](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39673760844),
commit `34247fd5`, quatro jobs aprovados, incluindo ARM64/linkagem sem APK.
As mudanças posteriores exigem CI próprio; não herdam essa aprovação.

Regressão combinada após a integração Gore: 65/65 verificações locais aprovadas.
O teste JS original também confirma que o registro de Gore preserva os assets e
valores C# anteriores. O fixture nativo confirma a direção inversa: reconstruir
as tabelas C# preserva uma entrada externa adicionada depois.
O ExampleMod está em **0.1.64 nos fontes**, sem pacote de entrega ou APK.
