# ExampleMod C# 0.1.39 — correcao de assets do pacote

O relatorio Android da .90.29.23 identifica ExampleDefenseBuff.png ausente
no .bmod 0.1.38. A falta interrompe o registro e os prefixos subsequentes
nao recebem IDs; avisos prefix=-1 e itens desconhecidos sao consequencias.
Nao ha evidencia de crash novo na sessao atual desse relatorio.

A entrega anterior pelo workflow de auditoria omitia seis texturas. A build
de APK copiava essas imagens manualmente, mas o pacote agora e autocontido:
SourceBolt, SourceWand, ExampleDefenseBuff, ExampleBuffPotion,
ExampleFocusAccessory e ExampleSlimeNPC estao versionados em Playable.
UIDs, fontes C#, IDs persistidos e codigo nativo nao mudaram.

Os dois workflows verificam o ZIP final antes de disponibiliza-lo, incluindo
texturas calculadas pela execucao dos defaults compilados, visuais, slots de
equipamento e highlight. O validador confere existencia/case, estrutura e CRCs
dos PNGs. Os testes removem individualmente os seis assets reais do ExampleMod
e confirmam a recusa, alem de corromper PNG e remover a folha das asas.
Isso testa empacotamento, nao renderizacao ou gameplay Android.

Importar ExampleMod 0.1.39 com o mesmo UID, encerrar o jogo completamente e
reabrir. Confirmar versao no relatorio, ausencia de AssetMissing, registro
do ExampleDefenseBuff e dos dois prefixos. Conferir ferramentas/acessorios
salvos e ExampleWorkbench/receitas. Nao apagar personagem, mundo ou arquivos
auxiliares para tentar reparar a falta de uma imagem no mod.

Nao requer APK novo para esta correcao: o problema e o conteudo do .bmod.
