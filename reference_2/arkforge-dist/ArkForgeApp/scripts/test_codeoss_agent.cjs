'use strict';
// Exercise the shipped extension through its registered VS Code language model tools.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const documents=new Map(),files=new Map([['/workspace/entry/main.ets','Text("before")\nText("other")\n']]);
const commands=new Map(),replies=[];let provider,terminalOptions; const registeredTools=new Map();
const diagnosticEntries=new Map(),outputLines=[];
class URI {
  constructor(value){this.value=value;const url=new URL(value);this.path=url.pathname;this.authority=url.host;this.scheme=url.protocol.slice(0,-1);}
  static parse(value){return new URI(value);}
  static joinPath(uri,...parts){return new URI('arkforge://'+uri.authority+path.posix.join(uri.path,...parts));}
}
class Disposable {dispose(){}}
class EventEmitter {constructor(){this.listeners=[];this.event=fn=>{if(fn)this.listeners.push(fn);return new Disposable();};}fire(value){this.listeners.forEach(fn=>fn(value));}dispose(){this.listeners=[];}}
class Range {constructor(start,end){this.start=start;this.end=end;}}
class WorkspaceEdit {replace(uri,range,text){this.edit={uri,range,text};}}
class Diagnostic {constructor(range,message,severity){Object.assign(this,{range,message,severity});}}
const fileError=(code,detail)=>Object.assign(new Error(String(detail)),{code});
const nativeCalls=[];
const native=async(method,args)=>{
  nativeCalls.push(method);
  if(method==='compile_project')return 'Prepared build: 123\nBUILD SUCCESSFUL';
  if(method==='fs.stat') {if(!files.has(args.path))throw new Error('No such file or directory');return JSON.stringify({type:1,size:files.get(args.path).length,mtime:1,ctime:1});}
  if(method==='fs.read'){if(!files.has(args.path))throw new Error('No such file or directory');return Buffer.from(files.get(args.path)).toString('base64');}
  if(method==='fs.write'){if(files.has(args.path)&&args.overwrite===false)throw new Error('File exists');files.set(args.path,Buffer.from(args.data,'base64').toString());return '{}';}
  if(method==='fs.mkdir')return '{}';
  if(method==='git_run')return JSON.stringify({output:'git version 2.55.0\n',code:0});
  if(method==='sdk_diagnostics')return JSON.stringify([{fileName:'/project/entry/main.ets',line:1,column:2,code:2322,category:1,message:'Official SDK message'}]);
  throw new Error('Unexpected native request: '+method);
};
commands.set('arkforge.native',native);
const vscode={Uri:URI,Disposable,EventEmitter,Range,WorkspaceEdit,Diagnostic,FileChangeType:{Changed:1,Created:2,Deleted:3},DiagnosticSeverity:{Error:0,Warning:1},
  FileSystemError:{NoPermissions:message=>fileError('NoPermissions',message),Unavailable:message=>fileError('Unavailable',message),FileNotFound:uri=>fileError('FileNotFound',uri.path)},
  languages:{createDiagnosticCollection:()=>({clear(){diagnosticEntries.clear();},get(uri){return diagnosticEntries.get(uri.path)||[];},set(uri,items){diagnosticEntries.set(uri.path,items);},dispose(){}})},
  lm:{selectChatModels:async()=>[],registerTool:(name,value)=>{registeredTools.set(name,value);return new Disposable();},registerLanguageModelChatProvider:()=>new Disposable()},
  chat:{createChatParticipant:()=>new Disposable()},
  ThemeIcon:class {},
  StatusBarAlignment:{Right:2},
  LanguageModelTextPart:class {constructor(value){this.value=value;}},
  LanguageModelToolResult:class {constructor(content){this.content=content;}},
  commands:{executeCommand:async(id,...args)=>{if(!commands.has(id))throw new Error('Unknown command '+id);return commands.get(id)(...args);},registerCommand:(id,fn)=>{commands.set(id,fn);return new Disposable();}},
  workspace:{workspaceFolders:[{uri:URI.parse('arkforge://p-test/workspace')}],registerFileSystemProvider:(scheme,value)=>{provider=value;return new Disposable();},
    registerFileSearchProvider:()=>new Disposable(),registerTextSearchProvider:()=>new Disposable(),
    fs:{stat:async uri=>provider.stat(uri),readFile:async uri=>uri.path.endsWith('agent.html')?new TextEncoder().encode('<script></script>'):provider.readFile(uri),createDirectory:async uri=>provider.createDirectory(uri)},
    openTextDocument:async uri=>{
      if(documents.has(uri.path))return documents.get(uri.path);
      await provider.stat(uri);let text=new TextDecoder().decode(await provider.readFile(uri));
      const document={uri,version:1,getText:()=>text,positionAt:offset=>offset,
        apply:(range,replacement)=>{text=text.slice(0,range.start)+replacement+text.slice(range.end);document.version++;},
        save:async()=>{await provider.writeFile(uri,new TextEncoder().encode(text),{create:true,overwrite:true});return true;}};
      documents.set(uri.path,document);return document;
    },
    applyEdit:async edit=>{const item=edit.edit;documents.get(item.uri.path).apply(item.range,item.text);return true;},saveAll:async()=>true},
  window:{createStatusBarItem:()=>({show(){},dispose(){}}),createOutputChannel:()=>({appendLine(text){outputLines.push(text);},info(){},warn(){},error(){},show(){},dispose(){}}),showTextDocument:async()=>{},showErrorMessage(){},
    registerTerminalProfileProvider:()=>new Disposable(),
    createTerminal:options=>{terminalOptions=options;return {show(){},dispose(){}};},
    registerWebviewViewProvider:(id,value)=>{viewProvider=value;return new Disposable();}}
};
const sandbox={exports:{},require:name=>{assert.equal(name,'vscode');return vscode;},self:{},TextEncoder,TextDecoder,Uint8Array,atob,btoa,console};
vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../.workbench-build/extension.js'),'utf8'),sandbox);
sandbox.exports.activate({subscriptions:[],extensionUri:URI.parse('arkforge:/extension')});
(async()=>{
  async function call(method,args){
    try {const value=await registeredTools.get('arkforge_'+method).invoke({input:args},{isCancellationRequested:false});return {result:value.content.map(part=>part.value).join(''),error:''};}
    catch(error){return {error:error.message};}
  }
  const compileStart=nativeCalls.length;assert.match((await call('compile_project',{})).result,/BUILD SUCCESSFUL/);assert.deepEqual(nativeCalls.slice(compileStart),['compile_project']);
  const read=JSON.parse((await call('read_file',{path:'entry/main.ets'})).result);
  assert.deepEqual(read,{path:'entry/main.ets',version:1,start_line:1,end_line:3,total_lines:3,content:'Text("before")\nText("other")\n',truncated:false});
  assert.equal((await call('replace_text',{path:'entry/main.ets',old_text:'before',new_text:'after',expected_version:1})).error,'');
  assert.equal(files.get('/workspace/entry/main.ets'),'Text("after")\nText("other")\n');
  assert.match((await call('write_file',{path:'entry/main.ets',content:'stale',expected_version:1})).error,/版本/);
  assert.match((await call('replace_text',{path:'entry/main.ets',old_text:'Text',new_text:'Wrong',expected_version:2})).error,/唯一/);
  assert.match((await call('read_file',{path:'../outside'})).error,/相对/);
  assert.equal((await call('write_file',{path:'entry/new.ets',content:'new file',expected_version:0})).error,'');
  assert.equal(files.get('/workspace/entry/new.ets'),'new file');
  assert.match((await call('write_file',{path:'entry/new.ets',content:'overwrite',expected_version:0})).error,/版本/);
  await assert.rejects(()=>provider.rename(URI.parse('arkforge://p-test/workspace/entry/main.ets'),URI.parse('arkforge://other-project/workspace/entry/moved.ets'),{overwrite:false}),/其他工程/);assert(files.has('/workspace/entry/main.ets'));
  await assert.rejects(()=>provider.stat(URI.parse('arkforge://other-project/workspace/entry/main.ets')),/其他工程/);
  commands.get('arkforge.openGitTerminal')();
  const terminalOutput=[];terminalOptions.pty.onDidWrite(text=>terminalOutput.push(text));terminalOptions.pty.open();
  terminalOptions.pty.handleInput('git --version');terminalOptions.pty.handleInput('\r');
  await new Promise(resolve=>setImmediate(resolve));
  assert.match(terminalOutput.join(''),/git version 2\.55\.0/);
  terminalOptions.pty.handleInput('bogus');terminalOptions.pty.handleInput('\r');
  await new Promise(resolve=>setImmediate(resolve));assert.match(terminalOutput.join(''),/\x1b\[31m\[ArkForge\]/);
  terminalOptions.pty.close();
  const original="ArkTS:ERROR File: /project/entry/main.ets:1:2\n Official SDK message";
  await commands.get('arkforge.appendBuildLog')(original+'\nBUILD FAILED');
  assert.equal(outputLines.slice(-3).join('\n'),original+'\nBUILD FAILED');
  assert.equal(diagnosticEntries.get('/workspace/entry/main.ets')[0].code,2322);
  console.log('PASS: read ranges, exact replacement and save, version conflict, ambiguous replacement, traversal, creation, overwrite rejection');
})().catch(error=>{console.error(error);process.exitCode=1;});
