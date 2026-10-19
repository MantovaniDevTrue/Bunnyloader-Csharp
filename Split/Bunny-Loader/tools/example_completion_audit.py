#!/usr/bin/env python3
"""Inventory ALL canonical examples; evidence, never compilation, determines status."""
import argparse
import hashlib
import json
import re
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = ROOT / 'tools/example_completion'
OUT = ROOT / 'docs/generated/example_completion'
STATES = ('Não implementado', 'Parcial', 'Implementado, aguardando validação',
          'Validado por testes automatizados', 'Validado no Android', 'Bloqueado por limitação comprovada')
CLASSES = re.compile(r'\bclass\s+(\w+)(?:\s+extends\s+(\w+))?')
METHODS = re.compile(r'^\s*(?:static\s+)?(?:get\s+|set\s+)?([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{', re.M)
LOADER = {'ModItem':'ModItem.js', 'ModProjectile':'ModProjectile.js', 'ModNPC':'ModNPC.js',
          'ModTile':'ModTile.js', 'ModWall':'ModWall.js', 'ModPlayer':'ModPlayer.js',
          'ModBuff':'ModBuff.js', 'ModMount':'ModMount.js', 'ModBiome':'ModBiome.js',
          'ModPrefix':'ModPrefix.js', 'ModSystem':'ModSystem.js', 'GlobalNPC':'GlobalNPC.js',
          'ModCommand':'ModCommand.js', 'ModConfig':'ModConfig.js', 'ModBossBar':'ModBossBar.js'}

def category(path):
    if path == 'main.js': return 'Systems'
    parts = path.split('/')
    if 'Items' in parts:
        tail = parts[parts.index('Items')+1:]
        return tail[0] if len(tail)>1 else 'Items'
    if 'NPCs' in parts: return 'Bosses' if 'ExampleBoss' in parts else 'NPCs'
    if 'BossBars' in parts: return 'Bosses'
    if 'GlobalNPCs' in parts: return 'NPCs'
    if 'Players' in parts: return 'Players'
    return parts[1] if len(parts)>2 else 'Systems'

def lot(cat):
    if cat in ('Items','Accessories','Armor','Consumables','Materials','Tools','Ammo','Quests','Rarities','Prefixes'): return 2
    if cat in ('Weapons','Projectiles','DamageClasses'): return 3
    if cat in ('NPCs','Bosses'): return 4
    if cat in ('Placeable','Tiles','Walls','TileEntities'): return 5
    if cat in ('Biomes','Clouds'): return 6
    return 7

def facts(source):
    classes = [{'name':n, 'base':b} for n,b in CLASSES.findall(source)]
    methods = sorted(set(METHODS.findall(source)) - {'if','for','while','switch','catch','constructor'})
    properties = sorted(set(re.findall(r'\b(?:this\.(?:Item|Projectile|NPC)|player|npc|projectile|item)\.([A-Za-z_]\w*)',source)))
    calls = sorted(set(re.findall(r"\[['\"]([^'\"\n]+\([^'\"\n]*\))['\"]\]",source)))
    imports = sorted(set(re.findall(r"\bfrom\s+['\"]([^'\"]+)['\"]",source)))
    return dict(classes=classes, hooksAndMethods=methods, properties=properties,
                nativeSignatures=calls, imports=imports)

