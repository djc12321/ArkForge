const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),os=require('node:os'),vm=require('node:vm');
const ts=require('C:/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/ets/build-tools/ets-loader/node_modules/typescript');
const dir=fs.mkdtempSync(path.join(os.tmpdir(),'arkforge-web-runtime-'));let timer;
for(const name of ['check','transform'])fs.mkdirSync(path.join(dir,name));
class Controller{static setRenderProcessMode(mode){assert.equal(mode,1);}loadUrl(url){if(this.fail&&url.startsWith('file:'))throw Error('load failure');this.url=url;}}
const fileIo={OpenMode:{CREATE:1,WRITE_ONLY:2,TRUNC:4},openSync:p=>({fd:fs.openSync(p,'w')}),writeSync:fs.writeSync,closeSync:file=>fs.closeSync(file.fd)};
const sandbox={exports:{},setTimeout:fn=>{timer=fn;return 1;},clearTimeout(){},require:name=>name.includes('ArkWeb')?{webview:{WebviewController:Controller,RenderProcessMode:{MULTIPLE:1}}}:{fileIo}};
vm.runInNewContext(ts.transpileModule(fs.readFileSync('entry/src/main/ets/services/SDKWebRuntime.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2020}}).outputText,sandbox);
(async()=>{
 const api=sandbox.exports.SDKWebRuntime,transformBundle=path.join(dir,'transform/ui-transform.web.js').replaceAll('\\','/'),checkBundle=path.join(dir,'check/sdk-check.web.js').replaceAll('\\','/'),logs=[],transformLogs=[];
 const transform=api.transform(transformBundle,'{"files":{"a":"</script>"}}',['a'],s=>transformLogs.push(s));
 const check=api.check(checkBundle,'{}',s=>logs.push(s));
 const html=fs.readFileSync(path.join(dir,'transform/check.html'),'utf8'),input=fs.readFileSync(path.join(dir,'transform/input.js'),'utf8');
 assert.match(html,/src="ui-transform.web.js"/);assert.match(html,/connect-src 'none'/);assert.match(html,/__arkforgeTransformResult/);assert(!html.includes('</script>"'));assert.match(input,/__arkforgeBatchFileNames=\["a"\]/);
 assert.match(fs.readFileSync(path.join(dir,'check/check.html'),'utf8'),/sdk-check.web.js/);
 await assert.rejects(api.check(checkBundle,'{}',()=>{}),/already running/);
 await assert.rejects(api.transform(transformBundle,'{}',[],()=>{}),/already running/);
 api.log('checker log');api.transformLog('transform log');assert.deepEqual(logs,['checker log']);assert.deepEqual(transformLogs,['transform log']);
 api.transformBridge.complete('stale','bad');assert.notEqual(api.transformController.url,'about:blank');
 api.transformBridge.complete('1','{"ok":true}');assert.equal(await transform,'{"ok":true}');assert.equal(api.transformController.url,'about:blank');assert.notEqual(api.controller.url,'about:blank','transform completion must not terminate checker');
 api.bridge.complete('1','{"ok":true}');assert.equal(await check,'{"ok":true}');assert.equal(api.controller.url,'about:blank');
 const canceled=api.transform(transformBundle,'{}',[],()=>{});api.cancelTransform();assert.equal(JSON.parse(await canceled).ok,false);
 const retry=api.transform(transformBundle,'{}',[],()=>{});api.transformBridge.complete('2','old');assert.notEqual(api.transformController.url,'about:blank');api.transformBridge.complete('3','new');assert.equal(await retry,'new');
 const timeout=api.check(checkBundle,'{}',()=>{});timer();assert.equal(JSON.parse(await timeout).ok,false);
 api.controller.fail=true;assert.match(JSON.parse(await api.check(checkBundle,'{}',()=>{})).error,/load failure/);api.controller.fail=false;
 process.stdout.write('PASS: parallel offline SDK/ArkUI sessions, isolated input/logs, per-session concurrency, stale replies, cancellation, timeout and load failure cleanup.\n');
})().catch(e=>{console.error(e);process.exitCode=1;}).finally(()=>{for(const name of ['check','transform']){const root=path.join(dir,name);for(const n of fs.readdirSync(root))fs.unlinkSync(path.join(root,n));fs.rmdirSync(root);}fs.rmdirSync(dir);});
