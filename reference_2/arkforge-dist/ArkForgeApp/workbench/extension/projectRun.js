const vscode=require('vscode');

exports.registerProjectRun=function(context,native,output,setProgress) {
  let busy=false;
  const status=vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right,100);
  status.text='$(play) 编译当前工程';
  status.tooltip='在平板编译当前 HarmonyOS 工程，成功后确认安装';
  status.command='arkforge.run';status.show();context.subscriptions.push(status);
  context.subscriptions.push(vscode.commands.registerCommand('arkforge.run',async()=>{
    if(busy){vscode.window.showInformationMessage('当前工程正在构建或等待安装确认，请先完成当前操作。');return;}
    busy=true;status.text='$(sync~spin) 编译当前工程';
    const checked=result=>{
      if(!result.includes('BUILD SUCCESSFUL')||result.includes('BUILD FAILED')) {
        const lines=result.split('\n'),index=lines.findIndex(line=>/ArkTS:ERROR/.test(line));
        const end=lines.findIndex((line,i)=>i>index&&/ArkTS:(ERROR|WARN)|BUILD FAILED|> ArkForge|^Error:/.test(line));
        throw new Error(index>=0?lines.slice(index,end>=0?end:undefined).join('\n'):lines.filter(line=>/error (SDK|TS)|Error:|失败|缺少/.test(line)).slice(-3).join('\n')||'操作失败，请查看构建输出');
      }
      return result;
    };
    const task=async(title,method,args={})=>vscode.window.withProgress({location:vscode.ProgressLocation.Notification,title,cancellable:false},async progress=>{
      setProgress(progress);
      try{return checked(await native(method,args));}finally{setProgress(undefined);}
    });
    try {
      if(!await vscode.commands.executeCommand('arkforge.ensureSigning'))return;
      if(!await vscode.workspace.saveAll(false))throw new Error('请完成文件保存后重新构建');
      output.show(true);
      const built=await task('ArkForge：正在本机编译当前工程','compile_project');
      const id=built.match(/> Prepared build: (\d+)/)?.[1];
      if(!id)throw new Error('请查看构建输出，确认 HAP 产物及构建记录');
      status.text='$(play) 等待安装确认';
      const choice=await vscode.window.showInformationMessage('当前工程编译成功，是否安装到此设备？',{
        modal:true,detail:'已在平板完成 SDK 检查、编译、HAP 打包、签名及验签。安装会更新此工程对应的应用；取消后保留已编译产物。'
      },'安装并运行','仅安装');
      if(!choice){output.info('用户取消安装；编译产物已保留，设备应用未更新。');return;}
      status.text='$(sync~spin) 正在安装';
      const installed=await task('ArkForge：正在安装已编译的 HAP','install_prepared',{path:id});
      if(choice==='安装并运行')await native('run_app');
      else vscode.window.showInformationMessage('当前工程已安装成功。','运行应用').then(value=>{
        if(value==='运行应用')vscode.commands.executeCommand('arkforge.launch');
      });
      return installed;
    } catch(error) {
      output.error(error.message);
      // Notifications may stay open indefinitely. Release the build lock as soon
      // as the task finishes so fixing code and pressing Play can start a retry.
      vscode.window.showErrorMessage(error.message,'查看构建输出','查看问题').then(async choice=>{
        if(choice==='查看构建输出')output.show(true);
        if(choice==='查看问题')await vscode.commands.executeCommand('workbench.actions.view.problems');
      }).catch(()=>{});
    } finally {busy=false;status.text='$(play) 编译当前工程';}
  }));
};
