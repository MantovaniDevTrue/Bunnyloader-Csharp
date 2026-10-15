const assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync(process.argv[2],'utf8');
function harness(first=15) {
 const jobs=[],requests=[],grows=[];
 const array=n=>{const a=new Array(n);a.cloneResized=m=>{const b=array(m);for(let i=0;i<Math.min(n,m);++i)b[i]=a[i];return b;};return a;};
 const shield=array(first),shoes=array(25);shield[0]={vanilla:true};
 const c={Mod:class {},EquipTexture:class {},EquipType:{Body:'Body',Legs:'Legs',Wings:'Wings'},
  Terraria:{ID:{ArmorIDs:{Shield:{Count:first},Shoe:{Count:25}}},GameContent:{TextureAssets:{AccShield:shield,AccShoes:shoes}}},
  ContentLookup:{RequestFile(path){requests.push(path);return {path};}},
  bl:{mod:{path:'/test'},path:{join:(a,b)=>a+'/'+b},file:{exists:()=>true},items:{growEquipSets:(kind,from,to)=>grows.push([kind,from,to])},log(){},
   csharp:new Proxy({},{get(){throw Error('JS equipment must not consume CSharp descriptors');}})},
  ModFiles:{Texture:t=>t+'.png'},Ready:{Add(fn){jobs.push(fn);}},Safe:{Run(_,fn){fn();}}};
 vm.runInNewContext(source+'\nglobalThis.Equip=EquipLoader;',c);
 return {c,jobs,requests,grows};
}
const h=harness(),E=h.c.Equip;
const shield={Type:6197},boots={Type:6198};
E.AddEquipTexture('Shield_Shield','Shield',shield,'Shield');
E.AddEquipTexture('Boots_Shoes','Shoes',boots,'Boots');
h.jobs[0]();
assert.equal(h.requests.length,2);assert.equal(h.c.Terraria.ID.ArmorIDs.Shield.Count,16);assert.equal(h.c.Terraria.ID.ArmorIDs.Shoe.Count,26);
const item={};E.Apply(item,6197);assert.equal(item.shieldSlot,15);E.Apply(item,6198);assert.equal(item.shoeSlot,25);
assert.equal(h.c.Terraria.GameContent.TextureAssets.AccShield[0].vanilla,true);
assert.equal(E.GetEquipSlot('Shield','Shield'),15);E.Install();assert.equal(h.requests.length,2);
const limit=harness(128);limit.c.Equip.AddEquipTexture('Overflow','Shield',{Type:6200},'Overflow');
assert.throws(()=>limit.jobs[0](),/limite/);assert.equal(limit.c.Terraria.ID.ArmorIDs.Shield.Count,128);
console.log('JS_EQUIPMENT_PRESERVED_WITHOUT_CSHARP_REGISTRATION=PASS');
