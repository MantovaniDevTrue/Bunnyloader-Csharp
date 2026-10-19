# Probes visuais de tiles C# — Android

Estes dois blocos de teste usam os PNGs originais do ExampleModBunny, sem editar os assets. Não são ports funcionais dos móveis inteiros: o registro público de `BunnyTileShape` aceita somente `Single` e `Style2x1`, enquanto a fogueira original é 3×2 e a luminária original é 1×3.

O pacote contém `ExampleCampfireVisualProbe` e `ExampleLampVisualProbe`, ambos em 1×1, e dois itens colocáveis com receitas: um `ExampleItem` e uma bancada vanilla (tile 18) para produzir cada item. Os probes são uma bancada de validação visual do `PostDraw`, do `DrawEffects`/`SpecialDraw`, de sprites espelhados e das propriedades nativas de `Dust`, não da mecânica de ativação por fios, da área do buff de fogueira nem do objeto composto.

## Roteiro de validação

1. Usar exclusivamente um APK experimental que contenha o commit desta PR, quando houver build de teste autorizada. Verificar que ExampleMod CSharp compila e carrega antes de entrar no mundo.
2. Criar os dois itens nas receitas da bancada vanilla, colocar um de cada lado a lado e observar chamas, iluminação, partículas e reflexo horizontal entre posições pares/ímpares.
3. Comparar os desenhos contra o ExampleModBunny com as mesmas texturas, observando que a geometria do objeto composto está intencionalmente ausente e que a trepidação usa offsets inteiros, não subpixel.
4. Verificar os efeitos em blocos isolados, grupos de quatro e grupos de doze, medindo FPS durante a aproximação, permanência e quebra. Repetir com um mod JavaScript de tiles ativo.
5. Validar pausa, retorno ao menu, nova entrada no mundo, retirada dos itens e persistência do tile. Guardar vídeo ou capturas com logs C# e FPS antes de classificar as funções como verificadas no Android.

**Critério de aprovação:** ausência de crash, desenho padrão preservado quando não houver overlay, efeitos visíveis quando válidos, partículas sem explosão de contagem, sem erros da ponte MonoVM e FPS sustentados perto de 60 no dispositivo usado para testes.

O CI verifica compilação, presença e identidade dos PNGs e chamadas à API já existente, mas não executa Terraria nem mede FPS. O port completo só pode começar após suporte nativo validado a formas 3×2 e 1×3, sprites/flicker com coordenadas fracionárias e interações equivalentes às originais. A PR #98 segue draft; não distribuir APK nem .bmod como release.
