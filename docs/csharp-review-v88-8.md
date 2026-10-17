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

Estado antes de CI: testes locais aprovados, Android build pendente. Execução no Xiaomi, estabilidade, custo de carregamento e 60 FPS aguardam teste real. Não há relato de defeito explícito do dispositivo nesta revisão; não alegar validação de gameplay.

## Próxima execução

1. Conferir o resultado da build .88.8 e dos testes independentes. Corrigir falhas antes da publicação privada.
2. No Android, testar Lights sozinho e com ExampleMod: dia/noite, descer até não haver luz solar, retornar à superfície, salvar/sair/entrar e combate. Conferir relatório ScreenFx, dados parciais IL2CPP e FPS; não utilizar apenas um item vanilla como prova.
3. Medir a primeira compilação e o cache separadamente; não substituir o alvo de 5 s por um timeout menor.
4. Continuar a expansão das APIs e ports após a auditoria da referência oficial do ExampleMod tModLoader, mantendo separada a matriz existente do ExampleModBunny. Um nome de classe coincidente não é evidência de port funcional.
