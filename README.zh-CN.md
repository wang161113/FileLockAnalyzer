# FileUsageAnalyzer

[![English](https://img.shields.io/badge/English-373737?style=for-the-badge&logo=none&logoColor=white)](./README.md) [![中文](https://img.shields.io/badge/%E4%B8%AD%E6%96%87-0078D4?style=for-the-badge&logo=none&logoColor=white)](#)

**文件占用分析工具** — 基于 C++17 / MFC 的 Windows 桌面工具，用于识别「哪个进程占用了这个文件/文件夹」，并提供解除占用、卸载 DLL、强制关闭句柄等操作。支持资源管理器右键菜单集成。

![logo](FileUsageAnalyzer_256.png)

## 功能特性

| 能力 | 说明 |
|---|---|
| 占用检测（首选） | Windows Restart Manager API (`Rstrtmgr.lib`) — 稳定、由系统托管 |
| 占用检测（兜底） | `NtQuerySystemInformation` SystemExtendedHandleInformation (Class 64) — 跨进程句柄表扫描，覆盖 Restart Manager 看不到的 Kernel handle 类型 |
| 解除占用 1 | 结束占用进程 (`TerminateProcess`) |
| 解除占用 2 | 强制远程 `DuplicateHandle(... DUPLICATE_CLOSE_SOURCE)` 关闭目标进程内的文件句柄 |
| 解除占用 3 | 远程线程注入 `FreeLibrary` 卸载占用 DLL |
| 删除文件 | 先释放句柄再 `DeleteFile`；不行就 `MoveFileEx` 重启时删除 |
| Shell 集成 | 在 `Directory` / `Directory\Background` / `AllFilesystemObjects` 三处写注册表实现右键菜单，自动提权注册 |
| 单实例 | 专属窗口类 + `Local\` Mutex + `QueryFullProcessImageName` 进程模块名校验，防止第三方同名类假阳性；右键菜单新进程通过 `WM_COPYDATA` 把目标路径交给老实例并拉到前台 |
| 托盘 | 关闭按钮默认最小化到托盘，托盘右键菜单「退出」才真正关闭；支持气泡 Tip 国际化 |
| 国际化 | 中/英双语一键切换，跟随系统默认 UI 语言，选择写入 `HKCU` 持久化 |
| UI 布局 | 运行时 `WM_SIZE + DeferWindowPos Anchor 机制无闪烁拉伸，拦截 Enter/Esc |
| x64 兼容 | 严格对齐的 `SYSTEM_HANDLE_INFORMATION_EX` 自定义结构体 + 缓冲区上界检查，避免 x64 下 `ULONG_PTR` 对齐错误导致的缓冲区溢出 |

## 系统需求

- Windows 10 / 11（x86 或 x64）
- Visual Studio 2022（v143 工具集，C++17）
- **强制句柄关闭 / 跨进程操作必须以管理员权限运行**

## 编译

```bat
:: x64 Release
msbuild FileUsageAnalyzer.sln /t:Rebuild /p:Configuration=Release /p:Platform=x64

:: x86 Release
msbuild FileUsageAnalyzer.sln /t:Rebuild /p:Configuration=Release /p:Platform=Win32
```

产物路径：
- x64：`x64\Release\FileUsageAnalyzer.exe`
- x86：`Release\FileUsageAnalyzer.exe`

项目在 VS 工程里强制开启 `/utf-8` 编译选项，中文源码无需额外设置。

## 使用

### 1. 普通模式
直接双击运行 → 拖放文件/文件夹到窗口，或用「文件...」「目录...」按钮选路径 → 点「开始分析」。

### 2. 管理员提权
涉及「强制关闭句柄」「卸载模块」「删除文件」「注册右键菜单」等操作时，会弹出 `是否以管理员身份重新启动？` 确认框，确认后通过 `ShellExecute("runas")` UAC 提权重启自己，原实例干净关闭。

### 3. 资源管理器右键菜单
点击界面里的「注册右键菜单」，确认 UAC 后，即可在任意文件、文件夹、文件夹空白处右键看到：
- 中文系统：`使用文件占用分析工具检测`
- 英文系统：`Analyze File Lock`

点击后由单实例机制接管；若已有实例在跑，会把新路径抛给它并拉到前台。

## 命令行参数

| 参数 | 含义 |
|---|---|
| `<文件或目录路径>` | 启动后直接分析该目标 |
| `/shellregister` | 启动后立即执行注册表右键菜单注册（会走单实例白名单，允许提权临时进程通过） |
| `/shellunregister` | 启动后立即撤销右键菜单注册表 |

单实例拦截对 `/shellregister` 和 `/shellunregister` 做了白名单放行，避免提权临时进程被拦截导致注册失败。

## 源码结构

```
├── FileUsageAnalyzer.sln         # Solution
├── FileUsageAnalyzer.vcxproj     # Project
├── FileUsageAnalyzer.rc          # Resources (ICON, dialog, string-table)
├── FileUsageAnalyzer_256.png     # Logo preview
├── FileUsageAnalyzer.ico         # Multi-size ICO: 16/32/48/64/128/256
├── render_logo.py                # Pillow script to build the ICO (Python)
├── FileUsageAnalyzer.h / .cpp    # CWinApp entry; single-instance; elevation; language detection
├── MainDlg.h / .cpp              # Main dialog: UI, i18n, anchor resize, shell register/unregister, tray, operations
├── FileLockDetector.h / .cpp     # Detection engine: Restart Manager + NtQueryInfo dual path
├── stdafx.h                      # PCH (includes #pragma comment for Rstrtmgr.lib etc.)
├── resource.h                    # Resource ID definitions
└── targetver.h
```

### 核心类/函数速查

| 文件 | 关键项 | 作用 |
|---|---|---|
| FileUsageAnalyzer.cpp | `EnsureSingleInstance` | 白名单 + FindWindow/EnumWindows 双保险，权限升级场景允许并存 |
| FileUsageAnalyzer.cpp | `IsOurProcessWindow` | 用 `QueryFullProcessImageName` 校验 HWND 真的是本进程，防假阳性 |
| MainDlg.cpp | `PerformAnalysis` | 路径合法性 → 调 Detector；空结果一定给 MessageBox 反馈，禁止静默 |
| MainDlg.cpp | `OnClose` | `SW_HIDE` 到托盘，避免点关闭就退出 |
| MainDlg.cpp | `RelaunchAsAdmin` | ShellExecute runas + 原进程 EndDialog，不走 WM_CLOSE 避免被 OnClose 截住 |
| MainDlg.cpp | `RegisterShellMenu` | 三处 `HKLM\Software\Classes` 注册表 + SHChangeNotify 三次刷新 |
| FileLockDetector.cpp | `AnalyzeFile / AnalyzeFolder` | 返回 TRUE 只代表分析流程完成，**空结果≠失败**；失败只在入参非法时才返回 FALSE |
| FileLockDetector.cpp | `AnalyzeByNtQueryInfo` | 64 号类句柄表扫描，严格上界检查防溢出 |
| FileLockDetector.cpp | `ForceCloseFileHandle` | 远程 DUPLICATE_CLOSE_SOURCE |
| FileLockDetector.cpp | `UnloadModuleFromProcess` | CreateRemoteThread(FreeLibrary) |

## Logo 设计哲学

Logo 采用「铜绿文件夹后盖 + 钢蓝前盖 + 中央白色"占"字」三段式：前盖后盖的气隙对应文件被锁住需要分析层间关系，与程序核心的「目标文件 ↔ 周边资源」拓扑语义 1:1 映射。

使用 Pillow 脚本 `render_logo.py` 用 LANCZOS 从 256 逐级缩到 16，保证托盘小图仍能识别出文件夹轮廓。

## License

本项目仅用于本地开发者工具场景。
