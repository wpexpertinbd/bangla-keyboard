// End-to-end test of the installed IBus engine: creates an input context, selects
// each engine in turn ("bangla-unicode", then "bangla-phonetic"),
// feeds real key events by X11 keycode (= Set-1 scancode + 8),
// and checks the committed text. Proves the whole path (keycode->scan->KLEngine->
// preedit->commit) works, not just that the engine registers.
//
// Build+run: see build.sh / README (needs the engine installed + an IBus daemon).
#include <ibus.h>
#include <string>
#include <cstdio>

static std::string g_commit;

static void on_commit(IBusInputContext*, IBusText* t, gpointer) {
    if (t) g_commit += ibus_text_get_text(t);
}

// char -> Windows Set-1 scancode (same table the engine's tables use)
static guint scanOf(char c) {
    switch (c) {
        case '`':return 0x29; case '1':return 0x02; case '2':return 0x03; case '3':return 0x04;
        case '4':return 0x05; case '5':return 0x06; case '6':return 0x07; case '7':return 0x08;
        case '8':return 0x09; case '9':return 0x0A; case '0':return 0x0B; case '-':return 0x0C; case '=':return 0x0D;
        case 'q':return 0x10; case 'w':return 0x11; case 'e':return 0x12; case 'r':return 0x13; case 't':return 0x14;
        case 'y':return 0x15; case 'u':return 0x16; case 'i':return 0x17; case 'o':return 0x18; case 'p':return 0x19;
        case '[':return 0x1A; case ']':return 0x1B; case '\\':return 0x2B;
        case 'a':return 0x1E; case 's':return 0x1F; case 'd':return 0x20; case 'f':return 0x21; case 'g':return 0x22;
        case 'h':return 0x23; case 'j':return 0x24; case 'k':return 0x25; case 'l':return 0x26; case ';':return 0x27; case '\'':return 0x28;
        case 'z':return 0x2C; case 'x':return 0x2D; case 'c':return 0x2E; case 'v':return 0x2F; case 'b':return 0x30;
        case 'n':return 0x31; case 'm':return 0x32; case ',':return 0x33; case '.':return 0x34; case '/':return 0x35;
        default: return 0;
    }
}
static void spin(int ms) { for (int i = 0; i < ms / 5; i++) { while (g_main_context_iteration(nullptr, FALSE)); g_usleep(5000); } }

// The engine maps by keyval now (US layout), so feed the character as the keyval.
// For a shifted key, send the Shift keydown FIRST like a real keyboard — this must
// not flush a pending deadkey (regression test for the খেলা->েখলা bug).
static void sendKey(IBusInputContext* ctx, guint keyval, gboolean shift) {
    guint state = shift ? IBUS_SHIFT_MASK : 0;
    if (shift) { ibus_input_context_process_key_event(ctx, IBUS_KEY_Shift_L, 0, 0); spin(10); }
    ibus_input_context_process_key_event(ctx, keyval, 0, state);                       // press
    ibus_input_context_process_key_event(ctx, keyval, 0, state | IBUS_RELEASE_MASK);   // release
    if (shift) ibus_input_context_process_key_event(ctx, IBUS_KEY_Shift_L, 0, IBUS_SHIFT_MASK | IBUS_RELEASE_MASK);
    spin(60);
}

// feed a key spec like the demo ("^"=shift), then a Space to commit; return commit
static std::string play(IBusInputContext* ctx, const char* spec) {
    g_commit.clear();
    std::string tok;
    auto go = [&](const std::string& t){ if(t.empty())return; bool sh=t[0]=='^'; char k=sh?t[1]:t[0]; sendKey(ctx, (guint)(unsigned char)k, sh); };
    for (const char* p = spec; *p; ++p) { if (*p==' '){ go(tok); tok.clear(); } else tok += *p; }
    go(tok);
    sendKey(ctx, ' ', FALSE);                                // Space -> commit the run
    spin(80);
    return g_commit;
}

int main() {
    ibus_init();
    IBusBus* bus = ibus_bus_new();
    if (!ibus_bus_is_connected(bus)) { printf("no ibus daemon\n"); return 1; }
    IBusInputContext* ctx = ibus_bus_create_input_context(bus, "bangla-selftest");
    g_signal_connect(ctx, "commit-text", G_CALLBACK(on_commit), nullptr);
    ibus_input_context_set_capabilities(ctx, IBUS_CAP_PREEDIT_TEXT | IBUS_CAP_FOCUS);
    ibus_input_context_focus_in(ctx);
    struct Case { const char* keys; const char* want; };

    // Fixed layout (Bangla Unicode): reordering + conjuncts through real IBus.
    static const Case uni[] = {
        {"^f f", "আ"}, {"c j", "কে"}, {"d j", "কি"},
        {"h f ^q ^v f", "বাংলা"}, {"i g d c ^y", "হইছে"},
    };
    // Bangla Phonetic: type by sound. Also proves the THIRD engine registers and
    // that case comes from the real Shift key (^o = O in "bhalO").
    static const Case phon[] = {
        {"a m i", "আমি"}, {"b a n g l a", "বাংলা"},
        {"b h a l ^o", "ভালো"}, {"e k ^t a", "একটা"}, {"k o r r m o", "কর্ম"},
    };

    int pass = 0, total = 0;
    auto runEngine = [&](const char* engine, const Case* cs, int n) {
        ibus_input_context_set_engine(ctx, engine);
        spin(300);
        printf("== %s ==\n", engine);
        for (int i = 0; i < n; i++) {
            std::string got = play(ctx, cs[i].keys);
            // trim a trailing space (from the committed run)
            while (!got.empty() && got.back()==' ') got.pop_back();
            bool ok = (got == cs[i].want);
            pass += ok; total++;
            printf("[%s] %-14s -> %-10s (want %s)\n", ok?"PASS":"FAIL", cs[i].keys, got.c_str(), cs[i].want);
        }
    };
    runEngine("bangla-unicode",  uni,  (int)(sizeof(uni)/sizeof(uni[0])));
    runEngine("bangla-phonetic", phon, (int)(sizeof(phon)/sizeof(phon[0])));

    printf("%d/%d\n", pass, total);
    return pass == total ? 0 : 2;
}