def build(upstream):
    refs = json.loads((SPEC/'references.json').read_text())
    evidence = json.loads((SPEC/'evidence.json').read_text())
    originals = {x['path']:x for x in refs['upstreamExampleFiles'] if x['path'].endswith('.js')}
    fork = {x['path']:x for x in refs['forkExampleFiles'] if x['path'].endswith('.js')}
    expected=refs.get('expectedJsCounts', {'canonical':199,'fork':178})
    if len(originals)!=expected['canonical'] or len(fork)!=expected['fork']: raise ValueError('Reference inventory changed: update pinned snapshot deliberately')
    snapshot_path=SPEC/'package-reference.json'
    package=json.loads(snapshot_path.read_text()) if snapshot_path.is_file() else None
    packaged={entry['id']:entry for entry in package['examples']} if package else {}
    if package:
        for relative, entry in packaged.items():
            originals['samples/ExampleMod/content/'+relative]={'path':'samples/ExampleMod/content/'+relative,
                'sha':entry['gitBlob'],'size':entry['size']}
    if set(evidence['examples'])-set(p.removeprefix('samples/ExampleMod/content/') for p in originals):
        raise ValueError('Evidence refers to nonexistent JS example')
    rows=[]
    for path,record in sorted(originals.items()):
        rel=path.removeprefix('samples/ExampleMod/content/')
        reference=packaged.get(rel)
        detail=reference['facts'] if reference else facts((upstream/path).read_text())
        state=evidence['examples'].get(rel,{})
        status=state.get('status',STATES[0])
        if status not in STATES: raise ValueError('Invalid state: '+rel)
        cs=state.get('cSharp',[])
        for p in cs:
            if not (ROOT/p).is_file(): raise ValueError('Missing C# counterpart: '+p)
        if status==STATES[4] and not state.get('deviceEvidence'):
            raise ValueError('Validated without device evidence: '+rel)
        if status==STATES[5] and not state.get('technicalBlockEvidence'):
            raise ValueError('Technical block without proof: '+rel)
        if status in STATES[2:5] and (not cs or state.get('missingFeatures')):
            raise ValueError('Complete implementation still missing features: '+rel)
        if status==STATES[3] and not state.get('automatedEvidence'):
            raise ValueError('Host validation without evidence: '+rel)
        bases=[c['base'] for c in detail['classes'] if c['base']]
        corePaths={x['path'] for x in refs['upstreamCoreFiles']} | {x['path'] for x in refs['forkCoreFiles']}
        dependencies=[]
        for b in bases:
            if b in {c['name'] for c in detail['classes']}: continue
            pathCandidate='app/src/main/cpp/script/js/mod/'+b+'.js'
            if pathCandidate not in corePaths:
                pathCandidate='app/src/main/cpp/script/js/mod/Loaders/'+b.removeprefix('Mod')+'Loader.js'
            dependencies.append(pathCandidate if pathCandidate in corePaths else 'Requer resolver classe JS herdada: '+b)
        dependencies=sorted(set(dependencies))
        imports=[]
        for imp in detail['imports']:
            imported=(Path(path).parent/imp).as_posix()
            imports.append(imported)
        cap='Sem contrato C# reutilizavel equivalente para '+', '.join(bases or ['este modulo/helper'])
        missing=state.get('missingFeatures',[cap])
        if reference and reference['comparison']=='Changed':
            missing=list(missing)+['Revisar diferencas de comportamento da referencia fornecida '+package['manifest']['version']+'.']
            if status in STATES[2:5]: status=STATES[1]
        source_evidence=([{'package':package['origin'],'version':package['manifest']['version'],
            'archiveSha256':package['archiveSha256'],'path':reference['path'],
            'sha256':reference['sha256'],'comparison':reference['comparison']}]
            if reference else [{'repository':refs['upstream']['repository'],'commit':refs['upstream']['commit'],
                'path':path,'blob':record['sha']}])
        related_assets=([entry['path'] for entry in package['files'] if entry['path']==reference['path'].removesuffix('.js')+'.png'
            or entry['path'].startswith(reference['path'].removesuffix('.js')+'_')] if reference else
            [entry['path'] for entry in refs['upstreamExampleFiles'] if entry['path'].startswith(path.removesuffix('.js')+'_') or entry['path']==path.removesuffix('.js')+'.png'])
        rows.append(dict(id=rel,category=category(rel),plannedLot=lot(category(rel)),
            javaScript={'path':path, 'forkPath': 'Split/Bunny-Loader/'+path if path in fork else None,
                'upstreamBlob':record['sha'], 'forkBlob':fork.get(path,{}).get('sha'),
                'differentFromFork':path in fork and record['sha']!=fork[path]['sha'], **detail},
            cSharp=cs,requiredApis=sorted(set(bases+detail['hooksAndMethods']+detail['nativeSignatures'])),
            status=status,assets=state.get('assets',related_assets),automatedEvidence=state.get('automatedEvidence',[]),preservedFeatures=state.get('preservedFeatures',[]),missingFeatures=missing,
            nativeDependencies=state.get('nativeDependencies',dependencies),
            automatedTests=state.get('automatedTests',[]),
            terrariaTests=state.get('terrariaTests',[
                'Comparar cada hook/propriedade listado com o comportamento JS original no Terraria 1.4.5.8.6.',
                'Carregar junto de um mod JS; entrar/sair do mundo e conferir falhas e desempenho.',
                'Verificar persistencia, remocao do mod e morte/respawn quando o exemplo mantiver estado.']),
            validation=state.get('validation','Sem validacao funcional C# deste exemplo inteiro.'),
            deviceEvidence=state.get('deviceEvidence',[]),
            technicalBlockEvidence=state.get('technicalBlockEvidence',[]),
            sourceEvidence=source_evidence))
    groups=defaultdict(Counter)
    for row in rows: groups[row['category']][row['status']]+=1
    coverage={}
    for cat,count in sorted(groups.items()):
        total=sum(count.values());implemented=sum(count[state] for state in STATES[2:5])
        coverage[cat]=dict(total=total,states=dict(count),implemented=implemented,validated=count[STATES[4]],automated=count[STATES[3]]+count[STATES[4]],automatedPercent=round(100*(count[STATES[3]]+count[STATES[4]])/total,2),
            partial=count[STATES[1]],implementedPercent=round(100*implemented/total,2),
            validatedPercent=round(100*count[STATES[4]]/total,2),
            partialPercent=round(100*count[STATES[1]]/total,2),
            limitations=count[STATES[5]],missingExamples=total-implemented)
    csharp=[]
    csroot=ROOT/'samples/ExampleModCSharp/Playable/content'
    for p in sorted(csroot.rglob('*.cs')):
        body=p.read_text();rel=p.relative_to(ROOT).as_posix()
        csharp.append({'path':rel,'classes':re.findall(r'\bclass\s+(\w+)',body),
            'mappedToJs':[r['id'] for r in rows if rel in r['cSharp']],
            'deviceEvidence':evidence.get('additionalCSharpDeviceEvidence',{}).get(rel,[]),
            'note':'Compilar nao comprova equivalencia com JS nem desempenho no Android.'})
    upstreamCore={x['path']:x for x in refs['upstreamCoreFiles']}
    forkCore={x['path']:x for x in refs['forkCoreFiles']}
    core=dict(sharedUnchanged=sum(p in upstreamCore and upstreamCore[p]['sha']==x['sha'] for p,x in forkCore.items()),
        modified=sorted(p for p,x in forkCore.items() if p in upstreamCore and upstreamCore[p]['sha']!=x['sha']),
        forkOnly=sorted(set(forkCore)-set(upstreamCore)),upstreamOnly=sorted(set(upstreamCore)-set(forkCore)))
    return dict(schemaVersion=1,references={**{k:refs[k] for k in ('fork','upstream','initialReferenceCount')},
                **({'package':{'version':package['manifest']['version'],'sha256':package['archiveSha256']}} if package else {})},
        counts={'forkJsFiles':len(fork),'canonicalJsFiles':len(originals),'newUpstreamFiles':len(set(originals)-set(fork)),
                'cSharpSourceFiles':len(csharp)},statePolicy='Validado no Android exige evidencia de jogo do exemplo inteiro; Validado por testes automatizados nao prova execucao no aparelho.',
        coverageByCategory=coverage,examples=rows,additionalCSharp=csharp,coreComparison=core,
        deviceBaseline=evidence.get('deviceBaseline',{}))

