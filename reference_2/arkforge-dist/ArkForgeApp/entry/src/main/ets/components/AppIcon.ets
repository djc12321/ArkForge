/**
 * AppIcon —— 自绘矢量线性图标（不使用 emoji，保证跨平台风格统一）
 *
 * 24x24 网格、线宽 1.8、圆角端点与连接，与 OpenCode 设计语言一致。
 * 注意：HarmonyOS Path 对 SVG 命令支持有限，全部使用 M/L/C/Z 绝对命令并以空格分隔。
 * 仅 play（实心三角）与 dot（实心圆点）使用填充绘制。
 */
@Component
export struct AppIcon {
  @Prop name: string = 'file';
  @Prop iconSize: number = 18;
  @Prop color: string = '#FFFFFF';

  build() {
    Path()
      .commands(this.pathData())
      .fill(this.isFilled() ? this.color : Color.Transparent)
      .stroke(this.isFilled() ? Color.Transparent : this.color)
      .strokeWidth(1.8)
      .strokeLineCap(LineCapStyle.Round)
      .strokeLineJoin(LineJoinStyle.Round)
      .width(this.iconSize)
      .height(this.iconSize)
  }

  /** 是否使用填充绘制 */
  private isFilled(): boolean {
    return this.name === 'play' || this.name === 'dot';
  }

  private pathData(): string {
    switch (this.name) {
      case 'folder':
        return 'M2 6 L10 6 L12 8 L22 8 L22 20 L2 20 Z M2 6 L2 4 L9 4 L11 6';
      case 'folderOpen':
        return 'M2 9 L2 5 L10 5 L12 7 L21 7 L21 10 M2 20 L5 10 L23 10 L20 20 Z';
      case 'menu':          // 汉堡菜单
        return 'M3 6 L21 6 M3 12 L21 12 M3 18 L21 18';
      case 'file':          // 文件（折角文档）
        return 'M13 3 L7 3 C5.9 3 5 3.9 5 5 L5 19 C5 20.1 5.9 21 7 21 L17 21 C18.1 21 19 20.1 19 19 L19 9 L13 3 M13 3 L13 9 L19 9';
      case 'plus':          // 新建（十字）
        return 'M12 5 L12 19 M5 12 L19 12';
      case 'close':         // 关闭（叉）
        return 'M6 6 L18 18 M18 6 L6 18';
      case 'play':          // 运行（实心三角）
        return 'M8 5 L19 12 L8 19 Z';
      case 'save':          // 保存（软盘）
        return 'M19 21 L5 21 C3.9 21 3 20.1 3 19 L3 5 C3 3.9 3.9 3 5 3 L16 3 L21 8 L21 19 C21 20.1 20.1 21 19 21 M17 21 L17 13 L7 13 L7 21 M7 3 L7 8 L15 8';
      case 'launch':        // 独立运行（外链箭头）
        return 'M14 4 L20 4 L20 10 M20 4 L11 13 M18 14 L18 19 C18 19.6 17.6 20 17 20 L5 20 C4.4 20 4 19.6 4 19 L4 7 C4 6.4 4.4 6 5 6 L10 6';
      case 'more':          // 更多（水平三点，圆头线帽渲染为圆点）
        return 'M6 12 L6.01 12 M12 12 L12.01 12 M18 12 L18.01 12';
      case 'theme':         // 主题（月牙）
        return 'M20 13 C19 16.5 15.8 19 12 19 C7.6 19 4 15.4 4 11 C4 6.6 7.6 3 12 3 C12.4 3 12.8 3 13.2 3.1 C11.2 4.4 10 6.6 10 9 C10 13 13 16 17 16 C18.1 16 19.2 15.6 20 13 Z';
      case 'terminal':      // 控制台（命令行）
        return 'M7 9 L10 12 L7 15 M13 15 L17 15';
      case 'preview':       // 预览（眼睛）
        return 'M2 12 C2 12 5.5 6 12 6 C18.5 6 22 12 22 12 C22 12 18.5 18 12 18 C5.5 18 2 12 2 12 Z M12 9 C10.3 9 9 10.3 9 12 C9 13.7 10.3 15 12 15 C13.7 15 15 13.7 15 12 C15 10.3 13.7 9 12 9 Z';
      case 'result':        // 结果（对勾）
        return 'M4 12.5 L9 17.5 L20 6.5';
      case 'chevronLeft':   // 收起
        return 'M15 6 L9 12 L15 18';
      case 'chevronRight':  // 展开
        return 'M9 6 L15 12 L9 18';
      case 'dot':           // 未保存标记（实心圆点）
        return 'M12 8 C9.8 8 8 9.8 8 12 C8 14.2 9.8 16 12 16 C14.2 16 16 14.2 16 12 C16 9.8 14.2 8 12 8 Z';
      default:
        return 'M13 3 L7 3 C5.9 3 5 3.9 5 5 L5 19 C5 20.1 5.9 21 7 21 L17 21 C18.1 21 19 20.1 19 19 L19 9 L13 3 M13 3 L13 9 L19 9';
    }
  }
}
