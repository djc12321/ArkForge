import { BuildLogEntry } from '../../services/BuildConsole';

@Component
export struct BuildConsoleView {
  @Prop @Watch('followOutput') entries: BuildLogEntry[] = [];
  @Prop textColor: string = '#d4d4d4';
  private scroller: Scroller = new Scroller();
  onDiagnostic: (path: string, line: number) => void = () => {};

  private followOutput(): void {
    setTimeout(() => { this.scroller.scrollEdge(Edge.Bottom); }, 0);
  }

  build() {
    List({ scroller: this.scroller }) {
      ForEach(this.entries, (entry: BuildLogEntry) => {
        ListItem() {
          Text('[' + entry.time + '] ' + entry.text)
            .fontSize(12).fontFamily('monospace')
            .fontColor(entry.level === 'error' ? '#f44747' :
              (entry.level === 'warning' ? '#d7a524' : (entry.level === 'success' ? '#3fa56b' : this.textColor)))
            .copyOption(CopyOptions.InApp).width('100%').textAlign(TextAlign.Start)
            .padding({ left: 12, right: 12, top: 2, bottom: 2 })
            .onClick(() => {
              const match = /^(.+?):(\d+):(\d+):\s*(error|warning)\b/.exec(entry.text);
              if (match) this.onDiagnostic(match[1], parseInt(match[2], 10));
            })
        }
      }, (entry: BuildLogEntry) => String(entry.id))
    }.width('100%').height('100%')
  }
}
