// Bangla Keyboard — tray switcher (standalone, no IME registration).
//
// A background app with a system-tray icon + popup menu (Bangla Unicode /
// Bangla Classic / English). A global low-level keyboard hook runs each keystroke
// through the shared engines and injects the result with SendInput, so it types
// Bangla in ANY app without registering a TSF IME or admin.
//
//   Bangla Unicode  — standard Unicode Bangla (../engine/engine.*). Reorders a
//                     prebase vowel / reph, so it injects by back-spacing the live
//                     syllable and retyping (one backspace == one code point).
//   Bangla Classic  — legacy ANSI Bangla encoding (../engine/classic.*), matching
//                     the macOS Classic layout. Visual order, so injection is
//                     append-only (robust). Renders as Bangla only in a legacy
//                     ANSI Bangla font (the one used for old documents).
//   Bangla Phonetic — type Bangla the way it SOUNDS (ami -> আমি, bhalO -> ভালো),
//                     driven by the same keylayout FSM (../engine/phonetic_table.h)
//                     generated from the macOS "Bangla Phonetic.keylayout". Case is
//                     significant (t/T, d/D, o/O …) and comes from the real Shift key
//                     only — Caps Lock alone must stay the plain map.
//   English         — passthrough.
//
// Switch with the tray menu, a left-click on the icon, or Ctrl+Alt+V / Ctrl+Alt+B /
// Ctrl+Alt+P (each toggles its layout <-> English).
//
// Build: g++ -std=c++17 -O2 -static -mwindows -municode -finput-charset=UTF-8 \
//        tray.cpp ../engine/engine.cpp ../engine/classic.cpp -o ../dist/bangla-tray.exe \
//        -lgdi32 -luser32 -lshell32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <vector>
#include "../engine/klengine.h"
#include "../engine/unicode_table.h"
#include "../engine/classic_table.h"
#include "../engine/phonetic_table.h"

#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif

using bangla::KLEngine;
using Str = std::u16string;

enum Mode { MODE_ENGLISH = 0, MODE_UNICODE = 1, MODE_CLASSIC = 2, MODE_PHONETIC = 3 };

// ---- state -----------------------------------------------------------------
static HINSTANCE       g_hInst;
static HWND            g_hWnd;
static HHOOK           g_hook;
static HHOOK           g_mouseHook;   // clicks move the caret -> drop the run
static HWND            g_lastWnd;     // window the current run belongs to
static KLEngine        g_uni(&bangla::unicode_table::TABLE);  // Unicode (keylayout-driven)
static KLEngine        g_classic(&bangla::classic_table::TABLE); // Classic
static KLEngine        g_phon(&bangla::phonetic_table::TABLE);   // Phonetic (type by sound)
static Str             g_committed;        // finalised text of the current run
static Str             g_shown;            // what's on screen now (committed + live preview)
static Mode            g_mode = MODE_ENGLISH;
static Mode            g_lastBangla = MODE_UNICODE;  // what Ctrl+Alt+B / click returns to
static bool            g_classicHintShown = false;
static NOTIFYICONDATAW g_nid    = {};
static HICON           g_icoUni = nullptr; // অ on flag-red circle
static HICON           g_icoCls = nullptr; // ক on brick-red circle
static HICON           g_icoEn  = nullptr; // E on grey circle
static HICON           g_icoPhon = nullptr; // প on teal circle
static HBITMAP         g_bmpUni = nullptr; // 16x16 versions for the popup menu
static HBITMAP         g_bmpCls = nullptr;
static HBITMAP         g_bmpEn  = nullptr;
static HBITMAP         g_bmpPhon = nullptr;
static HBITMAP         g_bmpVoiceBn = nullptr; // green mic (Bangla Voice)
static HBITMAP         g_bmpVoiceEn = nullptr; // blue mic  (English Voice)

