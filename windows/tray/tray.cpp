// Bangla Keyboard — Bijoy-style tray switcher (standalone, no IME registration).
//
// A background app with a system-tray icon + popup menu (Unicode / Bijoy Classic /
// English), exactly like the classic Bijoy switcher. In "Unicode" mode a global
// low-level keyboard hook runs each keystroke through the shared engine
// (../engine/) and injects the resulting Bangla with SendInput, so it works in
// ANY app (Notepad, browsers, chat) without registering a TSF IME or admin rights.
//
// Toggle with the tray menu, a left-click on the icon, or Ctrl+Alt+B.
//
// Build (GUI app, no console):
//   g++ -std=c++17 -O2 -static -mwindows -finput-charset=UTF-8 \
//       tray.cpp ../engine/engine.cpp -o ../dist/bangla-tray.exe \
//       -lgdi32 -luser32 -lshell32
//
// NOTE on the injection model: with no IME composition we keep the current
// syllable as editable on-screen text (`g_shown`) and realise each engine Result
// by back-spacing what we showed and retyping — this is how Bijoy reorders a
// prebase vowel / reph. One backspace == one code point in most editors (Bengali
// is all BMP here). Apps with grapheme-cluster backspace may differ; that's the
// known trade-off of the hook approach vs the TSF IME in ../tsf/.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <vector>
#include "../engine/engine.h"

using bangla::Engine;
using bangla::Result;
using bangla::ResultKind;
using bangla::Str;

// ---- state -----------------------------------------------------------------
static HINSTANCE       g_hInst;
static HWND            g_hWnd;
static HHOOK           g_hook;
static Engine          g_engine;
static Str             g_shown;            // current editable on-screen syllable
static bool            g_bangla = false;   // false = English passthrough
static NOTIFYICONDATAW g_nid    = {};
static HICON           g_iconBn = nullptr; // green "অ"
static HICON           g_iconEn = nullptr; // red   "E"

#define WM_TRAY     (WM_APP + 1)
#define ID_UNICODE  1001
#define ID_CLASSIC  1002
#define ID_ENGLISH  1003
#define ID_ABOUT    1010
#define ID_EXIT     1011
#define HOTKEY_TOGGLE 1

// ---- key injection ---------------------------------------------------------
static void sendBackspaces(int n) {
    if (n <= 0) return;
    std::vector<INPUT> in;
    in.reserve(n * 2);
    for (int i = 0; i < n; ++i) {
        INPUT d = {}; d.type = INPUT_KEYBOARD; d.ki.wVk = VK_BACK; in.push_back(d);
        INPUT u = {}; u.type = INPUT_KEYBOARD; u.ki.wVk = VK_BACK; u.ki.dwFlags = KEYEVENTF_KEYUP; in.push_back(u);
    }
    SendInput((UINT)in.size(), in.data(), sizeof(INPUT));
}
static void sendUnicode(const Str& s) {
    if (s.empty()) return;
    std::vector<INPUT> in;
    in.reserve(s.size() * 2);
    for (char16_t c : s) {
        INPUT d = {}; d.type = INPUT_KEYBOARD; d.ki.wScan = c; d.ki.dwFlags = KEYEVENTF_UNICODE; in.push_back(d);
        INPUT u = {}; u.type = INPUT_KEYBOARD; u.ki.wScan = c; u.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP; in.push_back(u);
    }
    SendInput((UINT)in.size(), in.data(), sizeof(INPUT));
}

// Realise one engine Result by editing the on-screen syllable.
static void realize(const Result& r) {
    switch (r.kind) {
        case ResultKind::Ignore: return;
        case ResultKind::Compose:
            sendBackspaces((int)g_shown.size()); sendUnicode(r.text); g_shown = r.text; break;
        case ResultKind::Commit:
            sendBackspaces((int)g_shown.size()); sendUnicode(r.text); g_shown.clear(); break;
        case ResultKind::CommitThenCompose:
            sendBackspaces((int)g_shown.size()); sendUnicode(r.text); sendUnicode(r.comp); g_shown = r.comp; break;
    }
}

// Freeze the current syllable as final text (space / Enter / focus loss / chord).
static void flushEngine() {
    Str fin = g_engine.flush();
    if (fin != g_shown) { sendBackspaces((int)g_shown.size()); sendUnicode(fin); }
    g_shown.clear();
}

// ---- the global keyboard hook ----------------------------------------------
static LRESULT CALLBACK hookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && g_bangla) {
        auto* k = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        bool injected = (k->flags & LLKHF_INJECTED) != 0;     // skip our own SendInput
        if (!injected && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
            auto down = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
            bool ctrl  = down(VK_CONTROL), alt = down(VK_MENU);
            bool win   = down(VK_LWIN) || down(VK_RWIN);
            bool shift = down(VK_SHIFT);
            unsigned scan = k->scanCode;

            if (ctrl || alt || win) {
                flushEngine();                                // let the shortcut through
            } else if (g_engine.wouldHandle(scan)) {
                realize(g_engine.process(scan, shift));
                return 1;                                     // swallow the original key
            } else {
                flushEngine();                                // space/Enter/Tab/etc.
            }
        }
    }
    return CallNextHookEx(g_hook, code, wParam, lParam);
}

