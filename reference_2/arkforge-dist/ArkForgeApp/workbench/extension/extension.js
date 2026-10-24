const vscode = require('vscode');

exports.activate = function(context) {
  context.subscriptions.push(vscode.window.onDidChangeActiveTextEditor(editor=>{
    if(editor) vscode.commands.executeCommand('arkforge.phone.revealEditor').catch(console.error);
  }));
  const rootUri=()=>vscode.workspace.workspaceFolders?.[0]?.uri||vscode.Uri.parse('arkforge:/workspace');
  const native = async (method,args={}) => vscode.commands.executeCommand('arkforge.native',method,args);
  const events = new vscode.EventEmitter();
  context.subscriptions.push(events);
  const fs = async (operation,uri,extra={}) => {
    if(uri.authority!==rootUri().authority)throw vscode.FileSystemError.NoPermissions('文件属于其他工程，请先打开对应工程');
    try { return await native('fs.'+operation,{path:uri.path,...extra}); }
    catch(e) {
      if (/no such file|not exist|13900002/i.test(e.message)) throw vscode.FileSystemError.FileNotFound(uri);
      throw vscode.FileSystemError.Unavailable(e.message);
    }
  };
  const provider = {
    onDidChangeFile:events.event,
    watch:()=>new vscode.Disposable(()=>{}),
    stat:async uri=>JSON.parse(await fs('stat',uri)),
    readDirectory:async uri=>JSON.parse(await fs('list',uri)),
    readFile:async uri=>Uint8Array.from(atob(await fs('read',uri)),c=>c.charCodeAt(0)),
    writeFile:async(uri,content,options)=>{
      let binary=''; for(let i=0;i<content.length;i+=32768)binary+=String.fromCharCode(...content.subarray(i,i+32768));
      await fs('write',uri,{data:btoa(binary),...options});
      events.fire([{type:options.overwrite ? vscode.FileChangeType.Changed : vscode.FileChangeType.Created,uri}]);
    },
    createDirectory:async uri=>{await fs('mkdir',uri);events.fire([{type:vscode.FileChangeType.Created,uri}]);},
    delete:async(uri,options)=>{await fs('delete',uri,options);events.fire([{type:vscode.FileChangeType.Deleted,uri}]);},
    rename:async(from,to,options)=>{if(to.authority!==rootUri().authority)throw vscode.FileSystemError.NoPermissions('目标属于其他工程，请先打开对应工程');await fs('rename',from,{target:to.path,...options});events.fire([{type:vscode.FileChangeType.Deleted,uri:from},{type:vscode.FileChangeType.Created,uri:to}]);}
  };
  context.subscriptions.push(vscode.workspace.registerFileSystemProvider('arkforge',provider,{isCaseSensitive:true}));
  let taskProgress;
  const channel=vscode.window.createOutputChannel('ArkForge Build','log');
  const output={info:text=>channel.appendLine(text),warn:text=>channel.appendLine(text),error:text=>channel.appendLine(text),show:preserveFocus=>channel.show(preserveFocus)};
  const diagnostics=vscode.languages.createDiagnosticCollection('ArkForge SDK');
  context.subscriptions.push(diagnostics);
  context.subscriptions.push(channel);
  context.subscriptions.push(vscode.commands.registerCommand('arkforge.appendBuildLog',async text=>{
    for(const line of text.split('\n')) {
      if(line.includes('> ArkForge :'))taskProgress?.report({message:line.replace(/^.*> ArkForge :/,'')});
      if(line.includes('Started '))diagnostics.clear();
      if(/\berror\b|BUILD FAILED/i.test(line))output.error(line);
      else if(/\bwarning\b/i.test(line))output.warn(line);
      else output.info(line);
      const match=line.match(/(?:^|\]\s+)([^\[\]\s].*?):(\d+):(\d+): (error|warning) ([A-Za-z]+\d+): (.*)/);
      if(match) {
        const uri=vscode.Uri.joinPath(rootUri(),match[1]);
        const diagnostic=new vscode.Diagnostic(new vscode.Range(Number(match[2])-1,Number(match[3])-1,Number(match[2])-1,Number(match[3])),match[6],match[4]==='error'?vscode.DiagnosticSeverity.Error:vscode.DiagnosticSeverity.Warning);
        diagnostic.code=match[5];diagnostic.source='HarmonyOS SDK';
        diagnostics.set(uri,[...(diagnostics.get(uri)||[]),diagnostic]);
      }
    }
    if(/BUILD (?:FAILED|SUCCESSFUL)/.test(text)) {
      let items;
      try { items=JSON.parse(await native('sdk_diagnostics')); }
      catch(error) { output.info(error.message); return; }
      diagnostics.clear();
      for(const item of items) {
        const relative=item.fileName.replace(/^\/project\//,'');
        if(!relative||relative.startsWith('/')||relative.split('/').includes('..')||item.line<1||item.column<1)continue;
        const uri=vscode.Uri.joinPath(rootUri(),relative);
        const diagnostic=new vscode.Diagnostic(new vscode.Range(item.line-1,item.column-1,item.line-1,item.column),item.message,item.category===1?vscode.DiagnosticSeverity.Error:vscode.DiagnosticSeverity.Warning);
        diagnostic.code=item.code;diagnostic.source='HarmonyOS SDK';
        diagnostics.set(uri,[...(diagnostics.get(uri)||[]),diagnostic]);
      }
    }
  }));
  require('./projectRun').registerProjectRun(context,native,output,progress=>{taskProgress=progress;});
  require('./projectSearch').registerProjectSearch(context,native);
  require('./gitTerminal').registerGitTerminal(context,native);
  require('./developerSetup').registerDeveloperSetup(context,native);
  require('./deviceDebug').registerDeviceDebug(context,native);
  context.subscriptions.push(vscode.commands.registerCommand('arkforge.importSigning',()=>vscode.commands.executeCommand('arkforge.configureSigning')));
  const actions={openProject:'open_project',exportProject:'export_project',launch:'run_app',logs:'read_logs',checkSDK:'check_sdk'};
  for(const [command,method] of Object.entries(actions)) {
    context.subscriptions.push(vscode.commands.registerCommand('arkforge.'+command,async()=>{
      try {
        if(['openProject','exportProject'].includes(command) && !await vscode.workspace.saveAll(false))throw new Error('文件保存失败，工程操作已停止');
        output.show(true);
        const result=await native(method,{});
        if(result)output.info(result);
        if(command==='openProject' && JSON.parse(result).changed)await vscode.commands.executeCommand('workbench.action.reloadWindow');
      } catch(e) {output.error(e.message);vscode.window.showErrorMessage(e.message);}
    }));
  }
  context.subscriptions.push(vscode.commands.registerCommand('arkforge.build',async()=>{
    const selected=await vscode.window.showQuickPick([
      {label:'$(run-all) 编译并安装当前工程',description:'在此平板完成编译与安装',action:'build'},
      {label:'$(key) 首次使用：配置开发签名',description:'华为账号自动签名或手动导入，后续自动复用',action:'signing'},
      {label:'$(book) 查看构建方法',description:'打开工程 → 保存 → 签名 → 构建 → 运行',action:'guide'},
      {label:'$(beaker) 验证构建操作流程',description:'实际执行 SDK 自检及当前工程构建安装',action:'test'}
    ],{title:'ArkForge · 手动编译安装',placeHolder:'选择操作；首次使用请先配置当前工程和设备的开发签名'});
    if(!selected)return;
    if(selected.action==='signing')return vscode.commands.executeCommand('arkforge.configureSigning');
    if(selected.action==='guide')return vscode.commands.executeCommand('arkforge.buildGuide');
    if(selected.action==='test')return vscode.commands.executeCommand('arkforge.testBuildWorkflow');
    return vscode.commands.executeCommand('arkforge.run');
  }));
  const projectUri = path => {
    if(typeof path!=='string' || path.startsWith('/') || path.includes('\\') || path.split('/').some(part=>part==='..'||part==='.'||!part))throw new Error('请使用工程内的相对文件路径');
    return vscode.Uri.joinPath(rootUri(),path);
  };
  const readCoverage=new Map();
  function recordRead(path,version,start,end,total) {
    const old=readCoverage.get(path),ranges=old?.version===version?old.ranges:[];
    ranges.push([start,end]);ranges.sort((a,b)=>a[0]-b[0]);
    const merged=[];
    for(const range of ranges){const last=merged[merged.length-1];if(last&&range[0]<=last[1]+1)last[1]=Math.max(last[1],range[1]);else merged.push(range.slice());}
    readCoverage.set(path,{version,ranges:merged,complete:merged[0]?.[0]===1&&merged[0][1]>=total});
    if(readCoverage.size>100)readCoverage.delete(readCoverage.keys().next().value);
  }
  async function agentTool(method,args) {
    if(method==='read_file') {
      const document=await vscode.workspace.openTextDocument(projectUri(args.path));
      if(document.getText().length>2*1024*1024)throw new Error('文件超过 2 MB');
      const lines=document.getText().replace(/\r+\n/g,'\n').split('\n');
      const start=Math.max(1,Number(args.start_line)||1);
      const end=Math.min(lines.length,start+1199,Number(args.end_line)||start+599);
      const selected=[];let size=0;
      const budget=Math.max(1000,Math.min(24000,Number(args._budget)||24000));
      for(let line=start;line<=end;line++) {
        const text=lines[line-1];
        if(size+JSON.stringify(text).length>budget)break;
        selected.push(text);size+=JSON.stringify(text).length+1;
      }
      const actualEnd=start+selected.length-1;
      recordRead(args.path,document.version,start,actualEnd,lines.length);
      return JSON.stringify({path:args.path,version:document.version,start_line:start,end_line:actualEnd,total_lines:lines.length,content:selected.join('\n'),truncated:actualEnd<end,...(actualEnd<lines.length?{next_line:Math.max(start,actualEnd+1)}:{})});
    }
    if(method==='read_files') {
      if(!Array.isArray(args.files)||!args.files.length||args.files.length>6)throw new Error('请选择 1 至 6 个工程文件');
      const results=[];
      for(const file of args.files) {
        try { results.push(JSON.parse(await agentTool('read_file',{...file,_budget:Math.floor(28000/args.files.length)}))); }
        catch(error) { results.push({path:file.path,error:error.message}); }
      }
      return JSON.stringify({files:results});
    }
    if(method==='write_file'||method==='replace_text') {
      const uri=projectUri(args.path);
      let document;
      let exists=true;
      try {await vscode.workspace.fs.stat(uri);}
      catch(e) {if(args.expected_version!==0 || e.code!=='FileNotFound')throw e;exists=false;}
      // openTextDocument wraps FileNotFound in a generic Error in the real
      // extension host. Probe with workspace.fs first, whose error code survives.
      if(exists)document=await vscode.workspace.openTextDocument(uri);
      if(document && document.version!==args.expected_version)throw new Error('文件版本已变化，请重新读取后修改，避免覆盖用户编辑');
      if(document&&method==='write_file'&&!(readCoverage.get(args.path)?.version===document.version&&readCoverage.get(args.path)?.complete))throw new Error('请先完整读取当前版本，再保存整个文件；局部修改可使用 replace_text。');
      if(!document) {
        if(method!=='write_file'||typeof args.content!=='string')throw new Error('新文件需要 write_file 和完整内容');
        await vscode.workspace.fs.createDirectory(vscode.Uri.joinPath(uri,'..'));
        await provider.writeFile(uri,new TextEncoder().encode(args.content),{create:true,overwrite:false});
      } else {
        const text=document.getText();let start=0,end=text.length,replacement=args.content;
        if(method==='replace_text') {
          if(typeof args.old_text!=='string'||!args.old_text)throw new Error('请提供待替换的源码内容');
          const match=require('./textEdits').locateUnique(text,args.old_text);
          start=match.start;end=match.end;replacement=args.new_text;
        }
        if(typeof replacement!=='string'||replacement.length>2*1024*1024)throw new Error('写入内容无效或过大');
        if(method==='write_file')require('./textEdits').assertPublicMethodsRetained(text,replacement);
        const edit=new vscode.WorkspaceEdit();edit.replace(uri,new vscode.Range(document.positionAt(start),document.positionAt(end)),replacement);
        if(!await vscode.workspace.applyEdit(edit))throw new Error('编辑器拒绝修改');
        if(!await document.save())throw new Error('保存失败，修改仍保留在编辑器中');
      }
      const saved=await vscode.workspace.openTextDocument(uri);
      await vscode.window.showTextDocument(saved,{preview:false,preserveFocus:true});
      return JSON.stringify({ok:true,path:args.path,version:saved.version});
    }
    if(method==='create_directory') {
      await vscode.workspace.fs.createDirectory(projectUri(args.path));
      return JSON.stringify({ok:true,path:args.path});
    }
    if(method==='rename_file'||method==='delete_file') {
      const uri=projectUri(args.path),stat=await vscode.workspace.fs.stat(uri);
      if(stat.type!==1)throw new Error('请选择工程中的普通文件');
      const document=await vscode.workspace.openTextDocument(uri);
      if(document.version!==args.expected_version)throw new Error('请重新读取当前文件及版本后继续操作');
      if(document.isDirty&&!await document.save())throw new Error('请先完成当前文件保存');
      if(method==='rename_file')await vscode.workspace.fs.rename(uri,projectUri(args.new_path),{overwrite:false});
      else await vscode.workspace.fs.delete(uri,{recursive:false});
      return JSON.stringify({ok:true,path:args.path,...(method==='rename_file'?{new_path:args.new_path}:{})});
    }
    if(method==='get_diagnostics') {
      const all=JSON.parse(await native('sdk_diagnostics'));
      const selected=all.filter(item=>(!args.path||item.fileName.replace(/^\/project\//,'')===args.path)&&(!args.category||item.category===(args.category==='warning'?0:1)));
      const counts={total:all.length,errors:all.filter(item=>item.category===1).length,warnings:all.filter(item=>item.category===0).length,matched:selected.length};
      if(args.mode!=='details'&&args.offset===undefined&&args.limit===undefined || args.mode==='summary') {
        const groups=new Map();
        for(const item of selected) {
          const path=item.fileName.replace(/^\/project\//,''),key=JSON.stringify([path,item.category,item.code,item.message]);
          if(!groups.has(key))groups.set(key,{path,category:item.category===1?'error':'warning',code:item.code,message:item.message,locations:[]});
          groups.get(key).locations.push([item.line,item.column]);
        }
        return JSON.stringify({...counts,mode:'summary',complete:true,groups:[...groups.values()],instruction:'全部匹配诊断已汇总；locations 包含每一处行列位置。同一原因只显示一次。按文件批量读取源码并修复，修改后重新构建。'});
      }
      const offset=Math.max(0,Number(args.offset)||0),limit=Math.max(1,Math.min(200,Number(args.limit)||100));
      return JSON.stringify({...counts,mode:'details',offset,has_more:offset+limit<selected.length,diagnostics:selected.slice(offset,offset+limit)});
    }
    if(method==='list_dir') {
      const uri=!args.path||args.path==='.'?rootUri():projectUri(args.path);
      return JSON.stringify(await vscode.workspace.fs.readDirectory(uri));
    }
    if(method==='get_open_file') {
      const editor=vscode.window.activeTextEditor;
      if(editor && (editor.document.uri.scheme!=='arkforge'||editor.document.uri.authority!==rootUri().authority))throw new Error('请先打开当前工程中的文件');
      return JSON.stringify({path:editor?.document.uri.path.replace(/^\/workspace\//,'')||'',version:editor?.document.version||0,content:editor?.document.getText()||''});
    }
    if(method==='build_project'||method==='compile_project') {
      if(!await vscode.workspace.saveAll(false))throw new Error('请完成文件保存后重新构建');
      output.show(true);
    }
    return await native(method,args);
  }
  require('./buildWorkflow').registerBuildWorkflow(context,native,output);
  require('./nativeChat').registerNativeChat(context,native,agentTool,output);
  require('./welcomeGuide').registerWelcomeGuide(context);
};
