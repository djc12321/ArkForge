'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),os=require('node:os'),path=require('node:path'),vm=require('node:vm');
const ts=require('C:/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/ets/build-tools/ets-loader/node_modules/typescript');
const root=fs.mkdtempSync(path.join(os.tmpdir(),'arkforge-signing-'));let tick=Date.UTC(2026,9,23),bundle='com.test.app';const secrets=new Map();
const asset={Tag:{ALIAS:1,SECRET:2,CONFLICT_RESOLUTION:3,RETURN_TYPE:4},ConflictResolution:{OVERWRITE:1},ReturnType:{ALL:1},
  add:async value=>secrets.set(String(value.get(1)),value.get(2)),query:async value=>{const secret=secrets.get(String(value.get(1)));if(!secret)throw new Error('missing');return [new Map([[2,secret]])];}};
const fileIo={OpenMode:{READ_ONLY:0},accessSync:fs.existsSync,mkdirSync:p=>fs.mkdirSync(p,{recursive:true}),
  openSync:p=>({fd:fs.openSync(p,'r')}),closeSync:h=>fs.closeSync(h.fd),statSync:fd=>fs.fstatSync(fd),
  readTextSync:p=>fs.readFileSync(p,'utf8'),readSync:(fd,bytes)=>fs.readSync(fd,Buffer.from(bytes),0,bytes.byteLength,null),
  copyFileSync:(fd,p)=>fs.writeFileSync(p,fs.readFileSync(fd)),unlinkSync:fs.unlinkSync};
const util={TextEncoder:class {encodeInto(s){return new TextEncoder().encode(s);}},TextDecoder:class {decodeToString(b){return new TextDecoder().decode(b);}}};
const certificate={getNotBeforeTime:()=> '20000101000000Z',getNotAfterTime:()=> '20990101000000Z'};
const sandbox={exports:{},Date:class extends Date {static now(){return ++tick;}},Map,Uint8Array,ArrayBuffer,JSON,
  require:name=>name.includes('AssetStore')?{asset}:name.includes('ArkTS')?{util}:name.includes('CoreFile')?{fileIo}:
    name.includes('DeviceCertificate')?{cert:{EncodingFormat:{FORMAT_DER:0,FORMAT_PEM:1},createX509Cert:async()=>certificate}}:
    name==='libsigntool.so'?{default:{runSigner:async args=>{fs.writeFileSync(args[args.indexOf('-outFile')+1],JSON.stringify({'bundle-info':{'bundle-name':bundle},validity:{'not-before':0,'not-after':4e9}}));return 0;}}}:
    name.includes('AtomicFiles')?{AtomicFiles:{write:(p,s)=>fs.writeFileSync(p,s)}}:{}};
vm.runInNewContext(ts.transpileModule(fs.readFileSync('entry/src/main/ets/services/SigningMaterials.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2020}}).outputText,sandbox);
const signing=sandbox.exports.SigningMaterials;
(async()=>{
  const inputs=['key.p12','app.cer','profile.p7b'].map(name=>{const p=path.join(root,name);fs.writeFileSync(p,Buffer.from([1,2,3]));return p;});
  const credentials={keyAlias:'alias',storePassword:'not-on-disk',keyPassword:'not-on-disk',signAlg:'SHA256withECDSA'};
  await signing.importFiles({filesDir:root},inputs,credentials,'com.test.app');
  const logical=root+'/native-toolchain/signing/com.test.app',active=fs.readFileSync(logical+'/active.json','utf8');
  assert.equal((await signing.load(logical)).storePassword,'not-on-disk');
  assert.equal(await signing.validate(logical,'com.test.app'),new Date(4e12).toISOString(),'effective expiry must use the earlier Profile limit');
  const directory=signing.directory(logical);assert(!fs.existsSync(directory+'/provision.json'));
  for(const name of fs.readdirSync(directory))assert(!fs.readFileSync(directory+'/'+name).includes(Buffer.from('not-on-disk')));
  bundle='com.other.app';await assert.rejects(signing.importFiles({filesDir:root},inputs,credentials,'com.test.app'),/bundleName/);
  assert.equal(fs.readFileSync(logical+'/active.json','utf8'),active,'failed import must preserve old signing');
  await assert.rejects(signing.importFiles({filesDir:root},inputs,credentials,'../escape'),/无效/);
  console.log('PASS: manual signing import, encrypted passwords, no plaintext transport JSON, bundle mismatch and atomic preservation of existing configuration.');
})().catch(error=>{console.error(error);process.exitCode=1;}).finally(()=>{
  assert.equal(path.dirname(root),os.tmpdir());assert(path.basename(root).startsWith('arkforge-signing-'));fs.rmSync(root,{recursive:true});
});
