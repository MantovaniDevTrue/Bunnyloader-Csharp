# Integração de callbacks gerenciados em gameplay

O vínculo `BunnyEntitySourceDispatcher.Bind` devolve um `bool`. O código nativo não pode considerar qualquer objeto retornado como êxito: agora verifica o valor `true` após o unboxing e falha caso seja `false`, nulo ou uma exceção; ausência de string também reprova o binding. Isso evita declarar NPCs/projéteis prontos sem classe C# correspondente.

O `pauseManagedEntitySourceCallbacks` avança a sessão e descarta as filas da sessão antiga. Quando o worker MonoVM recebe o primeiro evento da nova sessão válida, chama `BunnyEntitySourceDispatcher.ResetWorld()` antes de registrar/vincular e processar eventos. O método dispara `OnDespawn` nas instâncias anteriores e limpa o cache gerenciado de NPCs/projéteis sem executar callback Mono sob mutex nativo. A sessão é conferida antes do reset, e exceções impedem a execução de gameplay na sessão incoerente.

`EntityLifecycleProbe` testa a execução real do dispatcher C# por reflexão e seus hooks: Register, Bind, OnSpawn, AI, OnHit, OnKill, prevenção de kill duplicado, ResetWorld, OnDespawn e novo spawn após limpeza. O fixture C++ verifica o resultado do binding nativo. São testes de host e gerenciados sem APK; ainda falta QA Android do limite de tempo do MonoVM, multiplayer e troca de mundos no dispositivo.
