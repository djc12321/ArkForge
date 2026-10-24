const vscode=require('vscode');
exports.registerDeveloperSetup=function(context,native) {
  let signingIn=false;
  const json=async(method,args)=>JSON.parse(await native(method,args));
  const open=page=>native('developer_open',{page});
  const command=(id,run)=>context.subscriptions.push(vscode.commands.registerCommand('arkforge.'+id,async()=>{
    try{return await run();}catch(error){vscode.window.showErrorMessage(error.message);}
  }));
  const updateAccount=async()=>{
    const account=await json('github.status');
    await vscode.commands.executeCommand('setContext','arkforge.githubSignedIn',account.signedIn);
    await vscode.commands.executeCommand('_arkforge.setAccountProfile',account.signedIn ? {login:account.login,avatar:account.avatar} : undefined);
    return account;
  };
  updateAccount().catch(()=>{});
  command('verifyGitHub',async()=>{
    const account=await json('github.verify');
    await updateAccount();
    return vscode.window.showInformationMessage('GitHub 账户有效：'+account.login);
  });
  command('signOutGitHub',async()=>{
    if(await vscode.window.showWarningMessage('退出当前平板上的 GitHub 账户？',{modal:true},'退出')!=='退出')return;
    await native('github.logout');await updateAccount();
    return vscode.window.showInformationMessage('已退出，平板保存的 GitHub 凭据已删除。');
  });
  command('signInGitHub',async()=>{
    if(signingIn)return vscode.window.showInformationMessage('GitHub 登录正在进行，请完成浏览器中的授权或取消当前登录。');
    signingIn=true;let flow;
    try {
      flow=await vscode.window.withProgress({location:vscode.ProgressLocation.Notification,title:'正在连接 GitHub',cancellable:false},()=>json('github.start'));
      await vscode.env.clipboard.writeText(flow.code);
      await open('github');
      const result=await vscode.window.withProgress({location:vscode.ProgressLocation.Notification,title:'GitHub：等待网页授权',cancellable:true},async(progress,token)=>{
        const deadline=Date.now()+flow.expiresIn*1000;let interval=flow.interval;
        while(!token.isCancellationRequested && Date.now()<deadline) {
          progress.report({message:'设备代码 '+flow.code+' 已复制 · 在 GitHub 网页粘贴并授权后返回 ArkForge'});
          // Cancellation also wakes the delay, without leaving a timer/listener behind.
          await new Promise(resolve=>{
            const timer=setTimeout(()=>{subscription.dispose();resolve();},interval*1000);
            const subscription=token.onCancellationRequested(()=>{clearTimeout(timer);subscription.dispose();resolve();});
          });
          if(token.isCancellationRequested)return;
          const reply=await json('github.poll',{id:flow.id});
          if(reply.state==='authenticated')return reply;
          if(reply.state!=='authorization_pending')throw new Error('GitHub 授权已结束：'+reply.state+'，请重新登录。');
          interval=reply.interval||interval;
        }
        if(!token.isCancellationRequested)throw new Error('GitHub 设备代码已过期，请重新登录。');
      });
      if(result) {
        await updateAccount();
        vscode.window.showInformationMessage('已登录 GitHub：'+result.login+'。凭据已存入系统加密资产库；可在本机 Git 终端访问已授权的 GitHub HTTPS 仓库。');
      }
    } finally {
      if(flow)await native('github.cancel',{id:flow.id});
      signingIn=false;
    }
  });
  command('configureGit',async()=>{
    const account=await json('github.status');
    const item=await vscode.window.showQuickPick([
      {label:'$(github) 使用 GitHub 登录',description:account.signedIn?'当前账户：'+account.login:'在 GitHub 官方网页授权',action:'login'},
      {label:'$(edit) 设置 Git 提交身份',description:'配置 user.name 和 user.email，与登录身份分别管理',action:'identity'},
      {label:'$(key) 使用个人访问令牌',description:'验证令牌后保存到鸿蒙加密资产库',action:'token'},
      {label:'$(link-external) 管理 GitHub 授权',description:'在 GitHub 官方网站查看或撤销权限',action:'access'},
      {label:'$(terminal) 打开本机终端',description:'官方 Shell 与 Git · 文件操作及 GitHub HTTPS 仓库',action:'terminal'},
      ...(account.signedIn?[{label:'$(sync) 核对 GitHub 登录状态',description:'直连 GitHub，临近过期时自动刷新令牌',action:'verify'},
        {label:'$(sign-out) 退出 '+account.login,description:'清除此平板的登录凭据；授权管理可前往 GitHub 官网',action:'logout'}]:[])
    ],{title:'Git · 账户与提交身份',placeHolder:'GitHub 登录与 Git 提交身份是两项独立配置'});
    if(!item)return;
    if(item.action==='login')return vscode.commands.executeCommand('arkforge.signInGitHub');
    if(item.action==='access')return open('githubAccess');
    if(item.action==='terminal')return vscode.commands.executeCommand('arkforge.openGitTerminal');
    if(item.action==='verify') {
      const verified=await json('github.verify');
      return vscode.window.showInformationMessage('GitHub 账户有效：'+verified.login);
    }
    if(item.action==='logout') {
      return vscode.commands.executeCommand('arkforge.signOutGitHub');
    }
    if(item.action==='token') {
      const choice=await vscode.window.showQuickPick(['输入已有令牌','打开 GitHub 创建令牌'],{title:'GitHub · 个人访问令牌'});
      if(!choice)return;
      if(choice==='打开 GitHub 创建令牌')await open('tokens');
      const token=await vscode.window.showInputBox({title:'GitHub · 个人访问令牌',password:true,ignoreFocusOut:true,prompt:'平板直连 GitHub 验证令牌，验证后保存到系统加密资产库'});
      if(!token)return;
      const result=await json('github.token',{token:token.trim()});
      await updateAccount();
      return vscode.window.showInformationMessage('已验证 GitHub 账户：'+result.login+'。可在本机 Git 终端访问已授权的 GitHub HTTPS 仓库。');
    }
    const name=await vscode.window.showInputBox({title:'Git · 提交者姓名',prompt:'写入此平板的 Git 全局 user.name',validateInput:value=>value.trim()&&!/[\r\n\0]/.test(value)?undefined:'请输入有效姓名'});
    if(name===undefined)return;
    const email=await vscode.window.showInputBox({title:'Git · 提交者邮箱',prompt:'可使用 GitHub 提供的 noreply 邮箱',validateInput:value=>/^[^\s@]+@[^\s@]+$/.test(value)?undefined:'请输入有效邮箱'});
    if(email===undefined)return;
    await native('git_identity',{name:name.trim(),email:email.trim()});
    return vscode.window.showInformationMessage('Git 提交身份已保存。');
  });
  require('./signingSetup').registerSigningSetup(context,native);
  command('developerSetup',async()=>{
    const item=await vscode.window.showQuickPick([
      {label:'$(repo) 配置 Git 与 GitHub',description:'提交身份、官网登录、令牌与退出',command:'arkforge.configureGit'},
      {label:'$(key) 配置当前工程开发签名',description:'查看有效性、官方证书平台、手动导入',command:'arkforge.configureSigning'},
      {label:'$(book) 查看本机构建操作流程',command:'arkforge.buildGuide'}
    ],{title:'ArkForge · 开发环境配置',placeHolder:'先打开 HarmonyOS 工程，再配置对应的开发签名'});
    if(item)return vscode.commands.executeCommand(item.command);
  });
  command('benchmarkSDKStaging',async()=>{
    const result=await vscode.window.withProgress({location:vscode.ProgressLocation.Notification,title:'测量平板 SDK 暂存耗时',cancellable:false},()=>json('benchmark_sdk_staging'));
    await vscode.window.showInformationMessage('SDK 暂存实测：重新生成 '+result.rewriteMs+' ms，校验复用 '+result.reuseMs+' ms。此数据为 SDK 暂存阶段耗时；完整构建耗时请查看构建输出。',{modal:true});
  });
};
