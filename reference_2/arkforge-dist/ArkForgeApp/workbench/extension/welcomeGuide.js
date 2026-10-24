const vscode=require('vscode');
const welcomeKey='arkforge.welcomeGuide.v1';
exports.registerWelcomeGuide=function(context) {
  const open=()=>vscode.commands.executeCommand('workbench.action.openWalkthrough','arkforge.arkforge-device#arkforge.welcome');
  context.subscriptions.push(vscode.commands.registerCommand('arkforge.usageGuide',open));
  context.subscriptions.push(vscode.commands.registerCommand('arkforge.gitGuide',()=>vscode.commands.executeCommand('workbench.action.openWalkthrough','arkforge.arkforge-device#arkforge.gitGuide')));
  const firstRun=(async()=>{
    if(!context.globalState||context.globalState.get(welcomeKey))return;
    await open();
    await context.globalState.update(welcomeKey,true);
  })();
  firstRun.catch(error=>vscode.window.showErrorMessage('请从“帮助 → ArkForge 使用介绍与操作指南”打开欢迎页。'+error.message));
  return firstRun;
};
