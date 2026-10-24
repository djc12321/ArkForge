'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const ts=require('C:/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/ets/build-tools/ets-loader/node_modules/typescript');
const commands=new Map(),calls=[],lines=[];let chosen='start',saved=true,current={connected:false,paused:false,status:'未连接',frames:[],variables:[]};
const vscode={commands:{registerCommand:(name,run)=>{commands.set(name,run);return {dispose(){}};}},ProgressLocation:{Notification:1},
 workspace:{saveAll:async()=>saved},window:{createOutputChannel:()=>({appendLine:text=>lines.push(text),show(){},dispose(){}}),
 showQuickPick:async items=>items.find(x=>x.action===chosen),withProgress:async(_,run)=>run(),showErrorMessage:message=>lines.push(message)}};
const extension={exports:{},require:()=>vscode};vm.runInNewContext(fs.readFileSync('workbench/extension/deviceDebug.js','utf8'),extension);
extension.exports.registerDeviceDebug({subscriptions:[]},async(method,args)=>{calls.push({method,args});return JSON.stringify(current);});
const run=()=>commands.get('arkforge.debug')();
(async()=>{
 await run();assert.equal(calls.at(-2).method,'debug_action');assert.equal(calls.at(-2).args.action,'start');
 saved=false;calls.length=0;await run();assert(!calls.some(x=>x.method==='debug_action'));assert(lines.some(x=>x.includes('保存失败')));
 saved=true;current={...current,connected:true};chosen='Debugger.pause';await run();assert.equal(calls.at(-2).args.action,'Debugger.pause');
 current={...current,paused:true,frames:[{functionName:'onCreate',source:{path:'entry/Main.ets',line:12,column:3}}],variables:[{name:'count',value:'42'}]};chosen='state';await run();
 assert(lines.some(x=>x.includes('entry/Main.ets:12:3')));assert(lines.some(x=>x.includes('count = 42')));
 let installedProject='/other',socketCount=0;const order=[];
 const native={exports:{},setTimeout,clearTimeout,require(name){
  if(name.includes('CoreFileKit'))return {fileIo:{OpenMode:{},openSync:()=>({fd:1}),writeSync(){},closeSync(){},accessSync:path=>!path.endsWith('breakpoints.json'),readTextSync:()=>JSON.stringify({project:installedProject,root:'/build',bundleName:'com.other'})}};
  if(name.includes('AtomicFiles'))return {AtomicFiles:{write(){}}};return {};
 }};
 const baseRequire=native.require;
 native.require=name=>{
  if(name.includes('SourceMaps'))return {SourceMaps:class {}};
  if(name.includes('LocalDevice'))return {LocalDevice:{prepareDebugger:async()=> '1234',debugForward:async()=>{}}};
  if(name.includes('NetworkKit'))return {webSocket:{createWebSocket(){
   const listeners={},control=socketCount++===0;let opened=false;
   return {on:(event,callback)=>listeners[event]=callback,connect:async(url,options)=>{assert.equal(options.proxy,'no-proxy');assert.equal(options.pingInterval,0);setTimeout(()=>{opened=true;listeners.open();},1);return true;},send:async text=>{
    assert(opened,'connect request resolution must not be treated as handshake completion');
    const message=JSON.parse(text);
    if(control)listeners.message({code:0},JSON.stringify({type:'addInstance',instanceId:0}));
    else {
     order.push(message.method);
     if(message.method==='Debugger.pause')listeners.message({code:0},JSON.stringify({method:'Debugger.paused',params:{reason:'other',callFrames:[]}}));
     else listeners.message({code:0},JSON.stringify({id:message.id,result:{}}));
    }
   },close:async()=>{}};
  }}};
  return baseRequire(name);
 };
 vm.runInNewContext(ts.transpileModule(fs.readFileSync('entry/src/main/ets/services/DeviceDebugger.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2020}}).outputText,native);
 const debuggerInstance=new native.exports.DeviceDebugger(()=>{});debuggerInstance.stop=async()=>{};
 await assert.rejects(()=>debuggerInstance.start({filesDir:'/files'},'/current'),/其他工程/);
 installedProject='/current';
 await debuggerInstance.start({filesDir:'/files',startAbility:async()=>order.push('editor')},'/current');
 assert(order.indexOf('Runtime.runIfWaitingForDebugger')<order.indexOf('editor'));
 await debuggerInstance.action('Debugger.pause');
 assert.equal(debuggerInstance.state.paused,true,'pause event confirms completion even before a separate response');
 console.log('PASS: native debug commands, save failure, pause and stack/variable output; cross-project installed targets are rejected before launch.');
})().catch(error=>{console.error(error);process.exitCode=1;});
