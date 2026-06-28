// Bangla Keyboard — voice host (bangla-voice.exe).
//
// Free online voice typing for the tray keyboard. A hidden, off-screen WebView2
// (mapped to a virtual https host so the mic gets a secure context) captures the
// microphone; the recognized text is injected at the cursor with SendInput.
//
//   English  — Web Speech in the page (en-US, instant, per-user free quota).
//   Bangla   — the page streams 16 kHz PCM utterances to this host, which POSTs
//              them to Google's free legacy endpoint as bn-BD (correct Bangladeshi
//              Bangla; see stt_http.h). If that free endpoint is throttled, the
//              host tells the page to fall back to bn-IN Web Speech.
//
//   Ctrl+Alt+S = Bangla voice,  Ctrl+Alt+D = English voice  (press again to stop).
//   The tray also drives it via PostMessage(WM_APP+1/+2) to the "BanglaVoice" window.
//
// The only visible UI is a small tray icon shown WHILE listening — its colour shows
// state (blue = listening, green = hearing speech) and it disappears when you stop.
//
// Needs WebView2Loader.dll next to the exe + the WebView2 Runtime (Edge ships it).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <wincrypt.h>
#include "WebView2.h"
#include "stt_http.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>

#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif

static ICoreWebView2Controller* g_controller = nullptr;
static ICoreWebView2*           g_webview    = nullptr;
static HWND   g_hwnd = nullptr;
static int    g_mode = 0;                 // 0 off, 1 Bangla, 2 English
static std::atomic<int> g_fail{0};        // consecutive empty bn-BD results
static NOTIFYICONDATAW g_nid = {};
static HICON  g_icoListen = nullptr;      // blue mic — listening, silent
static HICON  g_icoTalk   = nullptr;      // green mic — hearing speech
static wchar_t g_url[256] = L"https://voiceapp.local/voice.html?embed=1";

#define HK_BN 1
#define HK_EN 2
#define WM_VOICE_BN       (WM_APP + 1)    // tray -> toggle Bangla
#define WM_VOICE_EN       (WM_APP + 2)    // tray -> toggle English
#define WM_VOICE_QUIT     (WM_APP + 3)    // tray -> quit
#define WM_VOICE_FALLBACK (WM_APP + 4)    // worker thread -> switch Bangla to bn-IN
#define WM_TRAY_CB        (WM_APP + 5)

// ---- inject recognized text into the focused app -----------------------------
static void injectText(const std::wstring& s) {
    if (s.empty()) return;
    std::vector<INPUT> in; in.reserve(s.size() * 2);
    for (wchar_t c : s) {
        if (c < 0x20) continue;   // never inject control chars (Enter/Tab/etc.) from STT text
        INPUT d = {}; d.type = INPUT_KEYBOARD; d.ki.wScan = c; d.ki.dwFlags = KEYEVENTF_UNICODE; in.push_back(d);
        INPUT u = {}; u.type = INPUT_KEYBOARD; u.ki.wScan = c; u.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP; in.push_back(u);
    }
    SendInput((UINT)in.size(), in.data(), sizeof(INPUT));
}

static std::string base64decode(const std::string& s) {
    DWORD n = 0;
    if (!CryptStringToBinaryA(s.c_str(), (DWORD)s.size(), CRYPT_STRING_BASE64, nullptr, &n, nullptr, nullptr) || !n)
        return {};
    std::string out(n, 0);
    if (!CryptStringToBinaryA(s.c_str(), (DWORD)s.size(), CRYPT_STRING_BASE64, (BYTE*)&out[0], &n, nullptr, nullptr))
        return {};
    return out;
}

