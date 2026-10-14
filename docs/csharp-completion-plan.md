# Plano global de conclusao do ExampleMod C#

## Referencias e regra de conclusao

Base estavel atual: Bunny .90.28.4 / ExampleMod 0.1.34, relatada pelo usuario
como funcional no Android. A referencia historica .90.27.6 / 0.1.33 dos
equipamentos nativos tambem foi preservada. Original atualizado: JonataOliveiraa/
Bunny-Loader, commit bc4f240082a589823536eff74462a11acbdba8ec. O inventario e de
199 arquivos JS (178 no fork; 21 adicionados pelo upstream). A referencia antiga
de 169 arquivos nao limita o trabalho. Cada arquivo, inclusive helpers, main,
configs e interfaces, tem uma entrada na [matriz](../Split/Bunny-Loader/docs/generated/example_completion/matrix.md)
e dados completos em [JSON](../Split/Bunny-Loader/docs/generated/example_completion/matrix.json).

Estado implementado exige comportamento original, assets e bridge reutilizavel.
Estado Validado no Android exige evidencia de jogo do exemplo inteiro;
Validado por testes automatizados registra apenas os contratos exercitados no host. O efeito isolado de Shield/Shoes validado nao equivale
a portar dash, voo, waterWalk, pesca ou outras mecanicas do arquivo JS.
Ausencia de API C# nao e prova de limitacao tecnica do Terraria.

## Lotes e dependencias (nenhuma categoria excluida)

1. **Auditoria:** inventario canonico, classes/hooks/campos/assinaturas/imports,
   comparacao do core original/fork, exemplos adicionais C#, estados e evidencias.
2. **Itens/atributos/equipamento/armadura:** base geral de itens e receitas;
   ferramentas, materiais, consumiveis, ammo, raridades, prefixos completos;
   slots/sets/animacoes; atributos, dash, voo e efeitos de cada acessorio; conjunto
   de armadura e cosmeticos. Preservar Guardian/Trailblazer/Mystic atuais.
3. **Armas/projeteis:** DamageClass e mana/ammo; melee/ranged/magic/summon;
   lancas/chicotes/ioios/flails/espada curta, beams/lasers/holdouts, minions,
   sentries e tiros, animacao/collisions/hitboxes/efeitos e mira. IA fiel.
4. **NPCs/bosses:** estado/IA/animacao/spawn/loot/GlobalNPC; town NPC, shops,
   moedas customizadas, boss real/fases/ataques/barra/invocacao/progressao/save.
5. **Tiles/moveis/mundo:** todos os 37 tiles, 36 itens colocaveis, paredes;
   frames, portas/camas/chests/dressers/clocks/storage/drops/light/glow/plantas;
   banners/pylons/arvores/tile entities, trofeus/reliquias/music boxes/armadilhas.
6. **Biomas/ambiente:** contagem/deteccao, surface/underground/background,
   agua/cachoeiras/nuvens/music/condicoes. Reusar mecanismos do loader original.
7. **Especiais:** pesca/quest fish/bobbers/hooks/grappling; pets/light pets,
   montarias/minecarts, conquistas/emotes/hairs, commands/configs/UI/modmenu,
   systems globais, persistencia, audio e todos os helpers/main ainda pendentes.
8. **Auditoria final:** comparar novamente cada arquivo, SDK publico e bridge;
   preencher lacunas, testar convivencia JS/C#, multiplos mods, loadouts,
   Save/Exit/reentrada, morte/respawn, assets e desempenho no dispositivo.

A dependencia itens/receitas antecede armaduras e armas; pets/minions dependem
de projeteis e buffs; town shops dependem de itens/moedas; bosses dependem de
NPC/projetil/persistencia. A ordem interna pode mudar por essas dependencias,
mas todos os arquivos permanecem no backlog numerado na matriz.

## Lacunas da arquitetura identificadas antes do lote seguinte

- SourceParser/SourceLoader agora reconhecem BunnyItem alem das bases especializadas.
  Ainda faltam contratos gerais de ModSystem, Tile/Wall, biome e outros sistemas.
- Mods somente de BunnyItem ja carregam sem arma/projetil. A faixa de verificacao
  Mono foi ampliada de 2..32 para 1..1024 com testes de fronteira; preservar esse contrato.
- Arquivo atualizado no grupo .90.28.5: 512 .cs, 256 KiB cada, 16 MiB totais; defaults
  wire de 2 MiB, 256 por categoria/1024 tipos. Contratos de fronteira continuam
  obrigatorios; slots e filas nativas tem limites proprios.
- SetDefaults e politica precomputada nao substituem callbacks de jogo; cada
  hook novo precisa de contexto, caminho runtime e teste de comportamento.
- O pipeline JS original ja tem receitas, equipment, tiles e sistemas; priorizar
  reutilizacao no host em vez de duplicar logica de Terraria dentro do MonoVM.

## Entrega e evidencia por lote

Codigo-fonte publico + .bmod com fontes/guia/assets + APK no Builds privado.
CI gera/conferir matriz usando checkout upstream PINADO e executa regressao.
Relatorio deve distinguir contrato/bridge testado no host, compilacao Android,
execucao real no aparelho e FPS medido. O alvo e 60 FPS; nenhuma captura ou
compilacao isolada prova FPS sustentado. Shaders customizados estao fora desta etapa.

