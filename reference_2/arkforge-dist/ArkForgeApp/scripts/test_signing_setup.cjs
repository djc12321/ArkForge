'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
async function scenario(options={}) {
  const commands=new Map(),calls=[],messages=[],picks=[...(options.picks||[])],inputs=[...(options.inputs||[])];
  const state=new Map();let valid=!!options.valid;
  const vscode={ProgressLocation:{Notification:1},commands:{registerCommand:(id,fn)=>{commands.set(id,fn);return {dispose(){}};},executeCommand:async(id,...args)=>id==='setContext'?undefined:commands.get(id)?.(...args)},
    env:{clipboard:{writeText:async text=>calls.push(['clipboard',text])}},
    window:{withProgress:async(settings,run)=>run({report(){}},{isCancellationRequested:!!options.cancelLogin,onCancellationRequested:()=>({dispose(){}})}),
      showQuickPick:async items=>{const selected=picks.shift();return items.find(x=>(x.action||x.id||x.name)===selected);},
      showInputBox:async()=>inputs.shift(),showInformationMessage:async(text,settings)=>{messages.push(text);if(!settings?.modal&&options.pendingNotice)return new Promise(()=>{});return settings?.modal?options.confirm:undefined;},
      showWarningMessage:async text=>{messages.push(text);return options.revokeChoice;},showErrorMessage:async(text,settings)=>{messages.push(text);if(settings?.detail)messages.push(settings.detail);return options.errorChoice;}}};
  const native=async(method,args={})=>{
    calls.push([method,args]);
    if(method==='signing_status')return JSON.stringify({valid,bundleName:'com.test.app',expires:'2099',reason:'missing signing'});
    if(method==='import_signing'||method==='signing_import_files'){
      if(options.importError)throw new Error('Profile bundle mismatch');
      if(options.cancel)return JSON.stringify({cancelled:true});valid=true;return JSON.stringify({cancelled:false});
    }
    if(method==='huawei.start')return JSON.stringify(options.loginFlow?{id:1,expiresIn:600}:{authenticated:true});
    if(method==='huawei.prepare')return JSON.stringify({name:'Developer',teams:[{id:'team1',name:'My team'}],udid:'a'.repeat(64)});
    if(method==='huawei.certificates')return JSON.stringify({names:['existing-user-certificate.cer']});
    if(method==='huawei.issue'){if(options.quota)throw new Error('205389872');assert.equal(args.teamId,'team1');assert.equal(args.udid.length,64);valid=true;return '{}';}
    if(method==='huawei.revokeCertificate'){assert.equal(args.certName,'existing-user-certificate.cer');return '{}';}
    if(method==='huawei.cancel')return '{}';
    if(method==='developer_open')return '{}';throw new Error(method);
  };
  const sandbox={exports:{},require:()=>vscode,setTimeout,clearTimeout,Date};vm.runInNewContext(fs.readFileSync('workbench/extension/signingSetup.js','utf8'),sandbox);
  sandbox.exports.registerSigningSetup({subscriptions:[],workspaceState:{get:key=>state.get(key),update:async(key,value)=>state.set(key,value)}},native);
  await new Promise(resolve=>setImmediate(resolve));calls.length=0;messages.length=0;
  const result=await commands.get('arkforge.'+(options.command||'configureSigning'))();
  return {result,calls,messages,state};
}
(async()=>{
  let r=await scenario({command:'ensureSigning',valid:true});assert.equal(r.result,true);
  r=await scenario({command:'ensureSigning'});assert.equal(r.result,false);assert(!r.calls.some(x=>x[0].startsWith('import')));
  r=await scenario({picks:['huaweiManage','team1','existing-user-certificate.cer']});assert(!r.calls.some(x=>x[0]==='huawei.revokeCertificate'));
  r=await scenario({picks:['huaweiManage','team1','existing-user-certificate.cer'],revokeChoice:'撤销此证书'});assert.equal(r.calls.filter(x=>x[0]==='huawei.revokeCertificate').length,1);
  r=await scenario({picks:['portal']});assert(r.calls.some(x=>x[0]==='developer_open'&&x[1].page==='certificates'));
  r=await scenario({picks:['huawei','team1'],confirm:'申请并配置'});assert.equal(r.result,true);assert(r.calls.some(x=>x[0]==='huawei.issue'));
  r=await scenario({picks:['huawei','team1'],confirm:'?????',quota:true});assert(!r.result);assert(r.calls.some(x=>x[0]==='huawei.certificates'));assert(r.messages.some(x=>x.includes('existing-user-certificate.cer')));assert(!r.calls.some(x=>/delete|revoke/.test(x[0])));
  r=await scenario({picks:['huawei','team1']});assert(!r.result);assert(!r.calls.some(x=>x[0]==='huawei.issue'));
  r=await scenario({picks:['huawei'],loginFlow:true,cancelLogin:true});assert(!r.result);assert(r.calls.some(x=>x[0]==='huawei.cancel'));assert(!r.calls.some(x=>x[0]==='huawei.issue'));
  r=await Promise.race([scenario({picks:['zip'],pendingNotice:true}),new Promise((_,reject)=>setTimeout(()=>reject(new Error('signing blocked by undismissed success notice')),300))]);assert.equal(r.result,true);
  r=await scenario({picks:['zip']});assert.equal(r.result,true);assert(r.messages.some(x=>x.includes('开发签名已配置')));
  r=await scenario({picks:['zip'],cancel:true});assert(!r.result);assert(!r.messages.some(x=>x.includes('开发签名已配置')));
  r=await scenario({picks:['zip'],importError:true});assert.equal(r.result,false);assert(r.messages.includes('Profile bundle mismatch'));
  r=await scenario({picks:['files'],inputs:['alias',undefined]});assert(!r.calls.some(x=>x[0]==='signing_import_files'));
  r=await scenario({picks:['files'],inputs:['alias','test-password','','SHA256withECDSA'],confirm:'选择签名文件'});
  assert.equal(r.result,true);const args=r.calls.find(x=>x[0]==='signing_import_files')[1];assert.equal(args.keyPassword,args.storePassword);
  assert(!r.messages.some(x=>x.includes('test-password')));assert.equal(r.state.get('signingOnboarding:com.test.app'),true);
  r=await scenario({command:'ensureSigning',picks:['zip'],confirm:'配置开发签名'});assert.equal(r.result,true);
  console.log('PASS: first-use signing, build gate, official browser flow, manual credentials, cancellation, validation failure and secret-free notifications.');
})().catch(error=>{console.error(error);process.exitCode=1;});
