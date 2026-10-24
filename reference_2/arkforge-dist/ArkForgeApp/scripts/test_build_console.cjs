const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const ts=require('C:/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/ets/build-tools/ets-loader/node_modules/typescript');
const sandbox={exports:{},require:name=>name.includes('PerformanceAnalysisKit')?{hilog:{info(){}}}:{}};
vm.runInNewContext(ts.transpileModule(fs.readFileSync('entry/src/main/ets/services/BuildConsole.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2020}}).outputText,sandbox);
const api=sandbox.exports.BuildConsole,console=Object.create(api.prototype),writes=[],entries=[];
// Use the real write method without native capture/timers; outputs must be durable before subscribers run.
Object.assign(console,{sequence:0,started:123,output:'text',events:'jsonl',appendFile:(p,t)=>writes.push({p,t}),notify:entry=>{assert.equal(writes.length,2);entries.push(entry);}});
const lines=Array.from({length:1000},(_,i)=>'official line '+i).concat(['file.ets:4:2: error: type mismatch','sign -keyPwd secret','[../nweb.cpp:1] telemetry']);
console.write(lines.join('\n'));
assert.equal(writes.length,2);assert.equal(entries.length,1002);assert.equal(entries[1000].level,'error');assert(!writes.some(w=>w.t.includes('secret')));assert.match(writes[0].t,/official line 999/);
assert.equal(writes[1].t.trim().split('\n').length,1002);assert.equal(entries[1001].id,'123-1002');
process.stdout.write('PASS: 1,002 official log lines persisted in two writes with ordering, error levels and credential masking preserved.\n');
