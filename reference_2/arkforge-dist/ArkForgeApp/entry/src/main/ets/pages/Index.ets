import { WorkbenchRuntime } from '../services/WorkbenchRuntime';
import { CodeOSSWorkbench } from '../components/workspace/CodeOSSWorkbench';
import { WorkbenchFiles } from '../services/WorkbenchFiles';
import officialCli from 'libarkforge_cli.so';
import { GitHubAccount } from '../services/GitHubAccount';
import { HuaweiSigning } from '../services/HuaweiSigning';
import { AgentPanel } from '../components/workspace/AgentPanel';
import { MonacoEditor } from '../components/workspace/MonacoEditor';
import { SDKWebRuntime } from '../services/SDKWebRuntime';
import { AtomicFiles } from '../services/AtomicFiles';
import { WorkspaceRecovery } from '../services/WorkspaceRecovery';
import { ResourceCompiler } from '../services/ResourceCompiler';
import { DeviceDebugger, DebugState, DebugFrame } from '../services/DeviceDebugger';
import { DeviceDebugView } from '../components/workspace/DeviceDebugView';
import { SigningMaterials, SigningCredentials } from '../services/SigningMaterials';
/**
 * Index.ets —— ArkForge IDE 主界面
 *
 * 前端工作区容器复用 multism_lab（MIT），中间区域使用 xxCode 语法高亮。
 * 复用开源组件：SyntaxHighlightService / ThemeService / FileTypeBadge / InputDialog
 *              （来源 KeloYuan/xxCode，MIT License）
 * 自绘资源：AppIcon（矢量线性图标，不使用 emoji）
 *
 * 组件：GlobalMenuBar / ProjectTree / CodeEditor / BottomDock / StatusBar
 * 支持真实 HarmonyOS 示例工程的离线源码增量构建、签名和本机安装。
 */
import { Jsvm, SAMPLE_CODE, EvalResult } from '../bridge/Jsvm';
import { UiNode } from '../model/UiNode';
import { DynamicRenderer } from '../view/DynamicRenderer';
import { SyntaxHighlightService, Token } from '../services/SyntaxHighlightService';
import { ThemeService, Theme } from '../services/ThemeService';
import { AppIcon } from '../components/AppIcon';
import { InputDialog } from '../components/AppInputDialog';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { util } from '@kit.ArkTS';
import { fileIo } from '@kit.CoreFileKit';
import { common } from '@kit.AbilityKit';
import { promptAction, window, display } from '@kit.ArkUI';
import { picker } from '@kit.CoreFileKit';
import { zlib } from '@kit.BasicServicesKit';
import { WorkspaceShell } from '../components/workspace/WorkspaceShell';
import { GlobalMenuBar } from '../components/workspace/GlobalMenuBar';
import { BottomDock } from '../components/workspace/BottomDock';
import { ResizablePane, ResizeAxis } from '../components/workspace/ResizablePane';
import { ProjectArchive } from '../services/ProjectArchive';
import { ProjectTree } from '../components/workspace/ProjectTree';
import { NativeToolchain } from '../services/NativeToolchain';
import { LocalDevice } from '../services/LocalDevice';
import { StaticArkCompiler } from '../services/StaticArkCompiler';
import { OfflineProjectBuilder } from '../services/OfflineProjectBuilder';
import { SDKProjectChecker, SDKDiagnostic } from '../services/SDKProjectChecker';
import { InstallJournal } from '../services/InstallJournal';
import { BuildConsole, BuildLogEntry, BuildProgress } from '../services/BuildConsole';
import { BuildConsoleView } from '../components/workspace/BuildConsoleView';

const FS: number = 14;            // 编辑器字号
const LH: number = 22;            // 编辑器行高
const MONO: string = 'monospace'; // 等宽字体
const PAD_T: number = 12;         // 代码区上内边距
const PAD_L: number = 14;         // 代码区左内边距
const TAG: string = 'ArkForge';

interface WorkspaceManifest {
  active?: string;
  files?: string[];
  root?: string;
}
interface AgentArgs { path?: string; content?: string; action?: string; query?: string; }
interface WorkbenchRequest { workspaceRoot?: string; }
interface GitRequest { path: string; args: string[]; }
interface TerminalRequest { id: number; data: string; cols: number; rows: number; }
interface DeveloperRequest { page: string; name: string; email: string; }
interface CloudSigningRequest { teamId: string; udid: string; }
interface SDKReport { project: string; diagnostics: SDKDiagnostic[]; }
interface BuiltApplication { bundleName: string; moduleName: string; abilityName: string; }
interface BuildLogManifest { output: string; }
interface EditorWindow { start: number; version: number; text: string; }



// ===================== 通用按钮（图标 [+ 文字]）=====================
@Component
struct CodeEditor {
  @Prop @Watch('onExternalCode') code: string = '';
  @Prop themeId: string = 'opencode';
  @Prop activeName: string = '';
  @Prop @Watch('revealRequested') revealToken: string = '';
  @State localText: string = '';
  @State pageStart: number = 0;
  @State totalLines: number = 0;
  @State documentWidth: number = 320;
  @State windows: EditorWindow[] = [];
  private generation: number = 0;
  private fullText: string = '';
  private allLines: string[] = [];
  private readonly pageSize: number = 160;
  @State lines: string[] = [];
  @State tokens: Token[][] = [];
  @State currentLine: number = 1;
  private syntax: SyntaxHighlightService = SyntaxHighlightService.getInstance();
  private vScroll: Scroller = new Scroller();
  private hScroll: Scroller = new Scroller();
  onContentChange: (v: string) => void = () => {};
  onLineSelected: (line: number) => void = () => {};

  aboutToAppear(): void { this.loadSource(); }

  onExternalCode(): void {
    if (this.code !== this.fullText) this.loadSource();
  }

  private loadSource(): void {
    this.fullText = this.code;
    this.allLines = this.fullText.split('\n');
    this.totalLines = this.allLines.length;
    this.documentWidth = Math.max(320, this.allLines.reduce((max: number, line: string) => Math.max(max, line.length), 0) * 9 + PAD_L + 40);
    this.pageStart = 0;
    this.loadPage();
    this.revealRequested();
  }

  private revealRequested(): void {
    if (this.totalLines === 0) return;
    const line = Math.max(0, Math.min(this.totalLines - 1, (parseInt(this.revealToken, 10) || 1) - 1));
    this.currentLine = line + 1;
    const start = Math.min(Math.max(0, this.totalLines - this.pageSize), Math.floor(Math.max(0, line - 40) / 40) * 40);
    if (start !== this.pageStart) { this.pageStart = start; this.loadPage(); }
    const version = this.generation;
    setTimeout(() => { if (version === this.generation) this.vScroll.scrollTo({ xOffset: 0, yOffset: line * LH }); }, 0);
  }

  private loadPage(): void {
    this.localText = this.allLines.slice(this.pageStart, this.pageStart + this.pageSize).join('\n');
    this.reparse();
    this.windows = [{ start: this.pageStart, version: ++this.generation, text: this.localText }];
  }

  private followViewport(): void {
    const visible = Math.floor(Math.max(0, this.vScroll.currentOffset().yOffset) / LH);
    const start = Math.min(Math.max(0, this.totalLines - this.pageSize), Math.floor(Math.max(0, visible - 40) / 40) * 40);
    if (start !== this.pageStart) { this.pageStart = start; this.loadPage(); }
  }

  private changed(value: string, start: number, version: number): void {
    if (start !== this.pageStart || version !== this.generation) return;
    if (value === this.localText) return;
    const count = this.localText.split('\n').length;
    this.localText = value;
    this.allLines.splice(this.pageStart, count, ...value.split('\n'));
    this.totalLines = this.allLines.length;
    this.fullText = this.allLines.join('\n');
    this.reparse();
    this.windows[0].text = value;
    this.onContentChange(this.fullText);
  }

  reparse(): void {
    this.lines = this.localText.split('\n');
    this.documentWidth = Math.max(this.documentWidth, this.maxLineLen() * 9 + PAD_L + 40);
    this.tokens = this.lines.map((line: string) => this.syntax.tokenize(line, this.language()));
  }

  private language(): string {
    return this.syntax.getLanguageFromExtension(this.extOf(this.activeName));
  }

  private extOf(name: string): string {
    const i = name.lastIndexOf('.');
    return i >= 0 ? name.substring(i + 1) : 'js';
  }

  private maxLineLen(): number {
    let m: number = 0;
    for (const line of this.lines) {
      if (line.length > m) { m = line.length; }
    }
    return m;
  }