// ---- tray indicator (shown only while listening) ----------------------------
static HICON makeMicIcon(COLORREF bg) {
    const int sz = 32;
    HDC sdc = GetDC(nullptr), dc = CreateCompatibleDC(sdc);
    HBITMAP color = CreateCompatibleBitmap(sdc, sz, sz);
    HBITMAP mask  = CreateBitmap(sz, sz, 1, 1, nullptr);   // all-0 = opaque
    HGDIOBJ ob = SelectObject(dc, color);
    RECT r = {0, 0, sz, sz};
    HBRUSH b = CreateSolidBrush(bg); FillRect(dc, &r, b); DeleteObject(b);
    HBRUSH white = (HBRUSH)GetStockObject(WHITE_BRUSH);
    HGDIOBJ op = SelectObject(dc, GetStockObject(NULL_PEN));
    HGDIOBJ obr = SelectObject(dc, white);
    RoundRect(dc, 12, 6, 20, 19, 8, 8);     // mic capsule
    Rectangle(dc, 15, 19, 17, 24);          // stem
    Rectangle(dc, 10, 24, 22, 26);          // base
    HPEN wp = CreatePen(PS_SOLID, 2, RGB(255,255,255));
    SelectObject(dc, wp); SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Arc(dc, 9, 9, 23, 22, 23, 16, 9, 16);   // cradle under the capsule
    SelectObject(dc, op); SelectObject(dc, obr); DeleteObject(wp);
    SelectObject(dc, ob);
    ICONINFO ii = {}; ii.fIcon = TRUE; ii.hbmColor = color; ii.hbmMask = mask;
    HICON ic = CreateIconIndirect(&ii);
    DeleteObject(color); DeleteObject(mask); DeleteDC(dc); ReleaseDC(nullptr, sdc);
    return ic;
}
static void trayShow() {
    g_nid.cbSize = sizeof(g_nid); g_nid.hWnd = g_hwnd; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; g_nid.uCallbackMessage = WM_TRAY_CB;
    g_nid.hIcon = g_icoListen;
    lstrcpynW(g_nid.szTip, L"Voice typing — listening", ARRAYSIZE(g_nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}
static void trayHide() { Shell_NotifyIconW(NIM_DELETE, &g_nid); }
static void trayState(bool talking) {
    g_nid.hIcon = talking ? g_icoTalk : g_icoListen;
    lstrcpynW(g_nid.szTip, talking ? L"Voice typing — speaking" : L"Voice typing — listening", ARRAYSIZE(g_nid.szTip));
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

// ---- generic single-interface COM handler ------------------------------------
template <class I> struct Handler : I {
    LONG ref = 1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** ppv) override { *ppv = this; AddRef(); return S_OK; }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r = InterlockedDecrement(&ref); if (!r) delete this; return r; }
};
struct PermHandler : Handler<ICoreWebView2PermissionRequestedEventHandler> {
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* a) override {
        COREWEBVIEW2_PERMISSION_KIND k; a->get_PermissionKind(&k);
        if (k == COREWEBVIEW2_PERMISSION_KIND_MICROPHONE) a->put_State(COREWEBVIEW2_PERMISSION_STATE_ALLOW);
        return S_OK;
    }
};
// page -> host: "a:<base64 16k PCM>" (Bangla utterance), "t:<text>" (English final),
// "s:speak"/"s:idle" (mic state for the tray colour).
struct MsgHandler : Handler<ICoreWebView2WebMessageReceivedEventHandler> {
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* a) override {
        LPWSTR raw = nullptr;
        if (FAILED(a->TryGetWebMessageAsString(&raw)) || !raw) return S_OK;
        std::wstring m(raw); CoTaskMemFree(raw);
        if (m.rfind(L"s:", 0) == 0) { trayState(m == L"s:speak"); return S_OK; }
        if (m.rfind(L"t:", 0) == 0) { injectText(m.substr(2)); return S_OK; }
        if (m.rfind(L"a:", 0) == 0) {
            int n = WideCharToMultiByte(CP_ACP, 0, m.c_str() + 2, -1, nullptr, 0, nullptr, nullptr);
            std::string b64(n > 0 ? n - 1 : 0, 0);
            WideCharToMultiByte(CP_ACP, 0, m.c_str() + 2, -1, &b64[0], n, nullptr, nullptr);
            std::string pcm = base64decode(b64);
            if (pcm.size() > 3200) {                     // ignore <0.1s blips
                std::thread([pcm]() {
                    std::wstring t = stt::googleSTT(pcm.data(), (DWORD)pcm.size(), 16000, L"bn-BD");
                    if (!t.empty()) { g_fail = 0; injectText(t + L"\x0964 "); }   // append "।"
                    else if (++g_fail >= 2) PostMessageW(g_hwnd, WM_VOICE_FALLBACK, 0, 0);
                }).detach();
            }
            return S_OK;
        }
        injectText(m);   // legacy: raw text
        return S_OK;
    }
};
struct ControllerHandler : Handler<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler> {
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT res, ICoreWebView2Controller* c) override {
        if (FAILED(res) || !c) return res;
        g_controller = c; c->AddRef();
        c->get_CoreWebView2(&g_webview);
        RECT rc; GetClientRect(g_hwnd, &rc); c->put_Bounds(rc);
        EventRegistrationToken tok;
        g_webview->add_PermissionRequested(new PermHandler(), &tok);
        g_webview->add_WebMessageReceived(new MsgHandler(), &tok);
        ICoreWebView2_3* w3 = nullptr;
        if (SUCCEEDED(g_webview->QueryInterface(IID_ICoreWebView2_3, (void**)&w3)) && w3) {
            wchar_t folder[MAX_PATH];
            if (!GetEnvironmentVariableW(L"VOICE_FOLDER", folder, MAX_PATH)) {
                GetModuleFileNameW(nullptr, folder, MAX_PATH);
                PathRemoveFileSpecW(folder); PathAppendW(folder, L"voice");
            }
            w3->SetVirtualHostNameToFolderMapping(L"voiceapp.local", folder,
                COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
            w3->Release();
        }
        g_webview->Navigate(g_url);
        return S_OK;
    }
};
struct EnvHandler : Handler<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler> {
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT res, ICoreWebView2Environment* env) override {
        if (FAILED(res) || !env) return res;
        return env->CreateCoreWebView2Controller(g_hwnd, new ControllerHandler());
    }
};
typedef HRESULT (STDAPICALLTYPE *PFN_CreateEnv)(PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions*,
                                                ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);

