const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
// Arrays already contain a C# resource, as happens before JS Ready.Install.
const csharpAsset={owner:'csharp-uid'};
class NativeArray extends Array {
    cloneResized(length) { const copy=new NativeArray(length).fill(0); this.forEach((v,i)=>copy[i]=v); return copy; }
}
const seed=(value)=>{const a=new NativeArray(21).fill(0);a[20]=value;return a;};
const textures={Gore:seed(csharpAsset)};
const sets={SpecialAI:seed(3),DisappearSpeed:seed(2),DisappearSpeedAlpha:seed(4),IsDrip:seed(true)};
const safety={SafeGore:seed(true)};
const ready=[];
const mod={uuid:'javascript-uid',path:'/mods/js/content'};
const context={
    Terraria:{GameContent:{TextureAssets:textures,ChildSafety:safety},ID:{GoreID:{Sets:sets}}},
    ModFiles:{ListTextures:()=>['Content/Gores/ExampleTree_Leaf.png']},
    Ready:{Add:callback=>ready.push(callback)},Safe:{Run:(_name,callback)=>callback()},
    bl:{mod,loadTextureAsset:path=>({path,owner:mod.uuid}),log:()=>{}}
};
vm.runInNewContext(fs.readFileSync('app/src/main/cpp/script/js/mod/Loaders/GoreLoader.js','utf8')+
    '\nthis.Loader=GoreLoader;',context);
context.Loader.Autoload();context.Loader.Autoload();
for(const callback of ready)callback();
assert.equal(textures.Gore.length,22);
assert.equal(textures.Gore[20],csharpAsset);
assert.equal(sets.SpecialAI[20],3);assert.equal(sets.DisappearSpeed[20],2);
assert.equal(sets.DisappearSpeedAlpha[20],4);assert.equal(sets.IsDrip[20],true);
assert.equal(safety.SafeGore[20],true);
assert.equal(context.Loader.TypeOf(mod.uuid,'ExampleTree_Leaf'),21);
assert.equal(context.Loader.TypeOfAny('ExampleTree_Leaf'),21);
assert.equal(sets.DisappearSpeed[21],1);assert.equal(sets.DisappearSpeedAlpha[21],1);
assert.equal(textures.Gore[21].path,'/mods/js/content/Content/Gores/ExampleTree_Leaf.png');
console.log('ORIGINAL_JS_GORE_REGISTRATION_PRESERVES_NATIVE_CSHARP_ASSETS_AND_SETTINGS=PASS');
