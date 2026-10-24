const vscode=require('vscode');
const tools=require('./tools.json');
const {runAgentTask,cancellableDelay}=require('./agentLoop');
const agentContext=require('./agentContext');

exports.registerNativeChat=function(context,native,agentTool,output) {
  const config=async()=>JSON.parse(await native('agent_config_get'));
  const schema=tool=>({type:'object',properties:tool.properties,required:tool.required,additionalProperties:false});
  const modelEvents=new vscode.EventEmitter();context.subscriptions.push(modelEvents);
  for(const tool of tools)context.subscriptions.push(vscode.lm.registerTool('arkforge_'+tool.name,{
    prepareInvocation:options=>({invocationMessage:tool.label,...(tool.name==='delete_file'?{confirmationMessages:{title:'删除工程文件',message:'删除 '+options.input.path+'。源文件会先保留一份本地历史。'}}:{})}),
    invoke:async(options,token)=>{
      if(token.isCancellationRequested)throw new vscode.CancellationError();
      const result=await agentTool(tool.name,options.input);
      return new vscode.LanguageModelToolResult([new vscode.LanguageModelTextPart(result)]);
    }
  }));

  context.subscriptions.push(vscode.commands.registerCommand('arkforge.configureAgentContext',()=>vscode.commands.executeCommand('workbench.action.openSettings','arkforge.agent')));
  context.subscriptions.push(vscode.commands.registerCommand('arkforge.configureAgent',async()=>{
    const previous=await config();
    const base=await vscode.window.showInputBox({title:'ArkForge · 模型 API 地址',value:previous.base,prompt:'填写模型服务的 HTTPS API 地址，平板将直接连接',validateInput:value=>/^https:\/\/[^\s]+$/.test(value)?undefined:'请输入 HTTPS API 地址'});
    if(base===undefined)return;
    const model=await vscode.window.showInputBox({title:'ArkForge · 模型名称',value:previous.model,validateInput:value=>value.trim()?undefined:'请输入模型名称'});
    if(model===undefined)return;
    const key=await vscode.window.showInputBox({title:'ArkForge · API Key',password:true,ignoreFocusOut:true,prompt:previous.hasKey?'留空保留已保存密钥；密钥存入鸿蒙系统加密资产库':'密钥保存在鸿蒙系统加密资产库'});
    if(key===undefined)return;
    await native('agent_config_save',{base:base.trim(),model:model.trim(),key:key.trim()});
    modelEvents.fire();vscode.window.showInformationMessage('模型配置已保存。文件、构建、签名、安装和日志工具在平板本机执行。');
  }));

  async function query(messages,token,options={}) {
    if(token.isCancellationRequested)throw new vscode.CancellationError();
    const settings=await config();
    if(token.isCancellationRequested)throw new vscode.CancellationError();
    if(!settings.hasKey)throw new Error('请运行“ArkForge: 配置 DeepSeek / 模型 API”填写密钥，即可使用 Agent。本机构建和流程测试可直接使用。');
    const stop=token.onCancellationRequested(()=>native('cancel_model').catch(()=>{}));
    try {
      const contextSettings=agentContext.settings(vscode);
      let maxTokens=contextSettings.maxOutputTokens;
      for(let attempt=0;attempt<4;attempt++) {
        if(token.isCancellationRequested)throw new vscode.CancellationError();
        try {
          const body={model:settings.model,stream:false,max_tokens:maxTokens,messages,...options};
          if(settings.base.includes('api.deepseek.com'))body.thinking={type:'disabled'};
          const response=JSON.parse(await native('chat',{base:settings.base,body:JSON.stringify(body)}));
          const choice=response.choices?.[0];
          if(!choice?.message)throw new Error('模型未返回有效消息');
          if(choice.finish_reason==='length') {maxTokens=Math.min(maxTokens*2,32768);throw new Error('模型输出达到长度限制，正在扩大输出窗口');}
          if(choice.finish_reason==='content_filter')throw new Error('模型服务要求调整本轮请求内容');
          if(['insufficient_system_resource','aborted'].includes(choice.finish_reason))throw new Error('模型服务暂时中断：'+choice.finish_reason);
          for(const call of choice.message.tool_calls||[])JSON.parse(call.function.arguments||'{}');
          return {...choice.message,__usage:response.usage};
        } catch(error) {
          if(token.isCancellationRequested)throw new vscode.CancellationError();
          const retry=/HTTP (408|429|500|502|503|504)|timeout|timed out|超时|connect.*fail|network|socket|2300028|2300007|2300006|2300052|输出达到长度限制|insufficient_system_resource|aborted/i.test(error.message);
          if(!retry||attempt===3)throw error;
          output.info('模型连接重试 '+(attempt+1)+' / 3：'+error.message);
          await cancellableDelay([1000,3000,8000][attempt],token);
        }
      }
    } finally {stop.dispose();}
  }

  context.subscriptions.push(vscode.lm.registerLanguageModelChatProvider('arkforge-deepseek',{
    onDidChangeLanguageModelChatInformation:modelEvents.event,
    provideLanguageModelChatInformation:async()=>{const settings=await config(),limits=agentContext.settings(vscode);return [{id:settings.model,name:settings.model,family:'deepseek',isDefault:true,isUserSelectable:true,version:'1',maxInputTokens:limits.contextWindow-limits.maxOutputTokens,maxOutputTokens:limits.maxOutputTokens,capabilities:{toolCalling:true},detail:'平板直连',tooltip:'ArkForge 本机工具；模型推理由平板直连 API'}];},
    provideTokenCount:async(model,text)=>new TextEncoder().encode(typeof text==='string'?text:JSON.stringify(text)).length,
    provideLanguageModelChatResponse:async(model,messages,options,progress,token)=>{
      const converted=[];
      for(const message of messages) {
        const text=message.content.filter(part=>part instanceof vscode.LanguageModelTextPart).map(part=>part.value).join('\n');
        const calls=message.content.filter(part=>part instanceof vscode.LanguageModelToolCallPart);
        const results=message.content.filter(part=>part instanceof vscode.LanguageModelToolResultPart);
        if(text||calls.length)converted.push({role:message.role===vscode.LanguageModelChatMessageRole.Assistant?'assistant':'user',content:text,...(calls.length?{tool_calls:calls.map(call=>({id:call.callId,type:'function',function:{name:call.name,arguments:JSON.stringify(call.input)}}))}:{})});
        for(const result of results)converted.push({role:'tool',tool_call_id:result.callId,content:result.content.filter(part=>part instanceof vscode.LanguageModelTextPart).map(part=>part.value).join('\n')});
      }
      const definitions=(options.tools||[]).map(tool=>({type:'function',function:{name:tool.name,description:tool.description,parameters:tool.inputSchema}}));
      const response=await query(converted,token,definitions.length?{tools:definitions}:{});
      if(response.content)progress.report(new vscode.LanguageModelTextPart(response.content));
      for(const call of response.tool_calls||[])progress.report(new vscode.LanguageModelToolCallPart(call.id,call.function.name,JSON.parse(call.function.arguments||'{}')));
    }
  }));

  vscode.lm.selectChatModels({vendor:'arkforge-deepseek'}).then(models=>output.info('ArkForge：原生 Chat 已加载 '+models.length+' 个平板直连模型'),error=>output.error('模型注册失败：'+error.message));

  context.subscriptions.push(vscode.commands.registerCommand('arkforge.resumeAgent',()=>vscode.commands.executeCommand('workbench.action.chat.open',{query:'继续上次任务，从已保存的进度继续修复并验证。'})));
  const participant=vscode.chat.createChatParticipant('arkforge.agent',async(request,chatContext,stream,token)=>{
    const direct={build:'build_project',logs:'read_logs',file:'get_open_file',test:'test_build_workflow',guide:'build_guide',setup:'configure_agent'};
    if(request.command && direct[request.command]) {
      try {
        stream.progress('正在执行 '+request.command+'…');
        const result=await (request.command==='test'?vscode.commands.executeCommand('arkforge.testBuildWorkflow'):
          request.command==='guide'?vscode.commands.executeCommand('arkforge.buildGuide'):
          request.command==='setup'?vscode.commands.executeCommand('arkforge.configureAgent'):
          vscode.lm.invokeTool('arkforge_'+direct[request.command],{input:{},toolInvocationToken:request.toolInvocationToken},token).then(value=>value.content.filter(part=>part instanceof vscode.LanguageModelTextPart).map(part=>part.value).join('\n')));
        if(result)stream.markdown('```text\n'+String(result).slice(-16000)+'\n```');
        return {};
      } catch(error) {return {errorDetails:{message:error.message}};}
    }
    const workspaceUri=vscode.workspace.workspaceFolders?.[0]?.uri;
    if(!workspaceUri)return {errorDetails:{message:'请先打开一个 HarmonyOS 工程，再使用 Agent。'}};
    const workspace=workspaceUri.toString();
    const settings=await config();
    if(!settings.hasKey) {
      stream.markdown('请先配置模型 API。工程文件和全部开发工具在平板本机执行。');
      stream.button({command:'arkforge.configureAgent',title:'配置 DeepSeek / 模型 API'});
      stream.button({command:'arkforge.build',title:'手动编译安装'});
      stream.button({command:'arkforge.testBuildWorkflow',title:'测试本机构建流程'});
      stream.button({command:'arkforge.buildGuide',title:'查看构建方法'});
      return {};
    }
    const key='arkforge.agent.checkpoint:'+workspace;
    const previous=[...chatContext.history].reverse().find(turn=>Array.isArray(turn.result?.metadata?.messages));
    const resume=/^(?:继续|接着|恢复|continue|resume)/i.test(request.prompt.trim());
    const saved=resume?context.globalState?.get(key):undefined;
    const history=previous?.result.metadata.workspace===workspace?previous.result.metadata:undefined;
    const prior=resume&&saved?.workspace===workspace&&saved.taskState?.status!=='completed'?saved:history;
    const messages=prior?.messages?.slice()||[];
    messages.push({role:'user',content:request.prompt});
    try {
    for(const reference of (request.references||[]).slice(0,4)) {
      const uri=reference.value?.uri||reference.value;
      if(uri?.scheme==='arkforge' && uri.authority===vscode.workspace.workspaceFolders[0].uri.authority) {
        const result=await agentTool('read_file',{path:uri.path.replace(/^\/workspace\//,''),start_line:reference.value?.range?.start.line+1||1});
        messages.push({role:'user',content:'附加的工程文件：\n'+result});stream.reference(uri);
      }
    }
      const definitions=tools.map(tool=>({type:'function',function:{name:tool.name,description:tool.description,parameters:schema(tool)}}));
      const goal=resume?(prior?.taskState?.goal||prior?.messages?.find(message=>message.role==='user'&&!/^(?:继续|接着|恢复|continue|resume)/i.test(message.content))?.content||request.prompt):request.prompt;
      return await runAgentTask({messages,workspace,goal,context:agentContext.settings(vscode),state:resume?prior?.taskState:undefined,token,stream,
        system:'你是 ArkForge 当前设备上的 HarmonyOS 开发助手。所有工具在本机执行。先读取源码和版本再精确修改。修改已有文件优先精确替换，保留公开方法、测试入口和调用方约定；保存整个文件前先完整读取当前版本。持续完成用户目标，可跨越多轮读取、搜索、修改和复查。先用 get_diagnostics(mode=summary) 一次取得全项目诊断，按文件和原因分组规划；使用 read_files 批量读取相关源码，小文件一次读完。避免重复翻页和反复读取相同内容。每批修改后编译并重新汇总诊断。清理所有 warnings 时按规则和文件分组逐项修复，保持检查规则和原有行为；先修复 ERROR。以当前源码重新构建的真实结果为完成依据。用户只要求检查时使用 compile_project，明确要求安装才使用 build_project。工具错误先调整参数或读取最新版本继续。若确需用户提供信息，明确给出进度和所需信息。',
        query:(messages,token)=>query(messages,token,{tools:definitions}),
        diagnostics:async()=>JSON.parse(await native('sdk_diagnostics')),
        checkpoint:context.globalState?data=>context.globalState.update(key,data):undefined,
        invoke:async(name,input)=>{
          const tool=tools.find(tool=>tool.name===name);
          if(!tool)throw new Error('模型请求了未注册的工具');
          stream.progress(tool.label+'…');
          const value=await vscode.lm.invokeTool('arkforge_'+name,{input,toolInvocationToken:request.toolInvocationToken},token);
          const result=value.content.filter(part=>part instanceof vscode.LanguageModelTextPart).map(part=>part.value).join('\n');
          if(input.path&&['read_file','write_file','replace_text'].includes(name))stream.reference(vscode.Uri.joinPath(vscode.workspace.workspaceFolders[0].uri,input.path));
          output.info('Agent 工具 '+name+'：'+(/BUILD FAILED|^工具失败：/.test(result)?'失败':'已完成'));
          return result;
        }
      });
    } catch(error) {return {errorDetails:{message:error.message},metadata:{messages,workspace}};}
  });
  participant.additionalWelcomeMessage='使用平板本机工具开发 HarmonyOS 工程。输入 /build 构建安装，/test 测试流程，/guide 查看方法，/setup 配置模型 API。';
  participant.iconPath=new vscode.ThemeIcon('tools');
  participant.followupProvider={provideFollowups:()=>[
    {prompt:'编译并安装当前工程',command:'build',label:'本机构建并安装'},
    {prompt:'验证当前工程的构建安装流程',command:'test',label:'测试构建流程'},
    {prompt:'说明手动构建安装的操作步骤',command:'guide',label:'构建方法说明'}
  ]};
  context.subscriptions.push(participant);
  context.subscriptions.push(vscode.commands.registerCommand('arkforge.openChat',()=>vscode.commands.executeCommand('workbench.action.chat.open')));
};
