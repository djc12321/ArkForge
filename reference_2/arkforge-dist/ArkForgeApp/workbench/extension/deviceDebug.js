const vscode=require('vscode');

exports.registerDeviceDebug=function(context,native){
  const output=vscode.window.createOutputChannel('ArkForge Debug');
  context.subscriptions.push(output);
  let busy=false;
  const state=async()=>JSON.parse(await native('debug_state'));
  const display=value=>{
    output.appendLine(value.status);
    for(const frame of value.frames||[]) {
      const source=frame.source;
      output.appendLine('  '+frame.functionName+' '+(source ? source.path+':'+source.line+':'+source.column : frame.url+':'+(frame.location.lineNumber+1)));
    }
    for(const variable of value.variables||[])output.appendLine('  '+variable.name+' = '+variable.value);
    output.show(true);
  };
  context.subscriptions.push(vscode.commands.registerCommand('arkforge.debug',async()=>{
    if(busy)return;
    busy=true;
    try {
      const current=await state();
      const items=current.connected ? [
        {label:current.paused?'$(debug-continue) 继续执行':'$(debug-pause) 暂停执行',action:current.paused?'Debugger.resume':'Debugger.pause'},
        ...(current.paused ? [
          {label:'$(debug-step-over) 单步跳过',action:'Debugger.stepOver'},
          {label:'$(debug-step-into) 单步进入',action:'Debugger.stepInto'},
          {label:'$(debug-step-out) 单步跳出',action:'Debugger.stepOut'}
        ]:[]),
        {label:'$(list-tree) 查看调用栈和变量',action:'state'},
        {label:'$(debug-stop) 断开调试连接',action:'stop'}
      ] : [{label:'$(debug-start) 启动当前工程已安装应用的调试',action:'start',description:'先通过三角按钮完成构建和安装'}];
      const choice=await vscode.window.showQuickPick(items,{title:'ArkForge · 本机调试',placeHolder:current.status});
      if(!choice)return;
      if(choice.action==='state'){display(await state());return;}
      if(choice.action==='start' && !await vscode.workspace.saveAll(false))throw new Error('请完成文件保存后启动调试');
      await vscode.window.withProgress({location:vscode.ProgressLocation.Notification,title:'ArkForge：本机调试',cancellable:false},()=>native('debug_action',{action:choice.action}));
      display(await state());
    } catch(error){output.appendLine(error.message);output.show(true);vscode.window.showErrorMessage(error.message);}
    finally{busy=false;}
  }));
};
