import { ProjectTreeEntry, ProjectTreeModel } from '../../model/ProjectTreeModel';
import { Theme, ThemeService } from '../../services/ThemeService';
import { AppIcon } from '../AppIcon';
import { FileTypeBadge } from '../FileTypeBadge';
import { pasteboard } from '@kit.BasicServicesKit';

@Component
export struct ProjectTree {
  @Prop themeId: string = 'harmony-light';
  @Prop @Watch('reload') fileList: string[] = [];
  @Prop @Watch('revealActive') activeName: string = '';
  @Prop dirty: boolean = false;
  @State private expanded: string[] = [];
  @State private rows: ProjectTreeEntry[] = [];
  private entries: ProjectTreeEntry[] = [];
  onOpen: (name: string) => void = () => {};
  onDelete: (name: string) => void = () => {};
  onRename: (name: string) => void = () => {};
  onNew: (directory: string) => void = () => {};

  aboutToAppear(): void { this.reload(); }

  reload(): void {
    this.entries = ProjectTreeModel.entries(this.fileList);
    this.revealActive();
  }

  revealActive(): void {
    this.expanded = ProjectTreeModel.reveal(this.activeName, this.expanded);
    this.rows = ProjectTreeModel.visible(this.entries, this.expanded);
  }

  private toggle(path: string): void {
    this.expanded = this.expanded.includes(path) ? this.expanded.filter((item: string) => item !== path) :
      this.expanded.concat([path]);
    this.rows = ProjectTreeModel.visible(this.entries, this.expanded);
  }

  private collapse(): void {
    this.expanded = [];
    this.rows = ProjectTreeModel.visible(this.entries, this.expanded);
  }

  private t(): Theme { return ThemeService.getInstance().getCurrentTheme(); }

  @Builder
  private contextMenu(entry: ProjectTreeEntry) {
    Menu() {
      if (entry.directory) {
        MenuItem({ content: '在此目录新建文件' }).onClick(() => { this.onNew(entry.path); })
        MenuItem({ content: this.expanded.includes(entry.path) ? '收起目录' : '展开目录' })
          .onClick(() => { this.toggle(entry.path); })
      } else {
        MenuItem({ content: '重命名文件' }).onClick(() => { this.onRename(entry.path); })
        MenuItem({ content: '删除文件…' }).onClick(() => { this.onDelete(entry.path); })
      }
      MenuItem({ content: '复制相对路径' }).onClick(() => {
        pasteboard.getSystemPasteboard().setData(pasteboard.createData(pasteboard.MIMETYPE_TEXT_PLAIN, entry.path));
      })
    }
  }

  build() {
    Column() {
      Row() {
        Text('工程').fontSize(12).fontWeight(FontWeight.Medium).fontColor(this.t().colors.text)
        Blank()
        Button('新建').type(ButtonType.Normal).height(32).fontSize(11).borderRadius(0)
          .fontColor(this.t().colors.textSecondary).backgroundColor(Color.Transparent)
          .onClick(() => { this.onNew(''); })
        Button('收起').type(ButtonType.Normal).height(32).fontSize(11).borderRadius(0)
          .fontColor(this.t().colors.textSecondary).backgroundColor(Color.Transparent)
          .onClick(() => { this.collapse(); })
      }.height(40).width('100%').padding({ left: 12, right: 4 })
      Divider().color(this.t().colors.border)
      List({ space: 0 }) {
        ForEach(this.rows, (entry: ProjectTreeEntry) => {
          ListItem() {
            Row({ space: 6 }) {
              Text(entry.directory ? (this.expanded.includes(entry.path) ? '▾' : '▸') : '')
                .width(14).fontSize(13).fontColor(this.t().colors.textSecondary)
              if (entry.directory) {
                AppIcon({ name: this.expanded.includes(entry.path) ? 'folderOpen' : 'folder',
                  iconSize: 18, color: this.t().colors.primary })
              } else {
                FileTypeBadge({ fileName: entry.name, isActive: this.activeName === entry.path,
                  isDark: this.themeId !== 'harmony-light' })
              }
              Text(entry.name).fontSize(12).fontColor(this.t().colors.text).maxLines(1)
                .textOverflow({ overflow: TextOverflow.Ellipsis }).layoutWeight(1)
              if (!entry.directory && entry.path === this.activeName && this.dirty) {
                Circle({ width: 5, height: 5 }).fill(this.t().colors.warning)
              }
            }.height(34).width('100%')
            .padding({ left: 6 + entry.depth * 14, right: 8 })
            .backgroundColor(entry.path === this.activeName ? this.t().editor.selection : Color.Transparent)
            .onClick(() => { if (entry.directory) this.toggle(entry.path); else this.onOpen(entry.path); })
            .bindContextMenu(this.contextMenu(entry), ResponseType.LongPress)
            .accessibilityText((entry.directory ? '目录 ' : '文件 ') + entry.path)
          }
        }, (entry: ProjectTreeEntry) => entry.path)
      }.layoutWeight(1).width('100%').scrollBar(BarState.Auto)
      Text('长按文件或目录以管理').fontSize(10).fontColor(this.t().colors.textSecondary)
        .height(28).width('100%').padding({ left: 12 })
    }.width('100%').height('100%').backgroundColor(this.t().colors.surface)
  }
}
