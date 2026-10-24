/**
 * 文件类型徽章组件 - 鸿蒙设计风格
 * 纯文字扩展名徽章，支持多种文件类型的颜色标识
 */

@Component
export struct FileTypeBadge {
  @Prop fileName: string = '';
  @Prop primaryColor: ResourceColor = '#0A59F7';
  @Prop isActive: boolean = false;
  @Prop isDark: boolean = true;
  @Prop fontSize: number = 10;
  @Prop showDot: boolean = true; // 是否显示点号前缀

  /**
   * 获取文件扩展名徽章文本
   */
  private getExtensionBadge(): string {
    const ext = this.fileName.substring(this.fileName.lastIndexOf('.') + 1).toLowerCase();
    const extUpper = ext.toUpperCase();
    if (!ext || ext === this.fileName) {
      return 'FILE';
    }
    return extUpper.substring(0, 4);
  }

  /**
   * 根据扩展名获取徽章颜色
   */
  private getBadgeColor(): string {
    const ext = this.fileName.substring(this.fileName.lastIndexOf('.') + 1).toLowerCase();
    const colorMap: Record<string, string> = {
      'js': '#F7DF1E',     // JavaScript 黄色
      'ts': '#3178C6',     // TypeScript 蓝色
      'ets': '#0A59F7',    // ArkTS 鸿蒙蓝
      'py': '#3776AB',     // Python 蓝色
      'java': '#007396',   // Java 深蓝
      'html': '#E34F26',   // HTML 橙色
      'css': '#1572B6',    // CSS 蓝色
      'scss': '#CD6799',   // SCSS 粉色
      'json': '#292929',   // JSON 深灰
      'md': '#083FA1',     // Markdown 蓝色
      'txt': '#6B7280',    // 纯文本 灰色
      'xml': '#E97B00',    // XML 橙色
      'yaml': '#CB171E',   // YAML 红色
      'yml': '#CB171E'     // YML 红色
    };
    return colorMap[ext] || (this.primaryColor as string);
  }

  build() {
    Text(this.showDot ? `.${this.getExtensionBadge()}` : this.getExtensionBadge())
      .fontSize(this.fontSize)
      .fontColor(this.isActive ? Color.White : this.getBadgeColor())
      .fontWeight(FontWeight.Medium)
      .padding({ left: 5, right: 5, top: 2, bottom: 2 })
      .backgroundColor(this.isActive ? 'rgba(255, 255, 255, 0.2)' : `${this.getBadgeColor()}20`)
      .borderRadius(4)
      .border({
        width: this.isActive ? 0 : 0.5,
        color: this.getBadgeColor()
      })
  }
}