#define WM_TRAY       (WM_APP + 1)
#define ID_UNICODE      1001
#define ID_CLASSIC      1002
#define ID_ENGLISH      1003
#define ID_VOICE_BN     1004   // Bangla voice  (-> bangla-voice.exe)
#define ID_VOICE_EN     1005   // English voice
#define ID_VOICE_TOGGLE 1006   // enable/disable the voice companion
#define ID_PHONETIC     1007   // Bangla Phonetic (type by sound)
#define ID_ABOUT        1010
#define ID_EXIT         1011
#define HOTKEY_UNICODE 1   // Ctrl+Alt+V toggles Bangla Unicode  <-> English
#define HOTKEY_CLASSIC 2   // Ctrl+Alt+B toggles Bangla Classic  <-> English
#define HOTKEY_PHONETIC 3  // Ctrl+Alt+P toggles Bangla Phonetic <-> English

// messages posted to the bangla-voice.exe window ("BanglaVoice"); see voicehost.cpp
#define WM_VOICE_BN   (WM_APP + 1)
#define WM_VOICE_EN   (WM_APP + 2)
#define WM_VOICE_QUIT (WM_APP + 3)

// ---- voice companion (bangla-voice.exe, online voice typing) ----------------
// A separate process so a WebView2 hiccup can't disturb the keyboard hook. The
// tray launches it at startup (when enabled), the menu items just message it, and
// it owns its own Ctrl+Alt+S / Ctrl+Alt+D hotkeys + indicator. Enable state lives
// in HKCU so the installer's first-run opt-in and the menu toggle agree.
static bool g_voiceEnabled = true;

static bool readVoiceEnabled() {
    DWORD v = 1, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\BanglaKeyboard", L"VoiceEnabled",
                     RRF_RT_REG_DWORD, nullptr, &v, &sz) == ERROR_SUCCESS)
        return v != 0;
    return true;   // default ON when unset (installer normally writes it)
}
static void voiceExePath(wchar_t* out) {                 // <tray exe dir>\bangla-voice.exe
    GetModuleFileNameW(nullptr, out, MAX_PATH);
    wchar_t* s = wcsrchr(out, L'\\'); if (s) s[1] = 0;
    lstrcatW(out, L"bangla-voice.exe");
}
static HWND voiceWnd() { return FindWindowW(L"BanglaVoice", nullptr); }
static void launchVoice() {
    if (voiceWnd()) return;                              // already running (also self-guards)
    wchar_t exe[MAX_PATH]; voiceExePath(exe);
    if (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES) return;  // not installed
    wchar_t dir[MAX_PATH]; lstrcpynW(dir, exe, MAX_PATH);
    wchar_t* s = wcsrchr(dir, L'\\'); if (s) *s = 0;
    ShellExecuteW(nullptr, L"open", exe, nullptr, dir, SW_HIDE);
}
static void voicePost(UINT msg) {
    HWND v = voiceWnd();
    if (v) PostMessageW(v, msg, 0, 0);
    else   launchVoice();   // wasn't running; start it (use the hotkey once it's up)
}
static void quitVoice() { HWND v = voiceWnd(); if (v) PostMessageW(v, WM_VOICE_QUIT, 0, 0); }

// ---- key injection ---------------------------------------------------------
// Both return FALSE if the OS refused (or only partly accepted) the injection —
// typically UIPI, when the focused window runs at a higher integrity level than we
// do. The caller MUST then drop its picture of the screen: back-spacing later
// against characters that were never inserted would eat the user's real text.
static bool sendBackspaces(int n) {
    if (n <= 0) return true;
    std::vector<INPUT> in; in.reserve(n * 2);
    for (int i = 0; i < n; ++i) {
        INPUT d = {}; d.type = INPUT_KEYBOARD; d.ki.wVk = VK_BACK; in.push_back(d);
        INPUT u = {}; u.type = INPUT_KEYBOARD; u.ki.wVk = VK_BACK; u.ki.dwFlags = KEYEVENTF_KEYUP; in.push_back(u);
    }
    return SendInput((UINT)in.size(), in.data(), sizeof(INPUT)) == in.size();
}
static bool sendUnicode(const Str& s) {
    if (s.empty()) return true;
    std::vector<INPUT> in; in.reserve(s.size() * 2);
    for (char16_t c : s) {
        INPUT d = {}; d.type = INPUT_KEYBOARD; d.ki.wScan = c; d.ki.dwFlags = KEYEVENTF_UNICODE; in.push_back(d);
        INPUT u = {}; u.type = INPUT_KEYBOARD; u.ki.wScan = c; u.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP; in.push_back(u);
    }
    return SendInput((UINT)in.size(), in.data(), sizeof(INPUT)) == in.size();
}

