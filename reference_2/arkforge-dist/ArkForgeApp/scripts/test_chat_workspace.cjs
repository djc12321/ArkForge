const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
let modelProvider;let handler,folders=[{uri:{authority:'current',toString:()=> 'arkforge://current/workspace'}}],captured,readFailure=false;
class TextPart{constructor(value){this.value=value;}}
const vscode={CancellationError:class extends Error{},EventEmitter:class{constructor(){this.event=()=>{};}},ThemeIcon:class{},
  LanguageModelTextPart:TextPart,
  workspace:{get workspaceFolders(){return folders;}},
  lm:{registerTool(){return {};},registerLanguageModelChatProvider(id,p){modelProvider=p;return {};},selectChatModels:async()=>[]},
  commands:{registerCommand(){return {};}},
  chat:{createChatParticipant(id,callback){handler=callback;return {};}}};
const native=async(method,args)=>{
  if(method==='agent_config_get')return JSON.stringify({hasKey:true,model:'test',base:'https://example.test'});
  if(method==='chat'){captured=JSON.parse(args.body).messages;return JSON.stringify({choices:[{message:{content:'done'}}]});}
  throw new Error(method);
};
const sandbox={exports:{},TextEncoder,require:name=>name==='vscode'?vscode:[]};
vm.runInNewContext(fs.readFileSync('workbench/extension/nativeChat.js','utf8'),sandbox);
sandbox.exports.registerNativeChat({subscriptions:[]},native,async()=>{if(readFailure)throw new Error('read failed');return 'file';},{info(){},error(){}});
const stream={progress(){},markdown(){},reference(){}},token={isCancellationRequested:false,onCancellationRequested:()=>({dispose(){}})};
async function run(workspace){
  return handler({prompt:'current request',references:[]},{history:[{result:{metadata:{workspace,messages:[{role:'user',content:'old project secret'}]}}}]},stream,token);
}
(async()=>{
  let result=await run('arkforge://different/workspace');assert(!JSON.stringify(captured).includes('old project secret'));assert.equal(result.metadata.workspace,'arkforge://current/workspace');
  await run('arkforge://current/workspace');assert(JSON.stringify(captured).includes('old project secret'));
  await run(undefined);assert(!JSON.stringify(captured).includes('old project secret'));
  readFailure=true;
  result=await handler({prompt:'read',references:[{value:{uri:{scheme:'arkforge',authority:'current',path:'/workspace/a.ets'}}}]},{history:[]},stream,token);
  assert.equal(result.errorDetails.message,'read failed');
  captured=undefined;await assert.rejects(modelProvider.provideLanguageModelChatResponse({},[],{}, {report(){}},{...token,isCancellationRequested:true}));assert.equal(captured,undefined,'cancelled requests must not reach the model API');
  folders=[];result=await run('arkforge://current/workspace');assert(result.errorDetails);
  console.log('PASS: project-bound chat history, legacy-history isolation, reference errors and missing-workspace handling');
})().catch(error=>{console.error(error);process.exitCode=1;});
