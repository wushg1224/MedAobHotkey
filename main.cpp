#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <shellapi.h>

extern "C" void* memset(void* destination, int value, size_t count) {
    volatile unsigned char* bytes = static_cast<unsigned char*>(destination);
    for (size_t i = 0; i < count; ++i) bytes[i] = static_cast<unsigned char>(value);
    return destination;
}

namespace {
const wchar_t kClassName[] = L"MedAobHotkeyWindow_82E6983D";
const wchar_t kMutexName[] = L"Local\\MedAobHotkey_82E6983D";
const UINT kTrayId = 1;
const UINT kTimerId = 2;
const UINT kHotkeyPaste = 101;
const UINT kHotkeyF2 = 102;
const UINT kTrayMessage = WM_APP + 1;
const UINT kShowMessage = WM_APP + 2;
UINT g_taskbarCreated = 0;
const int kToggleId = 201;
const int kExitId = 202;
const int kStatusId = 203;
const int kMenuShow = 301;
const int kMenuToggle = 302;
const int kMenuExit = 303;
HWND g_window = NULL;
HWND g_toggle = NULL;
HWND g_status = NULL;
HANDLE g_mutex = NULL;
bool g_enabled = false;
bool g_busy = false;
bool g_trayAdded = false;
HICON g_icon = NULL;

void UpdateStatus() {
    SetWindowTextW(g_status, g_enabled ? (g_busy ? L"状态：执行中" : L"状态：已启用") : L"状态：已暂停");
    SetWindowTextW(g_toggle, g_enabled ? L"暂停" : L"启用");
}

void ShowRegistrationError(const wchar_t* key, DWORD error) {
    wchar_t message[256];
    wsprintfW(message, L"无法注册全局快捷键 %s（错误码 %lu）。\n该按键可能已被其他程序占用。请释放后重试。", key, error);
    MessageBoxW(g_window, message, L"快捷键注册失败", MB_OK | MB_ICONERROR);
}

bool EnableHotkeys() {
    if (g_enabled) return true;
    if (!RegisterHotKey(g_window, kHotkeyPaste, MOD_CONTROL | MOD_NOREPEAT, 'Q')) {
        ShowRegistrationError(L"Ctrl + Q", GetLastError());
        return false;
    }
    if (!RegisterHotKey(g_window, kHotkeyF2, MOD_NOREPEAT, VK_F2)) {
        DWORD error = GetLastError();
        UnregisterHotKey(g_window, kHotkeyPaste);
        ShowRegistrationError(L"F2", error);
        return false;
    }
    g_enabled = true;
    UpdateStatus();
    return true;
}

void DisableHotkeys() {
    if (!g_enabled) return;
    UnregisterHotKey(g_window, kHotkeyPaste);
    UnregisterHotKey(g_window, kHotkeyF2);
    g_enabled = false;
    KillTimer(g_window, kTimerId);
    g_busy = false;
    UpdateStatus();
}

bool SendInputs(INPUT* inputs, UINT count) {
    return SendInput(count, inputs, sizeof(INPUT)) == count;
}

bool SendText(const wchar_t* text) {
    INPUT inputs[8] = {};
    UINT count = 0;
    for (const wchar_t* p = text; *p; ++p) {
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wScan = *p;
        inputs[count].ki.dwFlags = KEYEVENTF_UNICODE;
        ++count;
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wScan = *p;
        inputs[count].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        ++count;
    }
    return SendInputs(inputs, count);
}

bool SendCtrlKey(WORD key) {
    INPUT inputs[4] = {};
    const WORD keys[4] = {VK_CONTROL, key, key, VK_CONTROL};
    for (int i = 0; i < 4; ++i) {
        inputs[i].type = INPUT_KEYBOARD;
        inputs[i].ki.wVk = keys[i];
        if (i >= 2) inputs[i].ki.dwFlags = KEYEVENTF_KEYUP;
    }
    return SendInputs(inputs, 4);
}

void InputError() {
    MessageBoxW(g_window, L"模拟输入未完成。请检查目标窗口是否允许模拟输入，以及权限是否一致。", L"输入失败", MB_OK | MB_ICONERROR);
}

void AddTray() {
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = g_window;
    data.uID = kTrayId;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = kTrayMessage;
    data.hIcon = g_icon;
    lstrcpyW(data.szTip, L"粘贴 / AOB 快捷输入");
    g_trayAdded = Shell_NotifyIconW(g_trayAdded ? NIM_MODIFY : NIM_ADD, &data) != FALSE;
    if (g_trayAdded) {
        data.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data);
    }
}

void ShowWindowNow() {
    ShowWindow(g_window, SW_RESTORE);
    SetForegroundWindow(g_window);
}