  private contentWidth(): number {
    return this.documentWidth;
  }

  private contentHeight(): number {
    return this.lines.length * LH + PAD_T + 60;
  }

  build() {
    Scroll(this.vScroll) {
      Column() {
      Column() {}.height(this.pageStart * LH).width('100%').flexShrink(0)
      Row() {
        this.lineNumbers()
        Scroll(this.hScroll) {
          Stack({ alignContent: Alignment.TopStart }) {
            this.codeLines()
            ForEach(this.windows, (visible: EditorWindow) => {
            TextArea({ text: visible.text, placeholder: '// 在此编辑项目源码…' })
              .maxLength(8 * 1024 * 1024)
              .fontSize(FS).fontFamily(MONO).lineHeight(LH)
              .fontColor(Color.Transparent)
              .caretColor(this.t().editor.cursor)
              .placeholderColor(this.t().colors.textSecondary)
              .backgroundColor(Color.Transparent)
              .border({ width: 0 })
              .padding({ left: PAD_L, top: PAD_T, right: 24, bottom: 40 })
              .width(this.contentWidth())
              .height(this.contentHeight())
              .onTextSelectionChange((start: number) => { this.currentLine = visible.start + this.localText.slice(0, start).split('\n').length; this.onLineSelected(this.currentLine); })
              .onChange((value: string) => {
                if (value === this.localText) return;
                this.changed(value, visible.start, visible.version);
              })
            }, (visible: EditorWindow) => String(visible.version))
          }
          .width(this.contentWidth())
          .height(this.contentHeight())
        }
        .scrollable(ScrollDirection.Horizontal)
        .align(Alignment.TopStart)
        .scrollBar(BarState.Auto)
        .height(this.contentHeight())
        .layoutWeight(1)
      }.alignItems(VerticalAlign.Top).height(this.contentHeight()).flexShrink(0)
      Column() {}.height(Math.max(0, this.totalLines - this.pageStart - this.lines.length) * LH).width('100%').flexShrink(0)
      }.width('100%').height(this.totalLines * LH + PAD_T + 60)
    }
    .scrollable(ScrollDirection.Vertical)
    .align(Alignment.TopStart)
    .scrollBar(BarState.Auto)
    .scrollBarColor(this.t().colors.border)
    .scrollBarWidth(6)
    .onDidScroll(() => { this.followViewport(); })
    .width('100%').height('100%')
    .backgroundColor(this.t().editor.background)
  }

  @Builder
  lineNumbers() {
    Column() {
      ForEach(this.lines, (_: string, index: number) => {
        Text((this.pageStart + index + 1).toString())
          .fontSize(FS).fontFamily(MONO)
          .fontColor(this.pageStart + index + 1 === this.currentLine ? this.t().colors.primary : this.t().editor.lineNumber)
          .textAlign(TextAlign.End)
          .width(44).height(LH)
          .onClick(() => { this.currentLine = this.pageStart + index + 1; this.onLineSelected(this.currentLine); })
      }, (_: string, index: number) => 'ln' + (this.pageStart + index))
    }
    .width(56).padding({ top: PAD_T, right: 10 })
    .alignItems(HorizontalAlign.End)
    .height(this.contentHeight())
    .border({ width: { right: 1 }, color: this.t().colors.border })
  }

  @Builder
  codeLines() {
    Column() {
      ForEach(this.lines, (line: string, lineIndex: number) => {
        Row() {
          Text() {
            if (line.length === 0) {
              Span(' ')
            } else {
              ForEach(this.tokens[lineIndex] || [], (token: Token) => {
                Span(token.value).fontColor(this.tokenColor(token.type))
              }, (token: Token) => 't' + token.start)
            }
          }
          .fontSize(FS).fontFamily(MONO).lineHeight(LH)
          .maxLines(1)
        }
        .width('100%').height(LH)
        .backgroundColor(this.pageStart + lineIndex + 1 === this.currentLine ? this.t().colors.surface : Color.Transparent)
        .onClick(() => { this.currentLine = this.pageStart + lineIndex + 1; })
      }, (line: string, index: number) => 'cl' + index + ':' + line)
    }
    .padding({ left: PAD_L, right: 24, top: PAD_T, bottom: 40 })
    .alignItems(HorizontalAlign.Start)
  }

  private tokenColor(type: string): string {
    const s = this.t().syntax;
    const map: Record<string, string> = {
      'keyword': s.keyword,
      'string': s.string,
      'comment': s.comment,
      'number': s.number,
      'function': s.function,
      'variable': s.variable,
      'operator': s.operator,
      'type': s.type
    };
    return map[type] || this.t().editor.text;
  }

  private t(): Theme {
    return ThemeService.getInstance().getCurrentTheme();
  }
}

// ===================== 输出面板 =====================
@Component
struct OutputPanel {
  @Prop themeId: string = 'opencode';
  @Prop uiTree: UiNode | null = null;
  @Prop logs: string = '';
  @Prop result: string = '';

  build() {
    Tabs({ barPosition: BarPosition.Start }) {
      TabContent() {
        Column() {
          if (this.uiTree === null) {
            Text('点击「运行」查看预览')
              .fontSize(13).fontColor(this.t().colors.textSecondary).margin(16)
          } else {
            DynamicRenderer({ node: this.uiTree })
          }
        }
        .width('100%').height('100%')
        .align(Alignment.Top)
        .justifyContent(FlexAlign.Start)
      }.tabBar('预览')

      TabContent() {
        Scroll() {
          Text(this.logs.length > 0 ? this.logs : '（暂无输出）')
            .fontSize(12).fontFamily(MONO)
            .fontColor(this.logs.length > 0 ? this.t().editor.text : this.t().colors.textSecondary)
            .width('100%')
        }
        .width('100%').height('100%')
        .backgroundColor(this.t().colors.background).padding(12)
      }.tabBar('控制台')

      TabContent() {
        Scroll() {
          Text(this.result.length > 0 ? this.result : '（暂无结果）')
            .fontSize(13).fontFamily(MONO)
            .fontColor(this.result.indexOf('✗') === 0 ? this.t().colors.error : this.t().colors.success)
            .width('100%')
        }
        .width('100%').height('100%')
        .backgroundColor(this.t().colors.background).padding(12)
      }.tabBar('结果')
    }
    .width('100%').height('100%')
    .backgroundColor(this.t().colors.surface)
  }

  private t(): Theme {
    return ThemeService.getInstance().getCurrentTheme();
  }
}

// ===================== 底部状态栏 =====================
@Component
struct StatusBar {
  @Prop themeId: string = 'opencode';
  @Prop activeName: string = '';
  @Prop lineText: string = '';
  @Prop costText: string = '';

  build() {
    Row({ space: 16 }) {
      Text(this.activeName.length > 0 ? this.activeName : 'untitled.js')
        .fontSize(11).fontColor(this.t().colors.textSecondary)
      Text(this.lineText)
        .fontSize(11).fontColor(this.t().colors.textSecondary)
      Blank()
      Text(this.costText)
        .fontSize(11).fontColor(this.t().colors.textSecondary)
      Text(this.t().name)
        .fontSize(11).fontColor(this.t().colors.primary)
    }
    .width('100%').height(32)
    .padding({ left: 16, right: 16 })
    .backgroundColor(this.t().colors.surface)
    .border({ width: { top: 1 }, color: this.t().colors.border })
  }

  private t(): Theme {
    return ThemeService.getInstance().getCurrentTheme();
  }
}

// ===================== 手机竖屏布局 =====================
@Entry
@Component
struct Index {
  @State private debugState: DebugState = {connected:false,paused:false,status:'尚未连接调试器',frames:[],variables:[]};
  private selectedLine: number = 1;
  private deviceDebugger: DeviceDebugger = new DeviceDebugger((state: DebugState) => {
    this.debugState = state;
    if (state.paused && state.frames[0]?.source) this.showDebugFrame(state.frames[0]);
  });
  @State private revealToken: string = '';
  @State private workbenchBuildEvent: string = '';
  @State private agentOpen: boolean = false;
  @State private showGuide: boolean = false;
  @State private code: string = '';
  @State private activeName: string = '';
  @State private fileList: string[] = [];
  @State private logs: string = '';
  @State private result: string = '';
  @State private uiTree: UiNode | null = null;
  @State private running: boolean = false;
  @State private lastCost: number = 0;
  @State private dirty: boolean = false;
  @State private showSidebar: boolean = true;
  @State private bp: string = 'lg';   // sm(<520vp) | md(520-839vp) | lg(>=840vp)
  @State private themeId: string = 'harmony-dark';
  @State private confirmName: string = '';   // 待确认删除的文件名
  @State private dockTab: number = 0;
  @State private dockHeight: number = 200;
  @State private sidebarWidth: number = 228;
  @State private showDock: boolean = true;
  @State private importing: boolean = false;
  @State private buildReport: string = '使用流程：文件 → 打开项目 ZIP → 编辑并保存 → 构建并安装 → 运行。帮助菜单提供完整教学。';
  @State private buildEntries: BuildLogEntry[] = [];
  @State private consoleVisible: boolean = false;
  private nativeTaskBusy: boolean = false;
  private resizeDockStart: number = 200;
  private resizeSidebarStart: number = 228;

