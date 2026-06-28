// Free Bangla STT via Google's legacy speech-api/v2 endpoint (the same one the
// open-source recognize_google uses) — POSTed from native code so there's no CORS.
// Send 16 kHz mono LINEAR16 (audio/l16) PCM; get back the transcript. Unofficial +
// rate-limited (throttles under heavy load) -> caller falls back to bn-IN Web Speech.
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <string>

namespace stt {

// the well-known free Chromium speech key (shared, may be throttled by Google)
static const wchar_t* KEY = L"AIzaSyBOti4mM-6x9WDnZIjIeyEU21OpBXqWBgw";

// extract the first "transcript":"..." value (raw UTF-8 in the response) -> UTF-16
inline std::wstring extractTranscript(const std::string& body) {
    const char* tag = "\"transcript\":\"";
    size_t p = body.find(tag);
    if (p == std::string::npos) return L"";
    p += strlen(tag);
    std::string out;
    for (size_t i = p; i < body.size(); ++i) {
        char c = body[i];
        if (c == '\\' && i + 1 < body.size()) {        // \" \\ \/ \uXXXX
            char n = body[++i];
            if (n == 'u' && i + 4 < body.size()) {
                int cp = (int)strtol(body.substr(i + 1, 4).c_str(), nullptr, 16);
                wchar_t w = (wchar_t)cp; char buf[8];
                int n8 = WideCharToMultiByte(CP_UTF8, 0, &w, 1, buf, 8, nullptr, nullptr);
                out.append(buf, n8); i += 4;
            } else out.push_back(n);
        } else if (c == '"') break;
        else out.push_back(c);
    }
    int wn = MultiByteToWideChar(CP_UTF8, 0, out.c_str(), (int)out.size(), nullptr, 0);
    std::wstring w(wn, 0);
    MultiByteToWideChar(CP_UTF8, 0, out.c_str(), (int)out.size(), &w[0], wn);
    return w;
}

// POST raw L16 PCM -> transcript. Empty can mean "silence" (HTTP 2xx, no result)
// OR a failure; *httpErr is set ONLY on a real network/HTTP error (incl. throttle
// 4xx/5xx), so the caller falls back to bn-IN on actual throttling, not on a silent
// chunk.
inline std::wstring googleSTT(const void* pcm, DWORD pcmLen, int rate, const wchar_t* lang, bool* httpErr = nullptr) {
    if (httpErr) *httpErr = false;
    std::wstring result; bool err = true;   // assume error until a clean 2xx response
    HINTERNET hS = WinHttpOpen(L"BanglaVoice/1.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hS) { if (httpErr) *httpErr = true; return result; }
    HINTERNET hC = WinHttpConnect(hS, L"www.google.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (hC) {
        wchar_t path[256];
        wsprintfW(path, L"/speech-api/v2/recognize?output=json&lang=%s&key=%s", lang, KEY);
        HINTERNET hR = WinHttpOpenRequest(hC, L"POST", path, nullptr, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (hR) {
            wchar_t hdr[64]; wsprintfW(hdr, L"Content-Type: audio/l16; rate=%d", rate);
            if (WinHttpSendRequest(hR, hdr, -1L, (LPVOID)pcm, pcmLen, pcmLen, 0) &&
                WinHttpReceiveResponse(hR, nullptr)) {
                DWORD status = 0, slen = sizeof(status);
                WinHttpQueryHeaders(hR, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &slen, WINHTTP_NO_HEADER_INDEX);
                if (status >= 200 && status < 400) {
                    err = false;            // clean response -> empty just means silence
                    std::string body; DWORD avail = 0;
                    do {
                        if (!WinHttpQueryDataAvailable(hR, &avail) || !avail) break;
                        std::string chunk(avail, 0); DWORD got = 0;
                        if (!WinHttpReadData(hR, &chunk[0], avail, &got)) break;
                        body.append(chunk.data(), got);
                    } while (avail > 0);
                    result = extractTranscript(body);
                }
            }
            WinHttpCloseHandle(hR);
        }
        WinHttpCloseHandle(hC);
    }
    WinHttpCloseHandle(hS);
    if (httpErr) *httpErr = err;
    return result;
}

} // namespace stt