static KLEngine* currentEngine() {
    return g_mode == MODE_CLASSIC  ? &g_classic
         : g_mode == MODE_PHONETIC ? &g_phon
                                   : &g_uni;
}
static void flushCurrent();

// Inject one handled key. The deadkey FSM defers, so we show a LIVE PREVIEW =
// committed text + what the pending deadkey would emit now (peek()). Each key thus
// appears immediately; we back-space only the part of the preview that actually
// changed (e.g. ে -> কে when a prebase vowel reorders).
static void applyKey(KLEngine* eng, unsigned scan, bool shift) {
    // Typing somewhere else than where this run started? Its back-spaces would land
    // at the wrong caret. Checking the foreground window on each key is cheap and —
    // unlike listening for focus EVENTS — can't be fooled by an app that emits focus
    // notifications while you type, which would otherwise break vowel reordering.
    HWND fg = GetForegroundWindow();
    if (fg != g_lastWnd) { flushCurrent(); g_lastWnd = fg; }
    // Cap the in-progress run so a very long burst / stuck auto-repeat can't grow
    // the buffers unbounded (it just commits and starts fresh — no real word is
    // this long, and word boundaries normally flush far sooner).
    if (g_committed.size() > 1024) flushCurrent();
    g_committed += eng->process(scan, shift);
    Str preview = g_committed + eng->peek();
    size_t i = 0;
    while (i < g_shown.size() && i < preview.size() && g_shown[i] == preview[i]) ++i;
    // If the OS refused the injection (UIPI: the focused window is more privileged
    // than us), nothing we think is on screen is actually there. Drop the run rather
    // than carrying a false g_shown into the next key, whose back-spaces would then
    // delete the user's own characters. Typing simply doesn't work in that window.
    if (!sendBackspaces((int)(g_shown.size() - i)) || !sendUnicode(preview.substr(i))) {
        flushCurrent();
        return;
    }
    g_shown = preview;
}

// Finalise the current run (space / Enter / chord / focus loss). The pending
// deadkey is already on screen via the preview, so just reset state + tracking.
static void flushCurrent() {
    if (g_mode != MODE_ENGLISH) currentEngine()->reset();
    g_committed.clear();
    g_shown.clear();
}

// ---- caret / focus changes --------------------------------------------------
// The live preview back-spaces to rewrite the syllable in progress, which is only
// safe while the caret stays where we left it. A mouse click can move it ANYWHERE —
// including inside the SAME control, which no window/focus check would catch — and
// the keyboard hook never sees it. Without this, typing `ক`, clicking elsewhere and
// typing again would back-space at the NEW caret and delete someone else's character.
// (Switching windows is handled separately, by the foreground check in applyKey.)
//
// Dropping the run costs nothing: the pending deadkey has already been shown by the
// preview, so flushCurrent() only resets our tracking — it injects nothing.
static LRESULT CALLBACK mouseProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION) {
        switch (wParam) {                 // any button press can reposition the caret
            case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
            case WM_NCLBUTTONDOWN: case WM_NCRBUTTONDOWN:
                try { flushCurrent(); } catch (...) { g_committed.clear(); g_shown.clear(); }
                break;
            default: break;               // moves/wheel: leave the run alone
        }
    }
    return CallNextHookEx(g_mouseHook, code, wParam, lParam);
}