  private winW: number = 0;           // 当前窗口宽（px）
  private workspaceDir: string = '';
  private manifestPath: string = '';
  private newFileParent: string = '';
  private renameTarget: string = '';

  private renameDialog: CustomDialogController = new CustomDialogController({
    builder: InputDialog({ title: '重命名文件', placeholder: '输入新名称，例如 Detail.ets',
      confirmText: '重命名', cancelText: '取消',
      onConfirm: (value: string) => { this.renameFile(value); }
    }), alignment: DialogAlignment.Center, autoCancel: true
  });

  private newFileDialog: CustomDialogController = new CustomDialogController({
    builder: InputDialog({
      title: '新建工程文件',
      placeholder: '例如 entry/src/main/ets/pages/Detail.ets',
      confirmText: '创建',
      cancelText: '取消',
      onConfirm: (value: string) => { this.createFile(value); }
    }),
    alignment: DialogAlignment.Center,
    autoCancel: true
  });

  /** 用窗口宽（px）÷ 屏幕密度得到 vp，再映射断点（mediaquery 在部分设备上不可靠） */
  private updateBreakpoint(): void {
    let wVp: number = 0;
    try {
      const d = display.getDefaultDisplaySync();
      const den = d.densityPixels > 0 ? d.densityPixels : 1;
      wVp = (this.winW > 0 ? this.winW : d.width) / den;
    } catch (e) {
      wVp = 0;
    }
    this.bp = wVp >= 840 ? 'lg' : (wVp >= 520 ? 'md' : 'sm');
  }

  aboutToAppear(): void {
    this.updateBreakpoint();
    this.showSidebar = (this.bp === 'lg');
    const ctx = getContext(this);
    this.workspaceDir = ctx.filesDir + '/projects';
    this.manifestPath = ctx.filesDir + '/.workspace.json';
    this.init().then(async () => {
      if (this.fileList.length === 0) this.showGuide = true;
      this.buildEntries = BuildConsole.restore(ctx as common.UIAbilityContext);
      this.consoleVisible = this.buildEntries.length > 0;
      const report = await InstallJournal.recover(ctx as common.UIAbilityContext);
      if (report) {
        this.buildReport = report;
        this.buildEntries = BuildConsole.recordRecovery(ctx as common.UIAbilityContext, report,
          report.indexOf('已确认') >= 0);
        this.consoleVisible = this.buildEntries.length > 0;
        this.showDock = true;
        this.dockTab = 0;
      }
    }).catch((error: Error) => {
      this.buildReport = '工作区加载失败：' + error.message;
      this.showDock = true;
      this.dockTab = 0;
    });
  }

  onPageShow(): void {
    window.getLastWindow(getContext(this)).then((win: window.Window) => {
      this.winW = win.getWindowProperties().windowRect.width;
      this.updateBreakpoint();
      win.on('windowSizeChange', (size: window.Size) => {
        this.winW = size.width;
        this.updateBreakpoint();
      });
    }).catch(() => {
      this.updateBreakpoint();
    });
  }

  private async init(): Promise<void> {
    // A fresh installation has no manifest or project directory yet. Git and
    // the workbench filesystem must still have a real root before ZIP import.
    if (!fileIo.accessSync(this.workspaceDir)) fileIo.mkdirSync(this.workspaceDir, true);
    try {
      const f = fileIo.openSync(this.manifestPath, fileIo.OpenMode.READ_ONLY);
      fileIo.closeSync(f.fd);
      this.loadManifest();
    } catch (e) {
      this.showGuide = true;
      return;
    }
    if (this.activeName && this.fileList.indexOf(this.activeName) >= 0) {
      this.openFile(this.activeName, false);
    } else if (this.fileList.length > 0) {
      this.openFile(this.fileList[0], false);
    } else {
      this.code = '';
      this.activeName = '';
    }
    const draft = WorkspaceRecovery.read(this.workspaceDir);
    if (draft && this.fileList.includes(draft.path)) {
      this.openFile(draft.path, false);
      this.code = draft.source;
      this.dirty = true;
      promptAction.showToast({ message: '已恢复上次编辑内容，请检查后保存' });
    }
  }

  private draftTimer: number = -1;
  private savedStamp: string = '';
  private stamp(name: string): string {
    if (!fileIo.accessSync(this.workspaceDir + '/' + name)) return '';
    const stat = fileIo.statSync(this.workspaceDir + '/' + name);
    return String(stat.mtime) + ':' + stat.size;
  }
  private persistDraft(): void {
    if (this.draftTimer >= 0) clearTimeout(this.draftTimer);
    if (this.dirty && this.activeName) WorkspaceRecovery.draft(this.workspaceDir, this.activeName, this.code);
  }
  onPageHide(): void { this.persistDraft(); }
  aboutToDisappear(): void { this.persistDraft(); }

  // ---------------- 工作区持久化 ----------------
  private loadManifest(): void {
    const f = fileIo.openSync(this.manifestPath, fileIo.OpenMode.READ_ONLY);
    const stat = fileIo.statSync(this.manifestPath);
    const buf = new ArrayBuffer(stat.size);
    fileIo.readSync(f.fd, buf);
    fileIo.closeSync(f.fd);
    const txt = util.TextDecoder.create('utf-8').decodeToString(new Uint8Array(buf));
    const obj: WorkspaceManifest = JSON.parse(txt) as WorkspaceManifest;
    const importRoot = getContext(this).filesDir + '/imports/';
    if (obj.root && obj.root.startsWith(importRoot) && !obj.root.includes('/../')) {
      this.workspaceDir = obj.root;
    }
    this.fileList = (obj.files ?? []).filter((name: string) => this.validName(name));
    this.activeName = obj.active ?? (this.fileList[0] ?? '');
  }

  private saveManifest(): void {
    const obj: WorkspaceManifest = { active: this.activeName, files: this.fileList, root: this.workspaceDir };
    AtomicFiles.write(this.manifestPath, JSON.stringify(obj));
  }

  private async seedWorkspace(): Promise<void> {
    try { fileIo.mkdirSync(this.workspaceDir, true); } catch (e) { /* 已存在 */ }
    const rm = getContext(this).resourceManager;
    const samples = ['hello.js', 'counter.js', 'layout.js'];
    const names: string[] = [];
    for (const name of samples) {
      try {
        const data: Uint8Array = await rm.getRawFileContent('projects/' + name);
        const txt = util.TextDecoder.create('utf-8').decodeToString(data);
        this.writeFile(name, txt);
        names.push(name);
      } catch (e) {
        hilog.error(0x0000, TAG, 'seed %{public}s failed', name);
      }
    }
    this.writeFile('welcome.js', SAMPLE_CODE);
    names.unshift('welcome.js');
    this.fileList = names;
    this.activeName = 'welcome.js';
    this.saveManifest();
  }

  private writeFile(name: string, content: string): void {
    if (!this.validName(name)) throw new Error('无效的文件路径');
    const path = this.workspaceDir + '/' + name;
    const parent = path.substring(0, path.lastIndexOf('/'));
    if (!fileIo.accessSync(parent)) fileIo.mkdirSync(parent, true);
    if (name === this.activeName && this.savedStamp && this.stamp(name) !== this.savedStamp) {
      throw new Error('磁盘上的文件已改变，未覆盖：' + name + '。请先导出当前修改并重新打开文件。');
    }
    WorkspaceRecovery.checkpoint(this.workspaceDir, name);
    AtomicFiles.write(path, content);
    WorkspaceRecovery.clear(this.workspaceDir, name);
    if (name === this.activeName) this.savedStamp = this.stamp(name);
  }

  private readFile(name: string): string {
    const path = this.workspaceDir + '/' + name;
    const f = fileIo.openSync(path, fileIo.OpenMode.READ_ONLY);
    const stat = fileIo.statSync(path);
    const buf = new ArrayBuffer(stat.size);
    fileIo.readSync(f.fd, buf);
    fileIo.closeSync(f.fd);
    return util.TextDecoder.create('utf-8').decodeToString(new Uint8Array(buf));
  }