// ---- tray icon / menu ------------------------------------------------------
static HICON makeIcon(const wchar_t* txt, COLORREF bg, COLORREF fg) {
    const int sz = 32;
    HDC sdc = GetDC(nullptr);
    HDC dc  = CreateCompatibleDC(sdc);
    HBITMAP color = CreateCompatibleBitmap(sdc, sz, sz);
    HBITMAP mask  = CreateBitmap(sz, sz, 1, 1, nullptr);
    HGDIOBJ ob = SelectObject(dc, color);
    RECT r = {0, 0, sz, sz};
    HBRUSH b = CreateSolidBrush(bg); FillRect(dc, &r, b); DeleteObject(b);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, fg);
    HFONT f = CreateFontW(26, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                          DEFAULT_PITCH, L"Nirmala UI");
    HGDIOBJ of = SelectObject(dc, f);
    DrawTextW(dc, txt, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of); DeleteObject(f);
    SelectObject(dc, ob);
    ICONINFO ii = {}; ii.fIcon = TRUE; ii.hbmColor = color; ii.hbmMask = mask;
    HICON ic = CreateIconIndirect(&ii);
    DeleteObject(color); DeleteObject(mask); DeleteDC(dc); ReleaseDC(nullptr, sdc);
    return ic;
}

static void updateTray() {
    g_nid.hIcon = g_bangla ? g_iconBn : g_iconEn;
    lstrcpynW(g_nid.szTip, g_bangla ? L"Bangla Keyboard — Unicode (Ctrl+Alt+B = English)"
                                    : L"Bangla Keyboard — English (Ctrl+Alt+B = Bangla)",
              ARRAYSIZE(g_nid.szTip));
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void setMode(bool bangla) {
    if (g_bangla == bangla) return;
    if (!bangla) flushEngine();   // leaving Bangla: finalise anything pending
    g_engine.reset(); g_shown.clear();
    g_bangla = bangla;
    updateTray();
}

static void showMenu() {
    POINT pt; GetCursorPos(&pt);
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING | (g_bangla ? MF_CHECKED : 0), ID_UNICODE, L"Unicode");
    AppendMenuW(m, MF_STRING | MF_GRAYED, ID_CLASSIC, L"Bijoy Classic  (coming soon)");
    AppendMenuW(m, MF_STRING | (!g_bangla ? MF_CHECKED : 0), ID_ENGLISH, L"English");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_ABOUT, L"About");
    AppendMenuW(m, MF_STRING, ID_EXIT, L"Close");
    SetForegroundWindow(g_hWnd);   // so the menu dismisses correctly
    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hWnd, nullptr);
    DestroyMenu(m);
}

static LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_TRAY:
            if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU) showMenu();
            else if (LOWORD(lp) == WM_LBUTTONUP) setMode(!g_bangla);   // left-click toggles
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case ID_UNICODE: setMode(true);  break;
                case ID_ENGLISH: setMode(false); break;
                case ID_ABOUT:
                    MessageBoxW(h,
                        L"Bangla Keyboard — tray switcher\n\n"
                        L"Unicode = type Bangla (fixed/Bijoy-style layout).\n"
                        L"English = normal typing.\n\n"
                        L"Switch: click the tray icon, this menu, or Ctrl+Alt+B.\n"
                        L"MIT licensed. No Bijoy trademark; emits standard Unicode.",
                        L"About Bangla Keyboard", MB_OK | MB_ICONINFORMATION);
                    break;
                case ID_EXIT: DestroyWindow(h); break;
            }
            return 0;
        case WM_HOTKEY:
            if (wp == HOTKEY_TOGGLE) setMode(!g_bangla);
            return 0;
        case WM_DESTROY:
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    g_hInst = hInst;

    // single instance
    HANDLE once = CreateMutexW(nullptr, TRUE, L"BanglaKeyboardTraySingleton");
    if (once && GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"Bangla Keyboard is already running (see the tray).",
                    L"Bangla Keyboard", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    WNDCLASSW wc = {};
    wc.lpfnWndProc = wndProc; wc.hInstance = hInst; wc.lpszClassName = L"BanglaKeyboardTray";
    RegisterClassW(&wc);
    g_hWnd = CreateWindowW(wc.lpszClassName, L"Bangla Keyboard", 0, 0, 0, 0, 0,
                           HWND_MESSAGE, nullptr, hInst, nullptr); // message-only window

    g_iconBn = makeIcon(L"অ", RGB(34, 153, 84),  RGB(255, 255, 255)); // green
    g_iconEn = makeIcon(L"E", RGB(192, 57, 43),   RGB(255, 255, 255)); // red

    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd   = g_hWnd;
    g_nid.uID    = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon  = g_iconEn;
    lstrcpynW(g_nid.szTip, L"Bangla Keyboard — English (Ctrl+Alt+B = Bangla)", ARRAYSIZE(g_nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, hookProc, hInst, 0);
    RegisterHotKey(g_hWnd, HOTKEY_TOGGLE, MOD_CONTROL | MOD_ALT, 'B');

    // first-run hint
    g_nid.uFlags |= NIF_INFO;
    lstrcpynW(g_nid.szInfoTitle, L"Bangla Keyboard", ARRAYSIZE(g_nid.szInfoTitle));
    lstrcpynW(g_nid.szInfo, L"Running in the tray. Click the icon or press Ctrl+Alt+B to type Bangla.",
              ARRAYSIZE(g_nid.szInfo));
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    g_nid.uFlags &= ~NIF_INFO;

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }

    if (g_hook) UnhookWindowsHookEx(g_hook);
    UnregisterHotKey(g_hWnd, HOTKEY_TOGGLE);
    if (g_iconBn) DestroyIcon(g_iconBn);
    if (g_iconEn) DestroyIcon(g_iconEn);
    if (once) { ReleaseMutex(once); CloseHandle(once); }
    return 0;
}
