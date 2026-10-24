@Component
export struct WorkspaceShell {
  @BuilderParam content: () => void = this.emptyContent;
  @Prop darkMode: boolean = false;

  @Builder
  private emptyContent(): void {}

  build() {
    Column() {
      this.content()
    }
    .width('100%')
    .height('100%')
    .backgroundColor(this.darkMode ? $r('app.color.workspace_dark_background') :
      $r('app.color.workspace_light_background'))
  }
}
