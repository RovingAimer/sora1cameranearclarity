# 相机反虚化

当前开发候选 **v0.2.0**：新增游戏前台 F8 开关、可配置键码及 `CameraNearClarity.ini` 内容热更新。候选等待用户实机验收，尚未正式发布；下面的公开下载仍为已验收的 v0.1.0。

候选配置放在 `mods/CameraNearClarity/CameraNearClarity.ini`：

```ini
[CameraNearClarity]
Enabled=1
ToggleKey=0x77
```

F8 在开启与恢复原生近淡之间切换，按住不会连续切换，仅游戏进程窗口在前台时响应。`ToggleKey=0` 禁用快捷键。快捷键仅改变本次运行状态、不写回文件；配置内容改变后约 500 ms 应用其中的 Enabled 和 ToggleKey，重启也按 Enabled 恢复。完整按键代码、配置优先级、安装及测试步骤见 [快捷键代码说明.txt](快捷键代码说明.txt)。

候选安装 ZIP 只含 DLL、模块声明和新增 INI；不含加载器。关闭时恢复原生近淡计算，视觉跟随游戏更新；远距离淡出、剧情透明度、景深和裁切面继续由游戏处理。


《空之轨迹 the 1st》人物近距离淡出调整 Mod。相机靠近人物时让人物保持显示，保留远距离淡出和剧情/整体透明度；景深和裁切面沿用游戏原逻辑。相机反虚化与通用加载器 0.2.0 的组合已由用户实机确认正常。

## 下载与安装

| 下载 | 用途 |
| --- | --- |
| [CameraNearClarity-v0.1.0.zip](https://github.com/RovingAimer/sora1cameranearclarity/releases/download/v0.1.0/CameraNearClarity-v0.1.0.zip) | 安装包：`mods/CameraNearClarity/CameraNearClarity.dll` 和同目录 `sora_mod.ini`。 |
| [CameraNearClarity-v0.1.0-source.zip](https://github.com/RovingAimer/sora1cameranearclarity/releases/download/v0.1.0/CameraNearClarity-v0.1.0-source.zip) | 完整源码、构建方法、来源记录与 MIT 许可，供查验或自行编译。 |
| [通用加载器 v0.2.0](https://github.com/RovingAimer/sora1looseload/releases/tag/v0.2.0) | 必装依赖；统一加载独立模块。 |

退出游戏，备份将覆盖的文件。先安装加载器，再将相机安装 ZIP 解压合并到 `sora_1st.exe` 所在目录，保留目录层级。恢复时退出游戏，恢复备份；安装前不存在的 `mods/CameraNearClarity` 文件可移除。详细步骤见 [测试步骤.md](测试步骤.md)。

只支持 `sora_1st.exe`，大小 `11862528`，SHA256 `9585d3d993874d65156aba633c00b8a25faab3afde8953a8a183c80fb8f657ac`；模块会校验 EXE 和原指令。

实现通过原生人物路径让近距离透明度进入游戏已有的恢复分支，同时保留远距离淡出和剧情/整体透明度。影响范围覆盖共享人物路径及其子对象；特殊场景 bit `0x10` 原本将近距离淡出归零，该路径会被绕过。完整实现见 [BUILDING.md](BUILDING.md)。

DLL 会在游戏进程内以当前玩家账户权限运行代码，可能影响游戏稳定性、文件和数据。只使用可信来源。源码、构建方法和 MIT 许可可供查验或自行编译；改编来源见 [src/ADAPTED_SOURCE.json](src/ADAPTED_SOURCE.json)。附件 SHA256 与源码提交记录在发布页底部。
