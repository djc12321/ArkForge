const assert=require('node:assert/strict'),vm=require('node:vm'),fs=require('node:fs'),path=require('node:path');
class URI{constructor(value){this.value=value;this.path=new URL(value).pathname;}static joinPath(uri,name){return new URI(uri.value+'/'+name);}}
class Range{constructor(...coordinates){this.coordinates=coordinates;}}
const root=new URI('arkforge://p-check/workspace');
const documents=new Map([['entry/a.ets','alpha probe\nprobeValue\nalpha.probe\n'],['entry/closed.ets','Alpha probe\nnext line\n'],['docs/readme.txt','alpha probe'],['image.bin','\0alpha probe']]);
let fileProvider,textProvider;
const vscode={Uri:URI,Range,workspace:{registerFileSearchProvider:(scheme,p)=>{fileProvider=p;return {};},registerTextSearchProvider:(scheme,p)=>{textProvider=p;return {};},fs:{stat:async uri=>({size:documents.get(uri.path.slice('/workspace/'.length)).length}),readFile:async uri=>new TextEncoder().encode(documents.get(uri.path.slice('/workspace/'.length)))}}};
const sandbox={exports:{},require:name=>name==='vscode'?vscode:require(path.resolve(__dirname,'../../../../_vendor/vscode/node_modules/minimatch')),TextDecoder};
vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../workbench/extension/projectSearch.js'),'utf8'),sandbox);
sandbox.exports.registerProjectSearch({subscriptions:[]},async(method,args)=>{
  if(method==='fs.index')return JSON.stringify([...documents.keys()]);
  assert.equal(method,'fs.readBatch');return JSON.stringify({consumed:args.files.length,unreadable:0,items:args.files.filter(name=>!documents.get(name).includes('\0')).map(name=>({name,text:documents.get(name)}))});
});
const token={isCancellationRequested:false};
async function search(query,extra={}){const results=[];const complete=await textProvider.provideTextSearchResults(query,{folder:root,includes:['**/*.ets'],excludes:[],maxResults:100,...extra},{report:r=>results.push(r)},token);return {results,complete};}
(async()=>{
  const found=await fileProvider.provideFileSearchResults({pattern:'closed'},{folder:root,includes:[],excludes:['docs/**']},token);assert.equal(found.length,1);assert.match(found[0].path,/closed.ets$/);
  assert.equal((await search({pattern:'alpha'})).results.length,3);
  assert.equal((await search({pattern:'alpha',isCaseSensitive:true})).results.length,2);
  assert.equal((await search({pattern:'.'})).results.length,1);
  assert.equal((await search({pattern:'probe',isWordMatch:true})).results.length,3);
  assert.equal((await search({pattern:'Alpha.*probe',isRegExp:true})).results.length,3);
  assert.equal((await search({pattern:'probe'},{excludes:['**/closed.ets']})).results.length,3);
  const limited=await search({pattern:'probe'},{maxResults:1});assert.equal(limited.results.length,1);assert.equal(limited.complete.limitHit,true);
  const multiline=await search({pattern:'probe\nnext'});assert.equal(multiline.results.length,1);assert.deepEqual([...multiline.results[0].ranges.coordinates],[0,6,1,4]);
  assert.equal((await search({pattern:'alpha'},{includes:[]})).results.length,4);
  token.isCancellationRequested=true;assert.equal((await search({pattern:'alpha'})).results.length,0);
  console.log('PASS: unopened-file search, fuzzy file lookup, globs, literal/regex, case, whole words, multiline positions, limits, binary exclusion and cancellation.');
})().catch(error=>{console.error(error);process.exitCode=1;});
