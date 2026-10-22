# Auditoria do próximo loader: SurfaceBackgroundLoader

Referência preservada:
`app/src/main/cpp/script/js/mod/Loaders/SurfaceBackgroundLoader.js`,
SHA-256 `4538e3d819e0d0c6a0b5be387fd0d2b10b306e8d44c09c9ef71b1a56c620c2a0`.
Complementos: `ModSurfaceBackgroundStyle.js` e o
`Content/Biomes/ExampleSurfaceBackgroundStyle.js` do anexo 1.9.0.
**Estado: parcial na infraestrutura nativa**, sem API pública incompleta
anunciada como implementada. Componentes executáveis e limites registrados em
[csharp-surface-native-components.md](csharp-surface-native-components.md).

## Contratos que não podem ser omitidos

1. Ler `SurfaceBackgroundID.Count` real e reservar slots compartilhados por
   UID/classe, incluindo gaps de outras linguagens. Add/Get e autoload.
2. Wake/Sleep: sem seleção/fade de mod, não chamar Mono/JS seis vezes por
   desenho. Enquanto houver fade de qualquer estilo, continuar desenhando.
3. Crescer atomicamente as tabelas `bgAlphaFrontLayer`,
   `bgAlphaFarBackLayer`, `Sets.IsForest`, `Sets.IsDesertVariant`, preservando
   entradas vanilla/JS. O Android pode recriar `LocalUserGameState`; validar
   novamente as instâncias/arrays antes de escrever, sem resolver metadata
   a cada quadro.
4. Prioridade: BiomeLow substitui estilos 0/10/11/12; BiomeMedium também
   3/7; BiomeHigh e superiores ganham de qualquer estilo vanilla. Respeitar
   o tema do menu, que é separado da cena do jogador.
5. BackLayer deve executar `DrawBG_ModifyBGFarBackLayerAlpha` para o estilo
   escolhido; FrontLayer preserva o original e chama `ModifyFarFades` depois.
   Os dois argumentos nullable devem preservar ausência e zero: não são a
   mesma operação. Confirmar campos/layout/ABI dos genéricos fechados reais.
6. Desenhar Far após BackMountainsStep1 e Middle após Step2, usando os loops,
   posição, escala e transparência deixados pelo jogo. Textura ausente não
   interrompe outros estilos. Manter a cor modificada nativa correspondente.
7. Antes de GetFogPower, executar PreDrawCloseBackground e permitir que o
   autor suprima a camada ou desenhe a própria. Um bool fixo substituindo o
   callback não seria implementação deste contrato.
8. ChooseCloseTexture pode modificar scale/parallax/a/b. O padrão é
   1.25/0.37/1800/1750; escala final ×2; top = floor(step1.top×a+b)+scAdj+push.
   Preservar arredondamento, repetição, limite de superfície e
   `SkyManager.DrawToDepth(1/parallax)` do original.
9. Exemplo completo: imagens Far/Close e quatro Middle originais. A animação
   avança a cada 13 chamadas de ChooseMiddleTexture, inclusive resolução do
   último frame pelo caminho completo do mod. Não substituir por imagem fixa.
10. Unload, mudança de mundo/menu, dependência ausente, resposta atrasada e
    fade anterior devem liberar recursos com segurança e diagnóstico por mod.

## Pontos de ligação nativos encontrados

| Método original | Responsabilidade |
| --- | --- |
| `int GetPreferredBGStyleForPlayer()` | Preservar escolha original e arbitrar mod |
| `void UpdateBGVisibility_BackLayer(Nullable\`1 targetBiomeOverride, Nullable\`1 transitionAmountOverride)` | Fades do fundo e tema de menu |
| `void UpdateBGVisibility_FrontLayer(Nullable\`1 targetBiomeOverride, Nullable\`1 transitionAmountOverride)` | Callback gerenciado após original |
| `void DrawBG_ModifyBGFarBackLayerAlpha(int desiredBG, Nullable\`1 desiredBG2, Nullable\`1 transitionAmountOverride)` | Rotina nativa dos alphas |
| `void DrawSurfaceBG_BackMountainsStep1(float backgroundTopMagicNumber, float bgGlobalScaleMultiplier, int pushBGTopHack)` | Captura de geometria e camada Far |
| `void DrawSurfaceBG_BackMountainsStep2(int pushBGTopHack)` | Camada Middle |
| `float DrawSurfaceBG_GetFogPower()` | Camada Close antes do original |
| `SpriteBatch.Draw(Texture2D,Vector2,Nullable\`1,Color,float,Vector2,float,SpriteEffects,float)` | Desenho no lote já aberto |
| `SkyManager.DrawToDepth(SpriteBatch,float)` | Ordenação da camada Close |

As assinaturas acima vieram dos fontes, não de uma nova inspeção no celular.
Resolver a classe/cor pela assinatura real: o Android já expôs
`Microsoft.Xna.Framework.Graphics.Color`, e procurar somente Framework.Color
reintroduziria uma falha conhecida. Validar storage, retorno, parâmetros,
layout dos valores e referências antes de instalar qualquer hook.

## Infraestrutura disponível e trabalho seguinte

Reutilizar `BackgroundTextures`, `BackgroundAssetLifetime`, `CheckedProperty`,
`ValueLayout`, `NativeColorLayout`, a descoberta genérica de módulos com UID e
o protocolo de worker com revisão. `CheckedProperty` atual só oferece leituras
necessárias ao subsolo; superfície precisa contratos de Single/arrays Single
e setters verificados. Não expor esses setters antes de ligá-los ao runtime.

Os testes desses componentes agora executam metadata controlada, valores
nullable, arrays recriados, equações de desenho e o overload real de desenho
por `runtime_invoke` no fixture. O original JS executado no Node fornece a
comparação de geometria/fades/prioridade e o contrato da animação. Isto ainda
não é integração C#/QuickJS: faltam registro, hooks e dispatchers. Depois
executar o exemplo original no QuickJS real contra o serviço compartilhado e
o exemplo C# por P/Invoke. Testes Android deverão comparar
camadas/fades/animação do ExampleMod, menu e coexistência JS/C#, com medição
de quadros. Nenhum dos testes de subsolo comprova esses contratos novos.
