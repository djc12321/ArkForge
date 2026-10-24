const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),os=require('node:os'),vm=require('node:vm');
const ts=require('C:/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/ets/build-tools/ets-loader/node_modules/typescript');
const dir=fs.mkdtempSync(path.join(os.tmpdir(),'arkforge-web-runtime-'));let timer;
class Controller{loadUrl(url){this.url=url;}}
const fileIo={OpenMode:{CREATE:1,WRITE_ONLY:2,TRUNC:4},openSync:p=>({fd:fs.openSync(p,'w')}),writeSync:fs.writeSync,closeSync:file=>fs.closeSync(file.fd)};
const sandbox={exports:{},setTimeout:fn=>{timer=fn;return 1;},clearTimeout(){},require:name=>name.includes('ArkWeb')?{webview:{WebviewController:Controller}}:{fileIo}};
vm.runInNewContext(ts.transpileModule(fs.readFileSync('entry/src/main/ets/services/SDKWebRuntime.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2020}}).outputText,sandbox);
(async()=>{
 const api=sandbox.exports.SDKWebRuntime,bundle=path.join(dir,'ui-transform.web.js').replaceAll('\\','/'),messages=[];
 const pending=api.transform(bundle,'{"files":{"a":"</script>"}}',['a'],s=>messages.push(s));
 const html=fs.readFileSync(path.join(dir,'check.html'),'utf8'),input=fs.readFileSync(path.join(dir,'input.js'),'utf8');
 assert.match(html,/src="ui-transform.web.js"/);assert.match(html,/connect-src 'none'/);assert.match(html,/__arkforgeTransformResult/);
 assert(!html.includes('</script>"'));assert.match(input,/__arkforgeBatchFileNames=\["a"\]/);
 await assert.rejects(api.check(bundle,'{}',()=>{}),/already running/);
 api.log('official log');assert.deepEqual(messages,['official log']);
 api.complete('stale','bad');assert.notEqual(api.controller.url,'about:blank');
 api.bridge.complete('1','{"ok":true}');assert.equal(await pending,'{"ok":true}');assert.equal(api.controller.url,'about:blank');
 const check=api.check(bundle,'{}',()=>{});assert.match(fs.readFileSync(path.join(dir,'check.html'),'utf8'),/sdk-check.web.js/);
 timer();assert.equal(JSON.parse(await check).ok,false);assert.equal(api.controller.url,'about:blank');
 console.log('PASS: isolated offline SDK/ArkUI jobs, input encoding, diagnostics forwarding, concurrency guard, stale replies and timeout cleanup.');
})().catch(e=>{console.error(e);process.exitCode=1;}).finally(()=>{for(const n of fs.readdirSync(dir))fs.unlinkSync(path.join(dir,n));fs.rmdirSync(dir);});