  // ---------------- 文件操作 ----------------
  private openFile(name: string, autosave: boolean = true): void {
    this.revealToken = '';
    if (autosave && this.dirty && this.activeName) {
      if (!this.saveActive()) return;
    }
    this.activeName = name;
    try {
      this.code = this.readFile(name);
    } catch (e) {
      this.code = '';
    }
    this.dirty = false;
    this.savedStamp = this.stamp(name);
    this.saveManifest();
  }

  private saveActive(): boolean {
    if (!this.activeName) return false;
    try {
      this.writeFile(this.activeName, this.code);
      this.dirty = false;
      promptAction.showToast({ message: '已保存 ' + this.activeName });
      return true;
    } catch (error) {
      this.persistDraft();
      this.buildReport = String(error);
      this.consoleVisible = false; this.showDock = true; this.dockTab = 0;
      return false;
    }
  }

  private createFile(rawName: string): void {
    let name = (rawName || '').trim();
    if (!name) { name = 'untitled'; }
    if (!/\.[a-zA-Z0-9]+$/.test(name)) { name = name + '.ets'; }
    if (this.newFileParent && !name.includes('/')) name = this.newFileParent + '/' + name;
    if (!this.validName(name)) {
      promptAction.showToast({ message: '请填写工程内的相对路径，目录层级使用正斜杠分隔' });
      return;
    }
    if (this.fileList.indexOf(name) >= 0) {
      this.openFile(name);
      promptAction.showToast({ message: '已打开 ' + name });
      return;
    }
    const header: string = '';
    this.writeFile(name, header);
    this.fileList = [...this.fileList, name];
    this.saveManifest();
    this.openFile(name);
  }

  private deleteFile(name: string): void {
    try { fileIo.unlinkSync(this.workspaceDir + '/' + name); } catch (e) { /* ignore */ }
    this.fileList = this.fileList.filter((n: string) => n !== name);
    this.saveManifest();
    if (this.activeName === name) {
      if (this.fileList.length > 0) {
        this.openFile(this.fileList[0]);
      } else {
        this.activeName = '';
        this.code = '';
      }
    }
  }

  private openNewFile(directory: string): void {
    this.newFileParent = directory;
    this.newFileDialog.open();
  }

  private renameFile(rawName: string): void {
    const old = this.renameTarget;
    const name = rawName.trim();
    if (!name || name.includes('/') || !this.validName(name) || !this.validName(old)) {
      promptAction.showToast({ message: '请输入新文件名，目录位置沿用当前文件' });
      return;
    }
    const parent = old.includes('/') ? old.substring(0, old.lastIndexOf('/') + 1) : '';
    const next = parent + name;
    if (next === old) return;
    if (this.fileList.includes(next)) {
      promptAction.showToast({ message: '同名文件已存在' });
      return;
    }
    try {
      if (this.activeName === old && this.dirty) this.writeFile(old, this.code);
      fileIo.renameSync(this.workspaceDir + '/' + old, this.workspaceDir + '/' + next);
      this.fileList = this.fileList.map((file: string) => file === old ? next : file);
      if (this.activeName === old) { this.activeName = next; this.dirty = false; }
      this.saveManifest();
    } catch (error) {
      promptAction.showToast({ message: '重命名失败：' + JSON.stringify(error) });
    }
  }

  // ---------------- 运行 ----------------
  private validName(name: string): boolean {
    return name.length > 0 && !name.startsWith('/') && !/[\\:\x00-\x1f]/.test(name) &&
      !name.split('/').some((part: string) => part === '..' || part === '.' || part.length === 0);
  }

