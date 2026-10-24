@Component
export struct BottomDock {
  @BuilderParam content: () => void = this.emptyContent;

  @Builder
  private emptyContent(): void {}

  build() {
    Column() {
      this.content()
    }
    .width('100%')
  }
}
