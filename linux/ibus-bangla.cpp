// Bangla Keyboard — Linux IBus engine.
//
// Reuses the shared C++ KLEngine (../windows/engine) — the SAME keylayout-driven
// FSM as macOS + Windows, so output is byte-identical (f->া, Shift+f->অ, reph
// reorders, etc.). Two engines are exposed: "bangla-unicode" and "bangla-classic".
//
// IBus mapping (much cleaner than the Windows backspace hack): the in-progress
// syllable run lives in the PREEDIT (underlined); process() appends, peek() shows
// the pending deadkey, and it commits on a word boundary / chord / focus-out.
//
// Key mapping: we drive the tables by Windows Set-1 scan code, and derive it from
// the IBus KEYVAL (the engine declares layout "us", so the keyval is always the
// US-QWERTY character for that physical key). This is robust across X11 AND Wayland
// (the raw `keycode` is the X11 keycode = evdev+8 on X11 but the bare evdev keycode
// on GNOME Wayland — an 8-key shift — so keycode is NOT reliable). Non-character
// keys (Backspace/Enter/arrows) map to scan 0 and correctly pass through.
//
// Build: see build.sh (g++ + `pkg-config --cflags --libs ibus-1.0`).
#include <ibus.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <pulse/simple.h>
#include <pulse/error.h>
#include "klengine.h"
#include "unicode_table.h"
#include "classic_table.h"
#include "stt_curl.h"

// Voice-typing state (heap-allocated; GObject gives raw memory so no C++ ctors run).
struct VoiceState {
    std::thread       th;
    std::atomic<bool> stop{false};
    std::atomic<int>  mode{0};      // 0 off, 1 Bangla, 2 English
    int  fail = 0;                  // consecutive HTTP errors -> stop
};

typedef struct _IBusBangla      { IBusEngine parent; bangla::KLEngine* eng; std::u16string* run; VoiceState* voice; } IBusBangla;
typedef struct _IBusBanglaClass { IBusEngineClass parent; } IBusBanglaClass;

GType ibus_bangla_get_type(void);
#define IBUS_TYPE_BANGLA (ibus_bangla_get_type())

G_DEFINE_TYPE(IBusBangla, ibus_bangla, IBUS_TYPE_ENGINE)

static gchar* u16utf8(const std::u16string& s) {
    return g_utf16_to_utf8((const gunichar2*)s.data(), (glong)s.size(), nullptr, nullptr, nullptr);
}

// US-QWERTY keyval (shifted OR unshifted) -> Windows Set-1 scan code of that physical
// key. Shift is taken from the modifier state, so both cases map to the same scan.
// Returns 0 for non-layout keys (Space/Backspace/Enter/arrows/…) -> pass through.
static unsigned scanFromKeyval(guint kv) {
    switch (kv) {
        case '`': case '~': return 0x29;
        case '1': case '!': return 0x02; case '2': case '@': return 0x03;
        case '3': case '#': return 0x04; case '4': case '$': return 0x05;
        case '5': case '%': return 0x06; case '6': case '^': return 0x07;
        case '7': case '&': return 0x08; case '8': case '*': return 0x09;
        case '9': case '(': return 0x0A; case '0': case ')': return 0x0B;
        case '-': case '_': return 0x0C; case '=': case '+': return 0x0D;
        case 'q': case 'Q': return 0x10; case 'w': case 'W': return 0x11;
        case 'e': case 'E': return 0x12; case 'r': case 'R': return 0x13;
        case 't': case 'T': return 0x14; case 'y': case 'Y': return 0x15;
        case 'u': case 'U': return 0x16; case 'i': case 'I': return 0x17;
        case 'o': case 'O': return 0x18; case 'p': case 'P': return 0x19;
        case '[': case '{': return 0x1A; case ']': case '}': return 0x1B;
        case '\\': case '|': return 0x2B;
        case 'a': case 'A': return 0x1E; case 's': case 'S': return 0x1F;
        case 'd': case 'D': return 0x20; case 'f': case 'F': return 0x21;
        case 'g': case 'G': return 0x22; case 'h': case 'H': return 0x23;
        case 'j': case 'J': return 0x24; case 'k': case 'K': return 0x25;
        case 'l': case 'L': return 0x26; case ';': case ':': return 0x27;
        case '\'': case '"': return 0x28;
        case 'z': case 'Z': return 0x2C; case 'x': case 'X': return 0x2D;
        case 'c': case 'C': return 0x2E; case 'v': case 'V': return 0x2F;
        case 'b': case 'B': return 0x30; case 'n': case 'N': return 0x31;
        case 'm': case 'M': return 0x32; case ',': case '<': return 0x33;
        case '.': case '>': return 0x34; case '/': case '?': return 0x35;
        default: return 0;
    }
}

