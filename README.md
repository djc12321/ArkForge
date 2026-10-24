# ArkForge

在 HarmonyOS 平板上打开和编辑 HarmonyOS 工程，执行本机 ArkTS 编译、开发签名、HAP 安装与调试；提供 Code-OSS 原生工作台、中文菜单、GitHub 账户和本机 Git，以及直连模型服务的 Agent。

## 下载

在 [GitHub Releases](https://github.com/djc12321/ArkForge/releases) 下载 HAP。当前发布的是 ARM64 开发调试包，其调试 Profile 指定允许安装的设备。其他设备请使用 DevEco 为自己的设备配置签名并构建。

## 工程位置与构建

1. 安装 DevEco Studio 和工程对应的 HarmonyOS SDK。当前工程配置为 HarmonyOS 6.1.0 / API 23，支持 ARM64。
2. 用 DevEco 打开 `reference_2/arkforge-dist/ArkForgeApp`，等待 OHPM 同步。
3. 在 Project Structure → Signing Configs 中启用 Automatically generate signature，为 `default` 产品关联自己的签名。
4. 选择连接的设备，执行 Build / Run。HAP 输出目录为 `entry/build/default/outputs/default/`。

仓库保留现有相对路径，以便 CMake 和构建脚本定位 `_vendor` 中的资源编译依赖。已移除开发者个人签名路径、签名密码和证书；构建产物、依赖缓存、截图、设备备份和运行日志均排除。

Windows PowerShell 命令行构建（先完成自己的签名配置）：

```powershell
cd reference_2/arkforge-dist/ArkForgeApp
.\scripts\build.ps1
```

工程包含构建所需的 ARM64 工具库、编译器运行资源及预打包工作台，常规 HAP 构建可直接使用。SDK 检查器和 ArkUI 转换器的刷新依赖本机 HarmonyOS SDK，来源与条款见 `THIRD_PARTY_NOTICES.md`。

## 平板端使用

首次打开显示欢迎指南。通过 **帮助 → ArkForge 使用介绍与操作指南** 随时查看详细步骤。

- **打开工程**：选择 HarmonyOS 工程 ZIP，检查资源管理器中的源码和模块配置。
- **GitHub 与 Git**：点击左下角账户登录 GitHub，配置提交姓名邮箱；从指南进入 Git 完整教程，依次克隆、建分支、提交和同步。Git 由本机官方 CLI 执行。
- **Agent**：点击聊天气泡，配置模型服务 HTTPS 地址、模型名和自己的 API Key，描述修改任务。
- **签名与安装**：配置华为账号自动签名或手动签名，保存代码，点击三角按钮编译；构建成功后在弹窗中确认安装。
- **调试**：通过运行日志和本机调试入口检查结果，返回编辑器继续修改。

## 当前实现范围

本机编译路径已用于真实工程的 ArkTS 源码编译、签名和安装。支持范围取决于工程依赖、SDK 检查及资源/原生依赖准备状态；任意工程全量 Hvigor/NDK 构建仍需继续完善。

本机终端恢复为已验证的官方 Git 转发方案；`cd`、`pwd`、`ls`、`clear` 由工程文件接口适配。完整交互 Shell/PTY 正在适配中，当前入口使用 Git 转发路径。

## 修改工作台前端

主工作台来自 Code-OSS；当前上游版本和提交保存在 `entry/src/main/resources/rawfile/codeoss-manifest.json`。ArkForge 扩展源码位于 `workbench/extension`，原生集成补丁位于 `workbench/patches`。

完整工作台源码按上述提交恢复到仓库根目录 `_vendor/vscode`，依照上游构建说明安装依赖，并按当前源码基线应用 `workbench/patches`。现有 Web 构建命令为 `node build/next/index.ts bundle --target web --minify --nls --out out-arkforge-web`（Node 24）。随后在 ArkForgeApp 目录执行 `python scripts/package_codeoss.py`，再构建 HAP。

## 第三方组件

许可说明见工程内 `THIRD_PARTY_NOTICES.md`、`licenses`、`third-party` 和各上游源码目录。Git、BusyBox、curl 的对应官方源代码归档保存在 `_vendor`，平台适配和构建脚本位于 `native`。工作台 ZIP 内保留 Code-OSS 的 MIT 许可和第三方声明。
