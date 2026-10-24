# 相机反虚化 v0.2.1

《空之轨迹 the 1st》人物近距离淡出调整 Mod。相机靠近人物时让人物保持显示，保留远距离淡出和剧情整体透明度；景深和裁切面沿用游戏原逻辑。

v0.2.1 移除额外 EXE 白名单，并改用特征定位。v0.2.0 新增默认 **F8** 开关、可配置快捷键和配置热更新。快捷键切换与配置热更新已由用户实机确认正常。

## 下载与安装

| 下载 | 用途 |
| --- | --- |
| [CameraNearClarity-v0.2.1.zip](https://github.com/RovingAimer/sora1cameranearclarity/releases/download/v0.2.1/CameraNearClarity-v0.2.1.zip) | 安装包：相机模块 DLL、加载声明和默认配置，共三个文件。 |
| [CameraNearClarity-v0.2.1-source.zip](https://github.com/RovingAimer/sora1cameranearclarity/releases/download/v0.2.1/CameraNearClarity-v0.2.1-source.zip) | 完整源码、构建工具、安装说明、键码说明、来源记录与 MIT 许可。无需放进游戏。 |
| [快捷键代码说明.txt](快捷键代码说明.txt) | 开关、改键、配置生效规则及常用虚拟键码。发布附件也提供独立文本。 |
| [通用加载器 v0.2.0](https://github.com/RovingAimer/sora1looseload/releases/tag/v0.2.0) | 必装依赖。已有该版本时无需替换加载器。 |

退出游戏，将现有 `mods/CameraNearClarity` 文件夹备份到游戏目录之外。首次安装先按加载器说明安装依赖，再将本 Mod 安装 ZIP 合并到 `sora_1st.exe` 所在目录，保留目录层级：

```text
mods/CameraNearClarity/
├─ CameraNearClarity.dll
├─ sora_mod.ini
└─ CameraNearClarity.ini
```

已有 v0.1.0 时，替换本模块 DLL 和声明并添加新 INI；已有自定义 INI 时保留自己的配置，不覆盖为默认值。安装包不包含 `xinput1_4.dll`，不需要修改加载器或其他 Mod 文件夹。详细安装、检查与回退见 [测试步骤.md](测试步骤.md)。

## 开关与配置

配置文件为 `mods/CameraNearClarity/CameraNearClarity.ini`：

```ini
[CameraNearClarity]
Enabled=1
ToggleKey=0x77
```

- `Enabled=1` 开启反虚化；`0` 关闭并恢复原生近距离淡出。
- 默认 `ToggleKey=0x77` 为 F8，`0x78` 为 F9，`0` 禁用快捷键。仅游戏进程窗口在前台时响应，按下切换一次，长按不重复。不拦截按键本身的游戏或其他 Mod 功能。
- 快捷键只改变本次运行状态，不写回配置。重启按 INI 的 Enabled 恢复。
- 保存不同的有效配置内容后约 0.5 秒同时应用 Enabled 和 ToggleKey。因此只改键位也会重新应用文件中的 Enabled；内容完全相同的保存不重置临时开关。
- 保留两个键；无效或暂时缺失的配置沿用上次状态，首次没有有效配置时默认开启、F8 切换。完整编码要求与键码表见 [快捷键代码说明.txt](快捷键代码说明.txt)。

## 支持范围

不再按 EXE 文件名、文件大小或整文件 SHA256 设置白名单。采用与 Hinkiii 原版加载器相同的掩码特征扫描方式定位实际代码；找不到对应特征时跳过相应接入。其他版本是否可用取决于目标代码及对象布局，不要求整份 EXE 与某一版本完全相同。

v0.2.1 的兼容性调整仅做静态验证；原有 F8 和配置热更新功能已在 v0.2.0 实机验收。

影响范围覆盖共享人物路径及其子对象。特殊场景 bit `0x10` 原本将近距离淡出归零，该路径会被绕过。遇到改写同一指令或周围校验区的其他补丁时拒绝继续修改；热键冲突可换键。实现、状态与构建见 [BUILDING.md](BUILDING.md)。

代码以 MIT 开源，改编来源见 [src/ADAPTED_SOURCE.json](src/ADAPTED_SOURCE.json)。DLL 在游戏进程内执行，请从可信来源取得，或按构建说明自行编译。
