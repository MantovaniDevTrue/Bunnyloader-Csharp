# Disparos C# — Example Shotgun

Os nomes exibidos do ExampleMod começam com **Example**, inclusive traduções e prefixos. IDs, classes, caminhos de assets e chaves de save não mudaram. Ex.: Example Espada, Example Bala, Example Escopeta, Example Poderoso.

`BunnyItem` e `BunnyWeapon` herdam `BunnyShootSource`. Sobrescreva `CanShoot(context)`, `ModifyShootStats(context, ref shot)` e/ou `OnShoot(context, shot)`. São métodos C# reais: herança, interfaces auxiliares, loops, condições e algoritmos próprios são permitidos. Não exigem QuickJS nem tModLoader. `OnShoot` corresponde ao Shoot do exemplo JS: usamos um nome diferente porque `BunnyWeapon.Shoot` já é a propriedade pública de referência ao projétil e permanece compatível.

- `CanShoot`: antes do disparo nativo. `false` aplica o tempo de uso nativo e impede o disparo; a escolha/consumo de munição ainda pertence ao Terraria.
- `ModifyShootStats`: recebe posição, velocidade, tipo, dano, knockback e ai0–ai2 já calculados pelo motor com a munição e os modificadores aplicáveis. Altere o `BunnyShot` por referência.
- `OnShoot`: `context.Spawn(shot)` adiciona pedidos; `false` substitui o tiro original, `true` preserva também esse tiro com os stats modificados. O limite é 64 pedidos adicionais e32KiB de resposta por chamada nativa. `shot.Projectile="UID/Nome"` resolve um projétil de outro mod pelo registro compartilhado; omitir preserva `shot.Type`.

O dono, IEntitySource e NewProjectileModifier originais permanecem na thread do jogo. Não podem ser fabricados como ponteiros em C#. O contexto oferece PlayerIndex, ItemType, WeaponDamage e Random; não contém Player/Item vivos. Instâncias de callbacks são por tipo de conteúdo e sessão, não uma instância persistida para cada item do inventário. Ainda não é uma API de estado arbitrário por item. O contexto é válido durante o callback no worker que o criou; Spawn recusa outra thread e permanece fechado após retorno ou exceção. Cálculos próprios podem usar outras estruturas, mas os comandos devem ser emitidos no callback.

O loader instala hooks com assinatura completa de Player.ItemCheck_Shoot e Projectile.NewProjectile, valida campos/tipos/tamanhos/armazenamento e recusa ABI incompatível. O protocolo SHOOT contém a máscara dos overrides. Cache do compilador v30 invalida descritores anteriores. O dispatcher e os métodos são preparados no worker antes de permitir disparos; não executamos gameplay do mod durante esse preparo. O SDK também aquece seu parsing, serialização de comandos, Random e matemática com dados internos; isso não mede nem garante o tempo de métodos auxiliares próprios de cada mod.

Receitas e disparos compartilham o transporte limitado do worker; os endpoints são distintos. Callback síncrono tem prazo de 3ms. Exceção, atraso, dependência ausente ou dados inválidos rejeitam o tiro e emitem diagnóstico com mod/classe/arquivo/etapa, sem substituir silenciosamente o comportamento pelo tiro vanilla. Se a falha ocorre após o motor consumir munição, esse consumo já ocorreu: o callback não desfaz o inventário. Erros repetidos são agrupados por conteúdo a cada 5s. Primeiro uso, pressão de filas e tempo real precisam de validação no Android.

Todos os pedidos são validados antes do primeiro spawn. Um pedido inválido não permite que os anteriores escapem. Depois de validados, cada spawn usa a fábrica real e pode ser recusado se a pool do Terraria estiver cheia, como qualquer projétil do jogo. Os spawns próprios percorrem os demais hooks, sem invocar recursivamente OnShoot. Exceções nativas nas chamadas registradas usam o guard existente do Bunny, preservam o diagnóstico original e restauram o escopo de disparo; a próxima criação de projétil não herda um callback antigo. Chamadas nativas distintas da mesma arma continuam independentes. A supressão devolve1000, o sentinela da fábrica nesta versão Android confirmado nos três loaders JS originais; a pool pode ter uma célula extra e seu tamanho não substitui esse contrato. A bridge verifica tipo/rank e pelo menos1000 células antes de usar o recurso. Esse valor é capacidade do motor, não um ID de conteúdo do ExampleMod.

## Exemplo original

`Content/Items/Weapons/Ranged/ExampleShotgun.cs` mantém uso55, dano10, knockback6, ranged, munição Bullet, shootSpeed10, preço de compra10gold, raridade Green, som Item36, textura44×18 e offset(-2,-2). O loop produz oito tiros, dispersão uniforme até15° para cada lado e velocidade entre70% e100%. Usa o tipo e o dano da munição recebidos do motor; não cria um ID fixo de bala e não força uma receita ausente no original.

## Teste no Terraria — somente conteúdos do ExampleMod

1. Ative ExampleMod e pegue **Example Escopeta** e **Example Bala** pelo catálogo do mod. Tenha espaço no inventário e observe a pilha de balas.
2. Dispare para ambos os lados, para cima e na diagonal. Cada uso deve produzir oito projéteis com dispersão; a cadência deve seguir useTime55. Confira sprite, offset e som.
3. Ataque **Example Slime**. Verifique dano da arma/munição, efeitos de acerto e comportamento do Example Bullet Projectile. A pilha deve ser consumida pelo uso, não uma vez por projétil emitido.
4. Remova todas as balas e tente disparar: não devem aparecer tiros gratuitos. Reponha Example Bala e repita.
5. Salve/saia/reentre com os itens no inventário, teste novamente e confira ausência de erros Shoot/Warmup no relatório.
6. Repita com seus mods JS e C# habituais, incluindo Lights And Shadows e Weapon Out Lite. Registre versão, relatório, travamentos e FPS observados.

Testes automatizados: `tests/shooting/run.sh` executa o C# real da escopeta e os hooks/decoder/binder C++ de produção com providers controlados, sem VM JS. `tests/summon_lifecycle/scheduler_test.cpp` verifica a fila de warmup e seus prazos. O pacote e seus nomes são verificados executando todos os SetDefaults. Isso não executa o motor ou mede FPS no telefone.
