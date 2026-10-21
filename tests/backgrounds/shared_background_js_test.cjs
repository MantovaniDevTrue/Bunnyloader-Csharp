const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const registry = new Map([['csharp-uid/Assets/Textures/Backgrounds/CSharp', 3]]);
let next = 4, publications = 0;
const jsMod = {uuid:'js-uid',id:'jsmod',path:'/mods/js/content'};
const csharpMod = {uuid:'csharp-uid',id:'csharpmod',path:'/mods/csharp/content'};
const callbacks=[];
const context={
 Terraria:{Main:{maxBackgrounds:3}},
 ModFiles:{ListTextures:()=>['Assets/Textures/Backgrounds/Far.png','Assets/Textures/Backgrounds/Mid.png']},
 Ready:{Add:callback=>callbacks.push(callback)},
 ModRegistry:{Find:name=>name==='csharpmod'||name==='csharp-uid'?csharpMod:null,All:()=>[jsMod,csharpMod],ClassName:mod=>mod.id},
 bl:{mod:jsMod,reserveBackgroundTexture:(mod,path,file)=>{
   assert.equal(mod,jsMod.uuid);assert.equal(file,jsMod.path+'/'+path+'.png');
   const key=mod+'/'+path;if(!registry.has(key))registry.set(key,next++);return registry.get(key);
 },backgroundTextureSlot:(mod,path)=>registry.get(mod+'/'+path)??-1,
 backgroundTextureCount:()=>next,publishBackgroundTextures:()=>++publications}
};
const source=fs.readFileSync('app/src/main/cpp/script/js/mod/Loaders/BackgroundTextureLoader.js','utf8');
vm.runInNewContext(source+'\nthis.Loader=BackgroundTextureLoader;',context);
const loader=context.Loader;
loader.Autoload();loader.Autoload();
assert.equal(loader.GetBackgroundSlot(jsMod,'Assets/Textures/Backgrounds/Far'),4);
assert.equal(loader.AddBackgroundTexture(jsMod,'Assets/Textures/Backgrounds/Far.png'),4);
assert.equal(loader.GetBackgroundSlot('csharpmod/Assets/Textures/Backgrounds/CSharp'),3);
assert.equal(loader.TotalCount,6);
const ref={value:99};assert.equal(loader.TryGetBackgroundSlot('csharpmod/Missing',ref),false);assert.equal(ref.value,0);
for(const callback of callbacks)callback();assert.equal(publications,1);
assert.throws(()=>loader.AddBackgroundTexture(jsMod,'Late')); 
console.log('ORIGINAL_JS_BACKGROUND_AUTOLOAD_SHARED_NATIVE_SLOTS_AND_CSHARP_LOOKUP=PASS');
