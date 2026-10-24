const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),os=require('node:os'),vm=require('node:vm'),crypto=require('node:crypto');
const ts=require('C:/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/ets/build-tools/ets-loader/node_modules/typescript');
const directory=fs.mkdtempSync(path.join(os.tmpdir(),'arkforge-arkui-'));
const sha=text=>crypto.createHash('sha256').update(text).digest('hex');
const fileIo={accessSync:fs.existsSync,mkdirSync:p=>fs.mkdirSync(p,{recursive:true}),readTextSync:p=>fs.readFileSync(p,'utf8')};
const sandbox={exports:{},require:name=>name.includes('CoreFileKit')?{fileIo,hash:{hash:async p=>sha(fs.readFileSync(p))}}:{AtomicFiles:{write:(p,s)=>fs.writeFileSync(p,s)}}};
vm.runInNewContext(ts.transpileModule(fs.readFileSync('entry/src/main/ets/services/ArkUIWebRuntime.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2020}}).outputText,sandbox);
(async()=>{
 const api=sandbox.exports.ArkUIWebRuntime;
 const source=fs.readFileSync('entry/src/main/resources/rawfile/compiler/ui-transform.bundle.js','utf8');
 const bundle=path.join(directory,'ui-transform.bundle.js');fs.writeFileSync(bundle,source);
 const stage=()=>api.stage({filesDir:directory},bundle,sha(fs.readFileSync(bundle)));
 const target=await stage(),first=fs.readFileSync(target,'utf8'),mtime=fs.statSync(target).mtimeMs;
 await stage();assert.equal(fs.statSync(target).mtimeMs,mtime);
 fs.writeFileSync(target,'corrupt');await stage();assert.equal(fs.readFileSync(target,'utf8'),first);
 const names=['/project/harmony_demo/src/main/ets/pages/Index.ets','/project/harmony_demo/src/main/ets/harmonydemoability/HarmonyDemoAbility.ets'];
 const files=Object.fromEntries(names.map((n,i)=>[n,fs.readFileSync(i?'harmony_demo/src/main/ets/harmonydemoability/HarmonyDemoAbility.ets':'harmony_demo/src/main/ets/pages/Index.ets','utf8')]));
 const options={moduleName:'harmony_demo',bundleName:'com.example.harmonyplayground',files};
 function run(script,options,wrapped){const context={__arkforgeOptions:options,__arkforgeBatchFileNames:names,console:{log(){},warn(){},error(){}},setTimeout,clearTimeout};const value=vm.runInNewContext(script,context,{timeout:60000});return JSON.parse(wrapped?context.__arkforgeTransformResult:value);}
 const before=run(source,options,false),after=run(first,options,true);assert.equal(before.ok,true);assert.deepEqual(after,before,'browser wrapper must preserve JS, source maps and diagnostics');
 const invalid={...options,files:{...files,[names[0]]:'@Entry @Component struct Broken { build() { Text( } }'}};
 const beforeError=run(source,invalid,false),afterError=run(first,invalid,true);assert.equal(beforeError.ok,false);assert.deepEqual(afterError,beforeError);
 fs.writeFileSync(bundle,'invalid wrapper');await assert.rejects(stage,/Unsupported ArkUI bundle wrapper/);
 console.log('PASS: ArkUI wrapper preserves official transformed JS/source maps/errors; verified cache reuse and corruption recovery.');
})().catch(e=>{console.error(e);process.exitCode=1;}).finally(()=>{for(const d of ['arkui-runtime','']){const root=path.join(directory,d);for(const n of fs.readdirSync(root)){const p=path.join(root,n);if(fs.statSync(p).isFile())fs.unlinkSync(p);}if(d)fs.rmdirSync(root);}fs.rmdirSync(directory);});
