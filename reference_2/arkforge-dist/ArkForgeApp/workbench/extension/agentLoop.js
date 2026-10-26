// The task ends on a verified result, cancellation, or an actionable interruption.
// Tool execution is never retried by the network retry policy.
const HISTORY_LIMIT = 36000;
const { historyLimit } = require('./agentContext');
const MUTATIONS = new Set(['write_file', 'replace_text', 'create_directory', 'rename_file', 'delete_file']);
const BUILDS = new Set(['compile_project', 'build_project']);

function toolSummary(call, result) {
  let input = {};
  try { input = JSON.parse(call.function.arguments || '{}'); } catch (_) {}
  const detail = result.length > 900 ? result.slice(0, 300) + ' … ' + result.slice(-600) : result;
  return call.function.name + (input.path ? ' ' + input.path : '') + ': ' + detail;
}

function compactHistory(messages, state, limit = HISTORY_LIMIT) {
  if (JSON.stringify(messages).length <= limit) return messages;
  const groups = [];
  for (const message of messages) {
    if (message.role === 'tool' && groups.length) groups[groups.length - 1].push(message);
    else groups.push([message]);
  }
  const reserve = Math.min(1500, Math.floor(limit / 4));
  let used = 0;
  const recent = [];
  // Keep complete assistant/tool groups: never send orphan tool responses.
  for (let i = groups.length - 1; i >= 0; i--) {
    const bytes = JSON.stringify(groups[i]).length;
    if (used + bytes > limit - reserve - 1000) break;
    recent.unshift(...groups[i]); used += bytes;
  }
  const summaryBudget = Math.min(10500, Math.max(500, limit - used - 1000));
  const summary = {
    goal: state.goal,
    revision: state.revision,
    lastBuild: state.lastBuild,
    runtimeEvidence:state.runtimeEvidence,
    lastFlow:state.lastFlow,
    diagnosticProgress: Object.fromEntries(Object.entries(state.diagnosticProgress || {}).map(([key,p])=>[key,{matched:p.matched,nextOffset:p.nextOffset,complete:p.complete,files:p.files.slice(0,12)}])),
    recentOperations: (state.operations || []).slice(-12).reverse(),
    instruction: '继续原任务。历史源码仅作记录，修改前重新读取当前文件及版本。使用 get_diagnostics(mode=summary) 一次查看剩余诊断，再 read_files 批量读取相关源码，最终重新编译验证。'
  };
  return [{ role: 'user', content: '任务进度检查点：\n' + JSON.stringify(summary).slice(0, summaryBudget) }, ...recent];
}

function needsVerification(state) {
  if(state.runtimeRepair && !(state.lastFlow?.status==='passed'&&state.lastFlow.revision===state.revision))return '请读取 get_runtime_diagnostics 的真实故障证据，并用 test_app_flow 复测原点击路径和 expected_id；编译通过后继续运行验证。';
  if (!state.verifyBuild) return '';
  const build = state.lastBuild;
  if (!build || build.revision !== state.revision) return '请调用 compile_project 验证当前修改，再核对 get_diagnostics，完成后汇报实际结果。';
  if (!build.ok) return '当前构建尚有错误。请读取真实诊断，修复并重新调用 compile_project。';
  if (state.warningFree && (!build.diagnosticsKnown || build.errors || build.warnings)) {
    return '本轮目标是清理全部 warnings。当前构建检查：' + JSON.stringify(build) + '。请用 get_diagnostics(mode=summary) 汇总剩余诊断，按文件修复后重新编译。保留检查规则和应用行为。';
  }
  return '';
}

