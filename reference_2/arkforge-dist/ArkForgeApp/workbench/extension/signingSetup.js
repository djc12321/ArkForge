const vscode=require('vscode');
exports.registerSigningSetup=function(context,native) {
  const json=async(method,args={})=>JSON.parse(await native(method,args));
  let configuring=false;
  const status=async()=>{
    const value=await json('signing_status');
    await vscode.commands.executeCommand('setContext','arkforge.signingReady',value.valid);
    return value;
  };
  const register=(name,run)=>context.subscriptions.push(vscode.commands.registerCommand('arkforge.'+name,async()=>{
    try{return await run();}catch(error){vscode.window.showErrorMessage(error.message);return false;}
  }));
  const importAndCheck=async(method,args={})=>{
    const result=await json(method,args);
    if(result.cancelled)return false;
    const checked=await status();
    if(!checked.valid)throw new Error(checked.reason||'请检查开发签名材料并重新校验');
    vscode.window.showInformationMessage('开发签名已配置：'+checked.bundleName+'。后续本机构建自动签名。');
    return true;
  };
  const manageCertificates=async(prepared,selectedTeam)=>{
    if(!prepared)prepared=await json('huawei.prepare');
    const team=selectedTeam||await vscode.window.showQuickPick(prepared.teams.map(item=>({label:item.name,description:item.id,id:item.id})),{title:'华为调试证书 · 选择团队',ignoreFocusOut:true});
    if(!team)return false;
    const certificates=await json('huawei.certificates',{teamId:team.id});
    const certificate=await vscode.window.showQuickPick(certificates.names.map(name=>({label:name,name})),{title:'华为调试证书 · '+team.label,placeHolder:'选择需要撤销的证书；撤销将影响使用该证书的应用',ignoreFocusOut:true});
    if(!certificate)return false;
    const choice=await vscode.window.showWarningMessage('撤销这张华为调试证书？',{modal:true,detail:'团队：'+team.label+'\n证书：'+certificate.name+'\n撤销后，该证书将永久失效。使用该证书的调试应用需要重新签名。确认后撤销上方所选证书。'},'撤销此证书');
    if(choice!=='撤销此证书')return false;
    await vscode.window.withProgress({location:vscode.ProgressLocation.Notification,title:'正在撤销所选调试证书',cancellable:false},()=>json('huawei.revokeCertificate',{teamId:team.id,certName:certificate.name}));
    vscode.window.showInformationMessage('已撤销：'+certificate.name+'。可重新运行华为自动签名继续配置。');
    return true;
  };
  const autoSigning=async current=>{
    let flow;
    try {
      flow=await vscode.window.withProgress({location:vscode.ProgressLocation.Notification,title:'华为开发者 · 登录',cancellable:false},()=>json('huawei.start'));
      if(!flow.authenticated) {
        const signedIn=await vscode.window.withProgress({location:vscode.ProgressLocation.Notification,title:'等待华为网页登录',cancellable:true},async(progress,token)=>{
          const deadline=Date.now()+flow.expiresIn*1000;
          progress.report({message:'请在华为官方页面完成登录；授权通过后返回 ArkForge'});
          while(!token.isCancellationRequested&&Date.now()<deadline) {
            await new Promise(resolve=>{
              const timer=setTimeout(()=>{subscription.dispose();resolve();},1000);
              const subscription=token.onCancellationRequested(()=>{clearTimeout(timer);subscription.dispose();resolve();});
            });
            if(token.isCancellationRequested)return false;
            const value=await json('huawei.poll',{id:flow.id});
            if(value.state==='authenticated')return true;
            if(value.state!=='pending')return false;
          }
          if(!token.isCancellationRequested)throw new Error('华为网页登录超时，请重新尝试');
          return false;
        });
        if(!signedIn)return false;
      }
      const prepared=await vscode.window.withProgress({location:vscode.ProgressLocation.Notification,title:'读取华为开发者团队与本机设备',cancellable:false},()=>json('huawei.prepare'));
      const team=await vscode.window.showQuickPick(prepared.teams.map(item=>({label:item.name,description:item.id,id:item.id})),{
        title:'华为自动签名 · 选择开发者团队',placeHolder:'为 '+current.bundleName+' 选择签名所属团队',ignoreFocusOut:true
      });
      if(!team)return false;
      const udid=prepared.udid||await vscode.window.showInputBox({title:'华为自动签名 · 当前设备 UDID',prompt:'请填写当前设备的 64 位十六进制 UDID，可从设备调试信息中获取',ignoreFocusOut:true,validateInput:value=>/^[A-Fa-f0-9]{64}$/.test(value)?undefined:'请输入 64 位十六进制 UDID'});
      if(!udid)return false;
      const choice=await vscode.window.showInformationMessage('为当前工程自动配置开发签名？',{
        modal:true,detail:'应用：'+current.bundleName+'\n华为账号：'+prepared.name+'\n团队：'+team.label+'\n设备 UDID：'+udid+'\n将在本机生成私钥，向华为申请调试证书、注册本设备并创建 Profile。材料校验通过后应用于当前工程。'
      },'申请并配置');
      if(!choice)return false;
      let result;
      try {
        result=await vscode.window.withProgress({location:vscode.ProgressLocation.Notification,title:'正在申请华为开发签名',cancellable:false},()=>json('huawei.issue',{teamId:team.id,udid}));
      } catch(error) {
        if(!/205389872/.test(error.message))throw error;
        const certificates=await json('huawei.certificates',{teamId:team.id});
        const action=await vscode.window.showErrorMessage('华为调试证书已达到账号配额（205389872）',{
          modal:true,detail:'团队：'+team.label+'\n现有证书：\n'+certificates.names.map(name=>'• '+name).join('\n')+'\n请选择一张闲置证书，核对后撤销以释放配额，再重新申请。可点击“管理调试证书…”继续。'
        },'管理调试证书…');
        if(action==='管理调试证书…')await manageCertificates(prepared,team);
        return false;
      }
      if(result.cancelled)return false;
      const checked=await status();if(!checked.valid)throw new Error(checked.reason||'签名校验失败');
      vscode.window.showInformationMessage('华为自动签名已配置：'+checked.bundleName+'。后续可在平板离线签名和构建。');
      return true;
    } finally {if(flow?.id)await native('huawei.cancel',{id:flow.id});}
  };
  register('configureSigning',async()=>{
    if(configuring)return false;
    configuring=true;
    try {
      for(;;) {
        const current=await status();
        const item=await vscode.window.showQuickPick([
          {label:'$(account) 华为账号自动签名…',description:'网页登录 → 返回平板 → 自动申请并校验签名',action:'huawei'},
          {label:'$(settings-gear) 管理华为调试证书…',description:'选择团队、查看证书并确认撤销，处理配额限制',action:'huaweiManage'},
          {label:'$(key) 手动配置签名文件…',description:'选择 .p12、.cer、.p7b，填写密钥别名和密码',action:'files'},
          {label:'$(archive) 导入已有签名包…',description:'导入已准备好的开发签名 ZIP',action:'zip'},
          {label:'$(refresh) 重新检查当前签名',description:current.valid?'证书有效期：'+current.expires:current.reason,action:'check'},
          {label:'$(copy) 复制当前应用包名',description:current.bundleName,action:'copy'},
          {label:'$(book) 查看官方签名说明',action:'help'},
          {label:'$(link-external) 打开华为证书管理平台',action:'portal'},
          {label:'$(sign-out) 清除本机华为登录凭据',description:'保留已配置的工程签名',action:'logout'}
        ],{title:'开发签名 · '+current.bundleName,placeHolder:current.valid?'已就绪 · 构建时自动使用现有签名':'首次配置 · 请选择签名材料的获取方式',ignoreFocusOut:true});
        if(!item)return current.valid;
        if(item.action==='huaweiManage'){await manageCertificates();continue;}
        if(item.action==='copy'){await vscode.env.clipboard.writeText(current.bundleName);continue;}
        if(item.action==='check'){
          if(current.valid){vscode.window.showInformationMessage('当前开发签名有效：'+current.bundleName);return true;}
          vscode.window.showWarningMessage(current.reason||'请配置当前工程的开发签名');continue;
        }
        if(item.action==='help'){await native('developer_open',{page:'signingHelp'});continue;}
        if(item.action==='portal'){await native('developer_open',{page:'certificates'});continue;}
        if(item.action==='logout'){
          if(await vscode.window.showWarningMessage('清除此平板保存的华为开发者登录凭据？',{modal:true},'退出')==='退出')await native('huawei.logout');
          continue;
        }
        if(item.action==='huawei') {
          if(await autoSigning(current))return true;
          continue;
        }
        if(item.action==='zip'){if(await importAndCheck('import_signing'))return true;continue;}
        if(item.action==='files') {
          const keyAlias=await vscode.window.showInputBox({title:'手动签名 · 1/4 密钥别名',prompt:'填写创建 .p12 密钥库时使用的别名',ignoreFocusOut:true,validateInput:s=>s.trim()&&!/[\r\n\0]/.test(s)?undefined:'请输入有效密钥别名'});
          if(keyAlias===undefined)continue;
          const storePassword=await vscode.window.showInputBox({title:'手动签名 · 2/4 密钥库密码',password:true,ignoreFocusOut:true,prompt:'密码保存在此设备的系统加密资产库',validateInput:s=>s?undefined:'请输入密钥库密码'});
          if(storePassword===undefined)continue;
          const keyPassword=await vscode.window.showInputBox({title:'手动签名 · 3/4 私钥密码',password:true,ignoreFocusOut:true,prompt:'与密钥库密码相同时可留空'});
          if(keyPassword===undefined)continue;
          const signAlg=await vscode.window.showInputBox({title:'手动签名 · 4/4 签名算法',value:'SHA256withECDSA',ignoreFocusOut:true,prompt:'与现有 DevEco 签名配置的 signAlg 保持一致',validateInput:s=>/^SHA(256|384)withECDSA$/.test(s)?undefined:'请输入官方签名算法，如 SHA256withECDSA'});
          if(signAlg===undefined)continue;
          const choice=await vscode.window.showInformationMessage('依次选择密钥库、签名证书和 Profile',{
            modal:true,detail:'接下来系统文件选择器依次选择 .p12、.cer、.p7b。Profile 的包名必须为 '+current.bundleName+'，并包含当前调试设备。取消任一步骤均保留原有签名配置。'
          },'选择签名文件');
          if(!choice)continue;
          if(await importAndCheck('signing_import_files',{keyAlias:keyAlias.trim(),storePassword,keyPassword:keyPassword||storePassword,signAlg}))return true;
        }
      }
    } finally {configuring=false;}
  });
  register('ensureSigning',async()=>{
    const current=await status();
    if(current.valid)return true;
    const choice=await vscode.window.showInformationMessage('构建前需要配置当前工程的开发签名',{
      modal:true,detail:current.bundleName+'\n'+(current.reason||'请准备当前工程的签名材料')+'\n配置完成后自动继续构建；取消将返回编辑器。'
    },'配置开发签名');
    if(!choice)return false;
    await vscode.commands.executeCommand('arkforge.configureSigning');
    return (await status()).valid;
  });
  // Notify once per workspace/application. Build always checks again, including expiry.
  status().then(async current=>{
    if(current.valid||!context.workspaceState)return;
    const key='signingOnboarding:'+current.bundleName;
    if(context.workspaceState.get(key))return;
    await context.workspaceState.update(key,true);
    const choice=await vscode.window.showInformationMessage('首次使用此工程：配置开发签名后即可在平板构建并安装。','配置开发签名','稍后');
    if(choice==='配置开发签名')await vscode.commands.executeCommand('arkforge.configureSigning');
  }).catch(()=>{});
};
