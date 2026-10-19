# Áudio C# — desenvolvimento do port integral

Este fonte demonstra a API de áudio nativa; o conjunto completo de exemplos ainda está em desenvolvimento e não há nova entrega parcial.

```csharp
// Em SetDefaults de BunnyItem ou BunnyWeapon. O arquivo pertence ao próprio mod.
CustomUseSound = new BunnySoundStyle("Assets/Sounds/Items/Guns/ExampleGun.ogg",
    volume: 0.9f, pitchVariance: 0.2f, maxInstances: 3);
```

O host confere o arquivo dentro de content/, registra o estilo no serviço Android compartilhado e cria o LegacySoundStyle do item. O marker permanece protegido pelo GC. Volume0..4, pitch-1..1, variação0..2, máximo0..256 (0 sem limite por estilo; a fila global continua limitada). ReplaceOldest substitui o mais antigo; IgnoreNew preserva as instâncias existentes. As definições são snapshots de SetDefaults, não ações por frame.

Use ExampleGun com ExampleBullet para conferir munição, dano, som, variação e offset(-18,0). Alterne lados e mire para cima/baixo; pause/retorne e teste junto de mod JS de áudio. Compare o asset original, limite de sobreposição e relatórios. O teste de host verifica contratos e transporte; não prova reprodução/decodificação no Android. Preservar os outros exemplos e os206 arquivos da matriz.

Configurações de som registradas são imutáveis durante a sessão: repetir a mesma definição reutiliza o registro; mudar asset/opções requer reiniciar o jogo. A recusa explícita evita reutilizar um style ID ainda referenciado por itens antigos. Não é suporte a hot reload de áudio.

O cache nativo reutiliza o asset por caminho canônico e o marker por arquivo/opções; conteúdos que compartilham a mesma configuração compartilham também o limite de instâncias, como no Bunny original. O cache de markers é fraco, sem reter objetos de lotes recusados. Falha assíncrona de decode é registrada no logcat com ID, caminho quando disponível e status real do SoundPool.