def markdown(report):
    lines=['# Matriz completa ExampleModBunny → C#','',
        f"Referencia canonica: **{report['counts']['canonicalJsFiles']} JS**, fork: **{report['counts']['forkJsFiles']} JS**, referencia inicial informada: **{report['references']['initialReferenceCount']} JS**.",
        'As porcentagens contam arquivos JS completos; portas parciais ficam em coluna propria. Compilacao nunca promove Validado.',
        'O JSON ao lado conserva APIs, propriedades, dependencias, testes, lacunas e evidencias por arquivo.','',
        '| Categoria | Total | Implementados | Validados | Parciais | Limitacoes tecnicas |',
        '| --- | ---: | ---: | ---: | ---: | ---: |']
    for cat,v in report['coverageByCategory'].items():
        lines.append(f"| {cat} | {v['total']} | {v['implemented']} ({v['implementedPercent']}%) | {v['validated']} ({v['validatedPercent']}%) | {v['partial']} ({v['partialPercent']}%) | {v['limitations']} |")
    lines+=['','## Todos os arquivos','', '| Original JS | C# | Estado | Falta | Testes / evidencia |', '| --- | --- | --- | --- | --- |']
    for r in report['examples']:
        cells=[r['id'],'; '.join(r['cSharp']) or '—',r['status'],'; '.join(r['missingFeatures']) or 'Aguardando roteiro Android',
               '; '.join(r['automatedTests']) or r['validation']]
        lines.append('| '+' | '.join(s.replace('|','/').replace('\n',' ') for s in cells)+' |')
    lines+=['','## Evidencia e continuidade','',
        'A confirmacao Android da .90.27.6 cobre Shield/Shoes/chamas e a barreira existente; nao prova todas as mecanicas dos arquivos JS ExampleShield/ExampleBoots.',
        'Nenhuma ausencia de bridge C# e classificada como limitacao tecnica do Terraria sem prova.',
        'Plano global: ../../../../../docs/csharp-completion-plan.md. Evidencias editaveis: tools/example_completion/evidence.json.',
        'Gerar/conferir: python3 tools/example_completion_audit.py --upstream-root <checkout-original> --write/--check.','']
    return '\n'.join(lines)

