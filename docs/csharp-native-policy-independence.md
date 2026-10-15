# Mods C# sem dependencia de JavaScript

Base: PR #24, checkpoint Android `.90.29.41`, com a correcao de cache do #25.
Esta migracao nao altera versionCode/versionName nem publica um APK por commit.

Fontes `.cs` passam por Roslyn, SDK, verificacao de cache e MonoVM. A ponte C++
registra conteudo e aplica policies no IL2CPP na thread do jogo. Nenhum modulo
do runtime C# chama QuickJS para carregar, instalar ou executar gameplay.
O suporte a mods JS permanece disponivel e compartilha os registros nativos.
Uma selecao somente C# nem inicializa QuickJS: o motor JS inicia sob demanda
no primeiro mod JS habilitado (ou no fallback de mods JS embutidos).

| Recurso existente | Dono da instalacao/execucao |
| --- | --- |
| Itens, defesa, som, tooltip e receitas | NativeBasicItems, ContentAssets e NativeRecipes |
| Efeitos de acessorio | Hook funcional nativo e tabela imutavel |
| Atributos, imunidades, conjunto e tooltip de conjunto | NativeArmorPolicies |
| Head/Body/Legs, Beard/Back/Wings, Shield/Shoes | NativeItemPolicies e TypeTables |
| Manto e cor do cabelo | Hooks/offsets nativos validados |
| WingStats e cinco parametros verticais | Tabela do Terraria e hook WingMovement |
| Clone vanilla, animacao, luz/alpha no mundo | NativeItemPolicies |
| Overlays, barreira e rastros C# | NativeVisualRenderer, nos hooks das camadas nativas |
| Eventos C# continuos existentes | MonoVM e filas/dispatchers nativos existentes |

Os antigos CSharpItemBehaviorLoader, CSharpArmorPolicy e CSharpWingPolicy foram
retirados do bundle de producao. Copias historicas ficam apenas em fixtures de
teste. EquipLoader/RecipeLoader JS nao consultam descritores C# nem fazem ACKs.
As camadas BunnyCSharp de PlayerDrawHooks tambem foram retiradas. O desenho
nativo visita o estado visual sem copiar strings, respeita a capacidade de
DrawDataCache e usa barreiras do GC nos structs com referencia a textura.
Carrega ate oito assets novos por passagem, com cache limitado a 64 texturas;
assets ativos nao sao removidos para liberar espaco. Novos efeitos acima dessa
capacidade permanecem pendentes, com diagnostico, sem voltar a JS.
JSON em BehaviorDefinition e somente um formato de dados; nao e codigo JS.

O instalador valida a forma dos descritores antes de publicar o lote, confere
tipos/tamanhos/offsets e registra assets somente na thread do jogo. Usa os Counts
atuais e conserva os slots e arrays existentes, inclusive os de mods JS.
Assets e clones ficam enraizados no GC; referencias em arrays passam por
Array.Copy/SetValue e referencias copiadas de clones usam a barreira do GC.
Limites sbyte de acessorios continuam sendo limites reais do Terraria.

Falhas de asset/metadados deixam fase e motivo pendentes, sem declarar sucesso
ou voltar para JS. A fila alterna itens, com ate oito tentativas por passagem.
WingStats e animacoes sao reaplicados nos inicializadores que os recriam.
Nao existe novo contrato de hot-unload/reconstrucao completa de todo registro.

O host C++ expoe nativeItemPolicyStatus e nativeArmorPolicyStatuses. O console
opcional bl.csharp.nativeItemPolicies()/nativeArmorPolicies() so consulta esses
dados; nao e necessario para carregar um mod C#.

## Verificacao

- Rodar SDKFoundation e ItemBatch para compilar os fontes C# reais e gerar wire.
- run_native_item_policies.py consome os nove itens reais com descritores,
  testa layouts raw/boxed, erros de metadados, retry de asset/array, visuais,
  manto, cabelo, clone, luz/alpha, animacao e restauracao de WingStats. O voo
  roda 10 mil ciclos sem alocacao no hook, incluindo gravidade invertida.
- run_native_armor_policies.py verifica pecas, imunidades, conjuntos completos
  e incompletos, lookup/tooltip vanilla, retry e reset em 10 mil ciclos.
- run_native_equip_effects.py verifica efeitos/isolamento e recusa de metadados
  invalidos sem fallback JS.
- run_csharp_independent_loading.py exercita o ModLoader de producao: so C#,
  selecao mista, falha de inicializacao JS e fallback embutido.
- O renderer verifica geometria/tint, invisibilidade/sombra, cache cheio,
  metadados incorretos e 10 mil desenhos sem novos objetos IL2CPP.
- Todos esses executaveis compilam com BL_HAVE_QUICKJS=0. A guarda
  test_csharp_no_javascript_dependency.py impede chamadas ao motor no runtime C#.

Os testes de host simulam metadados/objetos IL2CPP e a criacao de texturas Unity.
Nao comprovam renderizacao, ABI ou 60 FPS no Android. No checkpoint Android,
testar ExampleMod e ArmorPoliciesCSharp: crafting, defesa, mana/minion/imunidade,
conjunto e tooltip, hair tint, manto, asas/gravidade, shield/shoes, Soul no mundo,
inventario/bau, loadouts, morte/respawn, troca de mundo e save/reabertura.
Repetir com um mod JS e medir RAM/FPS/cold/warm start no mesmo aparelho.

As categorias ainda nao implementadas na matriz permanecem pendentes. Retirar
os adaptadores JS das APIs existentes nao cria automaticamente NPCs/bosses,
interfaces, worldgen, multiplayer ou callbacks livres para todas as categorias.

SpawnParticle permanece apenas no contrato de fila experimental anterior;
este lote nao implementa um novo renderer de particulas nem shaders gerais.
