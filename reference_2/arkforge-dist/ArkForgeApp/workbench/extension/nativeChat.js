const vscode=require('vscode');
const tools=require('./tools.json');

exports.registerNativeChat=function(context,native,agentTool,output) {
  const config=async()=>JSON.parse(await native('agent_config_get'));
  const schema=tool=>({type:'object',properties:tool.properties,required:tool.required,additionalProperties:false});
  const modelEvents=new vscode.EventEmitter();context.subscriptions.push(modelEvents);
  for(const tool of tools)context.subscriptions.push(vscode.lm.registerTool('arkforge_'+tool.name,{
    prepareInvocation:()=>({invocationMessage:tool.label}),
    invoke:async(options,token)=>{
      if(token.isCancellationRequested)throw new vscode.CancellationError();
      const result=await agentTool(tool.name,options.input);
      return new vscode.LanguageModelToolResult([new vscode.LanguageModelTextPart(result)]);
    }
  }));

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
      const body={model:settings.model,stream:false,messages,...options};
      if(settings.base.includes('api.deepseek.com'))body.thinking={type:'disabled'};
      const response=JSON.parse(await native('chat',{base:settings.base,body:JSON.stringify(body)}));
      if(!response.choices?.[0]?.message)throw new Error('模型未返回有效消息');
      return response.choices[0].message;
    } finally {stop.dispose();}
  }

  context.subscriptions.push(vscode.lm.registerLanguageModelChatProvider('arkforge-deepseek',{
    onDidChangeLanguageModelChatInformation:modelEvents.event,
    provideLanguageModelChatInformation:async()=>{const settings=await config();return [{id:settings.model,name:settings.model,family:'deepseek',isDefault:true,isUserSelectable:true,version:'1',maxInputTokens:64000,maxOutputTokens:8000,capabilities:{toolCalling:true},detail:'平板直连',tooltip:'ArkForge 本机工具；模型推理由平板直连 API'}];},
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
    const previous=[...chatContext.history].reverse().find(turn=>Array.isArray(turn.result?.metadata?.messages));
    const messages=previous?.result.metadata.workspace===workspace?previous.result.metadata.messages.slice():[];
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
      for(let round=0;round<12&&!token.isCancellationRequested;round++) {
        stream.progress('正在分析工程…');
        const response=await query([{role:'system',content:'你是 ArkForge 平板上的 HarmonyOS 开发助手。所有文件、编译、资源编译、签名、安装和调试工具在当前平板本机执行，不使用电脑或 shell。先读取源码和版本，再用 replace_text 或 write_file 修改；构建失败后按真实 SDK 错误修复。只有工具明确成功才能报告成功。构建通过不代表应用已经运行。用户仅要求编译检查或明确不要安装时，必须使用 compile_project；只有明确要求安装时才能使用 build_project。'},...messages],token,{tools:definitions});
        if(token.isCancellationRequested)break;
        const calls=response.tool_calls||[];
        messages.push({role:'assistant',content:response.content||'',...(calls.length?{tool_calls:calls}:{})});
        if(response.content)stream.markdown(response.content);
        if(!calls.length)return {metadata:{messages,workspace}};
        for(const call of calls) {
          if(token.isCancellationRequested){messages.push({role:'tool',tool_call_id:call.id,content:'已取消，未执行'});continue;}
          let result;
          try {
            const tool=tools.find(tool=>tool.name===call.function.name);
            if(!tool)throw new Error('模型请求了未注册的工具');
            const input=JSON.parse(call.function.arguments||'{}');
            stream.progress(tool.label+'…');
            const value=await vscode.lm.invokeTool('arkforge_'+tool.name,{input,toolInvocationToken:request.toolInvocationToken},token);
            result=value.content.filter(part=>part instanceof vscode.LanguageModelTextPart).map(part=>part.value).join('\n');
            if(input.path && ['read_file','write_file','replace_text'].includes(tool.name))stream.reference(vscode.Uri.joinPath(vscode.workspace.workspaceFolders[0].uri,input.path));
          } catch(error) {result='工具失败：'+error.message;}
          // Preserve final diagnostics and build/install status in long logs.
          // A prefix-only truncation can hide the very error the Agent must fix.
          const content=result.length>24000?result.slice(0,6000)+'\n[中间输出已省略；以下为末尾结果]\n'+result.slice(-18000):result;
          output.info('Agent 工具 '+call.function.name+'：'+(/BUILD FAILED|^工具失败：/.test(result)?'失败':'已完成'));
          messages.push({role:'tool',tool_call_id:call.id,content});
        }
      }
      if(!token.isCancellationRequested)stream.markdown('\n已完成 12 轮工具调用。请检查当前结果后继续。');
      return {metadata:{messages,workspace}};
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
