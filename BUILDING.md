# 相机反虚化 0.1.0：实现与构建

## 环境与命令

需要 Windows x64、Python 3.9+、Visual Studio 2022 C++ x64 工具及 Windows SDK。在源码根目录运行：

```powershell
python build.py
```

若 SDK 位于自定义目录，可指定 SDK 根目录：

```powershell
python build.py --sdk-root "E:/path/to/sdk"
```

SDK 根目录包含 `Include/10.x` 和 `Lib/10.x`，或工作区使用的 `um/ucrt` 目录。原生 DLL 输出到 `build/native/CameraNearClarity.dll`，报告及临时文件位于 `build/native/`。构建使用静态 CRT、`/O2 /MT /W4 /WX`，链接 `/Brepro`。源码可独立构建。

源码提交干净后运行打包命令：

```powershell
python tools/package_native.py
```

输出写入 `build/packages/`，包含 `CameraNearClarity-v0.1.0.zip`、`CameraNearClarity-v0.1.0-source.zip`、`manifest.json` 和 `native-build-report.json`。安装 ZIP 仅含 `mods/CameraNearClarity/CameraNearClarity.dll` 与同目录 `sora_mod.ini`；源码 ZIP 含当前源码、`SOURCE_MANIFEST.json`、MIT 许可和补丁上下文记录。

## 当前 DLL 与来源

v0.1.0 安装包保留已由用户实机确认的 DLL 字节，SHA256 为 `001480f09d9381041fd411629fb7eadbf81db394231669a645c29916d32064f6`。`build/native/accepted-report.json` 是原编译报告；它记录安装 DLL 的实际编译输入和工具链；`SOURCE_MANIFEST.json` 记录正式文档和打包工具所在的当前源码快照。打包清单同时记录当前源码提交、原生报告及其 SHA256、实际编译输入和 `binary_reused`。文档与打包工具变更不需要重编译。

自行运行 `python build.py` 会生成新的 `build/native/report.json`；报告记录实际 C++ 输入、工具链与命令。打包时脚本核对 DLL 与原生报告、源码成员和来源清单。模块入口为 `extern "C" DWORD WINAPI SoraCameraInitialize(void)`；`DllMain` 不做初始化。

## 原生修改范围

只支持 `sora_1st.exe`，大小 `11862528`，SHA256 `9585d3d993874d65156aba633c00b8a25faab3afde8953a8a183c80fb8f657ac`。在 RVA `0x1C9616` 以 16 位 CAS 将 `72 37` 改为 `EB 37`，跳入游戏已有的近淡恢复块。运行前验证 152 字节原上下文。远距离因子 `+0x2440` 与剧情/整体 alpha `+0x21BC` 保留；共享人物路径及子对象会受影响。该修改绕过特殊场景 bit `0x10` 将近距离透明度归零的路径。状态值由模块定义，加载器只记录原始 32 位返回值；正常完整值为 `0x1F`。

模块只修改人物近距离因子 `+0x243C`；远距离因子 `+0x2440` 与剧情/整体 alpha `+0x21BC` 继续按游戏原逻辑参与最终透明度。景深和裁切面沿用游戏原逻辑。

## 安装与回退

[通用加载器 v0.2.0](https://github.com/RovingAimer/sora1looseload/releases/tag/v0.2.0) 与此模块已由用户实机确认正常。安装 ZIP 的模块路径是 `mods/CameraNearClarity/`，包含 DLL 和声明文件；安装与回退步骤见 [测试步骤.md](测试步骤.md)。内存修改随游戏进程结束消失。

## 来源与许可

代码采用 MIT，完整许可见 [LICENSE](LICENSE)。适配的 DialogueOutline 代码来源与原文件 SHA256 记录在 [src/ADAPTED_SOURCE.json](src/ADAPTED_SOURCE.json)；补丁目标字节与上下文位于 `src/`。源码仓库为 [RovingAimer/sora1cameranearclarity](https://github.com/RovingAimer/sora1cameranearclarity)，正式安装与完整源码附件见 [v0.1.0](https://github.com/RovingAimer/sora1cameranearclarity/releases/tag/v0.1.0)。
