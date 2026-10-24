const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('workbench/extension/welcomeGuide.js','utf8');
const manifest=JSON.parse(fs.readFileSync('workbench/extension/package.json','utf8'));
async function session(state,fail=false){
  const commands=new Map(),opens=[],errors=[];
  const vscode={commands:{registerCommand:(id,run)=>{commands.set(id,run);return {dispose(){}};},executeCommand:async(...args)=>{opens.push(args);if(fail)throw Error('load error');}},window:{showErrorMessage:text=>errors.push(text)}};
  const sandbox={exports:{},require:()=>vscode};vm.runInNewContext(source,sandbox);
  const context={subscriptions:[],globalState:{get:key=>state.get(key),update:async(key,value)=>state.set(key,value)}};
  const pending=sandbox.exports.registerWelcomeGuide(context);
  try{await pending;}catch{}
  return {commands,opens,errors};
}
(async()=>{
  const state=new Map();let first=await session(state);
  assert.equal(first.opens.length,1);assert.equal(first.opens[0][0],'workbench.action.openWalkthrough');
  assert.equal(first.opens[0][1],manifest.publisher+'.'+manifest.name+'#'+manifest.contributes.walkthroughs[0].id);
  let next=await session(state);assert.equal(next.opens.length,0,'later launches preserve the developer workspace');
  await next.commands.get('arkforge.usageGuide')();assert.equal(next.opens.length,1,'manual guide remains available');
  const retryState=new Map();const failed=await session(retryState,true);assert.equal(retryState.size,0,'opening failure permits a later retry');assert.equal(failed.errors.length,1);
  const retried=await session(retryState);assert.equal(retried.opens.length,1);
  const guide=manifest.contributes.walkthroughs[0];assert.equal(new Set(guide.steps.map(s=>s.id)).size,guide.steps.length);assert(guide.steps.length>=13);
  const known=new Set(manifest.contributes.commands.map(c=>c.command));
  await next.commands.get('arkforge.gitGuide')();
  assert.equal(next.opens.at(-1)[1],manifest.publisher+'.'+manifest.name+'#arkforge.gitGuide');
  for(const step of manifest.contributes.walkthroughs.flatMap(item=>item.steps)){
    assert(fs.existsSync('workbench/extension/'+step.media.image));
    for(const link of step.description.matchAll(/\]\(command:([^)?]+)\)/g))assert(known.has(link[1])||link[1].startsWith('workbench.'),'guide command exists: '+link[1]);
    assert(!/\u4e0d\u4f1a|\u4e0d\u80fd|\u4e0d\u8981|\u4e0d\u662f|\u4e0d\u4f7f\u7528|\u4e0d\u5199\u5165|\u4e0d\u652f\u6301/.test(step.description),'positive guide wording: '+step.id);
  }
  assert.equal(manifest.contributes.menus['menubar/help'][0].command,'arkforge.usageGuide');
  console.log('PASS: first-use welcome, returning-user state, manual reopening, retry after loading failure, guide navigation and positive instructions.');
})().catch(e=>{console.error(e);process.exitCode=1;});
