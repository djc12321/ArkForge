const assert=require('node:assert/strict');
const {createGitEngine,words}=require('../workbench/extension/gitEngine');
(async()=>{
 const calls=[];
 const engine=createGitEngine(async(method,args)=>{
  calls.push({method,args});
  if(method==='fs.stat')return JSON.stringify({type:args.path.endsWith('/file')?1:2});
  if(method==='fs.list')return JSON.stringify([['entry',2],['README.md',1]]);
  return JSON.stringify({output:'official Git output\n',code:7});
 });
 assert.deepEqual(words('git commit -m "two words"'),['git','commit','-m','two words']);
 assert.throws(()=>words('git status | cat'));
 assert.equal((await engine.execute('git --version')).output,'official Git output\n');
 assert.deepEqual(calls.at(-1),{method:'git_run',args:{path:'/workspace',args:['git','--version']}});
 await engine.execute('cd repo');assert.equal(engine.cwd,'/workspace/repo');
 const result=await engine.execute('git commit -m "two words"');assert.equal(result.code,7);
 assert.deepEqual(calls.at(-1).args,{path:'/workspace/repo',args:['git','commit','-m','two words']});
 await assert.rejects(()=>engine.execute('cd file'));assert.equal(engine.cwd,'/workspace/repo');
 await engine.execute('cd ..');assert.equal(engine.cwd,'/workspace');
 await assert.rejects(()=>engine.execute('cd ..'));
 assert.equal((await engine.execute('ls')).output,'entry\nREADME.md\n');
 console.log('PASS: restored Git terminal preserves official argv/output/exit codes, directory navigation and project boundaries.');
})().catch(e=>{console.error(e);process.exitCode=1;});
