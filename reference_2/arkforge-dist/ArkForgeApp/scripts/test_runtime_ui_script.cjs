const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict'),cp=require('node:child_process');
const ts=require('C:/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/ets/build-tools/ets-loader/node_modules/typescript');
const box={exports:{}};
vm.runInNewContext(ts.transpileModule(fs.readFileSync('entry/src/main/ets/services/RuntimeUIScript.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2020}}).outputText,box);
const render=box.exports.RuntimeUIScript.render.bind(box.exports.RuntimeUIScript);
const cases=[
 {name:'duplicates-optional',actions:[{id:'absent',optional:true},{id:'target'}],expected:'done',screens:[[{id:'target',bounds:'[0,0][20,20]'},{id:'target',bounds:'[0,0][20,20]'}],[{id:'done',bounds:'[0,0][20,20]'}]],passed:true},
 {name:'escaped-text',actions:[{text:`x'); touch /tmp/arkforge-injection-test; echo '$(id)\\"`}],expected:'done',screens:[[{text:`x'); touch /tmp/arkforge-injection-test; echo '$(id)\\"`,bounds:'[0,0][20,20]'}],[{id:'done',bounds:'[0,0][20,20]'}]],passed:true},
 {name:'ambiguous',actions:[{id:'target'}],expected:'done',screens:[[{id:'target',bounds:'[0,0][20,20]'},{id:'target',bounds:'[30,0][50,20]'}]],passed:false},
 {name:'hidden',actions:[{id:'target'}],expected:'done',screens:[[{id:'target',visible:'false',bounds:'[0,0][20,20]'}]],passed:false},
 {name:'click-failure',actions:[{id:'target'}],expected:'done',screens:[[{id:'target',bounds:'[0,0][20,20]'}]],clickFailure:true,passed:false},
 {name:'missing-expected',actions:[{id:'target'}],expected:'done',screens:[[{id:'target',bounds:'[0,0][20,20]'}],[]],passed:false},
 {name:'invalid-bounds',actions:[{id:'target'}],expected:'done',screens:[[{id:'target',bounds:'[20,0][0,20]'}]],passed:false},
 {name:'malformed-layout',actions:[{id:'target'}],expected:'done',screens:[[]],invalidJSON:true,passed:false},
 {name:'launch-failure',actions:[{id:'target'}],expected:'done',screens:[[]],launchFailure:true,passed:false}
];
for(const c of cases)c.script=render('com.target.app','EntryAbility','com.editor.app',c.actions,c.expected,c.name,'PREFIX_PLACEHOLDER');
const path=process.cwd().replace(/\\/g,'/').replace(/^([A-Za-z]):/,(_,d)=>'/mnt/'+d.toLowerCase());
const result=cp.spawnSync('wsl',['-d','Debian','--','python3',path+'/scripts/test_runtime_ui_script.py'],{input:JSON.stringify(cases),encoding:'utf8',timeout:60000});
assert.equal(result.status,0,result.stderr+'\n'+result.stdout);console.log(result.stdout.trim());