// Pick the table from the engine's own name the first time it's used.
static void ensure_engine(IBusBangla* self) {
    if (self->eng) return;
    gchar* name = nullptr;
    g_object_get(self, "engine-name", &name, nullptr);
    gboolean classic = (name && g_strcmp0(name, "bangla-classic") == 0);
    g_free(name);
    self->eng = new bangla::KLEngine(classic ? &bangla::classic_table::TABLE
                                             : &bangla::unicode_table::TABLE);
}

static void show_preedit(IBusBangla* self) {
    std::u16string p = *self->run + self->eng->peek();
    if (p.empty()) { ibus_engine_hide_preedit_text((IBusEngine*)self); return; }
    gchar* u8 = u16utf8(p);
    glong len = g_utf8_strlen(u8, -1);
    IBusText* t = ibus_text_new_from_string(u8);
    ibus_text_append_attribute(t, IBUS_ATTR_TYPE_UNDERLINE, IBUS_ATTR_UNDERLINE_SINGLE, 0, (guint)len);
    ibus_engine_update_preedit_text((IBusEngine*)self, t, (guint)len, TRUE);
    g_free(u8);
}

// Finalise the run: commit committed-text + the dangling deadkey, reset, hide preedit.
// Self-guarded so no C++ exception can unwind into IBus's C dispatch (it's called
// from the process/focus-out/reset/disable vfuncs).
static void commit_run(IBusBangla* self) {
    if (!self->eng) { ibus_engine_hide_preedit_text((IBusEngine*)self); return; }
    try {
        std::u16string all = *self->run + self->eng->peek();
        self->eng->reset();
        self->run->clear();
        ibus_engine_hide_preedit_text((IBusEngine*)self);
        if (!all.empty()) {
            gchar* u8 = u16utf8(all);
            ibus_engine_commit_text((IBusEngine*)self, ibus_text_new_from_string(u8));
            g_free(u8);
        }
    } catch (...) {
        self->eng->reset(); self->run->clear();
        ibus_engine_hide_preedit_text((IBusEngine*)self);
    }
}

// ===== Voice typing (Ctrl+Alt+S Bangla / Ctrl+Alt+D English) ================
// Free online STT (native libcurl -> Google's L16 endpoint; see stt_curl.h), so it
// works on X11 AND Wayland: the recognized text is committed THROUGH IBus (the input
// method commit is Wayland-safe, unlike external synthetic key injection). Runs only
// while this engine is active. Mic via PulseAudio/PipeWire (libpulse-simple).

static void voice_notify(const char* msg) {
    gchar* cmd = g_strdup_printf("notify-send -t 1500 -a 'Bangla Keyboard' \"%s\"", msg);
    g_spawn_command_line_async(cmd, nullptr);
    g_free(cmd);
}

// Spoken punctuation: convert ONLY when the WHOLE utterance is exactly the mark word
// (said alone), so a real word inside a sentence stays a word. Else pass the text.
static std::string apply_punct(const std::string& in, int mode) {
    size_t a = in.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = in.find_last_not_of(" \t\r\n");
    std::string w = in.substr(a, b - a + 1);
    if (mode == 1) {
        if (w=="দাঁড়ি"||w=="দাড়ি"||w=="দাড়ী"||w=="ফুলস্টপ") return "।";
        if (w=="কমা"||w=="কহনা"||w=="কহানা"||w=="কম্মা")       return ",";
        if (w=="প্রশ্ন"||w=="প্রশ্নবোধক"||w=="প্রশ্নচিহ্ন"||w=="প্রসন") return "?";
        if (w=="বিস্ময়"||w=="বিস্ময়বোধক"||w=="আশ্চর্যবোধক")   return "!";
        if (w=="সেমিকোলন") return ";";
        if (w=="কোলন")     return ":";
    } else {
        std::string l; for (char c : w) l += (char)tolower((unsigned char)c);
        if (l=="comma") return ",";
        if (l=="period"||l=="full stop"||l=="dot") return ".";
        if (l=="question mark"||l=="question")     return "?";
        if (l=="exclamation mark"||l=="exclamation") return "!";
        if (l=="semicolon") return ";";
        if (l=="colon")     return ":";
    }
    return in;
}

// commit recognized text on the MAIN thread (IBus is not thread-safe).
struct CommitData { IBusEngine* engine; gchar* text; };
static gboolean commit_idle(gpointer p) {
    CommitData* d = (CommitData*)p;
    ibus_engine_commit_text(d->engine, ibus_text_new_from_string(d->text));
    g_free(d->text); g_object_unref(d->engine); delete d;
    return G_SOURCE_REMOVE;
}
static void commit_from_thread(IBusEngine* engine, const std::string& utf8) {
    if (utf8.empty()) return;
    CommitData* d = new CommitData{ (IBusEngine*)g_object_ref(engine), g_strdup(utf8.c_str()) };
    g_idle_add(commit_idle, d);
}

