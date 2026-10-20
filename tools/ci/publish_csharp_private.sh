#!/usr/bin/env bash
# Extracted without removing private-destination, signature or same-SHA CI gates.
set -euo pipefail
# User now requires exactly five complete loaders and their original examples.
if [ "${GITHUB_REF:-}" = "refs/heads/feat/csharp-summon-lifecycle-v95" ] ||
   [ "${GITHUB_HEAD_REF:-}" = "feat/csharp-summon-lifecycle-v95" ]; then
  python3 "$(dirname "$0")/../loader_batch_gate.py" --delivery
fi
if [ -z "${GH_TOKEN:-}" ]; then
  echo "::error::BUNNY_PRIVATE_BUILD_TOKEN is missing. Full APK delivery was not attempted."
  exit 1
fi
PRIVATE_REPO="MantovaniDevTrue/Bunnyloader-Csharp-Builds"
IS_PRIVATE="$(gh api "repos/$PRIVATE_REPO" --jq '.private')"
if [ "$IS_PRIVATE" != "true" ]; then
  echo "Refusing to upload a game-bundled APK: destination is not private." >&2
  exit 1
fi
APK="Split/Bunny-Loader/app/build/outputs/apk/debug/app-debug.apk"
test -s "$APK"
"$ANDROID_HOME/build-tools/34.0.0/apksigner" verify "$APK" >/dev/null
VERSION="$(sed -nE 's/^[[:space:]]*versionName = "([^"]+)".*/\1/p' Split/Bunny-Loader/app/build.gradle.kts | head -n 1)"
test -n "$VERSION"
SAFE_VERSION="$(printf '%s' "$VERSION" | tr -c 'A-Za-z0-9._-' '_')"
FILE="BunnyLoader-CSharp-${SAFE_VERSION}-SignedV3.apk"
cp "$APK" "$RUNNER_TEMP/$FILE"
(cd "$RUNNER_TEMP" && sha256sum "$FILE" > "$FILE.sha256")
trap 'rm -f "$RUNNER_TEMP/$FILE" "$RUNNER_TEMP/$FILE.sha256"' EXIT
TAG="csharp-v3-run-${GITHUB_RUN_ID}-a${GITHUB_RUN_ATTEMPT}"
TITLE="Bunny Loader C# $VERSION | private signing v3"
NOTES="Private Android test build signed with the v3 key. Public source commit: ${GITHUB_SHA}. Workflow: https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}. Contains the Terraria runtime for restricted development/testing; do not publish or redistribute without appropriate rights."
if [ "$GITHUB_REF" = "refs/heads/codex/lot-a-complete-v90-29" ]; then
  # Experimental delivery requires all nine independent regression workflows.
  for attempt in $(seq 1 40); do
    gh api "repos/${GITHUB_REPOSITORY}/actions/runs?head_sha=${GITHUB_SHA}&per_page=100" > "$RUNNER_TEMP/lot-a-workflows.json"
    set +e
    python3 Split/Bunny-Loader/tools/check_lot_a_delivery.py "$RUNNER_TEMP/lot-a-workflows.json" "$GITHUB_SHA"
    DELIVERY_STATUS=$?
    set -e
    if [ "$DELIVERY_STATUS" = "0" ]; then break; fi
    if [ "$DELIVERY_STATUS" != "2" ] || [ "$attempt" = "40" ]; then
      echo "::error::Experimental delivery refused: independent regressions did not all succeed."
      exit 1
    fi
    sleep 15
  done
  TAG="experimental-lot-a-${GITHUB_RUN_ID}-a${GITHUB_RUN_ATTEMPT}"
  TITLE="EXPERIMENTAL — Lote A1 bancada C# | Bunny Loader C# $VERSION"
  NOTES="Experimental Lot A1 Android checkpoint: BunnyTile, ExampleWorkbench 2x1, native drops, dust, housing, adjacency, guide station icon and CSharp armor recipe support. Nine regression suites required. These features and 60 FPS have NOT yet been validated in Terraria Android; use ExampleModCSharp-v0.1.50.bmod for the unified test; disable the old standalone A1 probe to avoid duplicate workbenches. Compilation cache now survives native apply failures. Public source commit: ${GITHUB_SHA}. Workflow: https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}. Private runtime bundle; do not redistribute."
fi
if [ "$BUNNY_POLICY_AOT" = "true" ]; then
  for attempt in $(seq 1 40); do
    SOURCE_SHA="$BUNNY_SOURCE_SHA"
    gh api "repos/${GITHUB_REPOSITORY}/actions/runs?head_sha=${SOURCE_SHA}&per_page=100" > "$RUNNER_TEMP/compiler-workflows.json"
    set +e
    python3 Split/Bunny-Loader/tools/check_compiler_delivery.py "$RUNNER_TEMP/compiler-workflows.json" "$SOURCE_SHA"
    CHECK_STATUS=$?
    set -e
    if [ "$CHECK_STATUS" = "0" ]; then break; fi
    if [ "$CHECK_STATUS" != "2" ] || [ "$attempt" = "40" ]; then
      echo "::error::Compiler delivery refused: required independent regressions did not pass."
      exit 1
    fi
    sleep 10
  done
  TITLE="EXPERIMENTAL — CSharp compilador ARM64 mixed AOT | $VERSION"
  if [ "$BUNNY_POLICY_LIGHTS" = "true" ] || [ "$GITHUB_REF" != "refs/heads/codex/csharp-compiler-aot-v90-29-85" ]; then
    python3 - "$RUNNER_TEMP/compiler-workflows.json" "$SOURCE_SHA" <<'PYSCREENFX'
import json, sys
runs = json.load(open(sys.argv[1]))["workflow_runs"]
results = [r for r in runs if r["head_sha"] == sys.argv[2] and r["name"] == "CSharp Native ScreenFx"]
if not results or max(results, key=lambda r: r["id"]).get("conclusion") != "success":
    raise SystemExit("ScreenFx checks have not passed: refusing private .86 delivery")
print("SCREENFX_PRIVATE_DELIVERY_GATE=PASS")
PYSCREENFX
    TITLE="EXPERIMENTAL — CSharp shaders nativos e multi-mod | $VERSION"
  fi
  NOTES="Preserva compilador compacto .84, runtime Mono .72 e hooks QA 0.1.45. Partes do compilador Roslyn precompiladas em modo normal ARM64; mods continuam fontes editaveis compilados automaticamente. Hash do runtime e de todas as assemblies verificado antes do registro; fallback JIT. Inclui identificacao do codigo de Compiler.Compile no relatorio. Meta <=5 segundos e estabilidade Android ainda NAO comprovadas. Fonte ${GITHUB_SHA}; run https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}."
fi
if [ "$GITHUB_REF" = "refs/heads/codex/csharp-compact-compiler-v90-29-84" ]; then
  TITLE="EXPERIMENTAL — CSharp compilador com referencias compactas | $VERSION"
  NOTES="Preserva .83 e ExampleMod CSharp 0.1.45. Roslyn usa assemblies oficiais de referencia .NET 8.0.23 e metadata publica do SDK; runtime MonoVM permanece intacto. Snapshot de metadata limitado a um conjunto no worker, hashes SHA256 de todas as referencias, teste do ExampleMod completo contra referencias runtime/compactas com igualdade dos defaults e politicas. Perfil Roslyn da primeira compilacao preservado e publicado em logcat. Meta de ate 5 segundos NAO confirmada: requer medicao Xiaomi cold/warm; CI nao comprova tempo Android. Source ${GITHUB_SHA}; run https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}."
fi
if [ "$GITHUB_REF" = "refs/heads/codex/csharp-native-probe-optin-v90-29-83" ]; then
  TITLE="Bunny CSharp .83 — lote cinco hooks tipados | $VERSION"
  NOTES="Bunny CSharp .83 (versionCode 243) adiciona cinco assinaturas reais ao SDK: int(), void(int), void(int,int), int(int,int,int), void(int,int,int), em classes Terraria, estatico e instancia. Call original, return override, metadata estrita e fail-open. ExampleMod 0.1.42 tem cinco arquivos dedicados desativados por padrao. NativeAccess ResetEffects agora e opt-in e deixa de impactar quadros. Mantem packageName, assinatura privada V3 e precompile no launcher. Contratos e exemplos testados no CI; assinatura IL2CPP real/fps no Xiaomi ainda pendentes. Commit ${GITHUB_SHA}; run https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}."
fi
if [ "$GITHUB_REF" = "refs/heads/codex/csharp-unified-example-v90-29-82" ]; then
  TITLE="Bunny CSharp .82 — ExampleMod CSharp unificado | $VERSION"
  NOTES="Bunny CSharp .82, versionCode 242, preserva packageName e assinatura privada V3: ExampleMod CSharp 0.1.45 unico para jogar, com armaduras nativas existentes e NativeAccess read-only com um unico ResetEffects e amostragem 1/64. Hook int(int,int) sobre Terraria.Utilities.UnifiedRandom.Next disponivel em ExampleIntPairHook.Start/Stop; opt-in, por performance e integridade da aleatoriedade. Alvo inferido dos fontes Terraria PC e sujeito a validacao da assinatura IL2CPP no Android. Builds antigos dos dois probes preservados apenas para testes de regressao no CI. Necessario teste real Xiaomi e FPS. Commit ${GITHUB_SHA} e run https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}."
fi
if [ "$GITHUB_REF" = "refs/heads/codex/csharp-universal-intpair-v90-29-79" ]; then
  TITLE="EXPERIMENTAL — Bunny C# hooks int(int,int) | $VERSION"
  NOTES="Bunny CSharp .79: novos callbacks universais tipados int(int,int) para classes Terraria, validação estrita de ABI ARM64 com parâmetros System.Int32, retorno Int32, stub instancia e estatico, ticket do original, fallback vanilla, deadline 3ms e cooldown recuperavel existentes. SDK HookInt32Pair e testes host de assinatura, original, substituicao, expiracao e unload. Preserva as otimizações .78 de precompilação no launcher. Teste controlado com alvo real necessario; nenhum mod padrao instala hook novo automaticamente. Commit ${GITHUB_SHA}. Workflow https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}."
fi
if [ "$GITHUB_REF" = "refs/heads/codex/csharp-launcher-auto-precompile-v90-29-78" ]; then
  TITLE="EXPERIMENTAL — compilacao CSharp automatica no launcher | $VERSION"
  NOTES="Bunny CSharp .78: recompila em segundo plano os mods CSharp habilitados assim que o launcher termina de varrer os pacotes, apos instalar, atualizar ou ativar. Mantem fallback no botao Jogar, reaproveitamento verificado de DLL SHA256 da .77, thread MonoVM isolada, prioridades e content gate seguro. Inclui modo launcher no relatorio de diagnosticos. Objetivo: entregar Roslyn aquecido e DLL preparada antes de abrir Terraria, mantendo 60 FPS. Precisa de validacao Android cold/warm e teste de mod atualizado. SHA ${GITHUB_SHA}. CI https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}."
fi
if [ "$GITHUB_REF" = "refs/heads/codex/csharp-prefetch-reuse-v90-29-77" ]; then
  TITLE="EXPERIMENTAL — CSharp prefetch verificado com Roslyn reutilizado | $VERSION"
  NOTES="Bunny CSharp .77: corrige o precompile do Play no processo launcher e reaproveita DLL precompilada via manifesto SHA256 dos fontes, framework, DLL e cache. O worker oficial executa apenas CompiledAssemblyProbe.Verify, sem reinvocar Roslyn, quando o recibo confere; cache CSharp completo segue exclusivo ao autoloader apos gate nativo. Novo contador prefetchReused e resumo de Play. Mantem .74/75 como fallback, hooks de combate e 60 FPS como alvo a validar no Android. Run: https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}."
fi
if [ "$GITHUB_REF" = "refs/heads/codex/csharp-launch-precompile-v90-29-76" ]; then
  TITLE="EXPERIMENTAL — CSharp compila ao clicar Jogar | $VERSION"
  NOTES="Bunny CSharp .76: Botao Jogar do launcher e confirmacao de runtime misto iniciam precompilacao MonoVM isolada enquanto Terraria abre. Operacao somente Roslyn, sem executar SetDefaults, sem registrar conteudo/hook antecipado; fila de prioridade para carregamento oficial; status de precompile por mod separado; SHA256 do compilador preservado. A aplicacao de itens, NPCs, receitas e hooks usa o gate original. Testar cold, warm, 60 FPS, tempo ate entrar no mundo, cache, reentrada e ausencia de duplicacao. Source: ${GITHUB_SHA}. Run: https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}."
fi
if [ "$GITHUB_REF" = "refs/heads/codex/csharp-early-compile-v90-29-75" ]; then
  TITLE="EXPERIMENTAL — CSharp early compile antes de tabelas nativas | $VERSION"
  NOTES="Bunny CSharp .75: compilacao MonoVM isolada começa no menu quando cache externo falha, antes de modProjectilesSettled; aplicar SetDefaults e registrar conteudo nativo apenas apos tabelas prontas. Mantem worker residente .74, hooks RPC .73, fallback vanilla, SHA256. Stage=EarlyCompile no log. Testar cold e warm, menu 60 FPS, tempo de entrada no mundo, zero crash e hooks de combate. Source: ${GITHUB_SHA}. Workflow https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}."
fi
if [ "$GITHUB_REF" = "refs/heads/codex/csharp-resident-mono-compiler-v90-29-74" ]; then
  TITLE="EXPERIMENTAL — MonoVM CSharp residente por lote | $VERSION"
  NOTES="Bunny CSharp .74: compila varios mods num unico worker MonoVM isolado, thread serial e domain warm; fila de 16 pedidos, idle timeout 8s, verificacao SHA256 no primeiro pedido, process reuse, profiler MonoProbeMs/RoslynInvokeMs/VerifyInvokeMs e protecao de monitor por geracao. Preserva hooks .73, fallback vanilla, 60 FPS como meta; requer validacao Android cold/warm, recompilacao de fonte alterada e comparacao de PIDs. Source: ${GITHUB_SHA}. Run: https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}."
fi
if [ "$GITHUB_REF" = "refs/heads/codex/csharp-fast-start-rpc-v90-29-73" ]; then
  TITLE="EXPERIMENTAL — C# cache incremental e RPC rápida | $VERSION"
  NOTES="Bunny CSharp .73: copia incremental com SHA-256 dos assemblies MonoVM, stage antes de calcular cache de mods, preserva binario SDK para evitar recompilacao desnecessaria, mede framework e Roslyn separadamente e inclui fast poll RPC de ate 48us dentro do deadline de 3ms. Recuperacao .72 e fallback vanilla preservados. Testar cold/warm start, cache=Hit, impacto nos 60FPS, RPC transitorios, timeouts e reentrada em mundo. Source: ${GITHUB_SHA}. Workflow: https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}."
fi
if [ "$GITHUB_REF" = "refs/heads/codex/csharp-universal-soft-recovery-v90-29-72" ]; then
  TITLE="EXPERIMENTAL — Hooks CSharp com recuperação leve | $VERSION"
  NOTES="Bunny CSharp .72: falhas transitorias da ponte nativa agora usam cooldown curto de 4 ms, timeouts reais isolados usam 24 ms e a contagem de falhas consecutivas zera apos callback concluido. A suspensao 5/10/15 s permanece somente para tres timeouts reais consecutivos, preservando fallback vanilla e deadline de 3 ms. Testar NativeAccessProbe sozinho, reentrada no mundo, fps e novos contadores; nenhuma garantia de zero timeout ou 60 FPS antes do teste Android. Source: ${GITHUB_SHA}. Workflow: https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}."
fi
if [ "$GITHUB_REF" = "refs/heads/codex/csharp-native-rpc-deadline-v90-29-71" ]; then
  TITLE="EXPERIMENTAL — C# RPC sincronizada e recuperação de hooks | $VERSION"
  NOTES="Bunny CSharp .71: espera RPC alinhada ao deadline do callback, reserva para retorno ao MonoVM, falhas transitorias Busy/TimedOut recuperaveis sem destruir registro CSharp, telemetria de espera/execucao e pico. Mantem deadline de 3ms, fallback vanilla, backoff 5/10/15s e acesso nativo 100% CSharp. Testar com NativeAccessProbe; desligar UniversalHooksProbe para evitar hook duplicado. Sem validacao Android ou promessa de zero timeouts. Source: ${GITHUB_SHA}. Workflow: https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}."
fi
if [ "$GITHUB_REF" = "refs/heads/codex/csharp-native-access-adaptive-v90-29-70" ]; then
  TITLE="EXPERIMENTAL — Acesso nativo C# + retomada rápida | $VERSION"
  NOTES="Bunny CSharp .70 reúne PR43 e PR44: campos/propriedades, chamadas tipadas, lotes e novas leituras+lógica de método numa só operação nativa; hooks com suspensões 5/10/15 segundos. O NativeAccessProbe v0.1.0 está otimizado e não deve ser usado simultaneamente com UniversalHooksProbe (ambos interceptam Player.ResetEffects). A execução real Android desta fusão ainda deve ser validada. Source: ${GITHUB_SHA}. Workflow: https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}."
fi
if [ "$GITHUB_REF" = "refs/heads/codex/csharp-native-access-lot-a" ]; then
  TITLE="EXPERIMENTAL — Acesso nativo C# lote A | $VERSION"
  NOTES="Experimental .69: includes .68 typed Int32 hooks; typed native fields/properties, method calls with up to 4 supported arguments, Vector2/Color/Rectangle, scopes and batched reads. Install NativeAccessProbeCSharp-v0.1.0.bmod and disable Universal Hooks Probe CSharp because both use Player.ResetEffects. Probe is read-only: check Editor CSharp native access batch/method counters, zero writes, world re-entry, hook recovery and FPS. Android gameplay is NOT yet validated. Source: ${GITHUB_SHA}. Workflow: https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}. Restricted runtime bundle; do not redistribute."
fi
if [ "$BUNNY_POLICY_LIGHTS" = "true" ]; then
  NOTES="EXPERIMENTAL ${VERSION}: Lights And Shadows CSharp 0.4.2.3 with invisible-underground-sun fast paths; nine render passes and original texture assets preserved. Includes typed/sized IL2CPP scene read contracts, exact-zero underground transition, bounded workflow expressions, framebuffer copy telemetry and render-stat batching. Android heavy boss stress test pending. ExampleMod CSharp 0.1.50 and SIGABRT diagnostics retained. Commit ${GITHUB_SHA}. Android 60 FPS NOT yet confirmed."
fi
if [ "$GITHUB_REF" = "refs/heads/feat/csharp-native-recipe-groups-v89" ]; then
  TITLE="EXPERIMENTAL — CSharp native recipe groups | $VERSION"
  NOTES="EXPERIMENTAL ${VERSION}: reusable native RecipeGroup ingredients resolved by IL2CPP field identity, typed CSharp ingredient/station references, independent recipe cloning/removal and detailed contract diagnostics. ExampleMod CSharp 0.1.50 adds alternate group recipes for its actual CSharp ExampleWorkbench; original recipes and content IDs preserved. Includes Lights 0.4.2.3 and the .88.8 scene fixes. Host contract/integration checks and Android build passed; crafting, alternate-material consumption, save/re-entry, FPS and <=5-second cold compilation still require Xiaomi testing. Source ${GITHUB_SHA}."
fi
if [ "$GITHUB_REF" = "refs/heads/feat/csharp-buff-recipes-v90" ]; then
  TITLE="EXPERIMENTAL WIP — CSharp buffs and recipe callbacks | $VERSION"
  NOTES="EXPERIMENTAL INFRASTRUCTURE CHECKPOINT, NOT THE COMPLETE REQUESTED LOT. Integrates PR86 with preserved NativeAOT ABI, executed managed buff flags/description, all-slot buff iteration, real DelBuff, nurse immunity and defense multiplier. Native custom recipe groups, managed conditions/consume/on-craft callbacks use the existing bounded Mono worker and a language-neutral native provider. ExampleMod 0.1.50 includes a real ExampleWorkbench recipe and defense debuff/potion. Host and Android compilation/signing gates passed. Actual Android gameplay is NOT yet validated. Minion AI/staff/text counter, Blocky/costume, mounts/minecart and original firework callbacks remain explicitly pending. Preserve validated .89, Lights 0.4.2.3 and JavaScript. Source ${GITHUB_SHA}; see docs/csharp-buffs-recipes-v90.md."
fi
if [ "$GITHUB_REF" = "refs/heads/feat/csharp-loader-foundation-v91" ]; then
  for attempt in $(seq 1 40); do
    GH_TOKEN="$BUNNY_SOURCE_READ_TOKEN" gh api "repos/${GITHUB_REPOSITORY}/actions/runs?head_sha=${GITHUB_SHA}&per_page=100" > "$RUNNER_TEMP/v91-workflows.json"
    set +e
    python3 Split/Bunny-Loader/tools/check_lot_a_delivery.py "$RUNNER_TEMP/v91-workflows.json" "$GITHUB_SHA" --current
    DELIVERY_STATUS=$?
    set -e
    if [ "$DELIVERY_STATUS" = 0 ]; then break; fi
    if [ "$DELIVERY_STATUS" != 2 ] || [ "$attempt" = 40 ]; then
      echo "::error::Independent same-commit regressions not approved; delivery refused"
      exit 1
    fi
    sleep 15
  done
  TITLE="EXPERIMENTAL — CSharp recipe loader classes | $VERSION"
  NOTES="Experimental .91 / ExampleMod 0.1.50. Preserves .90.3.1 RecipeGroup and native craft-source ABI fixes. Adds reusable class-based recipe hooks, composable conditions, owner-level callback reconstruction isolation and explicit callback stages. ExampleWorkbench uses the public APIs. All 88 original loader modules inventoried; this is NOT completion of all loaders or Lot A. Native groups/conditions/callbacks remain connected to real Terraria hooks. Nine independent same-SHA checks and Android build/signing passed. Gameplay and FPS require Android testing. Source ${GITHUB_SHA}. See docs/csharp-loaders-v91.md."
fi
if [ "$GITHUB_REF" = "refs/heads/feat/csharp-owned-recipe-loader-v92" ]; then
  for attempt in $(seq 1 40); do
    GH_TOKEN="$BUNNY_SOURCE_READ_TOKEN" gh api "repos/${GITHUB_REPOSITORY}/actions/runs?head_sha=${GITHUB_SHA}&per_page=100" > "$RUNNER_TEMP/v91-workflows.json"
    set +e
    python3 Split/Bunny-Loader/tools/check_lot_a_delivery.py "$RUNNER_TEMP/v91-workflows.json" "$GITHUB_SHA" --current
    DELIVERY_STATUS=$?
    set -e
    if [ "$DELIVERY_STATUS" = 0 ]; then break; fi
    if [ "$DELIVERY_STATUS" != 2 ] || [ "$attempt" = 40 ]; then
      echo "::error::Independent same-commit regressions not approved; delivery refused"
      exit 1
    fi
    sleep 15
  done
  TITLE="EXPERIMENTAL — Bunny loaders próprios C# | $VERSION"
  NOTES="Loader de receitas próprio para BunnyItem, BunnyWeapon, BunnyAccessory e BunnyBuffPotion. ExampleMod 0.1.51 com três receitas na bancada CSharp e callback de recompensa compartilhado. Serviço nativo independente de QuickJS; contratos host e nove regressões exigidos no mesmo SHA. Gameplay, desempenho e compatibilidade IL2CPP no Android ainda aguardam validação. O conjunto de loaders e o Lote A NÃO estão concluídos. Source ${GITHUB_SHA}; run https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}."
fi
if [ "$GITHUB_REF" = "refs/heads/feat/csharp-owned-loaders-v93" ]; then
  for attempt in $(seq 1 40); do
    GH_TOKEN="$BUNNY_SOURCE_READ_TOKEN" gh api "repos/${GITHUB_REPOSITORY}/actions/runs?head_sha=${GITHUB_SHA}&per_page=100" > "$RUNNER_TEMP/v93-workflows.json"
    set +e
    python3 Split/Bunny-Loader/tools/check_lot_a_delivery.py "$RUNNER_TEMP/v93-workflows.json" "$GITHUB_SHA" --current
    DELIVERY_STATUS=$?
    set -e
    if [ "$DELIVERY_STATUS" = 0 ]; then break; fi
    if [ "$DELIVERY_STATUS" != 2 ] || [ "$attempt" = 40 ]; then
      echo "::error::Independent same-commit regressions not approved; delivery refused"
      exit 1
    fi
    sleep 15
  done
  TITLE="EXPERIMENTAL — Bunny CSharp recipes + projectile policies | $VERSION"
  NOTES="Integra a build .90.5 (alvos/combate minion) com os loaders proprios de receitas .92, preservando os dois historicos. .93 / ExampleMod 0.1.52: cajado, buff e minion no pacote principal, receita na bancada CSharp; animacao, alcance/frequencia de busca e movimento configuraveis; contratos nativos de tipo/tamanho/offset. Nove regressoes no mesmo SHA exigidas. Port de minion ainda parcial: faltam LOS, alvo manual, teleporte, separacao, luz e atributos originais do cajado. Gameplay/FPS Android ainda nao validados. Source ${GITHUB_SHA}; run https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}."
fi
if [ "$GITHUB_REF" = "refs/heads/feat/csharp-94-owned-summon-damage-lot" ]; then
  for attempt in $(seq 1 40); do
    GH_TOKEN="$BUNNY_SOURCE_READ_TOKEN" gh api "repos/${GITHUB_REPOSITORY}/actions/runs?head_sha=${GITHUB_SHA}&per_page=100" > "$RUNNER_TEMP/v94-workflows.json"
    set +e
    python3 Split/Bunny-Loader/tools/check_lot_a_delivery.py "$RUNNER_TEMP/v94-workflows.json" "$GITHUB_SHA" --current
    DELIVERY_STATUS=$?
    set -e
    if [ "$DELIVERY_STATUS" = 0 ]; then break; fi
    if [ "$DELIVERY_STATUS" != 2 ] || [ "$attempt" = 40 ]; then
      echo "::error::CSharp .94 independent same-SHA gates failed or timed out"
      exit 1
    fi
    sleep 15
  done
  TITLE="Bunny CSharp .94 + ExampleMod 0.1.53 | native summon combat"
  NOTES="Bunny .93 own-loader policies retained. ExampleMod CSharp 0.1.53 contains the summon staff, buff, and minion; no separate probe. Android AddBuff(fromNetPvP), MinionContactDamage, bound native restore for zero-damage minions; 17-field wire and typed native contracts. CI validates compilation and regression; real Android combat/FPS not validated. Source ${GITHUB_SHA}."
fi
if [ "$GITHUB_REF" = "refs/heads/feat/csharp-90-4-projectile-minion-contract" ]; then
  NOTES="EXPERIMENTAL native CSharp minion/summon SDK .90.4: own MonoVM source API, IL2CPP minion flags, weapon buff and managed follow AI. Standalone NativeMinionCSharpProbe 0.1.0 included. Legacy ExampleMod CSharp 0.1.49 and Lights 0.4.2.3 unchanged. Device and FPS validation pending. SHA ${GITHUB_SHA}; CI https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}."
fi
if [ "$GITHUB_REF" = "refs/heads/feat/csharp-90-5-native-minion-targeting-lot" ]; then
  TITLE="EXPERIMENTAL — CSharp minion native combat and animated summon | $VERSION"
  NOTES="Native CSharp summon .90.5 (MonoVM) with player/minion buff spawn, hostile-target packet, four-frame minion sprites, own C# steering class and real ExampleMinion visual assets. ExampleMod CSharp 0.1.49, recipe tests and Lights 0.4.2.3 preserved. Source-only NativeMinionCSharpProbe v0.1.1. Android gameplay, damage and 60 FPS REQUIRE testing. Source ${GITHUB_SHA}. Workflow https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}."
fi
if [ "$GITHUB_REF" = "refs/heads/feat/csharp-summon-lifecycle-v95" ] || [ "${GITHUB_HEAD_REF:-}" = "feat/csharp-summon-lifecycle-v95" ]; then
  SOURCE_SHA="$BUNNY_SOURCE_SHA"
  for attempt in $(seq 1 30); do
    GH_TOKEN="$BUNNY_SOURCE_READ_TOKEN" gh api "repos/${GITHUB_REPOSITORY}/actions/runs?head_sha=${SOURCE_SHA}&per_page=100" > "$RUNNER_TEMP/v95-workflows.json"
    set +e
    python3 Split/Bunny-Loader/tools/check_lot_a_delivery.py "$RUNNER_TEMP/v95-workflows.json" "$SOURCE_SHA" --current
    DELIVERY_STATUS=$?
    set -e
    if [ "$DELIVERY_STATUS" = 0 ]; then break; fi
    if [ "$DELIVERY_STATUS" != 2 ] || [ "$attempt" = 30 ]; then echo "[CSharp][ERROR][Delivery] Stage=RequiredChecks Result=Rejected Reason=ChecksNotSuccessful"; exit 1; fi
    sleep 10
  done
fi

EXAMPLE_VERSION="$(python3 -c 'import json; print(json.load(open("Split/Bunny-Loader/samples/ExampleModCSharp/Playable/manifest.json"))["version"])')"
RELEASE_ASSETS=(
  "$RUNNER_TEMP/$FILE"
  "$RUNNER_TEMP/$FILE.sha256"
  "$RUNNER_TEMP/ExampleModCSharp-v${EXAMPLE_VERSION}.bmod"
  "$RUNNER_TEMP/ExampleModCSharp-v${EXAMPLE_VERSION}.bmod.sha256"
  "Split/Bunny-Loader/build/combat-batch-results.json"
  "Split/Bunny-Loader/build/basic-items-results.json"
)
if [ "$GITHUB_REF" = "refs/heads/feat/csharp-90-4-projectile-minion-contract" ]; then
  RELEASE_ASSETS+=(
    "$RUNNER_TEMP/NativeMinionCSharpProbe-v0.1.0.bmod"
    "$RUNNER_TEMP/NativeMinionCSharpProbe-v0.1.0.bmod.sha256"
  )
fi
if [ "$GITHUB_REF" = "refs/heads/feat/csharp-90-5-native-minion-targeting-lot" ]; then
  RELEASE_ASSETS+=(
    "$RUNNER_TEMP/NativeMinionCSharpProbe-v0.1.1.bmod"
    "$RUNNER_TEMP/NativeMinionCSharpProbe-v0.1.1.bmod.sha256"
  )
fi
if [ "$BUNNY_POLICY_LIGHTS" = "true" ]; then
  RELEASE_ASSETS+=(
    "$RUNNER_TEMP/LightsAndShadowsCSharp-v0.4.2.3.bmod"
    "$RUNNER_TEMP/LightsAndShadowsCSharp-v0.4.2.3.bmod.sha256"
  )
fi
if [ "$GITHUB_REF" != "refs/heads/codex/csharp-compact-compiler-v90-29-84" ] && [ "$GITHUB_REF" != "refs/heads/codex/csharp-native-probe-optin-v90-29-83" ] && [ "$GITHUB_REF" != "refs/heads/codex/csharp-unified-example-v90-29-82" ]; then
  RELEASE_ASSETS+=(
    "$RUNNER_TEMP/NativeAccessProbeCSharp-v0.1.0.bmod"
    "$RUNNER_TEMP/NativeAccessProbeCSharp-v0.1.0.bmod.sha256"
    "$RUNNER_TEMP/ArmorPoliciesCSharp-v0.1.0.bmod"
    "$RUNNER_TEMP/ArmorPoliciesCSharp-v0.1.0.bmod.sha256"
    "$RUNNER_TEMP/NativePlacementProbe-v0.1.0.bmod"
    "$RUNNER_TEMP/NativePlacementProbe-v0.1.0.bmod.sha256"
  )
fi
if [ "$GITHUB_REF" = "refs/heads/hotfix/csharp-88-5-sigabrt-logcat-tail" ] || [ "${GITHUB_HEAD_REF:-}" = "hotfix/csharp-88-5-sigabrt-logcat-tail" ] || [ "$GITHUB_REF" = "refs/heads/perf/csharp-88-7-lights-invisible-ray-fastpath" ] || [ "${GITHUB_HEAD_REF:-}" = "perf/csharp-88-7-lights-invisible-ray-fastpath" ]; then
  SOURCE_SHA="$BUNNY_SOURCE_SHA"
  for attempt in $(seq 1 40); do
    gh api "repos/${GITHUB_REPOSITORY}/actions/runs?head_sha=${SOURCE_SHA}&per_page=100" > "$RUNNER_TEMP/integration-screenfx.json"
    if python3 - "$RUNNER_TEMP/integration-screenfx.json" "${SOURCE_SHA}" <<'PYSCREENFX_GATE'
import json, sys
runs = json.load(open(sys.argv[1]))["workflow_runs"]
matching = [run for run in runs if run.get("head_sha") == sys.argv[2] and run.get("name") == "CSharp Native ScreenFx"]
if not matching:
    raise SystemExit(1)
latest = max(matching, key=lambda run: run["id"])
if latest.get("conclusion") != "success":
    raise SystemExit(1)
PYSCREENFX_GATE
    then
      echo "BUNNY_INTEGRATION_SCREENFX_GATE=PASS"
      break
    fi
    if [ "$attempt" = 40 ]; then
      echo "::error::ScreenFx regression not successful, private APK delivery stopped"
      exit 1
    fi
    sleep 15
  done
fi
if [ "${GITHUB_REF:-}" = "refs/heads/feat/csharp-summon-lifecycle-v95" ] ||
   [ "${GITHUB_HEAD_REF:-}" = "feat/csharp-summon-lifecycle-v95" ]; then
  TITLE="EXPERIMENTAL — cinco loaders completos | Bunny CSharp $VERSION"
  NOTES="Lote de cinco loaders e um exemplo original completo por loader. Revisao de contratos e testes automatizados aprovados; gameplay e FPS Android aguardam validacao. Fonte ${BUNNY_SOURCE_SHA}; run https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}. Contratos, pendencias e roteiros no repositorio. Bundle privado, nao redistribuir."
fi
gh release create "$TAG" "${RELEASE_ASSETS[@]}" \
  --repo "$PRIVATE_REPO" \
  --target main \
  --title "$TITLE" \
  --notes "$NOTES" \
  --prerelease
echo "BUNNY_PRIVATE_APK_DELIVERY=SUCCESS"
if [ "$GITHUB_REF" = "refs/heads/codex/csharp-native-probe-optin-v90-29-83" ]; then
  GH_TOKEN="$BUNNY_SOURCE_READ_TOKEN" gh issue comment 62 --repo "$GITHUB_REPOSITORY" --body "Bunny CSharp .83 assinada V3 e ExampleMod 0.1.42 no release privado: ${TAG}. Build: https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}. SHA ${GITHUB_SHA}." || true
fi
if [ "$GITHUB_REF" = "refs/heads/codex/csharp-unified-example-v90-29-82" ]; then
  GH_TOKEN="$BUNNY_SOURCE_READ_TOKEN" gh issue comment 59 --repo "$GITHUB_REPOSITORY" --body "Bunny Loader CSharp .82 compilado, APK V3 e ExampleMod CSharp 0.1.45 no release privado tag ${TAG}. Run: https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}. Commit: ${GITHUB_SHA}." || true
fi
if [ "$GITHUB_REF" = "refs/heads/codex/csharp-universal-intpair-v90-29-79" ]; then
  GH_TOKEN="$BUNNY_SOURCE_READ_TOKEN" gh issue comment 59 \
    --repo "$GITHUB_REPOSITORY" \
    --body "APK CSharp .79 gerado e assinado (V3), entregue ao repositorio privado Builds. GitHub Actions run: https://github.com/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}. Release privada: MantovaniDevTrue/Bunnyloader-Csharp-Builds, tag ${TAG}, arquivo ${FILE}. Commit: ${GITHUB_SHA}." \
    || echo "::warning::APK entregue, mas comentario de confirmacao na PR nao foi publicado"
fi
