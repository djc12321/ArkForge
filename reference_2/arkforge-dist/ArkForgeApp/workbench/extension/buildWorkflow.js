const vscode=require('vscode');
exports.registerBuildWorkflow=function(context,native,output) {
  const steps=[
    {label:'1. 打开真实 HarmonyOS 工程',description:'打开工程 ZIP',detail:'选择 HarmonyOS 工程 ZIP，根目录应包含 build-profile.json5、AppScope 和实际模块目录。',command:'arkforge.openProject'},
    {label:'2. 编辑并保存源码',description:'资源管理器 → 打开 .ets 文件 → 保存',detail:'使用 ArkForge 编辑器修改源码。构建前会保存工程中已打开的文件；保存失败时终止构建。',command:'workbench.action.files.saveAll'},
    {label:'3. 准备开发签名',description:'华为账号自动签名或手动导入',detail:'选择华为账号自动签名，在官方网页登录后返回平板，选择开发者团队并确认申请。也可手动导入已有签名。请使用匹配 bundleName、包含当前设备且在有效期内的签名；密码保存在鸿蒙系统加密资产库。',command:'arkforge.configureSigning'},
    {label:'4. 检查本机 SDK',description:'官方 SDK 检查器及资源编译器自检',detail:'验证平板本机 SDK 检查器的类型、语法及 ArkUI 规则检查能力，并自检资源编译器。当前工程的完整检查在构建时执行。',command:'arkforge.checkSDK'},
    {label:'5. 点击三角按钮编译当前工程',description:'工程与签名检查 → 资源编译 → SDK 检查 → ArkUI → ABC → HAP → 签名 → 弹窗确认安装',detail:'点击编辑器三角按钮或状态栏「编译当前工程」，在平板本机构建成功后，使用 ArkForge 弹窗选择「安装并运行」或「仅安装」。选择安装方式后继续；选择取消返回编辑器。完整日志位于 Output → ArkForge Build；SDK 错误同时进入 Problems，可点击返回源码。相同构建输入会复用已校验的缓存。',command:'arkforge.build'},
    {label:'6. 运行应用并返回编辑器',description:'运行最近安装的应用',detail:'点击运行，在系统弹窗中选择打开。通过系统返回手势或应用提供的返回入口回到 ArkForge，继续修改和构建。',command:'arkforge.launch'},
    {label:'7. 本机调试',description:'启动调试、暂停、单步、调用栈和变量',detail:'先构建并安装当前工程，再选择「ArkForge: 本机调试…」。连接后再次执行该命令，可暂停、继续、单步及查看调用栈和变量；结果显示在 Output → ArkForge Debug。',command:'arkforge.debug'},
    {label:'8. 读取运行日志',description:'运行失败时查看真实设备日志',detail:'读取最近安装应用的本机日志。编译成功、安装成功和运行成功分别确认。',command:'arkforge.logs'}
  ];
  context.subscriptions.push(vscode.commands.registerCommand('arkforge.buildGuide',async()=>{
    const item=await vscode.window.showQuickPick(steps,{title:'ArkForge · 平板本机构建方法',placeHolder:'选择一个步骤查看说明或执行'});
    if(!item)return;
    const choice=await vscode.window.showInformationMessage(item.detail,{modal:true},'执行此步骤','查看构建输出');
    if(choice==='执行此步骤')return vscode.commands.executeCommand(item.command);
    if(choice==='查看构建输出')output.show(true);
  }));
  context.subscriptions.push(vscode.commands.registerCommand('arkforge.testBuildWorkflow',async()=>{
    const result=await vscode.window.withProgress({location:vscode.ProgressLocation.Notification,title:'ArkForge：验证本机构建流程',cancellable:false},async progress=>{
      if(!await vscode.workspace.saveAll(false))throw new Error('文件保存失败，流程测试已停止');
      if(!await vscode.commands.executeCommand('arkforge.ensureSigning'))return;
      output.show(true);progress.report({message:'检查真实 HarmonyOS 工程',increment:10});
      const project=JSON.parse(await native('project_preflight'));
      output.info('当前工程：'+project.bundleName+' / '+project.moduleName);
      progress.report({message:'验证平板本机 SDK 和资源编译器',increment:20});
      const checked=await native('check_sdk');
      if(!checked.includes('BUILD SUCCESSFUL')||checked.includes('BUILD FAILED'))throw new Error('请查看 ArkForge Build 中的 SDK 自检结果并按提示处理');
      progress.report({message:'本机编译、资源编译、打包、签名和安装',increment:30});
      const built=await native('build_project');
      if(!built.includes('BUILD SUCCESSFUL')||built.includes('BUILD FAILED'))throw new Error('请查看 Problems 和 ArkForge Build 中的构建安装结果并按提示处理');
      progress.report({message:'构建和安装已验证',increment:40});
      return '流程测试通过：真实工程检查、SDK 自检、本机编译、资源处理、HAP 打包、签名及安装。运行结果请打开应用单独确认。';
    });
    if(!result)return;
    const choice=await vscode.window.showInformationMessage(result,'运行应用','查看构建输出');
    if(choice==='运行应用')await vscode.commands.executeCommand('arkforge.launch');
    if(choice==='查看构建输出')output.show(true);
    return result;
  }));
};