// capture thread: 16 kHz mono PCM -> RMS/700ms-silence VAD -> per-utterance STT.
static void voice_run(IBusBangla* self, int mode) {
    pa_sample_spec ss; ss.format = PA_SAMPLE_S16LE; ss.rate = 16000; ss.channels = 1;
    pa_buffer_attr attr; attr.maxlength = (uint32_t)-1; attr.fragsize = 3200;
    attr.tlength = attr.prebuf = attr.minreq = (uint32_t)-1;
    int perr = 0;
    pa_simple* s = pa_simple_new(nullptr, "Bangla Voice", PA_STREAM_RECORD, nullptr,
                                 "speech", &ss, nullptr, &attr, &perr);
    if (!s) { voice_notify("Voice: microphone unavailable"); self->voice->mode.store(0); return; }
    std::vector<int16_t> utter;
    bool speaking = false; int silenceMs = 0;
    const int N = 1600; int16_t buf[N];                       // 0.1 s
    while (!self->voice->stop.load()) {
        if (pa_simple_read(s, buf, sizeof(buf), &perr) < 0) break;
        double sum = 0; for (int i = 0; i < N; i++) { double v = buf[i]; sum += v * v; }
        double rms = std::sqrt(sum / N) / 32768.0;
        if (rms > 0.012) { speaking = true; silenceMs = 0; utter.insert(utter.end(), buf, buf + N); }
        else if (speaking) {
            utter.insert(utter.end(), buf, buf + N);
            silenceMs += 100;
            if (silenceMs > 700) {                            // utterance end
                speaking = false; silenceMs = 0;
                if (utter.size() > 1600) {
                    bool herr = false;
                    const char* lang = (mode == 1) ? "bn-BD" : "en-US";
                    std::string t = stt::googleSTT(utter.data(), (long)(utter.size() * 2), 16000, lang, &herr);
                    if (!t.empty()) { self->voice->fail = 0; commit_from_thread((IBusEngine*)self, apply_punct(t, mode) + " "); }
                    else if (herr && ++self->voice->fail >= 3) { self->voice->stop.store(true); }
                }
                utter.clear();
            }
        }
    }
    pa_simple_free(s);
}

static void voice_stop(IBusBangla* self) {
    if (!self->voice || self->voice->mode.load() == 0) return;
    self->voice->stop.store(true);
    if (self->voice->th.joinable()) self->voice->th.join();
    self->voice->mode.store(0);
    voice_notify("Voice off");
}
static void voice_toggle(IBusBangla* self, int mode) {
    if (self->voice->mode.load() == mode) { voice_stop(self); return; }
    voice_stop(self);
    self->voice->stop.store(false); self->voice->fail = 0; self->voice->mode.store(mode);
    self->voice->th = std::thread(voice_run, self, mode);
    voice_notify(mode == 1 ? "\xF0\x9F\x8E\xA4 Bangla voice on" : "\xF0\x9F\x8E\xA4 English voice on");
}

static gboolean ibus_bangla_process_key_event(IBusEngine* engine, guint keyval, guint keycode, guint state) {
    IBusBangla* self = (IBusBangla*)engine;
    if (state & IBUS_RELEASE_MASK) return FALSE;              // key-up: ignore
    // A modifier pressed on its OWN (Shift/Ctrl/Alt/Super/Caps) must NOT flush the
    // pending deadkey — else Shift before a shifted consonant (খ = Shift+…) would
    // commit the pending prebase vowel (ে) before it can reorder (খেলা -> েখলা).
    switch (keyval) {
        case IBUS_KEY_Shift_L:   case IBUS_KEY_Shift_R:
        case IBUS_KEY_Control_L: case IBUS_KEY_Control_R:
        case IBUS_KEY_Alt_L:     case IBUS_KEY_Alt_R:
        case IBUS_KEY_Super_L:   case IBUS_KEY_Super_R:
        case IBUS_KEY_Meta_L:    case IBUS_KEY_Meta_R:
        case IBUS_KEY_Caps_Lock: case IBUS_KEY_Shift_Lock:
            return FALSE;                                     // keep the pending deadkey
    }
    try {
        ensure_engine(self);
        // Voice hotkeys (only while this engine is active): Ctrl+Alt+S = Bangla voice,
        // Ctrl+Alt+D = English voice. Recognized text is committed through IBus.
        if ((state & IBUS_CONTROL_MASK) && (state & IBUS_MOD1_MASK)) {
            guint k = (keyval >= 'A' && keyval <= 'Z') ? keyval + 32 : keyval;
            if (k == 's') { commit_run(self); voice_toggle(self, 1); return TRUE; }
            if (k == 'd') { commit_run(self); voice_toggle(self, 2); return TRUE; }
        }
        // Let other Ctrl / Alt / Super chords (copy/paste/save/etc.) pass through.
        if (state & (IBUS_CONTROL_MASK | IBUS_MOD1_MASK | IBUS_MOD4_MASK)) { commit_run(self); return FALSE; }
        bool shift = (state & IBUS_SHIFT_MASK) != 0;
        unsigned scan = scanFromKeyval(keyval);               // US keyval -> Set-1 scancode (X11+Wayland safe)
        if (scan && self->eng->wouldHandle(scan)) {
            if (self->run->size() > 1024) commit_run(self);   // bound the in-progress run (stuck key / no boundary)
            *self->run += self->eng->process(scan, shift);    // append to the live run
            show_preedit(self);                               // preedit = run + peek()
            return TRUE;                                      // consumed
        }
        commit_run(self);                                     // space/Enter/Backspace/punct -> finalise
        return FALSE;                                         // and let that key through
    } catch (...) {                                           // never unwind into IBus's C dispatch
        if (self->eng) self->eng->reset();
        if (self->run) self->run->clear();
        return FALSE;
    }
}