Ao retomar: ler matriz JSON, evidencias e este plano; verificar HEAD e atualizar
o inventario se houver novos exemplos upstream. Nunca promover estados de forma
automatica so porque o build passou. Roteiros Android acompanham cada lote.

## Ponto atual para retomar

Base validada: .90.28.7 / ExampleMod 0.1.35, incluindo mascara e robe confirmados
pelo usuario. Branch dessa base: codex/example-completion-v90-28; preservar seus
commits. Branch de trabalho do Lote A completo: codex/lot-a-complete-v90-29.

Escopo fechado e verificavel: 33 arquivos em lot-a-scope.json; inventario global
199 JS. Nao publicar novo lote por partes nem passar a B antes de concluir A.

Infraestrutura ja implementada na branch de trabalho: defesa nativa, UpdateEquip
com imunidade/mana/minions/atributos, ArmorSetBonuses com tooltip e bonus real,
receitas em estacoes de mod por UID/nome (inclusive tiles JS), wingSlot/WingStats/
VerticalWingSpeeds no WingLoader original, diagnosticos por receita e round-robin
nas filas de registros pendentes. APIs sao publicas e testes usam fontes C# de
mods independentes. Isso ainda nao e port dos exemplos finais.

Proxima dependencia concreta: BunnyTile e bridge para ExampleWorkbench original,
com TileObjectData 2x1, flags de mesa/estacao, drop, mapa, frames e AdjTiles; depois
port integral Helmet/Breastplate/Leggings com texturas e receitas originais.
Seguir todos os 33 arquivos e dependencias, inclusive Costume, asas/acessorios,
consumiveis, fishing/tools/projeteis, prefixos/raridades e tooltips/currency.
Checkpoint testado: runtime e466ae4c4fc1097b67ad0d0c6460a36e4fb66b8b,
run Android 38573406719, oito suites aprovadas.
Ver [contratos e continuidade](csharp-lot-a-development.md). CI compila Android,
mas nao publica APK desta branch de lote incompleto. Nenhuma nova alegacao de
validacao no Android ou 60 FPS para as APIs em desenvolvimento.

## Historico das correcoes preservadas

### Regressao Android .90.28.5 / correcao .90.28.6

ExampleBeard foi recusado no registro real por Texture sem extensao. A correcao
normaliza o caminho no registrador e elimina rejeicoes silenciosas no preflight.
Teste novo cruza fontes reais, parser e o contrato de caminho do registrador;
o CI tambem verifica os assets. Mantidos .90.28.4 como base estavel, todos os
199 exemplos e o backlog do Lote 2. Validacao Android da correcao pendente.

Build corretiva .90.28.6 aprovada no run `38563527063`, commit de runtime
`cec309b023f4092723d29c43567865c3ebb2fe33`, ExampleMod 0.1.35. Oito suites
independentes passaram. Proximo passo: confirmar registro no Android; o proximo
grupo de implementacao continua Workbench/receitas e armaduras.

## Regra de entrega atualizada pelo usuario

Lotes A, B, C e seguintes sao entregas integrais de suas categorias: todos os
exemplos contabilizados na matriz, com suas funcionalidades originais aplicaveis.
Grupos por dependencia servem para organizar commits e testes internos, mas nao
substituem o lote completo nem sao anunciados como uma entrega de novos exemplos.
Nao passar ao lote seguinte enquanto o atual estiver incompleto. Bloqueios reais
exigem evidencia e permanecem na matriz; nunca contam como exemplos funcionais.
Correcoes de regressao dos exemplos ja entregues podem gerar APK corretivo,
identificado explicitamente como correcao, sem declarar o lote concluido.

### Correcao .90.28.7: equipamento de vaidade

Usuario confirmou carregamento sem erros na .90.28.6, mas ExampleBossMask e
ExampleRobe nao equipam. Slots atribuídos apenas no host nao eram persistidos
no caminho nativo de SetDefaults dos BunnyItem. A bridge agora liga o slot ao
registro do item e o reaplica em novas instancias, validando campo/tipo/largura
e limite antes da escrita. O Lote A permanece aberto e precisa ser entregue
com TODOS os seus exemplos, nao apenas os seis da entrega anterior.

Correcao .90.28.7: pipeline `38566058511` aprovado, runtime commit
`e50e613f649e8806f8a82c919859dbc4c60309db`; oito suites de regressao passaram,
APK assinado e ExampleMod 0.1.35 gerados. Evidencia de CI final registrada apos
o build; o pacote inclui a matriz/roteiro do commit de runtime. Validacao
Android da mascara e do robe pendente. Lote A permanece incompleto.

## Continuacao integral do Lote A

Usuario confirmou .90.28.7 funcional. Desenvolvimento em
`codex/lot-a-complete-v90-29`, derivada da branch validada sem alterar a release.
Escopo completo: 33 arquivos, em lot-a-scope.json. Ver
[codigo, contratos e continuidade](csharp-lot-a-development.md).
Infraestrutura de armadura testada por mod de contrato independente; exemplos
ainda nao portados nao recebem promocao na matriz. Nao publicar entrega parcial.
