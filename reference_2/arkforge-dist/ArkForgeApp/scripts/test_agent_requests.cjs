const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
let provider,requests=[],delays=[],queue=[];
const token={isCancellationRequested:false,onCancellationRequested:()=>({dispose(){}})};
class Text { constructor(value){this.value=value;} }
const vscode={CancellationError:class extends Error{},EventEmitter:class{constructor(){this.event=()=>{};}},ThemeIcon:class{},
  LanguageModelTextPart:Text,LanguageModelToolCallPart:class{},
  workspace:{workspaceFolders:[]},commands:{registerCommand(){return {};}},
  lm:{registerTool(){return {};},selectChatModels:async()=>[],registerLanguageModelChatProvider(id,p){provider=p;return {};}},
  chat:{createChatParticipant(){return {};}}};
const native=async(method,args)=>{
  if(method==='agent_config_get')return JSON.stringify({hasKey:true,model:'test',base:'https://example.test'});
  if(method==='chat') {requests.push(JSON.parse(args.body));const next=queue.shift();if(next instanceof Error)throw next;return JSON.stringify(next);}
  throw new Error(method);
};
const engine=require('../workbench/extension/agentLoop');
const sandbox={exports:{},TextEncoder,require:name=>name==='vscode'?vscode:name==='./agentContext'?require('../workbench/extension/agentContext'):name==='./agentLoop'?{...engine,cancellableDelay:async(ms,t)=>{assert(!t.isCancellationRequested);delays.push(ms);}}:[]};
vm.runInNewContext(fs.readFileSync('workbench/extension/nativeChat.js','utf8'),sandbox);
sandbox.exports.registerNativeChat({subscriptions:[]},native,()=>{}, {info(){},error(){}});
const done={choices:[{finish_reason:'stop',message:{content:'verified'}}]};
const ask=()=>provider.provideLanguageModelChatResponse({},[],{}, {report(){}},token);
(async()=>{
  queue=[new Error('HTTP 503'),new Error('HTTP 429'),done];await ask();assert.equal(requests.length,3);assert.deepEqual(delays,[1000,3000]);
  requests=[];delays=[];queue=[{choices:[{finish_reason:'length',message:{tool_calls:[{id:'x',function:{name:'write_file',arguments:'{"incomplete'}}]}}]},done];
  await ask();assert.equal(requests[0].max_tokens,8192);assert.equal(requests[1].max_tokens,16384);
  requests=[];queue=[new Error('HTTP 401')];await assert.rejects(ask(),/401/);assert.equal(requests.length,1);
  requests=[];queue=[{choices:[{finish_reason:'insufficient_system_resource',message:{content:'partial'}}]},done];await ask();assert.equal(requests.length,2);
  requests=[];token.isCancellationRequested=true;await assert.rejects(ask());assert.equal(requests.length,0);
  console.log('PASS: temporary HTTP failures retry only model requests; truncated tools are discarded before execution; auth errors and cancellation stop; resource interruptions retry.');
})().catch(error=>{console.error(error);process.exitCode=1;});
