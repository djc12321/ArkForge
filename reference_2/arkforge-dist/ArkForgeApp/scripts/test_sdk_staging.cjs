'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),os=require('node:os'),path=require('node:path'),crypto=require('node:crypto'),vm=require('node:vm');
const ts=require('C:/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/ets/build-tools/ets-loader/node_modules/typescript');
const directory=fs.mkdtempSync(path.join(os.tmpdir(),'arkforge-sdk-stage-'));
const marker='JSON.stringify(__load("/checker.cjs").check(__arkforgeOptions));';
let bundle='var __load=function(){return {check:function(){return {ok:true}}}};'+marker,writes=0;
const sha=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');
const fileIo={accessSync:fs.existsSync,mkdirSync:p=>fs.mkdirSync(p,{recursive:true}),readTextSync:p=>fs.readFileSync(p,'utf8'),
  OpenMode:{CREATE:0,WRITE_ONLY:0,TRUNC:0},openSync:p=>({fd:fs.openSync(p,'w')}),writeSync:(fd,bytes)=>{writes++;return fs.writeSync(fd,typeof bytes==='string'?bytes:Buffer.from(bytes));},closeSync:file=>fs.closeSync(file.fd)};
const sandbox={exports:{},require:name=>name.includes('CoreFile')?{fileIo,hash:{hash:async p=>sha(fs.readFileSync(p))}}:
  name.includes('ArkTS')?{util:{TextDecoder:class {decodeToString(bytes){return new TextDecoder().decode(bytes);}}}}:
  name.includes('AtomicFiles')?{AtomicFiles:{write:(p,text)=>{writes++;fs.writeFileSync(p,text);}}}:{}};
vm.runInNewContext(ts.transpileModule(fs.readFileSync('entry/src/main/ets/services/SDKProjectChecker.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2020}}).outputText,sandbox);
const checker=sandbox.exports.SDKProjectChecker;
const context={filesDir:directory,resourceManager:{getRawFileContent:async name=>new TextEncoder().encode(name.endsWith('.json')?JSON.stringify({sha256:sha(bundle)}):bundle)}};
(async()=>{
  const original=await checker.stage(context),target=original.replace('bundle.js','web.js');
  const first=fs.readFileSync(target,'utf8'),count=writes;
  assert.match(first,/__arkforgeCheckResult/);await checker.stage(context);assert.equal(writes,count,'unchanged SDK must not be rewritten');
  fs.writeFileSync(target,'corrupt');await checker.stage(context);assert.equal(fs.readFileSync(target,'utf8'),first,'corruption must regenerate');
  bundle='var changed=true;'+bundle;await checker.stage(context);assert.match(fs.readFileSync(target,'utf8'),/changed=true/);
  const count2=writes;await checker.stage(context,true);assert(writes>count2,'benchmark baseline must actually rewrite');
  bundle='invalid SDK wrapper';await assert.rejects(checker.stage(context),/Unsupported SDK bundle wrapper/);
  console.log('SDK staging: verified cache reuse, corrupt output recovery, SDK upgrade invalidation and unsupported wrapper rejection passed.');
})().catch(error=>{console.error(error);process.exitCode=1;}).finally(()=>{
  const root=path.join(directory,'sdk-check');if(fs.existsSync(root)){for(const name of fs.readdirSync(root))fs.unlinkSync(path.join(root,name));fs.rmdirSync(root);}fs.rmdirSync(directory);
});
