const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const ts=require('C:/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/ets/build-tools/ets-loader/node_modules/typescript');
let destroyed=0,status=400;
const key='test-provider-key-that-must-stay-private';
const native={exports:{},require:name=>name==='@kit.NetworkKit'?{http:{RequestMethod:{POST:'POST'},HttpDataType:{STRING:0},createHttp:()=>({request:async()=>({responseCode:status,result:JSON.stringify({error:{message:'maximum context length exceeded; '+key+' sk-123456789012345678901234'}})}),destroy:()=>destroyed++})}}:{},JSON};
vm.runInNewContext(ts.transpileModule(fs.readFileSync('entry/src/main/ets/services/AgentModel.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2020}}).outputText,native);
(async()=>{
 const model=native.exports.AgentModel;
 for(status of [400,401])await assert.rejects(model.request({},JSON.stringify({base:'https://provider.example',key,body:'{}'})),error=>{assert.match(error.message,/maximum context length exceeded/);assert.match(error.message,new RegExp('HTTP '+status));assert(!error.message.includes(key));assert(!error.message.includes('sk-123'));return true;});
 assert.equal(destroyed,2);assert.equal(model.active,undefined);
 console.log('PASS: native HTTP errors retain context-limit evidence for recovery, redact provider keys and release each request.');
})().catch(error=>{console.error(error);process.exitCode=1;});
