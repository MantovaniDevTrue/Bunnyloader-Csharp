# Registro de texturas de fundo C#

Base preservada: `5a18a67899c1f6ee88a34097be3adb6e7877edd4`, com as
correções de contagem de classes, referências Android, Dust e ItemUse.
Esta branch é source-only. Nenhum APK ou release é autorizado enquanto os
15 loaders de `csharp-loader-completion-15.json` não passarem pelo gate.

## Contrato implementado

`BackgroundTextureLoader` registra imagens e disponibiliza slots. Ele não
seleciona estilos nem desenha biomas: essas responsabilidades pertencem a
`SurfaceBackgroundLoader`, `UndergroundBackgroundLoader` e `SceneEffectLoader`,
contabilizados separadamente e ainda pendentes no programa de 15.

- Autoload determinístico dos PNGs em pastas `Backgrounds/`, com limites de
  4096 imagens, profundidade 16 e rejeição de symlinks.
- Registro explícito por classe `BunnyBackgroundTexture`, inclusive fora dessas
  pastas. Identidade por UID/caminho; deduplicação não depende da ordem JS/C#.
- `BunnyBackgroundTextures.GetSlot`, `TryGetSlot` e `GetSlot<T>` para resolver
  caminhos sem guardar números temporários em saves.
- `TextureAssets.Background`, `Main.backgroundWidth` e `backgroundHeight`
  recebem referências/dimensões reais, sem tocar nas máscaras vanilla.
- Tipos/ranks/storage e tamanhos de arrays são validados antes de escrever.
  As três novas tabelas são preparadas e retidas pelo GC antes de qualquer
  publicação; falha de alocação preserva todas as tabelas anteriores.
- Criação/destruição de texturas somente na thread Unity observada pelo núcleo.
  A consulta C#/PInvoke é somente leitura e pode ocorrer no worker.
- Falha de um asset preserva os outros mods. O estado original da falha é
  mantido por asset; a aplicação de defaults recusa um mod com asset falho.
- Descarregamento limpa referências/dimensões próprias e libera os assets,
  preservando outros mods. Slots não são reutilizados durante o processo.

O caminho JavaScript usa o mesmo registro nativo. Get/TryGet, autoload,
registro antecipado e consulta de imagens C# continuam disponíveis. C# não
inicializa QuickJS para registrar, publicar ou consultar os próprios fundos.

## Exemplo e reutilização

`Common/Backgrounds/ExampleBackgroundTexture.cs` declara a imagem original
`ExampleBiomeSurfaceFar`. Os outros nove PNGs originais do arquivo enviado
são incluídos e descobertos automaticamente. O comando `/examplebackground`
consulta o recurso; `/examplebackground Assets/Textures/Backgrounds/ExampleBiomeSurfaceMid0`
consulta outro PNG da mesma coleção. Os comentários dos fontes explicam UID,
caminhos, carga, slots e a diferença entre uma textura e o estilo que a usa.

`BunnyCommandContext.ModId` fornece a identidade real do registro ao comando;
a consulta não depende do UID conhecido do ExampleMod nem de um slug fixo.
Este comando testa o registro/asset, não substitui a validação visual dos
estilos de fundo, que pertencem aos loaders seguintes.

## Evidência e limites

`tests/backgrounds/run.sh` executa:

1. O serviço C++ de produção com metadata IL2CPP controlada: reserva JS/C#,
   dimensões, GC, autoload, rejeição de tipos, rollback de alocação, thread,
   isolamento de falha, descarregamento e nova reserva.
2. C# real chamando o serviço de produção por P/Invoke numa biblioteca host.
   Executa a classe e o comando do ExampleMod e verifica UID e mensagens.
3. O JavaScript original adaptado sendo executado pelo Node, com autoload,
   lookup compartilhado, total, deduplicação e bloqueio de registro tardio.

`tests/ready_prs/managed_source_class_count_test.cpp` compara o inventário
nativo do ExampleMod com o total do verificador gerenciado. A seleção de bases
usa o mesmo mapa de domínios nativo para que adicionar uma categoria não a
omita silenciosamente do parser. Diagnósticos do verificador registram o
mesmo total devolvido ao host.

Compilação de biblioteca ARM64, testes de host e execução dos defaults não
certificam renderização, estabilidade ou FPS no Xiaomi. A validação Android
continua pendente e não existe APK deste lote. O gate não trata a lista de
contratos como prova automática de fidelidade: a revisão e os testes do
comportamento original continuam obrigatórios.
