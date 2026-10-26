const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const ts=require('C:/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/ets/build-tools/ets-loader/node_modules/typescript');
const files=new Map(),directories=new Set(),removed=[];
const fileIo={accessSync:p=>files.has(p)||directories.has(p),mkdirSync:p=>directories.add(p),readTextSync:p=>{if(!files.has(p))throw Error(p);return files.get(p);},listFileSync:p=>[...files.keys()].filter(n=>n.startsWith(p+'/')&&!n.slice(p.length+1).includes('/')).map(n=>n.slice(p.length+1)),lstatSync:()=>({isFile:()=>true}),unlinkSync:p=>{removed.push(p);files.delete(p);}};
function load(name,deps){const box={exports:{},require:n=>deps[n]||{},Date,setTimeout:f=>{f();return 1;},clearTimeout(){}};vm.runInNewContext(ts.transpileModule(fs.readFileSync('entry/src/main/ets/services/'+name+'.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2020}}).outputText,box);return box.exports[name];}
const context={filesDir:'/files',tempDir:'/temp',applicationInfo:{name:'com.djc.arkforge'},resourceManager:{getRawFileContent:async()=>Buffer.from(JSON.stringify({sdkVersion:'26',sha256:'ABC',partialUpdateConfig:{optimizeComponent:true}}))}};
const built={id:String(new Date(2026,9,26,12).getTime()),bundleName:'com.target.app',abilityName:'EntryAbility',root:'/files/offline-build/1',project:'/project',compilerSha256:'ABC',sources:[{path:'entry/src/Index.ets',sha256:'source'}]};
files.set('/files/offline-build/latest.json',JSON.stringify(built));files.set('/project/entry/src/Index.ets','source code');files.set(built.root+'/source-maps.json',JSON.stringify([{path:'entry/src/Index.ets',javascript:'/generated.js',mapFile:'/map.json'}]));files.set('/generated.js','generated code');files.set('/map.json',JSON.stringify({sourceMap:JSON.stringify({version:3,sources:['Index.ets'],mappings:'AAAA'}),generatedLineMap:[0]}));
const SourceMaps=load('SourceMaps',{'@kit.CoreFileKit':{fileIo}});
const RuntimeDiagnostics=load('RuntimeDiagnostics',{'@kit.CoreFileKit':{fileIo,hash:{hash:async()=> 'source'}},'@kit.ArkTS':{util:{TextDecoder:class{decodeToString(b){return Buffer.from(b).toString();}}}},'@ohos.deviceInfo':{default:{sdkApiVersion:26,deviceType:'tablet'}},'./SourceMaps':{SourceMaps}});
const faultDir='/temp/runtime-faults/com.target.app/faultlogger';
const current='jscrash-com.target.app-1-20261026120100000.log',historical='jscrash-com.target.app-1-20261026115900000.log';
let live=false,transfer=true;
const run=async command=>{if(command.includes('ARKFORGE_PID='))return 'ARKFORGE_PID='+(live?'123':'');if(command.includes('hilog'))return 'pairing token abcdef Bearer secret sk-123456789012345678901234';if(command.includes('file recv')){directories.add(faultDir);files.set(faultDir+'/'+current,'Reason:TypeError\nat onTap (entry|entry|1.0|src/Index.ets:1:1)\nBearer secret');files.set(faultDir+'/'+historical,'Reason:TypeError\nat onTap (entry|entry|1.0|src/Index.ets:1:1)');files.set(faultDir+'/jscrash-com.other.app-1.log','unrelated');return transfer?'FileTransfer finish':'permission denied';}throw Error(command);};
(async()=>{
 const dead=await RuntimeDiagnostics.collect(context,run,'/project');assert.equal(dead.runningPid,'');assert.equal(dead.faultCollectionOk,true);assert.equal(dead.faults.length,2);assert.equal(dead.faults[0].frames[0].original.line,1);assert.equal(dead.faults[0].frames[0].sourceMatchesBuild,true);assert.equal(dead.faults[1].predatesBuild,true);assert.equal(dead.faults[1].frames.length,0);assert(removed.some(n=>n.includes('com.other.app')));assert(!JSON.stringify(dead).includes('Bearer secret'));assert(dead.installedCompilerMatches);
 live=true;transfer=false;const denied=await RuntimeDiagnostics.collect(context,run,'/project');assert.equal(denied.runningPid,'123');assert.equal(denied.faultCollectionOk,false);assert(denied.collectionNotes.some(n=>n.includes('permission denied')));assert(!denied.processLog.includes('abcdef'));assert(!denied.processLog.includes('sk-123'));await assert.rejects(RuntimeDiagnostics.collect(context,run,'/other'));

 let logsOk=true,newFault=false,alive=true,restored=true,collections=0,executions=0,badProtocol=false,failedStep=false;
 const collect=async()=>{collections++;return{lastArkForgeBuild:'1',runningPid:alive?'123':'',faultCollectionOk:logsOk,faults:newFault&&collections%2===0?[{file:'new-crash.log'}]:[]};};
 let sourceHash='source';
 const RuntimeUIScript=load('RuntimeUIScript',{});
 const UI=load('RuntimeUIFlow',{'@kit.CoreFileKit':{fileIo,hash:{hash:async()=>sourceHash}},'./RuntimeUIScript':{RuntimeUIScript},'./AtomicFiles':{AtomicFiles:{write:(p,text)=>files.set(p,text)}},'./RuntimeDiagnostics':{RuntimeDiagnostics:{collect,redact:s=>s}}});
 let activeLocal='',activeRemote='';
 const uiRun=async c=>{
  if(c.includes('file send')){activeLocal=/file send "([^"]+)\.sh"/.exec(c)[1];activeRemote=c.split(' ').at(-1).slice(0,-3);assert(files.get(activeLocal+'.sh').includes('sqlite3 :memory:'));return 'FileTransfer finish';}
  if(c.includes('shell sh ')){executions++;return 'start ability successfully';}
  if(c.includes('file recv')){
   if(c.includes('.result'))files.set(activeLocal+'.result',(badProtocol?'requestId:old':'requestId:'+activeLocal.split('/').at(-1))+'\n'+(failedStep?'error:selector-0':'step:0:clicked\nexpected:1')+'\nrestored:'+(restored?'1':'0')+'\ncomplete:1');
   else files.set(activeLocal+'.json',JSON.stringify({attributes:{},children:[{attributes:{id:'target',text:'Tap',bounds:'[0,0][20,20]'}}]}));
   return 'FileTransfer finish';
  }
  if(c.includes('shell aa start'))return 'start ability successfully';
  if(c.includes('shell rm')){assert(c.includes(activeRemote+'.sh'));return '';}
  throw Error(c);
 };
 const flow=async actions=>JSON.parse(await UI.run(context,'/project',JSON.stringify({actions,expected_id:'target'}),uiRun));
 assert.equal((await flow([{id:'target'}])).status,'passed');
 logsOk=false;assert.equal((await flow([{id:'target'}])).status,'failed');logsOk=true;
 newFault=true;assert.equal((await flow([{id:'target'}])).status,'failed');newFault=false;
 alive=false;assert.equal((await flow([{id:'target'}])).status,'failed');alive=true;
 failedStep=true;assert.equal((await flow([{id:'target'}])).status,'failed');failedStep=false;
 badProtocol=true;assert.equal((await flow([{id:'target'}])).status,'failed');badProtocol=false;
 restored=false;assert.equal((await flow([{id:'target'}])).status,'failed');restored=true;
 const accepted=await flow([{id:'target'}]);assert.equal(accepted.executionMode,'on-device-system-shell');assert.equal(accepted.backgroundBudgetMs,0);assert(accepted.recordedAt>0);
 const prior=executions;sourceHash='changed';await assert.rejects(flow([{id:'target'}]),/已安装版本与源码不同/);assert.equal(executions,prior);
 assert.equal(JSON.parse(files.get('/temp/runtime-ui/latest-result.json')).status,'passed');
 assert(![...files.keys()].some(p=>/runtime-ui\/\d+\.(sh|json|result)$/.test(p)),'per-run scratch files are removed');
 console.log('PASS: FaultLog/provenance/redaction; independent device-shell flow, failed steps, stale-result rejection, restore verification, crash/source checks and scratch cleanup.');
})().catch(e=>{console.error(e);process.exitCode=1;});