static void post(const wchar_t* s) { if (g_webview) g_webview->PostWebMessageAsString(s); }

static void setMode(int m) {
    if (g_mode == m) { g_mode = 0; post(L"stop"); trayHide(); return; }
    bool wasOff = (g_mode == 0);
    g_mode = m; g_fail = 0;
    if (wasOff) trayShow(); else trayState(false);
    post(m == 1 ? L"start:bn" : L"start:en");
}

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_HOTKEY:       if (w == HK_BN) setMode(1); else if (w == HK_EN) setMode(2); return 0;
        case WM_VOICE_BN:     setMode(1); return 0;
        case WM_VOICE_EN:     setMode(2); return 0;
        case WM_VOICE_QUIT:   trayHide(); DestroyWindow(h); return 0;
        case WM_VOICE_FALLBACK: post(L"fallback"); return 0;   // bn-BD throttled -> bn-IN
        case WM_SIZE:         if (g_controller) { RECT rc; GetClientRect(h, &rc); g_controller->put_Bounds(rc); } return 0;
        case WM_DESTROY:      trayHide(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    HANDLE once = CreateMutexW(nullptr, TRUE, L"BanglaVoiceSingleton");
    if (once && GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    // harden DLL search: implicit loads resolve from System32 only (no app-dir/CWD
    // planting). We load our one non-system DLL (WebView2Loader) by full path below.
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);

    // let the page's AudioContext run without a user gesture (we capture, not play)
    SetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS", L"--autoplay-policy=no-user-gesture-required");
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    WNDCLASSW wc = {}; wc.lpfnWndProc = WndProc; wc.hInstance = hInst; wc.lpszClassName = L"BanglaVoice";
    RegisterClassW(&wc);
    // off-screen + tool window: the webview is never seen; the tray icon is the only UI.
    g_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"BanglaVoice", L"Bangla voice",
        WS_POPUP, -3000, -3000, 240, 120, nullptr, nullptr, hInst, nullptr);
    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);   // "visible" to the OS (mic works) but off-screen

    g_icoListen = makeMicIcon(RGB(37, 99, 235));    // blue
    g_icoTalk   = makeMicIcon(RGB(34, 160, 90));    // green

    wchar_t dll[MAX_PATH]; GetModuleFileNameW(nullptr, dll, MAX_PATH);   // load by FULL path
    PathRemoveFileSpecW(dll); PathAppendW(dll, L"WebView2Loader.dll");   // (no search/planting)
    HMODULE loader = LoadLibraryW(dll);
    if (!loader) { MessageBoxW(nullptr, L"WebView2Loader.dll missing next to bangla-voice.exe.", L"Bangla Voice", MB_OK | MB_ICONERROR); return 1; }
    auto CreateEnv = (PFN_CreateEnv)GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions");
    wchar_t udf[MAX_PATH]; GetTempPathW(MAX_PATH, udf); lstrcatW(udf, L"BanglaKeyboardVoice");
    HRESULT hr = CreateEnv(nullptr, udf, nullptr, new EnvHandler());
    if (FAILED(hr))
        MessageBoxW(nullptr, L"Could not start WebView2. Install the Microsoft Edge WebView2 Runtime, then retry.",
                    L"Bangla Voice", MB_OK | MB_ICONWARNING);

    RegisterHotKey(g_hwnd, HK_BN, MOD_CONTROL | MOD_ALT, 'S');
    RegisterHotKey(g_hwnd, HK_EN, MOD_CONTROL | MOD_ALT, 'D');

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    trayHide();
    if (g_icoListen) DestroyIcon(g_icoListen);
    if (g_icoTalk)   DestroyIcon(g_icoTalk);
    if (g_webview) g_webview->Release();
    if (g_controller) g_controller->Release();
    if (once) { ReleaseMutex(once); CloseHandle(once); }
    return 0;
}
