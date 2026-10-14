# Plano global de conclusao do ExampleMod C#

## Referencias e regra de conclusao

Base preservada: Bunny .90.27.6 / ExampleMod 0.1.33, confirmada no Android pelo
usuario para os equipamentos nativos. Original atualizado: JonataOliveiraa/
Bunny-Loader, commit 5b65929a590ff16278a6a645cb322d76360fb547. O inventario e de
192 arquivos JS (178 no fork; 14 adicionados pelo upstream). A referencia antiga
de 169 arquivos nao limita o trabalho. Cada arquivo, inclusive helpers, main,
configs e interfaces, tem uma entrada na [matriz](../Split/Bunny-Loader/docs/generated/example_completion/matrix.md)
e dados completos em [JSON](../Split/Bunny-Loader/docs/generated/example_completion/matrix.json).

Estado implementado exige comportamento original, assets e bridge reutilizavel.
Estado Validado exige evidencia de jogo para o exemplo inteiro, alem dos testes
automatizados aplicaveis. O efeito isolado de Shield/Shoes validado nao equivale
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

- SourceParser/SourceLoader reconhecem apenas oito bases C# especializadas.
  Faltam contratos gerais de ModItem, ModSystem, Tile/Wall, biome etc.
- Verify/SourceLoader exigem arma + projetil mesmo para mods que so precisam de
  material ou ferramenta. O lote de itens deve corrigir esse bloqueio do loader.
- Arquivo: 64 .cs, 256 KiB cada, 2 MiB totais; defaults wire de 8 KiB e limites
  de 16 por categoria/128 tipos. A conclusao de 192 JS em um unico pacote exige
  evolucao desses budgets com testes de limites, sem simplesmente remover guards.
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