  private async importProject(reportErrors: boolean = false): Promise<void> {
    if (this.importing) return;
    this.importing = true;
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const selected = await new picker.DocumentViewPicker(context).select({
        maxSelectNumber: 1, fileSuffixFilters: ['项目 ZIP|.zip']
      });
      if (selected.length === 0) return;
      if (this.dirty && !this.saveActive()) return;
      const project = await ProjectArchive.open(selected[0], context.filesDir);
      this.workspaceDir = project.root;
      this.fileList = project.files;
      const page = this.fileList.find((name: string) => name.endsWith('/pages/Index.ets'));
      this.openFile(page ?? this.fileList[0], false);
      this.showSidebar = true;
      this.showDock = true;
      this.dockTab = 0;
      this.buildReport = '项目已导入：' + this.fileList.length + ' 个源码/配置文件。\n' +
        '编辑内容保存在平板工作区；原 ZIP 保持不变。\n' + this.workspaceDir;
    } catch (error) {
      this.buildReport = '打开项目失败：' + String(error);
      this.showDock = true;
      this.dockTab = 0;
      if (reportErrors) throw new Error(this.buildReport);
    } finally {
      this.importing = false;
    }
  }

  private async exportProject(reportErrors: boolean = false): Promise<void> {
    if (this.importing) return;
    this.importing = true;
    try {
      this.saveActive();
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const archive = context.cacheDir + '/project-' + Date.now() + '.zip';
      await zlib.compressFile(this.workspaceDir, archive, {});
      const selected = await new picker.DocumentViewPicker(context).save({
        newFileNames: ['ArkForge-project.zip']
      });
      if (selected.length > 0) {
        const target = fileIo.openSync(selected[0], fileIo.OpenMode.WRITE_ONLY);
        try {
          fileIo.copyFileSync(archive, target.fd);
        } finally {
          fileIo.closeSync(target);
        }
        this.buildReport = '已导出项目 ZIP';
        promptAction.showToast({ message: this.buildReport });
      }
      if (selected.length === 0) this.buildReport = '已取消导出项目';
      fileIo.unlinkSync(archive);
    } catch (error) {
      this.buildReport = '导出失败：' + String(error);
      this.showDock = true;
      this.dockTab = 0;
      if (reportErrors) throw new Error(this.buildReport);
    } finally {
      this.importing = false;
    }
  }

  private async exportBuildLog(): Promise<void> {
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const latest = JSON.parse(fileIo.readTextSync(context.filesDir + '/build-logs/latest.json')) as BuildLogManifest;
      if (!latest.output.startsWith(context.filesDir + '/build-logs/') || latest.output.includes('..')) throw new Error('日志路径无效');
      const selected = await new picker.DocumentViewPicker(context).save({ newFileNames: ['ArkForge-build.log'] });
      if (selected.length === 0) return;
      const target = fileIo.openSync(selected[0], fileIo.OpenMode.WRITE_ONLY);
      try { fileIo.copyFileSync(latest.output, target.fd); } finally { fileIo.closeSync(target); }
      promptAction.showToast({ message: '已导出完整构建日志' });
    } catch (_) { promptAction.showToast({ message: '请先执行构建，再导出构建日志' }); }
  }

  private async openHarmonySample(): Promise<void> {
    if (this.importing) return;
    this.importing = true;
    try {
      if (this.dirty) this.saveActive();
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const data = await context.resourceManager.getRawFileContent('HarmonyProject.zip');
      const archive = context.cacheDir + '/HarmonyProject.zip';
      const file = fileIo.openSync(archive,
        fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
      try { fileIo.writeSync(file.fd, data.buffer); } finally { fileIo.closeSync(file); }
      const project = await ProjectArchive.open(archive, context.filesDir);
      this.workspaceDir = project.root;
      this.fileList = project.files;
      this.openFile('harmony_demo/src/main/ets/pages/Index.ets', false);
      this.showSidebar = true;
      this.showDock = true;
      this.dockTab = 0;
      this.buildReport = '已打开真实 HarmonyOS / Stage 工程。\n' +
        '包含 AppScope、build-profile.json5、entry 与 harmony_demo 两个 HAP 模块。\n' +
        '当前文件是原生 ArkUI 页面，不使用 JavaScript 预览引擎。\n' +
        '从“构建”菜单可在本机重新编译 ArkTS、打包、签名并安装 harmony_demo。\n' +
        '当前支持资源未改变的增量构建；修改资源或模块配置时会要求更新资源缓存。';
    } catch (error) {
      this.buildReport = '示例加载失败：' + JSON.stringify(error);
      this.showDock = true;
      this.dockTab = 0;
    } finally {
      this.importing = false;
    }
  }

  private async checkToolchain(): Promise<void> {
    await this.runNativeTask('CheckOfficialSDK', async (context: common.UIAbilityContext, report: BuildProgress) => {
      const sdk = await SDKProjectChecker.verify(context, report);
      const resource = await ResourceCompiler.verify(context, report);
      return sdk + '\n' + resource;
    });
  }

  private async verifyNativeSigner(): Promise<void> {
    await this.runNativeTask('VerifyHap', (context: common.UIAbilityContext, report: BuildProgress) =>
      NativeToolchain.verifySigner(context, report));
  }

  private async signNativeDemo(): Promise<void> {
    await this.runNativeTask('SignHap', (context: common.UIAbilityContext, report: BuildProgress) =>
      NativeToolchain.signDemo(context, report));
  }

  private async localDeviceAction(install: boolean): Promise<void> {
    await this.runNativeTask(install ? 'InstallHap' : 'ConnectDevice',
      (context: common.UIAbilityContext, report: BuildProgress) => install ?
        LocalDevice.installSignedDemo(context, report) : LocalDevice.connect(context, report));
  }

  private async verifyStaticCompiler(): Promise<void> {
    await this.runNativeTask('CompileStaticArkTS', (context: common.UIAbilityContext, report: BuildProgress) =>
      StaticArkCompiler.probe(context, report));
  }

  private async buildOfflineProject(): Promise<void> {
    if (this.dirty && !this.saveActive()) return;
    await this.runNativeTask('BuildHarmonyProjectOffline', (context: common.UIAbilityContext, report: BuildProgress) =>
      OfflineProjectBuilder.build(context, this.workspaceDir, report));
  }

  private async readRuntimeLog(): Promise<void> {
    await this.runNativeTask('ReadInstalledAppLog', (context: common.UIAbilityContext, report: BuildProgress) =>
      LocalDevice.readRuntimeLog(context, report));
  }
  private async importSigningPackage(): Promise<void> {
    if (this.nativeTaskBusy) return;
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const selected = await new picker.DocumentViewPicker(context).select({maxSelectNumber:1,fileSuffixFilters:['开发签名包|.zip']});
      if (!selected.length) return;
      await this.runNativeTask('ImportSigning', async (ctx: common.UIAbilityContext, report: BuildProgress) => {
        const plan = await ResourceCompiler.plan(ctx, this.workspaceDir, report);
        return await SigningMaterials.importPackage(ctx, selected[0], plan.bundleName);
      });
    } catch (error) { promptAction.showToast({message:'签名导入失败：' + String(error)}); }
  }
  private showDebugFrame(frame: DebugFrame): void {
    if (!frame.source || !this.fileList.includes(frame.source.path)) return;
    this.openFile(frame.source.path);
    this.revealToken = String(frame.source.line) + ':' + Date.now();
  }
  private async debugAction(method: string): Promise<void> {
    try {
      if (method === 'start') {
        if (this.nativeTaskBusy || this.dirty) throw new Error('请先保存并完成构建，再启动调试');
        this.showDock = true; this.dockTab = 2;
        await this.deviceDebugger.start(this.getUIContext().getHostContext() as common.UIAbilityContext, this.workspaceDir);
      } else if (method === 'stop') await this.deviceDebugger.stop();
      else if (method === 'breakpoint') {
        await this.deviceDebugger.toggle(this.workspaceDir, this.activeName, this.selectedLine);
        this.showDock = true; this.dockTab = 2;
      } else await this.deviceDebugger.action(method);
    } catch (error) { promptAction.showToast({message:String(error)}); }
  }

  private async runNativeTask(title: string,
    task: (context: common.UIAbilityContext, report: BuildProgress) => Promise<string>): Promise<void> {
    if (this.nativeTaskBusy) return;
    this.nativeTaskBusy = true;
    this.showDock = true;
    this.dockTab = 0;
    this.consoleVisible = true;
    this.buildEntries = [];
    const started = Date.now();
    let session: BuildConsole | undefined = undefined;
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      session = new BuildConsole(context, title, (entry: BuildLogEntry) => {
        this.buildEntries = this.buildEntries.slice(-499).concat(entry);
        WorkbenchRuntime.append(entry.text);
      });
      const result = await task(context, (message: string) => {
        session?.flush();
        session?.write(message);
      });
      session.flush();
      session.write(result, 'info');
      session.finish(true);
    } catch (error) {
      if (session) {
        session.flush();
        session.write(String(error), 'error');
        session.finish(false);
      } else {
        this.consoleVisible = false;
        this.buildReport = '构建日志初始化失败：' + String(error);
      }
    } finally {
      this.lastCost = Date.now() - started;
      this.nativeTaskBusy = false;
    }
  }

  private async startHarmonyDemo(): Promise<void> {
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      await context.startAbility({
        bundleName: 'com.example.harmonyplayground',
        moduleName: 'harmony_demo',
        abilityName: 'HarmonyDemoAbility'
      });
    } catch (error) {
      this.buildReport = '原生示例未启动。请先安装 harmony_demo HAP：' + JSON.stringify(error);
      this.showDock = true;
      this.dockTab = 0;
    }
  }

  private async startBuiltApplication(reportErrors: boolean = false): Promise<void> {
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const app = JSON.parse(fileIo.readTextSync(context.filesDir + '/offline-build/latest.json')) as BuiltApplication;
      await context.startAbility({ bundleName: app.bundleName, moduleName: app.moduleName, abilityName: app.abilityName });
    } catch (error) {
      this.buildReport = '最近构建应用未启动：' + String(error);
      this.consoleVisible = false;
      this.showDock = true;
      this.dockTab = 0;
      if (reportErrors) throw new Error(this.buildReport);
    }
  }

  private run(): void {
    if (this.running) { return; }
    this.showDock = true;
    this.dockTab = 1;
    if (!/\.(js|mjs|cjs)$/.test(this.activeName)) {
      this.result = '此文件只能编辑；当前预览引擎支持 JavaScript，ArkTS 项目需接入本机工具链后构建。';
      return;
    }
    this.running = true;
    if (this.activeName) { this.writeFile(this.activeName, this.code); }
    try {
      const r: EvalResult = Jsvm.evalScript(this.code);
      hilog.info(0x0000, TAG, 'eval ok=%{public}s timeMs=%{public}d', String(r.ok), r.timeMs);
      this.logs = r.logs;
      this.uiTree = r.ui;
      this.lastCost = r.timeMs;
      this.dirty = false;
      if (r.timeout) {
        this.result = '⚠ 执行超时被中止（>2s）';
      } else if (!r.ok) {
        this.result = '✗ ' + r.error;
      } else {
        this.result = '→ ' + r.value;
      }
    } catch (e) {
      this.result = '✗ Native 调用异常：' + JSON.stringify(e);
    } finally {
      this.running = false;
    }
  }

  /** 导出当前代码并拉起同设备的独立 App 窗口 */
  private runStandalone(): void {
    if (!/\.(js|mjs|cjs)$/.test(this.activeName)) {
      this.checkToolchain();
      return;
    }
    if (this.activeName) { this.writeFile(this.activeName, this.code); }
    try {
      const path = getContext(this).filesDir + '/usercode.js';
      const f = fileIo.openSync(path,
        fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
      fileIo.writeSync(f.fd, this.code);
      fileIo.closeSync(f.fd);
      const ctx = getContext(this) as common.UIAbilityContext;
      ctx.startAbility({
        bundleName: 'com.example.harmonyplayground',
        abilityName: 'StandaloneAbility'
      }).then(() => {
        promptAction.showToast({ message: '已打开脚本预览窗口' });
      }).catch((error: Error) => {
        this.result = '预览窗口启动失败：' + error.message;
      });
    } catch (e) {
      promptAction.showToast({ message: '拉起失败：' + JSON.stringify(e) });
    }
  }

  private toggleTheme(): void {
    const svc = ThemeService.getInstance();
    const next = svc.getNextTheme();
    svc.switchTheme(next.id);
    svc.saveThemePreference(next.id);
    this.themeId = next.id;
  }

  // ---------------- 布局 ----------------
  @Builder
  private sidebar() {
    if (this.showSidebar) {
      ProjectTree({
        themeId: this.themeId,
        fileList: this.fileList,
        activeName: this.activeName,
        dirty: this.dirty,
        onOpen: (n: string) => { this.openFile(n); },
        onDelete: (n: string) => { this.confirmName = n; },
        onRename: (n: string) => { this.renameTarget = n; this.renameDialog.open(); },
        onNew: (directory: string) => { this.openNewFile(directory); }
      }).width(this.bp === 'sm' ? 160 : this.sidebarWidth).height('100%')
    }
  }

  @Builder
  private statusBar() {
    StatusBar({
      themeId: this.themeId,
      activeName: this.activeName,
      lineText: this.code.split('\n').length + ' 行',
      costText: this.lastCost > 0 ? '最近任务 ' + this.lastCost + ' ms' : '等待构建'
    }).width('100%')
  }

  /** 删除确认（与 InputDialog 同风格的自绘卡片） */
  @Builder
  private confirmDialog() {
    Column() {
    }
    .width('100%').height('100%')
    .backgroundColor('#000000').opacity(0.45)
    .onClick(() => { this.confirmName = ''; })
    Column() {
      Text('删除文件')
        .fontSize(16).fontWeight(FontWeight.Medium)
        .fontColor(this.t().colors.text).width('100%')
      Text(this.confirmName + ' 将从工作区移除，此操作不可撤销。')
        .fontSize(13).fontColor(this.t().colors.textSecondary)
        .width('100%').margin({ top: 8, bottom: 20 })
      Row({ space: 12 }) {
        Button('取消')
          .fontSize(14).fontColor(this.t().colors.text)
          .backgroundColor(this.t().colors.surface)
          .borderRadius(4).height(40).layoutWeight(1)
          .onClick(() => { this.confirmName = ''; })
        Button('删除')
          .fontSize(14).fontColor('#FFFFFF')
          .backgroundColor(this.t().colors.error)
          .borderRadius(4).height(40).layoutWeight(1)
          .onClick(() => {
            this.deleteFile(this.confirmName);
            this.confirmName = '';
          })
      }.width('100%')
    }
    .width(300).padding(20)
    .backgroundColor(this.t().colors.background)
    .borderRadius(4)
  }

  @Builder
  private menuBar() {
    Row() {
      Text('ArkForge').fontSize(15).fontWeight(FontWeight.Bold)
        .fontColor(this.t().colors.text).margin({ left: 12, right: 16 })
      Button('文件').type(ButtonType.Normal).height(34).borderRadius(0)
        .fontSize(13).fontColor(this.t().colors.text).backgroundColor(Color.Transparent)
        .bindMenu([
          { value: this.importing ? '正在导入…' : '打开项目 ZIP…', action: () => { this.importProject(); } },
          { value: '新建文件…', action: () => { this.openNewFile(''); } },
          { value: '保存当前文件', action: () => { this.saveActive(); } },
          { value: '导出项目 ZIP…', action: () => { this.exportProject(); } }
        ])
      Button('视图').type(ButtonType.Normal).height(34).borderRadius(0)
        .fontSize(13).fontColor(this.t().colors.text).backgroundColor(Color.Transparent)
        .bindMenu([
          { value: '显示 / 隐藏项目树', action: () => { this.showSidebar = !this.showSidebar; } },
          { value: '显示 / 隐藏输出面板', action: () => { this.showDock = !this.showDock; } },
          { value: '切换主题', action: () => { this.toggleTheme(); } }
        ])
      Button('构建').type(ButtonType.Normal).height(34).borderRadius(0)
        .fontSize(13).fontColor(this.t().colors.text).backgroundColor(Color.Transparent)
        .bindMenu([
          { value: '构建并安装当前工程', action: () => { this.buildOfflineProject(); } },
          { value: '导入当前工程签名包', action: () => { this.importSigningPackage(); } },
          { value: '检查构建环境', action: () => { this.checkToolchain(); } },
          { value: '导出完整构建日志…', action: () => { this.exportBuildLog(); } }
        ])
      Button('调试').type(ButtonType.Normal).height(34).borderRadius(0)
        .fontSize(13).fontColor(this.t().colors.text).backgroundColor(Color.Transparent)
        .bindMenu([
          { value: '启动断点调试', action: () => { this.debugAction('start'); } },
          { value: '切换所选行断点', action: () => { this.debugAction('breakpoint'); } },
          { value: '运行最近安装的应用', action: () => { this.startBuiltApplication(); } },
          { value: '读取已安装应用调试日志', action: () => { this.readRuntimeLog(); } },
        ])
      Button('Agent').height(34).onClick(() => { this.agentOpen = !this.agentOpen; })
      Button('帮助').type(ButtonType.Normal).height(34).borderRadius(0)
        .fontSize(13).fontColor(this.t().colors.text).backgroundColor(Color.Transparent)
        .onClick(() => { this.showGuide = true; })
      Blank()

      if (this.bp !== 'sm') {
        Text(this.importing ? '正在导入项目' : '本机工作区')
          .fontSize(12).fontColor(this.t().colors.textSecondary).margin({ right: 14 })
      }
    }.width('100%').height(40)
    .backgroundColor(this.t().colors.surface)
    .border({ width: { bottom: 1 }, color: this.t().colors.border })
  }

  @Builder
  private editorBody() {
    Column() {
      Row() {
        Text(this.activeName + (this.dirty ? ' •' : '')).fontSize(12)
          .fontColor(this.t().colors.text).maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis }).layoutWeight(1)
        Text('Monaco').fontSize(11).fontColor(this.t().colors.textSecondary)
      }.height(32).width('100%').padding({ left: 14, right: 14 })
      .backgroundColor(this.t().colors.surface)
      MonacoEditor({ onSave: () => { this.saveActive(); }, code: this.code, themeId: this.themeId, activeName: this.activeName, revealToken: this.revealToken,
        onLineSelected: (line: number) => { this.selectedLine = line; },
        onContentChange: (v: string) => {
          this.code = v; this.dirty = true;
          if (this.draftTimer >= 0) clearTimeout(this.draftTimer);
          this.draftTimer = setTimeout(() => { this.persistDraft(); }, 300);
        }
      }).layoutWeight(1).width('100%')
    }.width('100%').layoutWeight(1)
  }

  @Builder
  private dockBody() {
    Column() {
      Row({ space: 0 }) {
        Button('构建输出（最近 500 行）').type(ButtonType.Normal).height(32).borderRadius(0)
          .backgroundColor(this.dockTab === 0 ? this.t().colors.background : Color.Transparent)
          .fontColor(this.t().colors.text).fontSize(12).onClick(() => { this.dockTab = 0; })
        Button('断点调试').type(ButtonType.Normal).height(32).fontSize(12).onClick(() => { this.dockTab = 2; })
        Blank()
        Button('收起').type(ButtonType.Normal).height(32).borderRadius(0)
          .backgroundColor(Color.Transparent).fontColor(this.t().colors.textSecondary)
          .fontSize(12).onClick(() => { this.showDock = false; })
      }.width('100%').backgroundColor(this.t().colors.surface)
      if (this.dockTab === 2) {
        DeviceDebugView({state:this.debugState,onAction:(method: string) => { this.debugAction(method); },
          onFrame:(frame: DebugFrame) => { this.showDebugFrame(frame); this.deviceDebugger.inspectFrame(frame); }})
          .layoutWeight(1).width('100%')
      } else if (this.dockTab === 0) {
        if (this.consoleVisible) {
          BuildConsoleView({ entries: this.buildEntries, textColor: this.t().colors.text,
            onDiagnostic: (path: string, line: number) => {
              if (line > 0 && this.fileList.includes(path)) {
                this.openFile(path); this.revealToken = String(line) + ':' + Date.now();
              }
            }
          })
            .layoutWeight(1).width('100%')
        } else {
          Scroll() {
          Text(this.buildReport).fontSize(12).fontFamily(MONO).fontColor(this.t().colors.text)
            .width('100%').padding(12).textAlign(TextAlign.Start)
          }.layoutWeight(1).width('100%').align(Alignment.TopStart)
        }
      } else {
        OutputPanel({ themeId: this.themeId, uiTree: this.uiTree, logs: this.logs, result: this.result })
          .layoutWeight(1).width('100%')
      }
    }.height(this.dockHeight).width('100%').backgroundColor(this.t().colors.background)
  }

  @Builder
  private mainArea() {
    Row() {
      this.sidebar()
      if (this.showSidebar && this.bp !== 'sm') {
        ResizablePane({ axis: ResizeAxis.Horizontal, darkMode: this.themeId !== 'harmony-light',
          onResizeStart: () => { this.resizeSidebarStart = this.sidebarWidth; },
          onResizeUpdate: (offset: number) => {
            this.sidebarWidth = Math.max(160, Math.min(320, this.resizeSidebarStart + offset));
          }, onReset: () => { this.sidebarWidth = 228; }
        })
      }
      Column() {
        this.editorBody()
        if (this.showDock) {
          ResizablePane({ axis: ResizeAxis.Vertical, verticalThickness: 8,
            darkMode: this.themeId !== 'harmony-light',
            onResizeStart: () => { this.resizeDockStart = this.dockHeight; },
            onResizeUpdate: (offset: number) => {
              this.dockHeight = Math.max(120, Math.min(320, this.resizeDockStart - offset));
            }, onReset: () => { this.dockHeight = 200; }
          })
          BottomDock({ content: () => { this.dockBody(); } })
        }
        this.statusBar()
      }.layoutWeight(1).height('100%')
      if (this.agentOpen) {
        AgentPanel({onTool:(method: string,payload: string) => this.agentTool(method,payload)}).width('36%').height('100%')
      }
    }.layoutWeight(1).width('100%')
  }

  private async workbenchTool(method: string, payload: string): Promise<string> {
    if (method === 'workspace_info') return JSON.stringify({root:this.workspaceDir,name:this.workspaceDir.split('/').pop() || 'HarmonyOS'});
    if (['terminal_read','terminal_write','terminal_resize','terminal_close'].includes(method)) {
      const terminal = JSON.parse(payload) as TerminalRequest;
      if (method === 'terminal_read') return JSON.stringify(await officialCli.terminalRead(terminal.id));
      if (method === 'terminal_write') return JSON.stringify(await officialCli.terminalWrite(terminal.id,terminal.data));
      if (method === 'terminal_resize') officialCli.terminalResize(terminal.id,terminal.cols,terminal.rows);
      if (method === 'terminal_close') officialCli.terminalClose(terminal.id);
      return '{}';
    }
    const request = JSON.parse(payload) as WorkbenchRequest;
    if (request.workspaceRoot && request.workspaceRoot !== this.workspaceDir) throw new Error('当前工程已切换，请在新工作区重试');
    if (method === 'terminal_open') {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const terminal = JSON.parse(payload) as TerminalRequest;
      const home = context.filesDir + '/git-home';
      if (!fileIo.accessSync(home)) fileIo.mkdirSync(home,true);
      AtomicFiles.writeBytes(home + '/cacert.pem',await context.resourceManager.getRawFileContent('git/cacert.pem'));
      let credential = '';
      try { credential = await GitHubAccount.gitCredential(); } catch (_) { /* Account menu refreshes authorization. */ }
      return JSON.stringify(officialCli.terminalOpen(this.workspaceDir,home,credential,terminal.cols || 80,terminal.rows || 24));
    }
    if (method.startsWith('fs.')) {
      const operation = method.slice(3);
      if (!['stat','list','read','index','readBatch'].includes(operation) && this.nativeTaskBusy) throw new Error('构建期间不能修改工程');
      const result = WorkbenchFiles.execute(this.workspaceDir,operation,payload);
      if (!['stat','list','read','index','readBatch'].includes(operation)) {
        this.fileList = ProjectArchive.sourceFiles(this.workspaceDir);
        if (this.activeName && this.fileList.includes(this.activeName)) {
          this.code = this.readFile(this.activeName); this.savedStamp = this.stamp(this.activeName); this.dirty = false;
        }
        this.saveManifest();
      }
      return result;
    }
    if (method === 'project_preflight') {
      if (this.nativeTaskBusy) throw new Error('已有本机任务正在执行');
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const plan = await ResourceCompiler.plan(context,this.workspaceDir,(message: string) => { WorkbenchRuntime.append(message); });
      return JSON.stringify({bundleName:plan.bundleName,moduleName:plan.moduleName});
    }
    if (method === 'sdk_diagnostics') {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const path = context.filesDir + '/latest-sdk-diagnostics.json';
      if (!fileIo.accessSync(path)) return '[]';
      const result = JSON.parse(fileIo.readTextSync(path)) as SDKReport;
      return JSON.stringify(result.project === this.workspaceDir ? result.diagnostics : []);
    }
    if (method.startsWith('github.')) return await GitHubAccount.execute(method.slice(7), payload);
    if (method.startsWith('huawei.')) {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      if (method !== 'huawei.issue') return await HuaweiSigning.execute(context,method.slice(7),payload);
      if (this.nativeTaskBusy || this.importing) throw new Error('请等待当前本机任务完成后申请签名');
      this.nativeTaskBusy = true;
      try {
        const input = JSON.parse(payload) as CloudSigningRequest;
        const plan = await ResourceCompiler.plan(context,this.workspaceDir,(text: string) => {});
        return await HuaweiSigning.issue(context,plan.bundleName,input.teamId,input.udid,(text: string) => {WorkbenchRuntime.append(text);});
      } finally {this.nativeTaskBusy = false;}
    }
    if (method === 'developer_open') {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      return await GitHubAccount.open(context, (JSON.parse(payload) as DeveloperRequest).page);
    }
    if (method === 'signing_status') {
      if (this.nativeTaskBusy) throw new Error('请等待当前本机任务完成后查看签名');
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const plan = await ResourceCompiler.plan(context, this.workspaceDir, (text: string) => {});
      const logical = context.filesDir + '/native-toolchain/signing' + (plan.bundleName === 'com.example.harmonyplayground' ? '' : '/' + plan.bundleName);
      try { return JSON.stringify({bundleName:plan.bundleName,valid:true,expires:await SigningMaterials.validate(logical, plan.bundleName)}); }
      catch (cause) { return JSON.stringify({bundleName:plan.bundleName,valid:false,reason:String(cause)}); }
    }
    if (method === 'benchmark_sdk_staging') {
      if (this.nativeTaskBusy || this.importing) throw new Error('请等待当前本机任务完成');
      this.nativeTaskBusy = true;
      try {
        const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
        const result = await SDKProjectChecker.benchmarkStaging(context);
        AtomicFiles.write(context.filesDir + '/sdk-check/staging-benchmark.json',result);
        return result;
      } finally { this.nativeTaskBusy = false; }
    }
    if (method === 'git_identity') {
      if (this.nativeTaskBusy || this.importing) throw new Error('请等待当前本机任务完成');
      const input = JSON.parse(payload) as DeveloperRequest;
      if (!input.name?.trim() || /[\r\n\0]/.test(input.name) || !/^[^\s@]+@[^\s@]+$/.test(input.email)) throw new Error('Git 姓名或邮箱无效');
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const home = context.filesDir + '/git-home';
      if (!fileIo.accessSync(home)) fileIo.mkdirSync(home, true);
      this.nativeTaskBusy = true;
      try {
        for (const pair of [['user.name',input.name],['user.email',input.email]]) {
          const result = await officialCli.run(this.workspaceDir,home,['git','config','--global',pair[0],pair[1]]);
          if (result.code !== 0) throw new Error(result.output);
        }
        return '{}';
      } finally { this.nativeTaskBusy = false; }
    }
    if (method === 'git_run') {
      if (this.nativeTaskBusy || this.importing) throw new Error('[ArkForge] 请先完成当前本机任务');
      const command = JSON.parse(payload) as GitRequest;
      if (command.path !== '/workspace' && !command.path.startsWith('/workspace/')) throw new Error('[ArkForge] 无效的工程路径');
      if (command.path.split('/').some((part: string) => part === '..' || part === '.') || command.path.includes('\\')) throw new Error('[ArkForge] 无效的工程路径');
      if (!command.args || command.args[0] !== 'git') throw new Error('[ArkForge] 此入口只启动官方 Git');
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const home = context.filesDir + '/git-home';
      if (!fileIo.accessSync(home)) fileIo.mkdirSync(home, true);
      this.nativeTaskBusy = true;
      try {
        AtomicFiles.writeBytes(home + '/cacert.pem',await context.resourceManager.getRawFileContent('git/cacert.pem'));
        const result = await officialCli.run(this.workspaceDir + command.path.slice('/workspace'.length), home, command.args,await GitHubAccount.gitCredential());
        AtomicFiles.write(context.filesDir + '/git-last-exit.json', JSON.stringify({ code: result.code, waitCode: result.waitCode, traceCode: result.traceCode, bytes: result.output.length }));
        return JSON.stringify(result);
      }
      finally { this.nativeTaskBusy = false; }
    }
    if (method === 'open_project') {
      if (this.nativeTaskBusy || this.importing) throw new Error('请先完成当前构建或文件操作，再切换工程');
      const previous = this.workspaceDir;
      await this.importProject(true);
      return JSON.stringify({changed:previous !== this.workspaceDir});
    }
    if (method === 'export_project') { if (this.nativeTaskBusy) throw new Error('请先完成构建再导出'); await this.exportProject(true); return this.buildReport; }
    if (method === 'import_signing' || method === 'signing_import_files') {
      if (this.nativeTaskBusy || this.importing) throw new Error('请等待当前本机任务完成后配置签名');
      this.nativeTaskBusy = true;
      try {
        const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
        const plan = await ResourceCompiler.plan(context, this.workspaceDir, (text: string) => {});
        const view = new picker.DocumentViewPicker(context);
        if (method === 'import_signing') {
          const selected = await view.select({maxSelectNumber:1,fileSuffixFilters:['开发签名包|.zip']});
          if (!selected.length) return JSON.stringify({cancelled:true});
          await SigningMaterials.importPackage(context, selected[0], plan.bundleName);
        } else {
          const credentials = JSON.parse(payload) as SigningCredentials;
          const uris: string[] = [];
          for (const filter of ['1. 选择密钥库|.p12','2. 选择签名证书|.cer','3. 选择开发 Profile|.p7b']) {
            const selected = await view.select({maxSelectNumber:1,fileSuffixFilters:[filter]});
            if (!selected.length) return JSON.stringify({cancelled:true});
            uris.push(selected[0]);
          }
          await SigningMaterials.importFiles(context, uris, credentials, plan.bundleName);
        }
        return JSON.stringify({cancelled:false,bundleName:plan.bundleName});
      } finally { this.nativeTaskBusy = false; }
    }
    if (method === 'check_sdk') { await this.checkToolchain(); return this.buildEntries.map((entry: BuildLogEntry) => entry.text).join('\n'); }
    if (method === 'run_app') { await this.startBuiltApplication(true); return '已请求系统打开最近安装的应用'; }
    return await this.agentTool(method,payload);
  }

  private async agentTool(method: string, payload: string): Promise<string> {
    const args = JSON.parse(payload) as AgentArgs;
    if (method === 'search_text') {
      const query = args.query || '';
      if (!query || query.length > 512) throw new Error('搜索文本为空或过长');
      const matches: string[] = [];
      for (const path of this.fileList) {
        if (path.startsWith('.arkforge/')) continue;
        if (fileIo.statSync(this.workspaceDir + '/' + path).size > 2097152) continue;
        const lines = fileIo.readTextSync(this.workspaceDir + '/' + path).split('\n');
        for (let line = 0; line < lines.length; line++) {
          const column = lines[line].indexOf(query);
          if (column >= 0) matches.push(path + ':' + (line + 1) + ':' + (column + 1) + ': ' + lines[line].slice(0,300));
          if (matches.length >= 100) return matches.join('\n') + '\n[结果达到 100 条，请缩小查询范围]';
        }
      }
      return matches.length ? matches.join('\n') : '未找到匹配内容';
    }
    if (method === 'debug_state') return JSON.stringify(this.debugState);
    if (method === 'debug_action') {
      const action = args.action || '';
      if (this.nativeTaskBusy || this.dirty) throw new Error('请先保存文件并完成构建');
      if (action === 'start') await this.deviceDebugger.start(this.getUIContext().getHostContext() as common.UIAbilityContext, this.workspaceDir);
      else if (action === 'stop') await this.deviceDebugger.stop();
      else if (['Debugger.resume','Debugger.pause','Debugger.stepOver','Debugger.stepInto','Debugger.stepOut'].includes(action)) await this.deviceDebugger.action(action);
      else throw new Error('不支持的调试动作');
      this.showDock = true; this.dockTab = 2;
      return JSON.stringify(this.debugState);
    }
    if (method === 'get_open_file') return JSON.stringify({path:this.activeName,content:this.code});
    if (method === 'list_dir') return JSON.stringify(this.fileList);
    if (method === 'read_file' || method === 'write_file') {
      const path = args.path || '';
      if (!path || path.startsWith('/') || path.includes('\\') || path.split('/').some((part: string) => part === '..' || part === '.')) throw new Error('仅允许当前工程内的相对文件路径');
      if (!this.fileList.includes(path)) throw new Error('文件不在当前工程中：' + path);
      if (method === 'read_file') {
        if (path === this.activeName) return this.code;
        if (fileIo.statSync(this.workspaceDir + '/' + path).size > 2097152) throw new Error('文件超过 2 MB，请缩小读取范围');
        return fileIo.readTextSync(this.workspaceDir + '/' + path);
      }
      if (this.nativeTaskBusy) throw new Error('构建期间不能修改工程');
      if (args.content === undefined || args.content.length > 2097152) throw new Error('写入内容无效或超过 2 MB');
      if (this.dirty && !this.saveActive()) throw new Error('请先处理编辑器保存冲突');
      this.writeFile(path,args.content);
      if (path === this.activeName) { this.code = args.content; this.dirty = false; this.savedStamp = this.stamp(path); }
      return '已在平板保存：' + path;
    }
    if (method === 'build_project' || method === 'compile_project' || method === 'install_prepared' || method === 'read_logs') {
      if (this.nativeTaskBusy) throw new Error('已有本机任务正在执行');
      if (this.dirty && !this.saveActive()) throw new Error('请先处理编辑器保存冲突');
      await this.runNativeTask(method, (context: common.UIAbilityContext, report: BuildProgress) =>
        method === 'install_prepared' ? OfflineProjectBuilder.installPrepared(context,this.workspaceDir,args.path || '',report) :
        method === 'read_logs' ? LocalDevice.readRuntimeLog(context,report) :
        OfflineProjectBuilder.build(context,this.workspaceDir,report,method !== 'compile_project'));
      return this.buildEntries.map((entry: BuildLogEntry) => entry.text).join('\n');
    }
    throw new Error('未支持的本机工具：' + method);
  }

  @Builder
  private usageGuide() {
    Column({ space: 12 }) {
      Text('在平板上开发 HarmonyOS 应用').fontSize(22).fontWeight(FontWeight.Bold)
      Scroll() {
        Text('1. 准备工程\n将真实 HarmonyOS 工程打包为 ZIP，根目录保留 AppScope、build-profile.json5 和 entry 等模块目录。\n\n2. 打开并编辑\n选择「文件 → 打开项目 ZIP」，从系统文件选择器导入。展开左侧目录后点击文件；编辑后选择「保存当前文件」。\n\n3. 构建并安装\n选择「构建 → 构建并安装当前工程」。底部按顺序输出编译、打包、签名和安装结果；错误以红字显示。完整日志写入输出中列出的日志文件。\n\n支持 ArkTS 编译及本机资源编译。安装前须导入匹配当前工程和设备的签名材料。原生 C++ 暂时使用工程中已编译的库；修改 C++ 源码仍需外部工具链。\n\n4. 运行与返回\n构建成功后选择「调试 → 运行最近安装的应用」，在系统弹窗中点击「打开」。通过系统返回手势或顶部返回 ArkForge 返回编辑器，再修改和构建。\n\n未改动的源码构建会复用经哈希校验的 ABC。')
          .fontSize(15).lineHeight(24).width('100%')
      }.layoutWeight(1).width('100%')
      Button('开始使用').onClick(() => { this.showGuide = false; })
    }.padding(24).width('85%').height('85%')
      .backgroundColor(this.t().colors.surface).borderRadius(12)
  }

  @Builder
  private rootBody() {
    Column() {
      GlobalMenuBar({ content: () => { this.menuBar(); } })
      this.mainArea()
    }.width('100%').height('100%')

    if (this.confirmName.length > 0) {
      this.confirmDialog()
    }
  }

  build() {
    Stack({ alignContent: Alignment.Center }) {
      Web({ src: 'about:blank', controller: SDKWebRuntime.controller })
        .javaScriptAccess(true).fileAccess(true)
        .javaScriptProxy({ object: SDKWebRuntime.bridge, name: 'SdkBridge', methodList: ['complete'], controller: SDKWebRuntime.controller })
        .onConsole((event) => { if (event) SDKWebRuntime.log(event.message.getMessage()); return true; })
        .width(1).height(1).opacity(0)
      Web({ src: 'about:blank', controller: SDKWebRuntime.transformController })
        .javaScriptAccess(true).fileAccess(true)
        .javaScriptProxy({ object: SDKWebRuntime.transformBridge, name: 'SdkBridge', methodList: ['complete'], controller: SDKWebRuntime.transformController })
        .onConsole((event) => { if (event) SDKWebRuntime.transformLog(event.message.getMessage()); return true; })
        .width(1).height(1).opacity(0)
      CodeOSSWorkbench({buildEvent:this.workbenchBuildEvent,
        onTool:(method: string,payload: string) => this.workbenchTool(method,payload)})
    }.width('100%').height('100%')
    .backgroundColor(ThemeService.getInstance().getCurrentTheme().colors.background)
  }

  private t(): Theme {
    return ThemeService.getInstance().getCurrentTheme();
  }
}