async function runAgentTask(options) {
  const { token, stream, query, invoke, diagnostics, workspace, checkpoint, system } = options;
  let messages = options.messages.slice();
  const state = options.state ? {...options.state,operations:[...(options.state.operations || [])]} : { goal: options.goal, revision: 0, operations: [],
    runtimeRepair:/闪退|崩溃|crash|运行.*(?:异常|报错)/i.test(options.goal),
    verifyBuild: /编译|构建|警告|闪退|崩溃|\b(?:warnings?|compile|build|compile_project|build_project|crash)\b/i.test(options.goal),
    warningFree: /(?:所有|全部|清理|消除|修复|解决|all|zero|remove|fix).{0,30}(?:warnings?|警告)|(?:warnings?|警告).{0,30}(?:全部|清零|清理|all|zero)/i.test(options.goal) };
  const noCompile = /(?:不要|不|无需|禁止|不用)(?:再|重新|进行|执行)?(?:编译|构建)|\b(?:do not|don't|without)\s+(?:re)?(?:compile|build)\b/i.test(state.goal);
  if(noCompile)state.verifyBuild=false;
  const allowInstall = (/安装|\binstall\b/i.test(state.goal) || state.runtimeRepair && /修复|解决|\b(?:fix|repair)\b/i.test(state.goal)) &&
    !/(?:不要|不|无需|禁止).{0,4}安装|do not install|without install/i.test(state.goal);
  state.operations ||= [];
  let noTools = 0, repeated = 0, fingerprint = '', rounds = 0;
  let visits = new Map();
  let limit = options.context ? historyLimit(options.context) : HISTORY_LIMIT;
  let contextRetries = 0;
  const compact = () => {
    let effectiveLimit = limit;
    const pressure = options.context && state.usage?.prompt_tokens >=
      (options.context.contextWindow - options.context.maxOutputTokens) * options.context.compressionThreshold;
    if (pressure && state.compactedUsage !== state.usageSerial) {
      effectiveLimit = Math.max(6000, Math.min(limit, Math.floor(JSON.stringify(messages).length * 0.65)));
      state.compactedUsage = state.usageSerial;
    }
    const next = compactHistory(messages, state, effectiveLimit);
    if (next !== messages) {
      state.compactions = (state.compactions || 0) + 1;
      stream.progress('正在整理长对话上下文，保留任务目标、修改记录和最新诊断');
    }
    messages = next;
  };
  const save = async status => {
    compact();
    const data = { workspace, messages, taskState: { ...state, status }, updatedAt: Date.now() };
    try { await checkpoint?.(data); }
    catch (_) { stream.progress('任务进度保留在当前会话，正在继续处理'); }
    return { metadata: data };
  };
  try {
    while (!token.isCancellationRequested) {
      compact();
      stream.progress('正在处理工程 · 第 ' + (++rounds) + ' 轮');
      let response;
      try {
        response = await query([{ role: 'system', content: system }, ...messages], token);
        contextRetries = 0;
      } catch(error) {
        if(!token.isCancellationRequested && /context.{0,30}(?:length|window|limit)|maximum.{0,30}tokens|too many tokens|上下文.{0,10}(?:超|限制)/i.test(error.message) && contextRetries++<3 && limit>6000) {
          limit=Math.max(6000,Math.floor(limit*0.6));
          messages=compactHistory(messages,state,limit);
          state.compactions=(state.compactions||0)+1;
          stream.progress('正在按模型额度整理上下文，随后继续当前任务');
          continue;
        }
        throw error;
      }
      if (response.__usage) { state.usage = response.__usage; state.usageSerial = (state.usageSerial || 0) + 1; }
      if (token.isCancellationRequested) break;
      const calls = response.tool_calls || [];
      messages.push({ role: 'assistant', content: response.content || '', ...(typeof response.reasoning_content==='string'?{reasoning_content:response.reasoning_content}:{}), ...(calls.length ? { tool_calls: calls } : {}) });
      if (response.content && (calls.length || !needsVerification(state))) stream.markdown(response.content);
      if (!calls.length) {
        const pending = needsVerification(state);
        if (!pending) return await save('completed');
        if (++noTools >= 3) {
          stream.markdown('\n已保存当前修改和检查进度。' + pending);
          stream.button?.({ command: 'arkforge.resumeAgent', title: '继续修复并验证' });
          return await save('paused');
        }
        messages.push({ role: 'user', content: pending });
        continue;
      }
      noTools = 0;
      const outcomes = [];
      const guidance = [];
      let cycling = false;
      let interruption = '';
      for (const call of calls) {
        let result, input = {};
        if (token.isCancellationRequested) result = '用户已停止本轮任务，此工具尚未执行。';
        else if (interruption) result = '本轮进度已保存，系统额度恢复后继续此步骤。';
        else {
          try {
            input = JSON.parse(call.function.arguments || '{}');
            if (noCompile && BUILDS.has(call.function.name)) throw new Error('本轮按用户要求执行运行验收，请使用运行诊断与界面测试工具。');
            if (call.function.name === 'build_project' && !allowInstall) throw new Error('本轮使用 compile_project 完成编译检查。需要安装时由用户发起安装操作。');
            if (MUTATIONS.has(call.function.name)) state.revision++;
            if (BUILDS.has(call.function.name)) { state.lastBuild = { ok: false, revision: state.revision, diagnosticsKnown: false }; state.diagnosticProgress = {}; }
            result = String(await invoke(call.function.name, input));
            if(MUTATIONS.has(call.function.name)) visits.clear();
            if (BUILDS.has(call.function.name)) {
              const ok = /BUILD SUCCESSFUL/.test(result) && !/BUILD FAILED/.test(result);
              let items;
              try { items = await diagnostics(); } catch (_) {}
              state.lastBuild = { ok, revision: state.revision, diagnosticsKnown: Array.isArray(items),
                warnings: items?.filter(d => d.category === 0).length,
                errors: items?.filter(d => d.category === 1).length };
              result += '\n当前构建诊断汇总：' + JSON.stringify(state.lastBuild);
            }
            if(call.function.name==='get_runtime_diagnostics') {
              const runtime=JSON.parse(result);
              state.runtimeEvidence={revision:state.revision,bundleName:runtime.bundleName,buildId:runtime.lastArkForgeBuild,runningPid:runtime.runningPid,deviceApi:runtime.deviceApi,
                compiler:runtime.compiler,installedCompilerMatches:runtime.installedCompilerMatches,faultCollectionOk:runtime.faultCollectionOk,
                faults:(runtime.faults||[]).map(fault=>({file:fault.file,reason:fault.reason,predatesBuild:fault.predatesBuild,report:typeof fault.report==='string'?fault.report.slice(0,800):undefined,frames:fault.frames}))};
            }
            if(call.function.name==='test_app_flow') {
              const flow=JSON.parse(result);
              state.lastFlow={revision:state.revision,status:flow.status,buildId:flow.buildId,expected_id:input.expected_id,actions:input.actions,steps:flow.steps,newFaultFiles:flow.newFaultFiles,runningPid:flow.runningPid,errorCode:flow.errorCode,error:flow.error};
              if(flow.status==='blocked')interruption=flow.error||'系统条件恢复后继续界面验证。';
            }
            if (call.function.name === 'get_diagnostics') {
              const page = JSON.parse(result), key = (input.category || 'all') + ':' + (input.path || '*');
              state.diagnosticProgress ||= {};
              const progress = state.diagnosticProgress[key] || { matched: page.matched, ranges: [], files: [] };
              progress.matched = page.matched;
              const items=page.diagnostics || [];
              if(page.mode==='summary')progress.ranges=[[0,page.matched]];
              else progress.ranges.push([page.offset, page.offset + items.length]);
              progress.ranges.sort((a,b)=>a[0]-b[0]);
              const merged=[];
              for(const range of progress.ranges) {
                const last=merged[merged.length-1];
                if(last&&range[0]<=last[1])last[1]=Math.max(last[1],range[1]);else merged.push(range.slice());
              }
              progress.ranges=merged;
              progress.nextOffset=merged[0]?.[0]===0?merged[0][1]:0;
              progress.complete=progress.nextOffset>=page.matched;
              progress.files=[...new Set([...progress.files,...(page.mode==='summary'?page.groups.map(item=>item.path):items.map(item=>item.fileName))])].slice(0,150);
              state.diagnosticProgress[key]=progress;
              result+='\n诊断阅读进度：'+JSON.stringify({matched:progress.matched,nextOffset:progress.nextOffset,complete:progress.complete});
              if(progress.complete)result+='\n此筛选下的全部诊断已读取。请选定文件读取源码、修复后重新构建；构建后诊断页会更新。';
            }
          } catch (error) { result = '工具失败：' + error.message; }
        }
        const summary = toolSummary(call, result);
        outcomes.push(summary);
        const signature = call.function.name + JSON.stringify(Object.fromEntries(Object.entries(input).sort(([a],[b])=>a.localeCompare(b)))) + summary;
        const count = (visits.get(signature) || 0) + 1; visits.set(signature, count);
        if (count === 4) guidance.push({role:'user',content:'此工具及结果已经重复读取多次。请依据已保存的诊断阅读进度选择未处理的文件，执行实际修复或说明需要补充的信息。'});
        if (count >= 8) cycling = true;
        state.operations.push(summary);
        state.operations = state.operations.slice(-24);
        const resultLimit = Math.max(1000, Math.min(90000, Math.floor((limit - 3500) / calls.length)));
        const prefix = Math.floor(resultLimit / 4);
        const content = result.length > resultLimit ? result.slice(0, prefix) + '\n[中间输出已省略；请按较小行范围或诊断页码读取后续内容]\n' + result.slice(-(resultLimit - prefix)) : result;
        messages.push({ role: 'tool', tool_call_id: call.id, content });
      }
      messages.push(...guidance);
      // Persist only complete tool groups so a resumed request has a valid protocol.
      await save(token.isCancellationRequested ? 'cancelled' : 'running');
      if(interruption) {
        stream.markdown('\n'+interruption+' 已保留当前构建与完整点击路径。');
        stream.button?.({command:'arkforge.resumeAgent',title:'从已保存路径继续验证'});
        return await save('paused');
      }
      if (cycling) {
        stream.markdown('\n已保存修改和诊断进度。当前工具结果多次重复，请补充处理方向后继续。');
        stream.button?.({command:'arkforge.resumeAgent',title:'继续当前任务'});
        return await save('paused');
      }
      const current = outcomes.join('\n');
      repeated = current === fingerprint ? repeated + 1 : 0; fingerprint = current;
      if (repeated === 4) messages.push({ role: 'user', content: '最近工具步骤和结果重复，请更换排查方法，检查文件版本或具体诊断后继续推进。' });
      if (repeated >= 8) {
        stream.markdown('\n当前步骤连续返回相同结果，修改和诊断进度已保存。可以补充处理方向后继续。');
        stream.button?.({ command: 'arkforge.resumeAgent', title: '继续当前任务' });
        return await save('paused');
      }
    }
    return await save('cancelled');
  } catch (error) {
    const result = await save(token.isCancellationRequested ? 'cancelled' : 'error');
    if (!token.isCancellationRequested) {
      result.errorDetails = { message: error.message };
      stream.button?.({ command: 'arkforge.resumeAgent', title: '从当前进度继续' });
    }
    return result;
  }
}

function cancellableDelay(ms, token) {
  return new Promise((resolve, reject) => {
    if (token.isCancellationRequested) return reject(new Error('任务已停止'));
    let subscription;
    const timer = setTimeout(() => { subscription?.dispose(); resolve(); }, ms);
    subscription = token.onCancellationRequested(() => { clearTimeout(timer); subscription?.dispose(); reject(new Error('任务已停止')); });
  });
}

module.exports = { runAgentTask, compactHistory, needsVerification, cancellableDelay };
