import { common } from '@kit.AbilityKit';
import { fileIo, hash } from '@kit.CoreFileKit';
import { RuntimeUIScript, UIAction } from './RuntimeUIScript';
import { AtomicFiles } from './AtomicFiles';
import { RuntimeDiagnostics, RuntimeReport } from './RuntimeDiagnostics';

interface UITree { attributes: Record<string,string>; children?: UITree[]; }
interface UIRequest { actions?: UIAction[]; expected_id?: string; }
interface UISource { path: string; sha256: string; }
interface UITarget { bundleName: string; abilityName: string; project: string; sources?: UISource[]; }
interface UIObservation { id: string; text: string; type: string; bounds: string; }
interface UIStep { id: string; text: string; status: string; }
interface UIResult {
  status: string; recordedAt: number; backgroundBudgetMs: number; bundleName: string; buildId: string; steps: UIStep[]; expectedElementFound: boolean;
  newFaultFiles: string[]; runningPid: string; controls: UIObservation[]; error: string; runtime: RuntimeReport;
  executionMode: string; restored: boolean; durationMs: number;
}

/** Run a bounded sequence of official uitest actions against the installed project. */
export class RuntimeUIFlow {
  private static persist(context: common.UIAbilityContext,result: UIResult): string {
    const serialized = JSON.stringify(result);
    const directory = context.tempDir + '/runtime-ui';
    if (!fileIo.accessSync(directory)) fileIo.mkdirSync(directory,true);
    AtomicFiles.write(directory + '/latest-result.json',serialized);
    return serialized;
  }
  private static nodes(tree: UITree, output: UIObservation[], depth: number = 0): void {
    if (depth > 256 || output.length >= 20000) throw new Error('请缩小本次界面验证的目标范围');
    const a = tree.attributes;
    if (a && a.visible !== 'false' && a.enabled !== 'false' && (a.id || a.text)) {
      output.push({id:a.id || '',text:a.text || '',type:a.type || '',bounds:a.bounds || ''});
    }
    for (const child of tree.children || []) RuntimeUIFlow.nodes(child,output,depth + 1);
  }
  static async run(context: common.UIAbilityContext, project: string, payload: string,
    run: (command: string) => Promise<string>): Promise<string> {
    const request = JSON.parse(payload) as UIRequest;
    const actions = request.actions || [];
    if (actions.length > 12 || actions.some((item: UIAction) => (!item.id && !item.text) ||
      (item.id || '').length > 120 || (item.text || '').length > 120) || (request.expected_id || '').length > 120) throw new Error('每次可按控件 ID 或完整文字验证最多 12 步');
    const target = JSON.parse(fileIo.readTextSync(context.filesDir + '/offline-build/latest.json')) as UITarget;
    if (target.project !== project || !/^[A-Za-z0-9_.]+$/.test(target.bundleName) ||
      !/^[A-Za-z0-9_.]+$/.test(target.abilityName) || target.bundleName === context.applicationInfo.name) throw new Error('请先安装当前工程，再验证该工程的独立应用');
    if (!target.sources?.length) throw new Error('请先构建安装当前工程，生成源码版本记录后再验证界面');
    for (const source of target.sources) {
      if (source.path.startsWith('/') || source.path.split('/').includes('..')) throw new Error('请重新构建安装当前工程，更新源码版本记录');
      const physical = project + '/' + source.path;
      if (!fileIo.accessSync(physical) || (await hash.hash(physical,'sha256')).toUpperCase() !== source.sha256.toUpperCase())
        throw new Error('请先构建安装当前修改，再验证界面。已安装版本与源码不同：' + source.path);
    }
    const before = await RuntimeDiagnostics.collect(context,run,project);
    const baseline = before.faults.map(fault => fault.file);
    const steps: UIStep[] = [];
    let controls: UIObservation[] = [];
    let error = '';
    let restored = false;
    let found = false;
    let after: RuntimeReport = before;
    const started = Date.now();
    const requestId = String(started);
    const prefix = '/data/local/tmp/arkforge-ui-' + context.applicationInfo.name + '-' + requestId;
    const directory = context.tempDir + '/runtime-ui';
    if (!fileIo.accessSync(directory)) fileIo.mkdirSync(directory,true);
    const local = directory + '/' + requestId;
    AtomicFiles.write(local + '.sh',RuntimeUIScript.render(target.bundleName,target.abilityName,
      context.applicationInfo.name,actions,request.expected_id || '',requestId,prefix));
    try {
      const sent = await run('hdc -t 127.0.0.1:8710 file send "' + local + '.sh" ' + prefix + '.sh');
      if (!sent.includes('FileTransfer finish')) throw new Error('测试流程传输结果：' + sent.slice(-500));
      // The authorized system shell continues independently while ArkForge is
      // suspended. Its EXIT trap restores the editor before the HDC RPC returns.
      await run('hdc -t 127.0.0.1:8710 shell sh ' + prefix + '.sh');
      const received = await run('hdc -t 127.0.0.1:8710 file recv ' + prefix + '.result "' + local + '.result"');
      if (!received.includes('FileTransfer finish') || !fileIo.accessSync(local + '.result')) throw new Error('测试记录传输结果：' + received.slice(-500));
      const records = fileIo.readTextSync(local + '.result').trim().split('\n');
      if (records[0] !== 'requestId:' + requestId || !records.includes('complete:1')) throw new Error('请重新运行当前测试，生成完整的本次记录');
      restored = records.includes('restored:1');
      found = records.includes('expected:1');
      for (let i = 0; i < actions.length; i++) {
        const clicked = records.includes('step:' + i + ':clicked');
        const absent = records.includes('step:' + i + ':not-present');
        if (clicked || absent) steps.push({id:actions[i].id || '',text:actions[i].text || '',status:clicked?'clicked':'not-present'});
      }
      const failure = records.find((record: string) => record.startsWith('error:'));
      if (failure) error = '界面测试执行记录：' + failure.slice(6);
      if (!restored) error = '请返回 ArkForge 查看本次测试记录';
      if (steps.length !== actions.length && !error) error = '请根据当前界面补齐测试步骤';
      const layout = await run('hdc -t 127.0.0.1:8710 file recv ' + prefix + '.layout.json "' + local + '.json"');
      if (!layout.includes('FileTransfer finish') || !fileIo.accessSync(local + '.json')) throw new Error('界面传输结果：' + layout.slice(-500));
      RuntimeUIFlow.nodes(JSON.parse(fileIo.readTextSync(local + '.json')) as UITree,controls);
      found = found && (!request.expected_id || controls.some((node: UIObservation) => node.id === request.expected_id));
    } catch (caught) { error = RuntimeDiagnostics.redact((caught as Error).message); }
    finally {
      if (!restored) {
        try {
          const returned = await run('hdc -t 127.0.0.1:8710 shell aa start -b ' + context.applicationInfo.name + ' -a EntryAbility');
          restored = returned.includes('start ability successfully');
        } catch (caught) { error += '；返回编辑器：' + RuntimeDiagnostics.redact((caught as Error).message); }
      }
      try { await run('hdc -t 127.0.0.1:8710 shell rm -f ' + prefix + '.sh ' + prefix + '.result ' + prefix + '.layout.json ' + prefix + '.log'); }
      finally { after = await RuntimeDiagnostics.collect(context,run,project); }
      for (const suffix of ['.sh','.result','.json']) {
        if (fileIo.accessSync(local + suffix)) fileIo.unlinkSync(local + suffix);
      }
    }
    const newFaults = after.faults.filter(fault => !baseline.includes(fault.file)).map(fault => fault.file);
    const passed = !error && restored && !!after.runningPid && !newFaults.length && found && after.faultCollectionOk;
    const verified = steps.some((step: UIStep) => step.status === 'clicked') && !!request.expected_id;
    controls = controls.filter((node: UIObservation) => node.text.length <= 2048);
    controls.sort((a: UIObservation,b: UIObservation) => Number(!!b.id)-Number(!!a.id));
    const result: UIResult = {status:passed?(verified?'passed':'observed'):'failed',recordedAt:Date.now(),backgroundBudgetMs:0,
      executionMode:'on-device-system-shell',restored:restored,durationMs:Date.now()-started,bundleName:target.bundleName,buildId:after.lastArkForgeBuild,
      steps:steps,expectedElementFound:found,newFaultFiles:newFaults,runningPid:after.runningPid,
      controls:controls.slice(0,160),error:error,runtime:after};
    return RuntimeUIFlow.persist(context,result);
  }
}