def main():
    p=argparse.ArgumentParser();p.add_argument('--upstream-root',type=Path,required=True)
    p.add_argument('--reference-package',type=Path,help='Import a .bl inventory; requires --write, never executes JS')
    g=p.add_mutually_exclusive_group(required=True);g.add_argument('--write',action='store_true');g.add_argument('--check',action='store_true')
    args=p.parse_args()
    if args.reference_package:
        if not args.write: p.error('--reference-package requires --write')
        from example_completion.reference_package import inventory
        snapshot=inventory(args.reference_package,args.upstream_root,facts)
        (SPEC/'package-reference.json').write_text(json.dumps(snapshot,ensure_ascii=False,indent=2)+'\n')
    report=build(args.upstream_root)
    data={'matrix.json':json.dumps(report,ensure_ascii=False,indent=2)+'\n','matrix.md':markdown(report)}
    OUT.mkdir(parents=True,exist_ok=True)
    for name,body in data.items():
        # Ship the same evidence in the editable ExampleMod; stale bundled
        # documentation previously lagged behind the repository matrix.
        bundled = ROOT/'samples/ExampleModCSharp/Playable/content/Documentation'/name.replace('matrix.', 'COMPATIBILIDADE.')
        for path in (OUT/name, bundled):
            if args.write:path.write_text(body)
            elif not path.is_file() or path.read_text()!=body:raise ValueError('Matriz desatualizada: '+str(path))
    print(json.dumps(report['counts'],ensure_ascii=False))

if __name__=='__main__':main()