void ShowTrayMenu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, kMenuShow, L"显示窗口");
    AppendMenuW(menu, MF_STRING, kMenuToggle, g_enabled ? L"暂停" : L"启用");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"退出");
    POINT point;
    GetCursorPos(&point);
    SetForegroundWindow(g_window);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, point.x, point.y, 0, g_window, NULL);
    PostMessageW(g_window, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == g_taskbarCreated) {
        g_trayAdded = false;
        AddTray();
        return 0;
    }
    switch (message) {
    case WM_CREATE: {
        g_window = window;
        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HWND title = CreateWindowW(L"STATIC", L"Ctrl+Q：粘贴，1 秒后按 Ctrl+空格\nF2：输入 AOB，1 秒后按 Ctrl + 空格", WS_CHILD | WS_VISIBLE, 20, 20, 330, 48, window, NULL, NULL, NULL);
        g_status = CreateWindowW(L"STATIC", L"状态：已暂停", WS_CHILD | WS_VISIBLE, 20, 78, 300, 25, window, reinterpret_cast<HMENU>(kStatusId), NULL, NULL);
        g_toggle = CreateWindowW(L"BUTTON", L"启用", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 20, 112, 150, 34, window, reinterpret_cast<HMENU>(kToggleId), NULL, NULL);
        HWND exitButton = CreateWindowW(L"BUTTON", L"退出", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 190, 112, 150, 34, window, reinterpret_cast<HMENU>(kExitId), NULL, NULL);
        SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(g_status, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(g_toggle, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(exitButton, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        AddTray();
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case kToggleId: case kMenuToggle:
            if (g_enabled) DisableHotkeys(); else EnableHotkeys();
            return 0;
        case kExitId: case kMenuExit:
            DestroyWindow(window);
            return 0;
        case kMenuShow:
            ShowWindowNow();
            return 0;
        }
        break;
    case WM_HOTKEY:
        if (!g_enabled || g_busy) return 0;
        if (wparam != kHotkeyPaste && wparam != kHotkeyF2) return 0;
        g_busy = true;
        UpdateStatus();
        if (!(wparam == kHotkeyPaste ? SendCtrlKey('V') : SendText(L"AOB"))) {
            g_busy = false;
            UpdateStatus();
            InputError();
        } else if (!SetTimer(window, kTimerId, 1000, NULL)) {
            g_busy = false;
            UpdateStatus();
            MessageBoxW(window, L"无法启动 1 秒定时器。", L"定时器错误", MB_OK | MB_ICONERROR);
        }
        return 0;
    case WM_TIMER:
        if (wparam == kTimerId) {
            KillTimer(window, kTimerId);
            bool ok = g_enabled && SendCtrlKey(VK_SPACE);
            // Discard triggers received while this action was running.
            MSG pending;
            while (PeekMessageW(&pending, window, WM_HOTKEY, WM_HOTKEY, PM_REMOVE)) {}
            g_busy = false;
            UpdateStatus();
            if (g_enabled && !ok) InputError();
            return 0;
        }
        break;
    case WM_SIZE:
        if (wparam == SIZE_MINIMIZED) ShowWindow(window, SW_HIDE);
        return 0;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY: {
        DisableHotkeys();
        if (g_trayAdded) {
            NOTIFYICONDATAW data = {};
            data.cbSize = sizeof(data);
            data.hWnd = window;
            data.uID = kTrayId;
            Shell_NotifyIconW(NIM_DELETE, &data);
        }
        PostQuitMessage(0);
        return 0;
    }
    case WM_APP + 2:
        ShowWindowNow();
        return 0;
    case WM_APP + 1:
        if (LOWORD(lparam) == WM_CONTEXTMENU || LOWORD(lparam) == WM_RBUTTONUP) ShowTrayMenu();
        else if (LOWORD(lparam) == WM_LBUTTONDBLCLK || LOWORD(lparam) == NIN_SELECT) ShowWindowNow();
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    g_mutex = CreateMutexW(NULL, FALSE, kMutexName);
    if (!g_mutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(kClassName, NULL);
        if (existing) PostMessageW(existing, kShowMessage, 0, 0);
        CloseHandle(g_mutex);
        return 0;
    }
    g_icon = LoadIconW(NULL, IDI_APPLICATION);
    WNDCLASSEXW cls = {};
    cls.cbSize = sizeof(cls);
    cls.hInstance = instance;
    cls.lpfnWndProc = WindowProc;
    cls.lpszClassName = kClassName;
    cls.hIcon = g_icon;
    cls.hIconSm = g_icon;
    cls.hCursor = LoadCursorW(NULL, IDC_ARROW);
    cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&cls)) return 1;
    g_window = CreateWindowExW(0, kClassName, L"粘贴 / AOB 快捷输入", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 380, 195, NULL, NULL, instance, NULL);
    if (!g_window) return 1;
    ShowWindow(g_window, SW_SHOW);
    UpdateWindow(g_window);
    EnableHotkeys();
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CloseHandle(g_mutex);
    return static_cast<int>(msg.wParam);
}

// No compiler C/C++ runtime is needed; the program uses Win32 APIs only.
extern "C" void WINAPI EntryPoint() {
    ExitProcess(static_cast<UINT>(wWinMain(GetModuleHandleW(NULL), NULL, NULL, SW_SHOW)));
}
