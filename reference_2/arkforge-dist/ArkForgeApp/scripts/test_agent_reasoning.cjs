const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
let participant,provider,requests=[],checkpoint,mode='auto';
class Part{constructor(value){this.value=value;}}
const token={isCancellationRequested:false,onCancellationRequested:()=>({dispose(){}})};
const uri={toString:()=> 'arkforge://one/workspace',scheme:'arkforge',authority:'one'};
const vscode={EventEmitter:class{},ThemeIcon:class{},LanguageModelTextPart:Part,
 workspace:{workspaceFolders:[{uri}],getConfiguration:()=>({get:(name,fallback)=>name==='reasoningMode'?mode:fallback})},
 Uri:{joinPath:()=>uri},commands:{registerCommand:()=>({})},
 lm:{registerTool:()=>({}),selectChatModels:async()=>[],registerLanguageModelChatProvider:(id,p)=>{provider=p;return {};},invokeTool:async()=>({content:[new Part('current source') ]})},
 chat:{createChatParticipant:(id,handler)=>{participant=handler;return {};}}};
let queue=[];
const native=async(method,args)=>{
 if(method==='agent_config_get')return JSON.stringify({hasKey:true,base:'https://api.deepseek.com',model:'deepseek-flash'});
 if(method==='chat'){requests.push(JSON.parse(args.body));return JSON.stringify(queue.shift());}
 throw Error(method);
};
const sandbox={exports:{},TextEncoder,require:name=>name==='vscode'?vscode:name==='./tools.json'?require('../workbench/extension/tools.json'):require('../workbench/extension/'+name.slice(2))};
vm.runInNewContext(fs.readFileSync('workbench/extension/nativeChat.js','utf8'),sandbox);
sandbox.exports.registerNativeChat({subscriptions:[],globalState:{update:async(key,state)=>{checkpoint=state;},get:()=>undefined}},native,()=>{}, {info(){},error(){}});
const stream={progress(){},markdown(text){assert(!text.includes('private-reasoning'));},reference(){},button(){}};
const done={choices:[{finish_reason:'stop',message:{content:'inspected',reasoning_content:'final-private-reasoning'}}]};
(async()=>{
 queue=[{choices:[{finish_reason:'tool_calls',message:{reasoning_content:'private-reasoning',tool_calls:[{id:'a',function:{name:'read_file',arguments:'{"path":"a.ets"}'}}]}}]},done];
 const result=await participant({prompt:'inspect a file',references:[]},{history:[]},stream,token);
 assert.equal(result.metadata.taskState.status,'completed');assert.equal(requests.length,2);
 assert.equal(requests[0].thinking.type,'enabled');assert.equal(requests[0].reasoning_effort,'high');
 assert.equal(requests[1].messages.find(m=>m.role==='assistant').reasoning_content,'private-reasoning');
 assert.equal(checkpoint.messages.find(m=>m.content==='inspected').reasoning_content,'final-private-reasoning');
 queue=[done];requests=[];
 await provider.provideLanguageModelChatResponse({},[],{}, {report(){}},token);
 assert.equal(requests[0].thinking.type,'disabled','generic VS Code LM bridge does not transport provider-specific reasoning fields');
 mode='fast';queue=[done];requests=[];
 await participant({prompt:'inspect',references:[]},{history:[]},stream,token);assert.equal(requests[0].thinking.type,'disabled');
 console.log('PASS: native Agent enables DeepSeek thinking, preserves provider reasoning across tools/checkpoints, keeps reasoning out of UI, and supports fast mode.');
})().catch(error=>{console.error(error);process.exitCode=1;});
