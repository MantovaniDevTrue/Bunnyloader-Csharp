"""Inventory every shipped original loader module; no compilation-based promotions."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
SOURCE=Path('app/src/main/cpp/script/js/mod/Loaders')
# Mappings are evidence pointers, never a claim of parity with the JS module.
MAPPING={
 'BuffLoader':(['BunnyBuff','BunnyBuffPotion'],['experiments/csharp/NativeContent.cpp','content/buffs/ModBuffs.cpp']),
 'ItemLoader':(['BunnyItem','BunnyWeapon','BunnyAccessory','BunnyShootSource','BunnyShootContext','BunnyShot'],['experiments/csharp/NativeBasicItems.cpp','experiments/csharp/NativeItemPolicies.cpp','experiments/csharp/NativeShooting.cpp']),
 'EquipLoader':(['BunnyItemBehavior','BunnyArmorEffects','BunnyArmorSet'],['experiments/csharp/NativeItemPolicies.cpp','experiments/csharp/NativeArmorPolicies.cpp']),
 'PlayerLoader':(['BunnyPlayer'],['experiments/csharp/NativeContent.cpp','experiments/csharp/ManagedRuntime.cpp']),
 'PlayerUpdateHooks':(['BunnyPlayer'],['experiments/csharp/NativeContent.cpp']),
 'PlayerHurtHooks':(['BunnyPlayer'],['experiments/csharp/ManagedRuntime.cpp']),
 'ProjectileLoader':(['BunnyProjectile','BunnyMinionSteering','BunnyMinionMovement'],['experiments/csharp/NativeContent.cpp','experiments/csharp/ManagedRuntime.cpp','experiments/csharp/ProjectilePolicy.h','experiments/csharp/SummonPolicy.h']),
 'NPCLoader':(['BunnyNPC'],['experiments/csharp/NativeContent.cpp','experiments/csharp/ManagedRuntime.cpp']),
 'PrefixLoader':(['BunnyPrefix'],['experiments/csharp/NativeContent.cpp']),
 'WallLoader':(['BunnyWall','BunnyWorldLight'],['experiments/csharp/NativeWallRuntime.cpp','content/walls/ModWalls.cpp']),
 'TileLoader':(['BunnyTile'],['experiments/csharp/NativeTileGameplay.cpp']),
 'RecipeLoader':(['BunnyRecipeLoader','BunnyRecipeSource','BunnyRecipe','BunnyRecipeGroup','BunnyRecipeBehavior','BunnyRecipeCondition'],['content/recipes/NativeRecipes.cpp','experiments/csharp/NativeRecipeCallbacks.cpp']),
 'PlayerDrawHooks':(['BunnyVisual'],['experiments/csharp/NativeVisualRenderer.cpp']),
}

def build(root):
    cpp=root/'app/src/main/cpp'
    declarations='\n'.join(p.read_text() for p in (root/'experiments/csharp/BunnyCsSdk').glob('*.cs'))
    sdkTypes=set(re.findall(r'public\s+(?:(?:abstract|sealed|static|readonly|partial)\s+)*(?:class|struct|enum|interface|record)\s+(\w+)',declarations))
    rows=[]
    for file in sorted((root/SOURCE).glob('*.js')):
        name=file.stem;apis,bridges=MAPPING.get(name,([],[]))
        for bridge in bridges:
            if not (cpp/bridge).is_file():raise ValueError('Missing evidence: '+bridge)
        for api in apis:
            if api not in sdkTypes:raise ValueError('Unknown SDK API: '+api)
        text=file.read_text()
        hooks=sorted(set(re.findall(r"(?:has|Overrides)\([^\n)]*?['\"]([A-Za-z][A-Za-z0-9_]*)['\"]",text)))
        rows.append(dict(original=file.relative_to(root).as_posix(),sha256=hashlib.sha256(file.read_bytes()).hexdigest(),
            module=name,hookCandidates=hooks,publicApis=apis,nativeEvidence=bridges,
            state='Parcial' if apis else 'Auditoria pendente',
            verifiedParity=False,android='Não validado integralmente',
            nextStep='Conferir cada hook/propriedade e seus exemplos antes de promover a cobertura' if apis else 'Investigar contratos originais; implementar SDK, bridge e exemplos em conjunto'))
    if not rows:raise ValueError('No loader modules found')
    return dict(schemaVersion=1,reference='Bunnyloader-Csharp fork, source hash per module',
        preservedBase='77be9001c2e0328324bb9b60db0fd3a7a64ad23a',
        meaning='Inventory includes helpers in Loaders. Hook candidates are lexical hints, not complete contracts. APIs are evidence pointers, not a parity certificate.',
        counts=dict(modules=len(rows),partialMappings=sum(bool(r['publicApis']) for r in rows),verifiedComplete=0),modules=rows)

def render(data):
    s='# Auditoria dos loaders C#\n\n'
    s+=f"{data['counts']['modules']} módulos originais contabilizados, inclusive helpers. Nenhum módulo é certificado como completo por esta auditoria.\n\n"
    s+='As APIs indicadas já possuem caminhos reais, mas ainda precisam de comparação hook a hook. O JSON registra hash e caminho de todos os arquivos. Ausência de mapeamento significa auditoria pendente, não prova de que o Terraria impede o recurso.\n\n'
    s+='| Módulo original | API C# relacionada | Estado |\n| --- | --- | --- |\n'
    for r in data['modules']:s+=f"| `{r['module']}` | {', '.join(r['publicApis']) or '—'} | {r['state']} |\n"
    s+='\nPrioridades: receitas/itens e seus hooks; buffs/minions/projéteis; NPCs/bosses; tiles/móveis; montarias; biomas/ambiente; sistemas/UI/persistência. Preservar os 206 exemplos JS e o inventário oficial; este documento não substitui as matrizes funcionais. Não declarar liberdade equivalente à API PC enquanto persistirem lacunas.\n'
    return s

def main():
    p=argparse.ArgumentParser();p.add_argument('--write',action='store_true');p.add_argument('--check',action='store_true');args=p.parse_args()
    data=build(ROOT);directory=ROOT.parents[1]/'docs/generated';directory.mkdir(parents=True,exist_ok=True)
    outputs={directory/'csharp-loaders.json':json.dumps(data,ensure_ascii=False,indent=2)+'\n',directory/'csharp-loaders.md':render(data)}
    for path,body in outputs.items():
        if args.write:path.write_text(body)
        elif not path.exists() or path.read_text()!=body:raise SystemExit('Loader inventory stale: '+str(path))
    print('LOADER_AUDIT modules='+str(len(data['modules']))+' verifiedComplete=0')
if __name__=='__main__':main()
