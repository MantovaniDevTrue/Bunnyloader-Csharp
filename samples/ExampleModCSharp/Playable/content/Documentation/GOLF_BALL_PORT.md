# Bola de golfe: tradução do Bunny JS para C#

Fontes originais: `Content/Items/ExampleGolfBall.js` e
`Content/Projectiles/ExampleGolfBallProjectile.js`, do pacote Bunny JS 1.9.0.
Os dois PNGs foram copiados sem alterações. As classes C# ficam nos mesmos
caminhos de `Content`, como no tModLoader.

`ExampleGolfBall.SetDefaults` chama `DefaultToGolfBall` com o nome do
projétil. O helper usa os valores do `Item.DefaultToGolfBall` do Terraria
Android 301720: dimensões 18 × 20, uso de 15 ticks, velocidade 12,
raridade Green, valor interno 10000 e SoundID.Item1. A bola é equipável,
não consumível e não causa dano de contato como arma na mão.

O projétil mantém a IA nativa GolfBall (149), dimensões 7 × 7,
`netImportant`, penetração ilimitada e `tileCollide = false`, como no JS.
A simulação de golfe continua sendo executada pelo Terraria.

`SetStaticDefaults` declara `IsAGolfBall`, `TrailingMode` e
`TrailCacheLength`. O compilador executa esse hook depois dos defaults do
protótipo, seguindo `ModProjectile.SetupContent` do tModLoader. O host
instala as tabelas na thread do jogo antes da consulta de equipamentos e
antes do `Projectile.SetDefaults` original: esse método cria os arrays do
histórico usando `TrailCacheLength`. Configurá-lo apenas depois da criação
do projétil deixaria o tamanho errado.

As políticas são publicadas como um lote validado. Campos escalares não
são lidos como ponteiros de array; tipos, capacidade e limites são
conferidos. Cada ID pertence a um mod. Ao remover/recarregar o mod, o host
retira seus defaults e restaura as células na thread do jogo, preservando
outros mods e alterações posteriores de outro consumidor.

Validação deste lote é de host: compilação real Roslyn do ExampleMod,
execução do hook C++ de produção, ordem da inicialização, isolamento,
recarga, retirada concorrente e sanitizadores. Isso não confirma a física,
a imagem nem a rede dentro do Terraria Android. Nenhum APK foi gerado;
a tradução completa e os demais loaders seguem pendentes.
