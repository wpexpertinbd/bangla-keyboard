# Security

## Reporting

Found a security issue? Please open a GitHub issue (or contact BiswasHost at
<https://www.biswashost.com>). We'll respond promptly.

## Security posture

This project is a macOS keyboard‑layout installer. It has a deliberately small
attack surface:

- **No network access, no telemetry, no analytics, no auto‑update.**
- **No credentials handled or stored.**
- The only privileged actions are: installing two `.keylayout` files + free
  fonts into `/Library`, and (optionally) removing them again.

### Code that runs with elevated privileges

| Component | Runs as | Notes |
|---|---|---|
| `src/build/scripts/preinstall` | root (during `.pkg` install) | Only `rm -f` of **this project's own** hard‑coded layout paths. No dynamic input. |
| `src/build/scripts/postinstall` | root (during `.pkg` install) | Only absolute‑path `atsutil` calls to refresh the font cache. No dynamic input. |
| `Bangla Keyboard Installer.app` (`src/build/installer.applescript`) | admin (via `osascript … with administrator privileges`) | Install/Reinstall/Uninstall. All filesystem paths are passed to AppleScript as **arguments** and shell‑escaped with `quoted form of` — see below. |

### Injection‑safe privileged execution

The installer app never interpolates a path directly into a privileged shell
string. Paths are passed as `osascript` arguments and quoted by AppleScript
itself:

```applescript
on run argv
    do shell script "/usr/sbin/installer -pkg " & quoted form of (item 1 of argv) & " -target /" with administrator privileges
end run
```

This means a path containing quotes, spaces, or shell metacharacters cannot
break out of the command (verified with a `'; touch …; '` payload during
development). The uninstall path uses the same pattern for every file it removes.

### Data files

- The `.keylayout` files are XML consumed by macOS's text‑input engine. They
  contain only the standard system DTD reference (no external entities / XXE)
  and key→character mappings. The low‑byte `&#x00..;` outputs in **Bangla
  Classic** are legacy font glyph codes — data, never executed.
- Fonts are free/libre TrueType files from the projects listed in
  `fonts-licenses/FONTS.md`.

## Windows (tray app)

The Windows build (`windows/`, shipped as `bangla-tray.exe`) is a tray keyboard
that uses a global low-level keyboard hook (`WH_KEYBOARD_LL`) — the standard way an
input method sees keystrokes. Audit findings:

- **No keystroke logging, storage, or transmission (typing path).** For keyboard
  typing the app holds only the *current syllable* in memory and clears it on every
  word boundary — no file, network, or telemetry in the hook/engine path. The tray
  reads one registry value (`HKCU\Software\BanglaKeyboard\VoiceEnabled`) to decide
  whether to start the optional voice companion, and launches it by a fixed image
  name (`bangla-voice.exe`) from its own folder. No auto-update, no analytics.
- **No DLL-hijacking surface.** Both exes call
  `SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32)` at startup, so implicit DLL
  loads resolve from `System32` only (no app-dir/CWD planting). `bangla-tray.exe` is
  statically linked and imports **only system DLLs** (`kernel32`, `user32`, `gdi32`,
  `shell32`, `advapi32`, `msvcrt`). The optional `bangla-voice.exe` loads exactly one
  non-system DLL — `WebView2Loader.dll`, a Microsoft redistributable — and does so by
  its **full install-folder path** (not by bare name), so the search order can't be
  hijacked.
- **Least privilege.** Runs as the normal user (no elevation); the installer is
  **per-user** (`%LocalAppData%`), so installing/uninstalling needs no admin.
- **Exception-safe hook.** The hook callback is wrapped so no C++ exception can
  escape into the OS input dispatch (a crash there would affect every app); on any
  error it drops its pending state and stays alive.
- **Bounded buffers.** The in-progress run is capped (1024 chars) so a long burst or
  stuck auto-repeat can't grow memory/CPU unbounded.
- **Engine input safety.** The reordering engine is driven by **compile-time-constant
  tables** generated from the macOS `.keylayout` files; the only runtime input is the
  user's own scan codes (bounded to one byte). No untrusted data crosses into it.
- **Installer.** No remote code/downloads; the only external calls are fixed
  `taskkill /im bangla-tray.exe` and `/im bangla-voice.exe` (constant image names —
  no injection) to close the running apps before install/uninstall.

### Voice typing (optional `bangla-voice.exe`)

Opt-in at install (default on; untick to omit the files entirely). It is a separate
process so a WebView2 issue can never disturb the keyboard hook. Audit notes:

- **Mic only while listening.** The microphone is opened only after you press a voice
  shortcut and is released the moment you stop; an off-screen WebView2 (loading only
  our local `voice.html` over a virtual host) does the capture. A tray icon is shown
  whenever the mic is live.
- **Nothing recorded or stored.** Audio is held in memory just long enough to send
  one spoken sentence to the speech service; the recognized text is typed and the
  buffer dropped. No audio or transcript is written to disk, and nothing is logged.
- **Network scope is the speech service only.** English uses the browser's built-in
  Web Speech API; Bangla POSTs each utterance (16 kHz PCM) over HTTPS to Google's
  free legacy speech endpoint (the same one the open-source `recognize_google` uses)
  to get correct Bangladeshi text. No other host is contacted; no account, key entry,
  or payment is involved. This is a free, **unofficial** endpoint, so it may be
  rate-limited — the app then falls back to the browser's Bangla recognizer.
- **Local page only.** The WebView2 loads exactly one file we ship (`voice.html`);
  it browses nothing and renders no remote content. `--autoplay-policy` is the only
  browser flag set (so capture starts without a click); web security is **not**
  disabled — the cross-origin POST is done from native code, not the page.

The optional **TSF IME** DLL (`windows/tsf/`, not shipped in the release) is an
in-proc COM keyboard service; it is experimental and excluded from binary releases
until reviewed and code-signed.

## Known / accepted items

- **Windows: unsigned + keylogger-shaped tech.** The tray app is not code-signed
  yet (SmartScreen warns; *More info → Run anyway*), and a global keyboard hook is
  the same Windows API a keylogger uses — so some antivirus may heuristically flag
  it. It does **not** exfiltrate anything (see the audit above); a code-signing
  certificate + reputation removes both the SmartScreen prompt and most AV noise.

- **Voice typing is online + third-party.** With voice enabled, spoken audio is sent
  to a third-party speech service (the browser's Web Speech provider for English;
  Google's free legacy endpoint for Bangla) to be transcribed — inherent to any cloud
  STT. Nothing is stored locally, but if you would rather not send audio anywhere,
  leave the voice option unticked at install (the feature is then fully absent). The
  Bangla endpoint is unofficial and may be throttled; the app degrades gracefully.

- **Unsigned distribution.** The `.pkg` and `.command` are not code‑signed or
  notarized, so Gatekeeper shows an "unidentified developer" warning and users
  must right‑click → **Open** the first time. If you build a signed/notarized
  copy with an Apple Developer ID, that warning goes away. This is a trust/UX
  item, not a code vulnerability.
- Because installation writes to `/Library`, an admin password is required —
  this is expected for system‑wide keyboard layouts and fonts.
