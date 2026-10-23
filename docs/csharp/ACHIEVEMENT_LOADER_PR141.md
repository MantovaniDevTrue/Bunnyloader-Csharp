# AchievementLoader C# — fase gerenciada, sub-PR da #141

Referência: ExampleModBunny 1.9.0, arquivos ModAchievement.js, AchievementLoader.js, ExampleBossKilled.js e ManyWormsKilled.js.

Implementado neste lote: classes C# nativas ao SDK, sete tipos de condição, callbacks dirigidos por evento, progressão, registro por UID e revisão de assembly, rollback de registro em falha, unload seletivo e snapshot/restauração de progresso com verificação de compatibilidade. Teste host cobre dois UIDs, progresso do chefe e de cinco vermes, condição composta e recargas.

Ainda falta integrar este núcleo com a ponte MonoVM e o Terraria IL2CPP, incluindo o manager de conquistas, localização, ícones de 592 x 64, salvamento real, hooks de gameplay, notificação na UI e validação Android. Portanto este commit NÃO representa loader concluído em jogo, nem libera APK. Não altera LocalizationLoader, RarityLoader ou AnglerQuestLoader da PR #141.
