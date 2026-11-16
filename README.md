# MED / AOB 快捷输入

## 使用

双击 `MedAobHotkey.exe`，启动后默认启用。在目标软件的输入框放好光标，按 F1 输入 `MED`，或按 F2 输入 `AOB`；约 500 毫秒后发送 Ctrl + 空格。不附加回车、逗号或空格。按“暂停”可恢复原 F1/F2 功能；最小化进入系统托盘，关闭窗口或点击“退出”则结束程序。

本工具默认普通权限运行。如果目标程序以管理员权限运行，本工具也需要以管理员权限启动。目标软件是否接受模拟输入，以及 Ctrl + 空格是否执行预期操作，须在目标软件内验证。若系统或输入法占用快捷键，软件会显示注册失败的按键和错误码。

## 构建

源码是 `main.cpp`，使用 Win32 API，无 .NET、Python 或 Electron。目标是 Windows 7 及更新版本的 x86 程序。Windows 上准备 [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) 的工具链（将 `bin` 加入 PATH）后运行 `build.bat`。也可从 macOS 使用同一工具链：

```sh
i686-w64-mingw32-clang++ -std=c++11 -O2 -fno-builtin -fno-exceptions -fno-rtti -fno-stack-protector -nostdlib -mwindows -Wl,-e,_EntryPoint@0 -Wl,--subsystem,windows:6.01 main.cpp -o MedAobHotkey.exe -lshell32 -luser32 -lgdi32 -lkernel32
```

程序不调用编译器运行库，使用自定义入口和 `-nostdlib` 链接；Windows 系统 DLL 仍由操作系统提供。构建后应检查 PE Machine 为 `I386`，导入表无第三方 DLL 或 UCRT。500 毫秒是定时器目标值，系统繁忙时可能稍晚。

## Windows 验收清单

1. 在普通文本框测试 F1/F2 的文本、顺序及约 500 毫秒延迟，并确认没有额外字符。
2. 长按每个快捷键，再在等待期间按另一个快捷键，确认没有重复或交错。
3. 测试暂停、重新启用、托盘菜单、最小化、关闭和重复启动。
4. 在实际目标软件里确认其接受模拟输入，并确认 Ctrl + 空格执行预期操作。

当前在 macOS 开发；Windows 实机测试需另行完成。XP 不在支持范围。
