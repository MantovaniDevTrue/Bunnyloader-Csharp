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

Base estavel Android .90.28.4 preservada. Grupo .90.28.5: ver
[codigo, contratos e roteiro](csharp-lot2-item-behaviors.md). A matriz registra
199 arquivos e separa CI de Android. Proximo grupo obrigatorio: ExampleWorkbench,
receitas com estacao de mod e o conjunto Helmet/Breastplate/Leggings.
O Lote A/2 continua aberto; nenhum lote posterior foi abandonado.

Entrega .90.28.5 / ExampleMod 0.1.35 gerada pelo pipeline 38560108522,
commit de build `2d1012b7d067347688ea242827762b4dfe6f51b3`, com assinatura V3.
Todos os checks obrigatorios passaram. A matriz possui 199 arquivos: seis
novos ports validados em host, sem validacao Android nova. Nenhuma alteracao
funcional posterior ao commit de build nesta entrega.
