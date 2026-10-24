'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
async function scenario(options={}) {
  const commands=new Map(),calls=[],messages=[],inputs=[...(options.inputs||[])],picks=[...(options.picks||[])];let clipboard='';
  const vscode={StatusBarAlignment:{Right:2},ProgressLocation:{Notification:1},
    env:{clipboard:{writeText:async text=>{clipboard=text;}}},
    commands:{registerCommand:(id,fn)=>{commands.set(id,fn);return {dispose(){}};},executeCommand:async id=>commands.get(id)?.()},
    window:{createStatusBarItem:()=>({show(){},dispose(){}}),
      showQuickPick:async items=>{const choice=picks.shift();return items.find(item=>(item.action||item.command||item)===choice);},
      showInputBox:async()=>inputs.shift(),showWarningMessage:async()=>options.confirm,
      showErrorMessage:async text=>messages.push(text),
      showInformationMessage:async(text,settings)=>{messages.push(text);return settings?.modal?options.confirm:undefined;},
      withProgress:async(settings,run)=>run({report(){}},{isCancellationRequested:false,onCancellationRequested:()=>({dispose(){}})})}};
  const native=async(method,args={})=>{
    calls.push({method,args});
    if(method==='developer_open' && options.openError)throw new Error('Browser unavailable');
    if(method==='github.start')return JSON.stringify({id:3,code:'ABCD-EFGH',interval:1,expiresIn:900});
    if(method==='github.poll')return JSON.stringify({state:'authenticated',login:'Tester'});
    if(method==='github.status')return JSON.stringify({signedIn:!!options.signedIn,login:'Tester'});
    if(method==='github.verify')return JSON.stringify({login:'VerifiedTester'});
    if(method==='signing_status')return JSON.stringify({bundleName:'com.test.app',valid:false,reason:'No signing'});
    return '{}';
  };
  const sandbox={exports:{},require:name=>name==='vscode'?vscode:{registerSigningSetup(){}},setTimeout:fn=>setImmediate(fn),clearTimeout:clearImmediate,Date};
  vm.runInNewContext(fs.readFileSync('workbench/extension/developerSetup.js','utf8'),sandbox);
  sandbox.exports.registerDeveloperSetup({subscriptions:[]},native);
  await new Promise(resolve=>setImmediate(resolve));calls.length=0;
  await commands.get('arkforge.'+(options.command||'signInGitHub'))();
  return {calls,messages,clipboard};
}
(async()=>{
  let result=await scenario({openError:true});assert.deepEqual(result.calls.map(c=>c.method),['github.start','developer_open','github.cancel']);
  result=await scenario({confirm:'复制代码并打开 GitHub'});
  assert.deepEqual(result.calls.map(c=>c.method),['github.start','developer_open','github.poll','github.status','github.cancel']);
  assert.equal(result.clipboard,'ABCD-EFGH');assert(result.messages.some(t=>t.includes('已登录 GitHub：Tester')));
  result=await scenario({command:'configureGit',picks:['identity'],inputs:['Tester',undefined]});assert(!result.calls.some(c=>c.method==='git_identity'));
  result=await scenario({command:'configureGit',picks:['identity'],inputs:['Tester','test@example.invalid']});
  assert.equal(result.calls.at(-1).method,'git_identity');assert.equal(result.calls.at(-1).args.email,'test@example.invalid');
  result=await scenario({command:'configureGit',signedIn:true,picks:['verify']});assert.equal(result.calls.at(-1).method,'github.verify');
  console.log('Developer setup: sign-in confirmation, cancel, clipboard, verified login, identity cancellation, verification and signing portal disclosure passed.');
})().catch(error=>{console.error(error);process.exitCode=1;});
