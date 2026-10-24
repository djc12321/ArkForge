# 复用代码

- `entry/src/main/ets/components/workspace/`：来自 `djc12321/multism_lab` 的 WorkspaceShell、GlobalMenuBar、BottomDock、ResizablePane 和 WorkspaceVisualPolicy。仅复用前端容器、尺寸规则和拖动逻辑，不包含电路模拟器或仪器工具栏。MIT 许可全文见 `licenses/multism_lab-MIT.txt`。
- SyntaxHighlightService、ThemeService、FileTypeBadge、AppInputDialog：来自 KeloYuan/xxCode，已为 ArkForge 调整。MIT 许可全文见 `licenses/xxCode-MIT.txt`。
- QuickJS：许可见 `native/quickjs/LICENSE`。
- ArkTS-Dyn 编译器：OpenHarmony `arkcompiler_ets_frontend` 与 `arkcompiler_runtime_core`，Apache-2.0；由源码交叉编译为 OHOS ARM64，在 App 进程内调用。
- UI 转换器及组件配置：取自本机 HarmonyOS SDK API 26 的 ets-loader、其 TypeScript 版本、依赖及 HDS 外部组件/Kit 元数据（替换初始 API 23 转换器）；保留上游代码中的许可声明，相关材料适用各自原始许可和 SDK 条款。Node 桌面构建接口由本工程的虚拟文件系统适配，编译时不调用桌面进程。

## 本机开发工具与工作台

- Code-OSS：Microsoft，MIT；版本与源提交见 `entry/src/main/resources/rawfile/codeoss-manifest.json`。完整许可及第三方声明随 `codeoss.zip` 保存，ArkForge 集成补丁见 `workbench/patches`。
- Git 2.55.0：GPL-2.0；官方源码归档见仓库根 `_vendor/git-2.55.0.tar.xz`，许可见 `native/git-COPYING`，平台适配及构建脚本见 `native/git_exec_bridge.c`、`native/git_process_broker.cpp` 和 `native/build_git*.sh`。
- curl 8.23.0：curl 许可；源码归档见 `_vendor/curl-8.23.0.tar.xz`，许可见 `native/curl-COPYING`。HTTPS 后端使用 OpenSSL，许可见 `third-party/OpenSSL-LICENSE.txt`。
- BusyBox 1.37.0：GPL-2.0；官方源码归档见 `_vendor/busybox-1.37.0-source`，构建配置见 `native/busybox-ohos.config`，脚本见 `native/build_busybox.sh`，许可见 `entry/src/main/resources/rawfile/terminal/BusyBox-LICENSE.txt`。当前终端入口使用官方 Git 转发实现，BusyBox/PTY 接入保留为开发中的代码。
- musl strverscmp：许可见 `native/compat/musl-COPYRIGHT`，对应源码见 `native/compat/musl_strverscmp.c`。
- HapSign：MIT；平板自动签名链路参考该实现，许可见 `third-party/HapSign-LICENSE`。
- 资源编译依赖：`_vendor` 中保留 OpenHarmony restool、securec、cJSON、libpng、zlib 的必要源码和各自许可声明。
