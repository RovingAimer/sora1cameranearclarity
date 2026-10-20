# 相机反虚化：构建与运行时开关

当前源码为本地 v0.2.0 候选，尚未实机验收或公开发布。已验收的 v0.1.0 是原始近淡调整基线。

## 构建和打包

需要 Windows x64、Python 3.9+、Visual Studio C++ x64 工具及 Windows SDK。在源码目录运行：

```powershell
python build.py
# 若使用独立提取的 Windows SDK：
python build.py --sdk-root "X:/path/to/sdk"
```

SDK 需包含 Include/10.x、Lib/10.x，或工作区的 um/x64 与 ucrt/x64 库布局。脚本使用已安装的 MSVC；编译 core.cpp 和 runtime_control.cpp，采用 C++17、/O2 /MT /W4 /WX /utf-8，链接 /Brepro。只依赖系统 KERNEL32、bcrypt、USER32；USER32 用于前台窗口和按键状态查询。无需修改或重编译通用加载器。

输出 build/native/CameraNearClarity.dll 及 report.json，包含实际编译输入、编译器/SDK、导出与直接依赖。两个导出保持 SoraCameraInitialize、SoraCameraGetStatus，DllMain 不做初始化。构建不执行 DLL，不运行或安装游戏。

源码提交干净后运行 `python tools/package_native.py`。release_plan.json 的 local-candidate 渠道生成安装和完整源码两个候选 ZIP，以及包外 manifest.json、native-build-report.json。安装 ZIP 仅含 mods/CameraNearClarity 下的 DLL、sora_mod.ini 和 CameraNearClarity.ini。完整源码包含构建工具、补丁上下文、键码说明及 MIT 许可。

## 补丁所有权与开关

只接受 sora_1st.exe，大小11862528，SHA256 9585d3d993874d65156aba633c00b8a25faab3afde8953a8a183c80fb8f657ac。初始化核对 RVA 0x1C9613 起152字节上下文、MEM_IMAGE、原指令和对齐条件。地址0x1C9616的两字节为72 37（原生）与EB 37（反虚化）。每次切换重新核对完整上下文中属于本模块当前状态的字节，再使用16位CAS切换、恢复内存保护并刷新指令缓存。

发现其他写入时拒绝覆盖并停止后续切换；保护/缓存处理失败会设置错误位。只改变此处一个opcode，不修改远距离因子+0x2440、剧情alpha+0x21BC、景深或裁切面。实际人物恢复/淡出跟随原生人物更新，关闭不直接重写人物对象缓存。

## 输入、配置及生命周期

核心通过宿主核验后固定模块，再从加载器锁外初始化配置和线程池计时器。25ms计时器只在游戏进程窗口为前台时查询所配置键的GetAsyncKeyState高位；自己记录按下沿，不依赖可能被其他程序消费的低位，也不全局注册或拦截按键。首次获得焦点/修改键位时先采样，长按与带着按键切回游戏不会重复触发。回调以TryAcquireSRWLockExclusive串行化；模块和状态保留至进程结束。

每500ms读取最多8KiB的INI，仅当原始内容变化且整个配置有效时应用。两键均必需，避免保存中缺失某键时误重置；支持UTF-8及BOM UTF-16 LE。非法、暂时无法读或删除时沿用上次状态；首次缺失/非法时默认开启与F8。快捷键不写INI，修改有效配置会按Enabled重新应用，完全相同内容的保存不覆盖临时开关。

配置规则与按键代码见 快捷键代码说明.txt。编码及大小写比较复用DialogueOutline MIT实现，来源记录在src/ADAPTED_SOURCE.json，原版权保留。

## 状态与验收边界

默认开启且成功时0x1F；正常关闭时0x17。PatchActive(0x8)为当前确认启用位，初始化/错误位累计；0x4000表示控制初始化失败，0x8000表示固定模块失败，其他错误位定义见src/CameraNearClarity.h。与其他补丁发生冲突后不再保证当前功能状态。

候选只做工作区构建、静态/组件和包核验；游戏内按键、前台限制、近淡恢复、配置热更新及原有Mod兼容由用户手动验收。当前正式公开版仍为v0.1.0，不把原版本的验收记录当作本候选验收。
