# ExampleMod CSharp — cobertura rastreável

Gerado por `python3 tools/examplemod_coverage.py`. Fontes e evidências em `reference/` e `examplemod-evidence.json`.

**Trabalho em andamento. Não há paridade completa nem validação Android deste lote.**

Base: `fcdc3fd2c30d1e7432fddbafc4af9f6d65ca473a`, Bunny **1.0.11 Dust/Invasion QA**. Os 174 arquivos anexados eram idênticos à base; 98 fontes C# preservados.

FPS, memória e tempos no aparelho continuam `null`: não foram medidos. Relatos de versões antigas não validam esta versão.

O catálogo usa cada hook JS como unidade inicial de revisão, com hash do fonte. Os efeitos internos ainda precisam ser detalhados por lote. Coincidência de nome só sugere um candidato C#; não promove status. O inventário oficial tML inclui arquivos antigos, que exigem triagem de pertinência.

| Estado | Comportamentos |
|---|---:|
| não iniciado | 712 |
| implementado sem validar | 0 |
| validado automaticamente | 6 |
| validado no Android | 1 |
| falhou | 3 |
| bloqueio comprovado | 0 |

Inventários: 98 fontes C#, 206 fontes JS e 558 fontes oficiais tML. Esses números não medem conclusão.

Dependência encontrada: tooltips C# fornecem um plano nativo, mas `Exports.js` instala `TooltipLoader.js` para executar o comportamento. A independência completa de JS está pendente.

Dust e invasão: o baseline contém correções posteriores aos logs Android de falha de contrato/already-running; nenhuma nova validação em aparelho foi produzida aqui.

| Comportamento | Estado | Observação |
|---|---|---|
| requirement:tooltip-without-js | falhou | Dependência confirmada por inspeção de Exports.js/TooltipLoader.js; substituir por renderer nativo. |
| requirement:aim-before-shoot | validado automaticamente | Falha myPlayer do APK 311 corrigida no 312; usuário relatou armas funcionando. No lote 313, regressões de mira passam no host; nova opção do cajado ainda aguarda Android. |
| requirement:ExampleGun | validado automaticamente | Asserções dos valores/algoritmo do exemplo real e protocolo compilado no host. Trajetória, cano e consumo no motor Android pendentes. |
| requirement:ExampleRocketLauncher | validado automaticamente | Asserções dos valores/algoritmo do exemplo real e protocolo compilado no host. Trajetória, cano e consumo no motor Android pendentes. |
| requirement:ExampleShotgun | validado automaticamente | Asserções dos valores/algoritmo do exemplo real e protocolo compilado no host. Trajetória, cano e consumo no motor Android pendentes. |
| requirement:dust-emission-binding | validado automaticamente | Corrigido e executado no host; reteste Android do novo lote pendente. Ver SUMMON_DUST_INVASION_QA.md. |
| requirement:android-weapons-user-report | validado no Android | Usuário: As armas funcionou. Escopo: relato de funcionamento; contagem detalhada de pellets, multiplayer e FPS não informados. |
| requirement:summon-staff-reference | validado automaticamente | Corrigido e executado no host; reteste Android do novo lote pendente. Ver SUMMON_DUST_INVASION_QA.md. |
| requirement:android-dust-emission | falhou | Relatório 0.1.64: NewDustSignatureUnavailable. Correção do namespace Graphics.Color no novo lote ainda não confirmada em aparelho. |
| requirement:android-invasion-spawn | falhou | Running com SpawnFailed crescente e NoSafeSpawnSite; busca/diagnósticos ajustados, reteste pendente. |

Todos os hooks e o backlog oficial estão em [EXAMPLEMOD_COVERAGE.json](EXAMPLEMOD_COVERAGE.json).

Próximos lotes: tooltip nativo independente de JS; confirmação Android Dust/invasão; paridade de NPC/bosses/projéteis; tiles/entidades/persistência; players/globals/config/UI; multiplayer e integração/unload. Nenhum desses lotes é declarado completo por inventário ou compilação.