// ---- the global keyboard hook ----------------------------------------------
static LRESULT CALLBACK hookProc(int code, WPARAM wParam, LPARAM lParam) {
    bool eat = false;
    if (code == HC_ACTION && g_mode != MODE_ENGLISH) {
        // Never let a C++ exception (e.g. bad_alloc) escape into the OS hook
        // dispatch — that would destabilise input for every app on the desktop.
        try {
            auto* k = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
            bool injected = (k->flags & LLKHF_INJECTED) != 0; // skip our own SendInput
            if (!injected && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
                auto down = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
                bool ctrl  = down(VK_CONTROL), alt = down(VK_MENU);
                bool win   = down(VK_LWIN) || down(VK_RWIN);
                bool shift = down(VK_SHIFT);
                unsigned scan = k->scanCode;
                DWORD vk = k->vkCode;
                KLEngine* eng = currentEngine();

                // A modifier key pressed on its OWN (Shift/Ctrl/Alt/Win/Caps) must
                // NOT flush — otherwise pressing Shift before a shifted letter
                // (e.g. ছ) would commit a pending prebase vowel before it can
                // reorder (ে + ছ -> ছে).
                bool isModifier = vk == VK_SHIFT  || vk == VK_LSHIFT   || vk == VK_RSHIFT
                               || vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL
                               || vk == VK_MENU    || vk == VK_LMENU    || vk == VK_RMENU
                               || vk == VK_LWIN    || vk == VK_RWIN     || vk == VK_CAPITAL;

                if (isModifier) {
                    /* pass through, keep the pending deadkey */
                } else if (ctrl || alt || win) {
                    flushCurrent();                           // let the shortcut through
                } else if (eng->wouldHandle(scan)) {
                    applyKey(eng, scan, shift);               // immediate live preview
                    eat = true;                               // swallow the original key
                } else {
                    flushCurrent();                           // space / Enter / Tab / etc.
                }
            }
        } catch (...) {
            g_committed.clear(); g_shown.clear();             // drop state, stay alive
        }
    }
    if (eat) return 1;
    return CallNextHookEx(g_hook, code, wParam, lParam);
}

// ---- tray icon / menu ------------------------------------------------------
// Flag-style icon: green square + colored circle + a letter (matches the macOS
// icons). The circle/letter colours tell the modes apart — Bangla modes use a
// red/brick circle + white letter; English uses a white circle + black E so it
// stands out clearly as the "off" state.
static HICON makeIcon(const wchar_t* txt, COLORREF circle, COLORREF fg) {
    const int sz = 32;
    HDC sdc = GetDC(nullptr);
    HDC dc  = CreateCompatibleDC(sdc);
    HBITMAP color = CreateCompatibleBitmap(sdc, sz, sz);
    HBITMAP mask  = CreateBitmap(sz, sz, 1, 1, nullptr);   // all-0 = fully opaque
    HGDIOBJ ob = SelectObject(dc, color);
    RECT r = {0, 0, sz, sz};
    HBRUSH green = CreateSolidBrush(RGB(0, 106, 78));      // Bangladesh-flag green
    FillRect(dc, &r, green); DeleteObject(green);
    HBRUSH cb = CreateSolidBrush(circle);                  // red / brick / white circle
    HGDIOBJ ob2 = SelectObject(dc, cb);
    HGDIOBJ op  = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, 3, 3, sz - 3, sz - 3);
    SelectObject(dc, op); SelectObject(dc, ob2); DeleteObject(cb);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, fg);
    HFONT f = CreateFontW(22, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
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

// Same flag-style art as the tray icon, drawn into a 16x16 HBITMAP for the popup
// menu (menus take a bitmap via MIIM_BITMAP, not an HICON).
static HBITMAP makeMenuBitmap(const wchar_t* txt, COLORREF circle, COLORREF fg) {
    const int sz = 16;
    HDC sdc = GetDC(nullptr);
    HDC dc  = CreateCompatibleDC(sdc);
    HBITMAP bmp = CreateCompatibleBitmap(sdc, sz, sz);
    HGDIOBJ ob = SelectObject(dc, bmp);
    RECT r = {0, 0, sz, sz};
    HBRUSH green = CreateSolidBrush(RGB(0, 106, 78));
    FillRect(dc, &r, green); DeleteObject(green);
    HBRUSH cb = CreateSolidBrush(circle);
    HGDIOBJ ob2 = SelectObject(dc, cb);
    HGDIOBJ op  = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, 1, 1, sz - 1, sz - 1);
    SelectObject(dc, op); SelectObject(dc, ob2); DeleteObject(cb);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, fg);
    HFONT f = CreateFontW(13, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                          DEFAULT_PITCH, L"Nirmala UI");
    HGDIOBJ of = SelectObject(dc, f);
    DrawTextW(dc, txt, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of); DeleteObject(f);
    SelectObject(dc, ob); DeleteDC(dc); ReleaseDC(nullptr, sdc);
    return bmp;
}