static void ibus_bangla_focus_out(IBusEngine* engine) {
    commit_run((IBusBangla*)engine);
    IBUS_ENGINE_CLASS(ibus_bangla_parent_class)->focus_out(engine);
}
static void ibus_bangla_reset(IBusEngine* engine) {
    commit_run((IBusBangla*)engine);
    IBUS_ENGINE_CLASS(ibus_bangla_parent_class)->reset(engine);
}
static void ibus_bangla_disable(IBusEngine* engine) {
    voice_stop((IBusBangla*)engine);                 // stop voice when the engine is switched away
    commit_run((IBusBangla*)engine);
    IBUS_ENGINE_CLASS(ibus_bangla_parent_class)->disable(engine);
}

static void ibus_bangla_init(IBusBangla* self) {
    self->eng = nullptr;
    self->run = new std::u16string();
    self->voice = new VoiceState();
}
static void ibus_bangla_finalize(GObject* obj) {
    IBusBangla* self = (IBusBangla*)obj;
    voice_stop(self);
    delete self->voice; self->voice = nullptr;
    delete self->eng; self->eng = nullptr;
    delete self->run; self->run = nullptr;
    G_OBJECT_CLASS(ibus_bangla_parent_class)->finalize(obj);
}
static void ibus_bangla_class_init(IBusBanglaClass* klass) {
    G_OBJECT_CLASS(klass)->finalize = ibus_bangla_finalize;
    IBusEngineClass* ec = IBUS_ENGINE_CLASS(klass);
    ec->process_key_event = ibus_bangla_process_key_event;
    ec->focus_out = ibus_bangla_focus_out;
    ec->reset     = ibus_bangla_reset;
    ec->disable   = ibus_bangla_disable;
}

// ---- daemon entry point ----------------------------------------------------
static IBusBus* g_bus = nullptr;

int main(int argc, char** argv) {
    gboolean by_ibus = FALSE;      // launched by ibus-daemon (component XML exec has --ibus)
    const char* xml = nullptr;     // standalone test: register a component from this XML first
    for (int i = 1; i < argc; ++i) {
        if (g_strcmp0(argv[i], "--ibus") == 0) by_ibus = TRUE;
        else if (g_strcmp0(argv[i], "--xml") == 0 && i + 1 < argc) xml = argv[++i];
    }

    ibus_init();
    g_bus = ibus_bus_new();
    if (!ibus_bus_is_connected(g_bus)) { g_printerr("ibus-bangla: IBus daemon not running\n"); return 1; }
    g_object_ref_sink(g_bus);

    IBusFactory* factory = ibus_factory_new(ibus_bus_get_connection(g_bus));
    g_object_ref_sink(factory);
    ibus_factory_add_engine(factory, "bangla-unicode", IBUS_TYPE_BANGLA);
    ibus_factory_add_engine(factory, "bangla-classic", IBUS_TYPE_BANGLA);

    if (xml) {   // standalone: teach the running daemon about our engines at runtime
        IBusComponent* comp = ibus_component_new_from_file(xml);
        if (comp) ibus_bus_register_component(g_bus, comp);
    }
    ibus_bus_request_name(g_bus, "org.freedesktop.IBus.Bangla", 0);

    (void)by_ibus;
    ibus_main();
    return 0;
}
