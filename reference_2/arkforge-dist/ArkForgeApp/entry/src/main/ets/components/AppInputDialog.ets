/**
 * 统一风格的输入对话框
 * 用于新建 / 重命名文件。圆角 4px、配色跟随主题，与整体设计语言一致。
 *
 * 来源：KeloYuan/xxCode（MIT License）https://github.com/KeloYuan/xxCode
 */
import { ThemeService, Theme } from '../services/ThemeService';

@CustomDialog
export struct InputDialog {
  @Prop title: string = '输入';
  @Prop placeholder: string = '请输入...';
  @Prop defaultValue: string = '';
  @Prop confirmText: string = '确定';
  @Prop cancelText: string = '取消';
  
  @State inputValue: string = '';
  @State private currentTheme: Theme = ThemeService.getInstance().getCurrentTheme();
  
  controller: CustomDialogController;
  onConfirm: (value: string) => void = () => {};
  onCancel: () => void = () => {};
  
  private themeService: ThemeService = ThemeService.getInstance();

  aboutToAppear() {
    this.inputValue = this.defaultValue;
    this.themeService.onThemeChange((theme: Theme) => {
      this.currentTheme = theme;
    });
  }

  build() {
    Column() {
      // 标题
      Text(this.title)
        .fontSize(16)
        .fontWeight(FontWeight.Medium)
        .fontColor(this.currentTheme.colors.text)
        .width('100%')
        .margin({ bottom: 16 })

      // 输入框
      TextInput({ 
        placeholder: this.placeholder, 
        text: this.inputValue 
      })
        .placeholderColor(this.currentTheme.colors.textSecondary)
        .placeholderFont({ size: 14 })
        .fontSize(14)
        .fontColor(this.currentTheme.colors.text)
        .caretColor(this.currentTheme.colors.primary)
        .backgroundColor(this.currentTheme.colors.surface)
        .borderRadius(4)
        .padding({ left: 12, right: 12 })
        .height(40)
        .width('100%')
        .margin({ bottom: 20 })
        .onChange((value: string) => {
          this.inputValue = value;
        })
        .onSubmit(() => {
          this.handleConfirm();
        })

      // 按钮组
      Row({ space: 12 }) {
        Button(this.cancelText)
          .type(ButtonType.Normal)
          .fontSize(14)
          .fontColor(this.currentTheme.colors.text)
          .backgroundColor(this.currentTheme.colors.surface)
          .borderRadius(4)
          .height(40)
          .layoutWeight(1)
          .onClick(() => {
            this.onCancel();
            this.controller.close();
          })

        Button(this.confirmText)
          .type(ButtonType.Normal)
          .fontSize(14)
          .fontColor(Color.White)
          .backgroundColor(this.currentTheme.colors.primary)
          .borderRadius(4)
          .height(40)
          .layoutWeight(1)
          .enabled(this.inputValue.trim().length > 0)
          .onClick(() => {
            this.handleConfirm();
          })
      }
      .width('100%')
    }
    .width(320)
    .padding(20)
    .backgroundColor(this.currentTheme.colors.background)
    .borderRadius(4)
    .shadow({
      radius: 24,
      color: '#00000030',
      offsetX: 0,
      offsetY: 8
    })
  }

  private handleConfirm(): void {
    const value = this.inputValue.trim();
    if (value.length > 0) {
      this.onConfirm(value);
      this.controller.close();
    }
  }
}
