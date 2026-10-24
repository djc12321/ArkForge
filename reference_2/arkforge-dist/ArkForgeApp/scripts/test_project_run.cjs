'use strict';
const assert=require('node:assert/strict'),vm=require('node:vm'),fs=require('node:fs'),path=require('node:path');
async function scenario({compile='> Prepared build: 123\nBUILD SUCCESSFUL',choice,save=true,launchError=false,errors,signing=true,pendingError=false,retry=false}) {
  const calls=[],commands=new Map(),status={show(){this.visible=true;},dispose(){}};
  const vscode={StatusBarAlignment:{Right:2},ProgressLocation:{Notification:15},
    commands:{registerCommand:(id,run)=>{commands.set(id,run);return {dispose(){}};},executeCommand:async id=>id==='arkforge.ensureSigning'?signing:calls.push(id)},
    workspace:{saveAll:async()=>save},
    window:{createStatusBarItem:()=>status,withProgress:async(options,run)=>run({report(){}}),
      showInformationMessage:async(message,options)=>{if(options?.modal){calls.push('confirm');return choice;}},
      showErrorMessage:async message=>{errors?.push(message);calls.push('error');if(pendingError)return new Promise(()=>{});}}
  };
  const sandbox={exports:{},require:name=>{assert.equal(name,'vscode');return vscode;}};
  vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../workbench/extension/projectRun.js'),'utf8'),sandbox);
  sandbox.exports.registerProjectRun({subscriptions:[]},async(method,args)=>{
    calls.push(method);
    if(method==='compile_project')return compile;
    if(method==='install_prepared'){assert.equal(args.path,'123');return 'install bundle successfully\nBUILD SUCCESSFUL';}
    if(method==='run_app'){if(launchError)throw new Error('launch failed');return 'started';}
    throw new Error(method);
  },{info(){},error(){},show(){}},()=>{});
  assert.equal(status.command,'arkforge.run');assert.equal(status.visible,true);
  await Promise.race([commands.get('arkforge.run')(),new Promise((_,reject)=>setTimeout(()=>reject(new Error('build lock remained tied to an open notification')),200))]);
  if(retry)await commands.get('arkforge.run')();
  assert.equal(status.text,'$(play) 编译当前工程');
  return calls;
}
(async()=>{
  assert.deepEqual(await scenario({signing:false}),[]);
  assert.deepEqual(await scenario({compile:'BUILD FAILED',pendingError:true,retry:true}),['compile_project','error','compile_project','error']);
  assert.deepEqual(await scenario({compile:'error SDK2322\nBUILD FAILED'}),['compile_project','error']);
  assert.deepEqual(await scenario({compile:"ArkTS:ERROR File: /project/entry/Probe.ets:4:9\n Type 'string' is not assignable to type 'number'.\nBUILD FAILED"}),['compile_project','error']);
  const errors=[];
  await scenario({compile:'ArkTS:ERROR File: /project/entry/Probe.ets:4:9\n Official error body\nArkTS:WARN File: elsewhere:1:1\n Unrelated warning\nBUILD FAILED',errors});
  assert.equal(errors[0],'ArkTS:ERROR File: /project/entry/Probe.ets:4:9\n Official error body');
  assert.deepEqual(await scenario({choice:undefined}),['compile_project','confirm']);
  assert.deepEqual(await scenario({save:false}),['error']);
  assert.deepEqual(await scenario({compile:'BUILD SUCCESSFUL'}),['compile_project','error']);
  assert.deepEqual(await scenario({choice:'安装并运行'}),['compile_project','confirm','install_prepared','run_app']);
  assert.deepEqual(await scenario({choice:'仅安装'}),['compile_project','confirm','install_prepared']);
  assert.deepEqual(await scenario({choice:'安装并运行',launchError:true}),['compile_project','confirm','install_prepared','run_app','error']);
  console.log('PASS: permanent play command; compile failure, save failure, missing build record and cancel cannot install; confirmation installs exact build before launch.');
})().catch(error=>{console.error(error);process.exitCode=1;});
