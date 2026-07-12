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
// Physical-key note: a Linux evdev keycode EQUALS the Windows Set-1 scan code the
// tables are keyed by (both derive from the IBM PC/AT set), and IBus passes the
// X11 keycode = evdev + 8 — so scan = keycode - 8, no lookup table needed.
//
// Build: see build.sh (g++ + `pkg-config --cflags --libs ibus-1.0`).
#include <ibus.h>
#include <string>
#include "klengine.h"
#include "unicode_table.h"
#include "classic_table.h"

typedef struct _IBusBangla      { IBusEngine parent; bangla::KLEngine* eng; std::u16string* run; } IBusBangla;
typedef struct _IBusBanglaClass { IBusEngineClass parent; } IBusBanglaClass;

GType ibus_bangla_get_type(void);
#define IBUS_TYPE_BANGLA (ibus_bangla_get_type())

G_DEFINE_TYPE(IBusBangla, ibus_bangla, IBUS_TYPE_ENGINE)

static gchar* u16utf8(const std::u16string& s) {
    return g_utf16_to_utf8((const gunichar2*)s.data(), (glong)s.size(), nullptr, nullptr, nullptr);
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

static gboolean ibus_bangla_process_key_event(IBusEngine* engine, guint keyval, guint keycode, guint state) {
    IBusBangla* self = (IBusBangla*)engine;
    if (state & IBUS_RELEASE_MASK) return FALSE;              // key-up: ignore
    try {
        ensure_engine(self);
        // Let Ctrl / Alt / Super chords (copy/paste/save/etc.) pass through untouched.
        if (state & (IBUS_CONTROL_MASK | IBUS_MOD1_MASK | IBUS_MOD4_MASK)) { commit_run(self); return FALSE; }
        bool shift = (state & IBUS_SHIFT_MASK) != 0;
        unsigned scan = keycode - 8;                          // evdev == Windows Set-1 scancode
                                                              // (scan>0xFF is rejected inside KLEngine)
        if (self->eng->wouldHandle(scan)) {
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
    commit_run((IBusBangla*)engine);
    IBUS_ENGINE_CLASS(ibus_bangla_parent_class)->disable(engine);
}

static void ibus_bangla_init(IBusBangla* self) {
    self->eng = nullptr;
    self->run = new std::u16string();
}
static void ibus_bangla_finalize(GObject* obj) {
    IBusBangla* self = (IBusBangla*)obj;
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
