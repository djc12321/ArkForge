const assert = require('node:assert/strict');
const { runAgentTask, compactHistory } = require('../workbench/extension/agentLoop');
const {settings,historyLimit}=require('../workbench/extension/agentContext');
const limits=settings({workspace:{getConfiguration:()=>({get:(name,fallback)=>({contextWindow:100,maxOutputTokens:999999,compressionThreshold:0.99})[name]??fallback})}});
assert.deepEqual(limits,{contextWindow:16384,maxOutputTokens:4096,compressionThreshold:0.9});
assert(historyLimit(limits)>=6000);
const manifest = require('../workbench/extension/package.json');
for (const tool of require('../workbench/extension/tools.json')) {
  const contribution = manifest.contributes.languageModelTools.find(item => item.name === 'arkforge_' + tool.name);
  assert(contribution, 'every executable tool must be contributed to native Chat: ' + tool.name);
  assert.deepEqual(contribution.inputSchema.properties, tool.properties);
}
const {locateUnique,assertPublicMethodsRetained}=require('../workbench/extension/textEdits');
assert.throws(()=>assertPublicMethodsRetained('class A {\n  selfTest(): string { return \"ok\"; }\n}', 'class A {}'),/selfTest/);
assertPublicMethodsRetained('class A {\n  run() {}\n}','class A {\n  run() {changed();}\n}');
const mixed='before\r\r\nline1\r\nline2\nend';
const match=locateUnique(mixed,'line1\nline2');
assert.equal(mixed.slice(match.start,match.end),'line1\r\nline2');
assert.throws(()=>locateUnique('x\r\nx\nx','x'),/唯一/);
assert.deepEqual(locateUnique(mixed,'before\nline1'),{start:0,end:14});
const token = { isCancellationRequested: false };
const call = (name, args = {}, id = 'call') => ({ id, type: 'function', function: { name, arguments: JSON.stringify(args) } });
const stream = { progress() {}, markdown() {}, button() {} };
function options(extra) {
  return { token, stream, workspace: 'arkforge://one/workspace', messages: [{ role:'user', content:'task' }],
    goal: 'inspect project', system: 'system', diagnostics: async () => [], ...extra };
}
function validProtocol(messages) {
  let pending = new Set();
  for (const m of messages) {
    if (m.role === 'tool') { assert(pending.delete(m.tool_call_id), 'orphan or duplicate tool reply'); }
    else { assert.equal(pending.size, 0, 'missing tool reply'); pending = new Set((m.tool_calls || []).map(c => c.id)); }
  }
  assert.equal(pending.size, 0);
}
(async () => {
  let rounds = 0, executed = 0, saved;
  let result = await runAgentTask(options({
    query: async messages => {
      validProtocol(messages.slice(1));
      return ++rounds <= 30 ? { tool_calls: [call('read_file', { path:'file-' + rounds }, 'c' + rounds)] } : { content:'done' };
    }, invoke: async () => { executed++; return 'read content'; }, checkpoint: async state => { saved = structuredClone(state); }
  }));
  assert.equal(executed, 30); assert.equal(result.metadata.taskState.status, 'completed');
  assert.equal(saved.taskState.status, 'completed');

  rounds=0;
  result=await runAgentTask(options({context:{contextWindow:16384,maxOutputTokens:2048,compressionThreshold:0.7},
    query:async messages=>{validProtocol(messages.slice(1));assert(JSON.stringify(messages).length<8500);return ++rounds<=70?{tool_calls:[call('read_file',{path:'f'+rounds})]}:{content:'done'};},
    invoke:async()=> 'source'.repeat(1200)}));
  assert.equal(rounds,71);assert(result.metadata.taskState.compactions>0);

  const history = [];
  for (let i = 0; i < 25; i++) history.push({ role:'assistant',tool_calls:[call('read_file',{path:'f'+i},'g'+i)] }, { role:'tool',tool_call_id:'g'+i,content:'x'.repeat(5000) });
  const compact = compactHistory(history,{ goal:'fix warnings',revision:2,operations:['replace_text f1: saved'],lastBuild:{ok:false} });
  assert(JSON.stringify(compact).length < 36000); validProtocol(compact);
  assert(compact.some(message=>message.tool_call_id==='g24'),'newest tool group must survive compaction');
  assert(compact[0].content.includes('fix warnings')); assert(compact[0].content.includes('replace_text f1'));

  rounds=0; let builds=0, reads=0;
  result=await runAgentTask(options({goal:'修复所有 warnings 并正确编译',query:async()=>{
    const responses=[{tool_calls:[call('compile_project')]},{content:'done too early'},{tool_calls:[call('replace_text',{path:'a'})]},
      {content:'done without rebuild'},{tool_calls:[call('compile_project')]},{content:'finished after verification'}];
    return responses[rounds++];
  },invoke:async name=>name==='compile_project'?(builds++,'BUILD SUCCESSFUL'):'{"ok":true}',diagnostics:async()=>++reads===1?[{category:0,message:'real warning'}]:[]}));
  assert.equal(builds,2);assert.equal(rounds,6);assert.equal(result.metadata.taskState.lastBuild.warnings,0);
  assert.equal(result.metadata.taskState.lastBuild.revision,1);


  rounds=0;
  result=await runAgentTask(options({query:async()=>++rounds===1?{tool_calls:[call('get_diagnostics',{mode:'summary'})]}:{content:'done'},invoke:async()=>JSON.stringify({mode:'summary',matched:85,groups:[{path:'entry/a.ets',locations:[[1,2],[3,4]]}]})}));
  assert.equal(result.metadata.taskState.diagnosticProgress['all:*'].complete,true);
  assert.equal(result.metadata.taskState.diagnosticProgress['all:*'].nextOffset,85);
  assert.deepEqual(result.metadata.taskState.diagnosticProgress['all:*'].files,['entry/a.ets']);
  rounds=0;
  result=await runAgentTask(options({query:async()=>({tool_calls:[call('read_file',{path:++rounds%2?'a':'b'})]}),invoke:async()=> 'unchanged'}));
  assert.equal(result.metadata.taskState.status,'paused');assert(rounds<=16);validProtocol(result.metadata.messages);

  rounds=0;let originalLength;
  result=await runAgentTask(options({messages:[{role:'user',content:'x'.repeat(30000)}],query:async messages=>{if(!rounds++){originalLength=JSON.stringify(messages).length;throw new Error('HTTP 400 maximum context length exceeded');}assert(JSON.stringify(messages).length<originalLength);return {content:'done'};}}));
  assert.equal(rounds,2);assert(result.metadata.taskState.compactions>0);assert.equal(result.metadata.taskState.status,'completed');

  rounds=0;
  result=await runAgentTask(options({query:async()=>({tool_calls:[call(++rounds%2?'replace_text':'read_file',{path:'a'})]}),invoke:async name=>{if(name==='replace_text')throw new Error('unchanged failure');return 'same content';}}));
  assert.equal(result.metadata.taskState.status,'paused');assert(rounds<=16);validProtocol(result.metadata.messages);

  const cancelled={isCancellationRequested:false};rounds=0;executed=0;
  result=await runAgentTask(options({token:cancelled,query:async()=>({tool_calls:[call('read_file',{},'a'),call('write_file',{},'b')]}),invoke:async()=>{executed++;cancelled.isCancellationRequested=true;return 'read';}}));
  assert.equal(executed,1);assert.equal(result.metadata.taskState.status,'cancelled');validProtocol(result.metadata.messages);

  rounds=0;
  result=await runAgentTask(options({query:async()=>{if(rounds++)throw new Error('HTTP 503');return {tool_calls:[call('replace_text',{path:'a'})]};},invoke:async()=>'{"ok":true}'}));
  assert.equal(result.metadata.taskState.revision,1);assert.equal(result.metadata.taskState.status,'error');validProtocol(result.metadata.messages);

  rounds=0;executed=0;
  result=await runAgentTask(options({goal:'只编译检查，不安装',query:async()=>++rounds===1?{tool_calls:[call('build_project')]}:{content:'needs input'},invoke:async()=>{executed++;return 'installed';}}));
  assert.equal(executed,0,'compile-only requests must never invoke installing tool');
  assert.notEqual(result.metadata.taskState.status,'completed');
  console.log('PASS: 30 tool rounds, protocol-safe compaction, warning-free verification, rebuild after edits, cancellation, saved progress on error, compile-only install gate.');
})().catch(error=>{console.error(error);process.exitCode=1;});
