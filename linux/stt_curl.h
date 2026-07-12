// Free Bangla/English STT via Google's legacy speech-api/v2 endpoint (the same one
// the open-source recognize_google uses) — POSTed from native code with libcurl, so
// there's no CORS and no browser. Send 16 kHz mono LINEAR16 (audio/l16) PCM; get the
// UTF-8 transcript. Unofficial + rate-limited -> caller can fall back to bn-IN.
#pragma once
#include <curl/curl.h>
#include <string>
#include <cstdlib>

namespace stt {

// the well-known free Chromium speech key (public, shared, may be throttled)
static const char* KEY = "AIzaSyBOti4mM-6x9WDnZIjIeyEU21OpBXqWBgw";

inline size_t writeCb(char* p, size_t sz, size_t n, void* ud) {
    ((std::string*)ud)->append(p, sz * n);
    return sz * n;
}

// pull the first "transcript":"..." value out of the line-delimited JSON (UTF-8,
// handling \" \\ and \uXXXX). The endpoint returns UTF-8 directly, so this is light.
inline std::string extractTranscript(const std::string& body) {
    const std::string tag = "\"transcript\":\"";
    size_t p = body.find(tag);
    if (p == std::string::npos) return "";
    p += tag.size();
    std::string out;
    for (size_t i = p; i < body.size(); ++i) {
        char c = body[i];
        if (c == '\\' && i + 1 < body.size()) {
            char n = body[++i];
            if (n == 'u' && i + 4 < body.size()) {
                unsigned cp = (unsigned)strtol(body.substr(i + 1, 4).c_str(), nullptr, 16);
                if (cp < 0x80) out += (char)cp;
                else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
                else { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
                i += 4;
            } else out += n;
        } else if (c == '"') break;
        else out += c;
    }
    return out;
}

// POST raw L16 PCM -> UTF-8 transcript. *httpErr set only on a real network/HTTP
// error (incl. throttle), NOT on a clean-but-empty (silence) response.
inline std::string googleSTT(const void* pcm, long len, int rate, const char* lang, bool* httpErr = nullptr) {
    if (httpErr) *httpErr = false;
    CURL* c = curl_easy_init();
    if (!c) { if (httpErr) *httpErr = true; return ""; }
    std::string url = "https://www.google.com/speech-api/v2/recognize?output=json&lang=";
    url += lang; url += "&key="; url += KEY;
    std::string ctype = "Content-Type: audio/l16; rate=" + std::to_string(rate);
    struct curl_slist* hdr = curl_slist_append(nullptr, ctype.c_str());
    std::string body;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_POST, 1L);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, pcm);
    curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, len);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdr);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "BanglaVoice/1.0");
    CURLcode rc = curl_easy_perform(c);
    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(hdr);
    curl_easy_cleanup(c);
    if (rc != CURLE_OK || status < 200 || status >= 400) { if (httpErr) *httpErr = true; return ""; }
    return extractTranscript(body);
}

} // namespace stt
