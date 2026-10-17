# Bunny C# .88.8 — revisão do Lights e build

Base preservada: `b232de77f25396b84ad82a320cb67e719767b99d` da branch `hotfix/csharp-88-5-sigabrt-logcat-tail`, incluindo os commits concorrentes que adicionaram o workflow dedicado .88.7 e a documentação de desempenho. O novo trabalho fica em `fix/csharp-88-8-lights-review`; não reescreve a branch existente.

## Problemas comprovados no código

- O dispatch da .88.7 falhou com HTTP 422: `Exceeded max expression length 21000`, linha 2123. Listas de branches foram extraídas para uma política Python testada, com outputs curtos nos ifs. Todos os históricos autorizados foram preservados. Um wrapper pequeno e habilitado .88.8 reutiliza o workflow principal via workflow_call, sem duplicar o pipeline Android.
- ScreenScene lia alguns campos nativos diretamente em bool/int/Vector2 sem validar tipo e largura. Os contratos agora exigem o tipo real, class_value_size, campo estático, offset suportado e layout X/Y ou x/y. Float e Double continuam suportados para time/worldSurface, sempre na largura correta. Campo ausente usa fallback; incompatibilidade gera WARN específico no bind, sem formatação por frame. Isso corrige um risco de cópia fora do buffer, não comprova a causa de algum crash Android anterior.
- O decaimento exponencial de sunVisibility podia permanecer positivo e impedir os shaders de usar `<= 0` no fallback nativo. A revisão preserva a transição e zera apenas o resíduo menor que 0,000001 quando o alvo é zero. O teste confirma suavidade, término em zero e retorno ao emergir.

## Escopo e preservação

Bunny `0.1.10.16.90.29.88.8-csharp-scene-contracts`, versionCode 259. Lights And Shadows 0.4.2.3 e ExampleMod CSharp 0.1.47 permanecem compatíveis; não há mudança nos GLSL, nos nove passes, quatro alvos ou assets do Lights. MonoVM, compilação automática, JavaScript, sinais corrigidos, equipamentos, prefixos e saves continuam preservados. Não foram adicionados novos shaders.

## Verificações e entrega

Testes locais executados: política de build para todos os refs/eventos, forks recusados, outputs reais da CLI e limite de expressões; contrato nativo com tipos e tamanhos incorretos, flags, offsets inválidos, Vector2 XNA/Unity, layout invertido e canário de memória; transição dia/noite e subsolo. A entrega exige os oito workflows de regressão, a suíte ScreenFx e a build Android, com assinatura e repositório privado do fluxo existente.

Resultado final: [build Android 39077778658](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39077778658) aprovada no commit `af844936dfa0b9435388723d02724daa5024985f`, com compilação ARM64, assinatura V3, validação do APK e entrega privada experimental concluídas. As oito suítes independentes e a suíte ScreenFx passaram no mesmo SHA. O commit de documentação posterior não altera o runtime nem os pacotes.

Execução no Xiaomi, estabilidade, custo de primeira compilação e 60 FPS aguardam teste real. Não há relato de defeito explícito do dispositivo nesta revisão; não alegar validação de gameplay. Esta é uma build corretiva experimental, não a conclusão do Lote A.

Os seis shaders existentes também compilaram e ligaram em OpenGL ES 3.2 Mesa/llvmpipe; três caminhos neutros tiveram pixels RGBA verificados. O [script e resultado reproduzível](validation/lights-host-gles.md) indicam exatamente a cobertura e as limitações. Esse teste não executa a cadeia completa de framebuffer do Unity nem mede FPS.

## Próxima execução

1. Código .88.8, nove suítes e entrega experimental aprovados; registrar o relatório Android antes de promover a build como validada no dispositivo.
2. No Android, testar Lights sozinho e com ExampleMod: dia/noite, descer até não haver luz solar, retornar à superfície, salvar/sair/entrar e combate. Conferir relatório ScreenFx, dados parciais IL2CPP e FPS; não utilizar apenas um item vanilla como prova.
3. Medir a primeira compilação e o cache separadamente; não substituir o alvo de 5 s por um timeout menor.
4. Continuar a expansão das APIs e ports após a auditoria da referência oficial do ExampleMod tModLoader, mantendo separada a matriz existente do ExampleModBunny. Um nome de classe coincidente não é evidência de port funcional.

## Evidências de CI e artefatos

- Código testado: `af844936dfa0b9435388723d02724daa5024985f`, [PR 84](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/84). O commit concorrente `267d1369873ce68ba8687030d752f5e4a7460034` foi preservado como segundo pai. O ajuste posterior `0e4762d9239dc2c7d1ecd87cfe5abb13638bf089` no workflow dedicado .88.7 permanece intacto na branch original; este PR não modifica esse arquivo em relação ao ancestral comum.
- Suítes aprovadas: combate `39077781482`; fila visual gerenciada `39077781488`; fila visual nativa `39077781495`; sinais `39077781565`; ScreenFx `39077781517`; GC `39077781507`; lifecycle `39077781532`; auditoria ExampleMod `39077781490`; SDK `39077781487`.
- O pipeline principal executou, entre outros, o compilador de fontes, regressões de itens/prefixos/equipamentos, serviço nativo de conteúdo/receitas sem QuickJS, roteamento entre mods, hooks tipados, cache e AOT, antes da compilação/assinatura Android. O sucesso desses testes não cobre todo o comportamento do Terraria.
- Entrega privada experimental: tag `csharp-v3-run-39077778658-a1`. APK `BunnyLoader-CSharp-0.1.10.16.90.29.88.8-csharp-scene-contracts-SignedV3.apk` (241.416.481 bytes), ExampleMod `ExampleModCSharp-v0.1.47.bmod` e Lights `LightsAndShadowsCSharp-v0.4.2.3.bmod`; arquivos SHA-256 e resultados JSON acompanham a entrega. Nenhum runtime Terraria foi anexado aos artefatos públicos de fonte.
- Próximo escopo ampliado: [inventário oficial do ExampleMod](tmodloader-example-inventory.md), 449 fontes ativos, 112 legados e assets/dependência contabilizados. Nenhum port foi promovido por gerar o inventário. O Lote A e a matriz JavaScript continuam preservados.