// 16x16 microphone glyph (white) on a colored square, for the voice menu items.
static HBITMAP makeMicBitmap(COLORREF bg) {
    const int sz = 16;
    HDC sdc = GetDC(nullptr); HDC dc = CreateCompatibleDC(sdc);
    HBITMAP bmp = CreateCompatibleBitmap(sdc, sz, sz);
    HGDIOBJ ob = SelectObject(dc, bmp);
    RECT r = {0, 0, sz, sz};
    HBRUSH b = CreateSolidBrush(bg); FillRect(dc, &r, b); DeleteObject(b);
    HGDIOBJ op  = SelectObject(dc, GetStockObject(NULL_PEN));
    HGDIOBJ obr = SelectObject(dc, GetStockObject(WHITE_BRUSH));
    RoundRect(dc, 6, 2, 10, 9, 4, 4);   // mic capsule
    Rectangle(dc, 7, 9, 9, 12);         // stem
    Rectangle(dc, 5, 12, 11, 13);       // base
    SelectObject(dc, op); SelectObject(dc, obr);
    SelectObject(dc, ob); DeleteDC(dc); ReleaseDC(nullptr, sdc);
    return bmp;
}

static const wchar_t* modeTip() {
    switch (g_mode) {
        case MODE_UNICODE:  return L"Bangla Keyboard — Bangla Unicode  (Ctrl+Alt+V toggles)";
        case MODE_CLASSIC:  return L"Bangla Keyboard — Bangla Classic  (Ctrl+Alt+B toggles)";
        case MODE_PHONETIC: return L"Bangla Keyboard — Bangla Phonetic  (Ctrl+Alt+P toggles)";
        default:            return L"Bangla Keyboard — English  (Ctrl+Alt+V Unicode, Ctrl+Alt+B Classic, Ctrl+Alt+P Phonetic)";
    }
}

static void updateTray() {
    g_nid.hIcon = (g_mode == MODE_UNICODE)  ? g_icoUni
                : (g_mode == MODE_CLASSIC)  ? g_icoCls
                : (g_mode == MODE_PHONETIC) ? g_icoPhon
                                            : g_icoEn;
    lstrcpynW(g_nid.szTip, modeTip(), ARRAYSIZE(g_nid.szTip));
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void balloon(const wchar_t* title, const wchar_t* text) {
    g_nid.uFlags |= NIF_INFO;
    lstrcpynW(g_nid.szInfoTitle, title, ARRAYSIZE(g_nid.szInfoTitle));
    lstrcpynW(g_nid.szInfo, text, ARRAYSIZE(g_nid.szInfo));
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    g_nid.uFlags &= ~NIF_INFO;
}

static void setMode(Mode m) {
    if (g_mode == m) return;
    flushCurrent();                 // finalise anything pending in the old mode
    g_uni.reset(); g_classic.reset(); g_phon.reset();
    g_mode = m;
    if (m == MODE_UNICODE || m == MODE_CLASSIC || m == MODE_PHONETIC) g_lastBangla = m;
    updateTray();
    if (m == MODE_CLASSIC && !g_classicHintShown) {
        g_classicHintShown = true;
        balloon(L"Bangla Classic mode",
                L"Set your app/document font to a legacy ANSI Bangla font to see Bangla.");
    }
}
static void toggleEnglish() { setMode(g_mode == MODE_ENGLISH ? g_lastBangla : MODE_ENGLISH); }

static void showMenu() {
    POINT pt; GetCursorPos(&pt);
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING | (g_mode == MODE_UNICODE ? MF_CHECKED : 0), ID_UNICODE, L"Bangla Unicode");
    AppendMenuW(m, MF_STRING | (g_mode == MODE_CLASSIC ? MF_CHECKED : 0), ID_CLASSIC, L"Bangla Classic");
    AppendMenuW(m, MF_STRING | (g_mode == MODE_PHONETIC ? MF_CHECKED : 0), ID_PHONETIC, L"Bangla Phonetic");
    AppendMenuW(m, MF_STRING | (g_mode == MODE_ENGLISH ? MF_CHECKED : 0), ID_ENGLISH, L"English");
    // colored flag icon before each mode name (like the tray icon)
    auto setBmp = [&](UINT id, HBITMAP b) {
        MENUITEMINFOW mii = {}; mii.cbSize = sizeof(mii); mii.fMask = MIIM_BITMAP; mii.hbmpItem = b;
        SetMenuItemInfoW(m, id, FALSE, &mii);
    };
    setBmp(ID_UNICODE, g_bmpUni);
    setBmp(ID_CLASSIC, g_bmpCls);
    setBmp(ID_PHONETIC, g_bmpPhon);
    setBmp(ID_ENGLISH, g_bmpEn);
    if (g_voiceEnabled) {
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING, ID_VOICE_BN, L"Bangla Voice\tCtrl+Alt+S");
        AppendMenuW(m, MF_STRING, ID_VOICE_EN, L"English Voice\tCtrl+Alt+D");
        setBmp(ID_VOICE_BN, g_bmpVoiceBn);   // green mic
        setBmp(ID_VOICE_EN, g_bmpVoiceEn);   // blue mic
    }
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_ABOUT, L"About");
    AppendMenuW(m, MF_STRING, ID_EXIT, L"Close");
    SetForegroundWindow(g_hWnd);
    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hWnd, nullptr);
    DestroyMenu(m);
}

static LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_TRAY:
            if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU) showMenu();
            else if (LOWORD(lp) == WM_LBUTTONUP) toggleEnglish();
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case ID_UNICODE: setMode(MODE_UNICODE); break;
                case ID_CLASSIC: setMode(MODE_CLASSIC); break;
                case ID_PHONETIC: setMode(MODE_PHONETIC); break;
                case ID_ENGLISH: setMode(MODE_ENGLISH); break;
                case ID_VOICE_BN: voicePost(WM_VOICE_BN); break;
                case ID_VOICE_EN: voicePost(WM_VOICE_EN); break;
                case ID_ABOUT:
                    MessageBoxW(h,
                        L"Bangla Keyboard — tray switcher\n\n"
                        L"Bangla Unicode = standard Unicode Bangla (any Bangla font).\n"
                        L"Bangla Classic = legacy ANSI Bangla encoding (needs a legacy ANSI Bangla font).\n"
                        L"Bangla Phonetic = type Bangla by sound (ami -> আমি, bhalO -> ভালো).\n"
                        L"English = normal typing.\n\n"
                        L"Switch: this menu, click the tray icon, or shortcuts —\n"
                        L"  Ctrl+Alt+V = Bangla Unicode (press again = English)\n"
                        L"  Ctrl+Alt+B = Bangla Classic (press again = English)\n"
                        L"  Ctrl+Alt+P = Bangla Phonetic (press again = English)\n\n"
                        L"Voice typing (needs internet):\n"
                        L"  Ctrl+Alt+S = Bangla voice,  Ctrl+Alt+D = English voice\n\n"
                        L"All English shortcuts (Ctrl+C/V/A/S, etc.) work normally.\n"
                        L"Same fixed layout as the macOS build.\n"
                        L"MIT licensed.",
                        L"About Bangla Keyboard", MB_OK | MB_ICONINFORMATION);
                    break;
                case ID_EXIT: DestroyWindow(h); break;
            }
            return 0;
        case WM_HOTKEY:
            // each shortcut enables its layout, or disables it (back to English).
            if      (wp == HOTKEY_UNICODE) setMode(g_mode == MODE_UNICODE ? MODE_ENGLISH : MODE_UNICODE);
            else if (wp == HOTKEY_CLASSIC) setMode(g_mode == MODE_CLASSIC ? MODE_ENGLISH : MODE_CLASSIC);
            else if (wp == HOTKEY_PHONETIC) setMode(g_mode == MODE_PHONETIC ? MODE_ENGLISH : MODE_PHONETIC);
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
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);  // harden DLL search (no app-dir/CWD)

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
                           HWND_MESSAGE, nullptr, hInst, nullptr);

    const COLORREF white = RGB(255, 255, 255), black = RGB(0, 0, 0);
    g_icoUni = makeIcon(L"অ", RGB(244, 42, 65), white);   // red circle, white অ
    g_icoCls = makeIcon(L"ক", RGB(192, 57, 43), white);   // brick circle, white ক
    g_icoEn  = makeIcon(L"E", white, black);              // white circle, black E
    g_icoPhon = makeIcon(L"প", RGB(13, 148, 136), white); // teal circle, white প
    g_bmpUni = makeMenuBitmap(L"অ", RGB(244, 42, 65), white);
    g_bmpCls = makeMenuBitmap(L"ক", RGB(192, 57, 43), white);
    g_bmpEn  = makeMenuBitmap(L"E", white, black);
    g_bmpPhon = makeMenuBitmap(L"প", RGB(13, 148, 136), white);
    g_bmpVoiceBn = makeMicBitmap(RGB(34, 160, 90));   // green mic
    g_bmpVoiceEn = makeMicBitmap(RGB(37, 99, 235));   // blue mic

    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd   = g_hWnd;
    g_nid.uID    = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon  = g_icoEn;
    lstrcpynW(g_nid.szTip, modeTip(), ARRAYSIZE(g_nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, hookProc, hInst, 0);
    // Caret-safety hook (see mouseProc). Best-effort: if it fails to install, typing
    // still works — only the stale-caret guard is weaker, exactly as it was before.
    g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, mouseProc, hInst, 0);
    // Another app may already own one of these chords; RegisterHotKey then fails and the
    // shortcut would silently do nothing while the menu/About still advertise it. Say so
    // once instead — the tray menu and icon click still switch modes either way.
    struct { int id; UINT vk; const wchar_t* name; } hk[] = {
        { HOTKEY_UNICODE,  'V', L"Ctrl+Alt+V (Bangla Unicode)"  },
        { HOTKEY_CLASSIC,  'B', L"Ctrl+Alt+B (Bangla Classic)"  },
        { HOTKEY_PHONETIC, 'P', L"Ctrl+Alt+P (Bangla Phonetic)" },
    };
    std::wstring taken;
    for (auto& k : hk)
        if (!RegisterHotKey(g_hWnd, k.id, MOD_CONTROL | MOD_ALT, k.vk)) {
            if (!taken.empty()) taken += L"\n";
            taken += k.name;
        }
    if (!taken.empty())
        balloon(L"Shortcut already in use",
                (L"Another app has taken:\n" + taken +
                 L"\nSwitch from the tray icon/menu instead.").c_str());

    g_voiceEnabled = readVoiceEnabled();      // start the voice companion if opted in
    if (g_voiceEnabled) launchVoice();

    balloon(L"Bangla Keyboard", L"In the tray. Ctrl+Alt+V = Bangla Unicode, Ctrl+Alt+B = Bangla Classic (press again for English). Right-click for the menu.");

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }

    quitVoice();   // close the voice companion when the tray exits

    if (g_hook) UnhookWindowsHookEx(g_hook);
    if (g_mouseHook) UnhookWindowsHookEx(g_mouseHook);
    UnregisterHotKey(g_hWnd, HOTKEY_UNICODE);
    UnregisterHotKey(g_hWnd, HOTKEY_CLASSIC);
    if (g_icoUni) DestroyIcon(g_icoUni);
    if (g_icoCls) DestroyIcon(g_icoCls);
    if (g_icoEn)  DestroyIcon(g_icoEn);
    if (g_bmpUni) DeleteObject(g_bmpUni);
    if (g_bmpCls) DeleteObject(g_bmpCls);
    if (g_bmpEn)  DeleteObject(g_bmpEn);
    if (g_bmpVoiceBn) DeleteObject(g_bmpVoiceBn);
    if (g_bmpVoiceEn) DeleteObject(g_bmpVoiceEn);
    if (once) { ReleaseMutex(once); CloseHandle(once); }
    return 0;
}
