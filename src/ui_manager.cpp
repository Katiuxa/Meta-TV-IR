#include "ui_manager.h"

#include <M5Cardputer.h>
#include <SD.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "ir_engine.h"
#include "storage_manager.h"

enum Screen : uint8_t {
    SCR_BOOT = 0,
    SCR_REGION,
    SCR_HOME,
    SCR_SWEEP,
    SCR_POWER,
    SCR_REMOTE,
    SCR_NAME,
    SCR_FAVS,
    SCR_BRANDS,
    SCR_SD,
    SCR_SIGS,
    SCR_HELP,
};

struct Input {
    bool edge;
    bool tab;
    bool enter;
    bool space;
    bool del;
    bool fn;
    bool ctrl;
    char ch[6];
    int nch;
    bool up;
    bool down;
    bool left;
    bool right;
    bool vol_up;
    bool vol_down;
    bool esc;
};

static M5Canvas canvas(&M5Cardputer.Display);
static bool canvas_ok = false;
static bool dirty = true;
static Screen screen = SCR_BOOT;
static int help_top = 0;
static int home_sel = 0;
static int list_sel = 0;
static int sweep_i = -1;
static bool auto_on = false;
static uint32_t auto_at = 0;
static int last_shot = -1;
static uint32_t shot_at = 0;
static int probe = KEY_MUTE;
static int pad_i = 6;
static int blast_sel = 0;
static int blast_arm = 0;
static bool blast_on = false;
static int blast_i = 0;
static int blast_n = 0;
static int blast_idx[160];
static char blast_log[6][20];
static int blast_log_n = 0;
static uint32_t boot_at = 0;
static bool region_from_home = false;

static const char* kRegionName[] = {"Europe", "Asia", "Americas", "Japan", "Oceania", "All"};
static const uint8_t kRegionBit[] = {1, 2, 4, 8, 16, 31};

static uint32_t auto_gap() { return 450; }
static int rem_brand = 0;
static int rem_fav = -1;
static Screen back_to = SCR_HOME;
static Screen name_back = SCR_REMOTE;
static char name_buf[kFavName] = "";
static int batt = -1;
static uint32_t batt_at = 0;
static char note[28] = "";
static uint32_t note_until = 0;
static char held[8];
static int held_n = 0;
static bool vol_armed = false;
static uint32_t vol_at = 0;

static constexpr int kSigMax = 24;
static char sig_path[64];
static char sig_name[kSigMax][20];
static uint8_t sig_raw[kSigMax];
static int sig_n = 0;

static constexpr int kFileMax = 24;
static char file_path[kFileMax][64];
static char file_name[kFileMax][22];
static int file_n = 0;

static uint16_t C_BG, C_CARD, C_INK, C_DIM, C_AMBER, C_CYAN, C_LINE, C_POWER;

static void blip(int hz, int ms) {
    if (!storage_sound()) return;
    if (ms < 15) ms = 15;
    if (ms > 25) ms = 25;
    M5Cardputer.Speaker.tone((float)hz, (uint32_t)ms);
}

static void set_note(const char* text) {
    strncpy(note, text, sizeof(note) - 1);
    note[sizeof(note) - 1] = 0;
    note_until = millis() + 900;
    dirty = true;
}

static int battery() {
    if (millis() - batt_at > 4000) {
        batt_at = millis();
        int value = M5.Power.getBatteryLevel();
        int next = (value < 0 || value > 100) ? -1 : value;
        if (next != batt) dirty = true;
        batt = next;
    }
    return batt;
}

static void paint_begin() {
    if (canvas_ok) {
        canvas.fillSprite(C_BG);
        canvas.setTextDatum(textdatum_t::top_left);
        canvas.setTextSize(1);
    } else {
        M5Cardputer.Display.fillScreen(C_BG);
        M5Cardputer.Display.setTextDatum(textdatum_t::top_left);
        M5Cardputer.Display.setTextSize(1);
    }
}

static void paint_end() {
    if (canvas_ok) canvas.pushSprite(0, 0);
}

static lgfx::LovyanGFX* gfx() {
    return canvas_ok ? static_cast<lgfx::LovyanGFX*>(&canvas) : static_cast<lgfx::LovyanGFX*>(&M5Cardputer.Display);
}

static void text_at(const char* str, int x, int y, uint16_t color, const lgfx::IFont* font) {
    lgfx::LovyanGFX* g = gfx();
    g->setFont(font);
    g->setTextColor(color);
    g->setTextDatum(textdatum_t::top_left);
    g->drawString(str, x, y);
}

static void text_center(const char* str, int x, int y, uint16_t color, const lgfx::IFont* font) {
    lgfx::LovyanGFX* g = gfx();
    g->setFont(font);
    g->setTextColor(color);
    g->setTextDatum(textdatum_t::middle_center);
    g->drawString(str, x, y);
    g->setTextDatum(textdatum_t::top_left);
}

static void fill_ellipse(int x, int y, int rx, int ry, uint16_t color) {
    if (canvas_ok) canvas.fillEllipse(x, y, rx, ry, color);
    else M5Cardputer.Display.fillEllipse(x, y, rx, ry, color);
}

static void draw_ellipse(int x, int y, int rx, int ry, uint16_t color) {
    if (canvas_ok) canvas.drawEllipse(x, y, rx, ry, color);
    else M5Cardputer.Display.drawEllipse(x, y, rx, ry, color);
}

static void fill_r(int x, int y, int w, int h, int r, uint16_t color) {
    if (canvas_ok) canvas.fillRoundRect(x, y, w, h, r, color);
    else M5Cardputer.Display.fillRoundRect(x, y, w, h, r, color);
}

static void draw_r(int x, int y, int w, int h, int r, uint16_t color) {
    if (canvas_ok) canvas.drawRoundRect(x, y, w, h, r, color);
    else M5Cardputer.Display.drawRoundRect(x, y, w, h, r, color);
}

static void hline(int x, int y, int w, uint16_t color) {
    if (canvas_ok) canvas.drawFastHLine(x, y, w, color);
    else M5Cardputer.Display.drawFastHLine(x, y, w, color);
}

static void vline(int x, int y, int h, uint16_t color) {
    if (canvas_ok) canvas.drawFastVLine(x, y, h, color);
    else M5Cardputer.Display.drawFastVLine(x, y, h, color);
}

static void brackets(int x, int y, int w, int h, uint16_t color) {
    const int a = 8;
    hline(x, y, a, color);
    vline(x, y, a, color);
    hline(x + w - a, y, a, color);
    vline(x + w - 1, y, a, color);
    hline(x, y + h - 1, a, color);
    vline(x, y + h - a, a, color);
    hline(x + w - a, y + h - 1, a, color);
    vline(x + w - 1, y + h - a, a, color);
}

static void header(const char* mode) {
    const int a = 6;
    hline(0, 0, a, C_CYAN);
    vline(0, 0, a, C_CYAN);
    hline(234, 0, a, C_CYAN);
    vline(239, 0, a, C_CYAN);
    hline(0, 134, a, C_CYAN);
    vline(0, 129, a, C_CYAN);
    hline(234, 134, a, C_CYAN);
    vline(239, 129, a, C_CYAN);
    text_at("META TV IR", 8, 2, C_AMBER, &fonts::Font2);
    if (mode && mode[0]) text_at(mode, 118, 6, C_CYAN, &fonts::Font0);
    char bat[16];
    int level = battery();
    if (level < 0) snprintf(bat, sizeof(bat), "--");
    else snprintf(bat, sizeof(bat), "%d%%", level);
    text_at(bat, 204, 6, C_CYAN, &fonts::Font0);
    hline(0, 20, 240, C_LINE);
}

static void footer(const char* hint) {
    const char* shown = (note[0] && millis() < note_until) ? note : hint;
    text_at(shown, 8, 124, C_DIM, &fonts::Font0);
}

static bool has_char(const Input& in, char lower) {
    for (int i = 0; i < in.nch; ++i) {
        char c = in.ch[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c == lower) return true;
    }
    return false;
}

static void poll(Input& in) {
    memset(&in, 0, sizeof(in));
    for (int i = 0; i < held_n; ++i) {
        if (held[i] == '=' || held[i] == '+') in.vol_up = true;
        if (held[i] == '-' || held[i] == '_') in.vol_down = true;
    }
    if (!M5Cardputer.Keyboard.isChange()) return;
    if (!M5Cardputer.Keyboard.isPressed()) {
        held_n = 0;
        in.vol_up = false;
        in.vol_down = false;
        return;
    }
    Keyboard_Class::KeysState state = M5Cardputer.Keyboard.keysState();
    in.edge = true;
    in.tab = state.tab;
    in.enter = state.enter;
    in.space = state.space;
    in.del = state.del;
    in.fn = state.fn;
    in.ctrl = state.ctrl;
    held_n = 0;
    for (char c : state.word) {
        if (c == ' ') {
            in.space = true;
            continue;
        }
        if (c == '`') in.esc = true;
        if (held_n < 8) held[held_n++] = c;
        if (in.nch < 6) in.ch[in.nch++] = c;
        if (c == ';') in.up = true;
        if (c == ',') in.left = true;
        if (c == '.') in.down = true;
        if (c == '/') in.right = true;
    }
    in.vol_up = false;
    in.vol_down = false;
    for (int i = 0; i < held_n; ++i) {
        if (held[i] == '=' || held[i] == '+') in.vol_up = true;
        if (held[i] == '-' || held[i] == '_') in.vol_down = true;
    }
}

static void go(Screen next) {
    screen = next;
    list_sel = 0;
    dirty = true;
    auto_on = false;
    blip(2100, 16);
}

static const IrKey* remote_key(int id) {
    if (id < 0 || id >= KEY_COUNT) return nullptr;
    if (rem_fav >= 0) {
        const Favorite* fav = storage_at(rem_fav);
        if (!fav) return nullptr;
        if (fav->kind == FAV_CATALOG) {
            if (fav->catalog >= kBrandCount) return nullptr;
            return &kBrands[fav->catalog].key[id];
        }
        if (fav->kind == FAV_CUSTOM) return &fav->key[id];
        return nullptr;
    }
    if (rem_brand < 0 || rem_brand >= kBrandCount) return nullptr;
    return &kBrands[rem_brand].key[id];
}

static const char* remote_title() {
    if (rem_fav >= 0) {
        const Favorite* fav = storage_at(rem_fav);
        return fav ? fav->name : "Favorite";
    }
    if (rem_brand >= 0 && rem_brand < kBrandCount) return kBrands[rem_brand].name;
    return "Remote";
}

static const char* remote_proto() {
    if (rem_fav >= 0) {
        const Favorite* fav = storage_at(rem_fav);
        return fav ? fav->proto_name : "";
    }
    if (rem_brand >= 0 && rem_brand < kBrandCount) return kBrands[rem_brand].proto_name;
    return "";
}

static bool remote_is_raw() {
    if (rem_fav < 0) return false;
    const Favorite* fav = storage_at(rem_fav);
    return fav && fav->kind == FAV_RAW;
}

static void focus_key(int key);

static void send_id(int id) {
    focus_key(id);
    const IrKey* key = remote_key(id);
    if (!key || key->proto == PROTO_NONE) {
        set_note("No code");
        blip(220, 30);
        return;
    }
    ir_send(*key);
    blip(1700, 16);
    dirty = true;
}

static void open_catalog(int index) {
    back_to = screen;
    rem_brand = index;
    rem_fav = -1;
    pad_i = 6;
    go(SCR_REMOTE);
}

static void open_fav(int index) {
    const Favorite* fav = storage_at(index);
    if (!fav) return;
    back_to = SCR_FAVS;
    rem_fav = index;
    rem_brand = fav->kind == FAV_CATALOG ? fav->catalog : -1;
    pad_i = 6;
    go(SCR_REMOTE);
}

static void draw_home() {
    paint_begin();
    const char* tag = "ALL";
    switch (storage_region()) {
        case 1: tag = "EUROPE"; break;
        case 2: tag = "ASIA"; break;
        case 4: tag = "AMERICAS"; break;
        case 8: tag = "JAPAN"; break;
        case 16: tag = "OCEANIA"; break;
        default: tag = "ALL"; break;
    }
    header(tag);
    const char* labels[5] = {"1  Search", "2  On / Off", "3  Favorites", "4  Brands", "5  Help"};
    for (int i = 0; i < 5; ++i) {
        int y = 21 + i * 19;
        bool on = i == home_sel;
        fill_r(8, y, 224, 18, 3, C_CARD);
        draw_r(8, y, 224, 18, 3, on ? C_AMBER : C_LINE);
        if (on) vline(8, y + 2, 14, C_AMBER);
        text_at(labels[i], 18, y + 1, on ? C_AMBER : C_INK, &fonts::Font2);
    }
    footer("OK open    R region");
    paint_end();
}

static const char* probe_label() {
    if (probe == KEY_VOL_UP) return "VOL+";
    if (probe == KEY_POWER) return "POWER";
    return "MUTE";
}

static int find_probe(int from, int dir) {
    if (kBrandCount <= 0) return -1;
    int i = from;
    for (int n = 0; n < kBrandCount; ++n) {
        i = (i + dir + kBrandCount) % kBrandCount;
        if (storage_region() != 0 && storage_region() != 31 && (kRegion[i] & storage_region()) == 0) continue;
        if (kBrands[i].key[probe].proto != PROTO_NONE) return i;
    }
    return -1;
}

static bool in_region(int i) {
    uint8_t want = storage_region();
    if (want == 0 || want == 31) return true;
    return i >= 0 && i < kBrandCount && (kRegion[i] & want) != 0;
}

static void ensure_sweep() {
    if (sweep_i >= 0 && sweep_i < kBrandCount && in_region(sweep_i) && kBrands[sweep_i].key[probe].proto != PROTO_NONE) return;
    int next = find_probe(-1, 1);
    sweep_i = next < 0 ? 0 : next;
}

static void fire_current() {
    ensure_sweep();
    if (sweep_i < 0 || sweep_i >= kBrandCount || kBrands[sweep_i].key[probe].proto == PROTO_NONE) {
        set_note("No code");
        blip(220, 24);
        return;
    }
    ir_send(kBrands[sweep_i].key[probe]);
    last_shot = sweep_i;
    shot_at = millis();
    blip(auto_on ? 980 : 1800, 18);
    dirty = true;
}

static void chip(int x, const char* label, bool on, uint16_t fill) {
    fill_r(x, 22, 46, 14, 7, on ? fill : C_CARD);
    draw_r(x, 22, 46, 14, 7, on ? fill : C_LINE);
    text_at(label, x + 6, 25, on ? C_BG : C_DIM, &fonts::Font0);
}

static void keycap(int x, int y, const char* key, const char* action, bool hot, uint16_t hot_fill) {
    fill_r(x, y, 74, 26, 4, hot ? hot_fill : C_CARD);
    draw_r(x, y, 74, 26, 4, hot ? hot_fill : C_LINE);
    const bool wide = key[0] && key[1];
    const lgfx::IFont* key_font = wide ? static_cast<const lgfx::IFont*>(&fonts::Font0)
                                       : static_cast<const lgfx::IFont*>(&fonts::Font2);
    text_at(key, x + 5, y + (wide ? 9 : 5), hot ? C_BG : C_AMBER, key_font);
    text_at(action, x + (wide ? 20 : 18), y + 9, hot ? C_BG : C_INK, &fonts::Font0);
}

static void draw_sweep() {
    ensure_sweep();
    paint_begin();
    header("SEARCH");
    chip(8, "POWER", probe == KEY_POWER, C_POWER);
    chip(58, "MUTE", probe == KEY_MUTE, C_AMBER);
    chip(108, "VOL+", probe == KEY_VOL_UP, C_AMBER);
    text_at("Tab", 158, 25, C_DIM, &fonts::Font0);

    const bool flashed = last_shot == sweep_i && millis() - shot_at < 160;
    brackets(12, 40, 216, 30, flashed ? C_POWER : C_CYAN);
    fill_ellipse(120, 54, 100, 15, C_CARD);
    draw_ellipse(120, 54, 100, 15, flashed ? C_POWER : C_AMBER);
    if (auto_on) {
        int sx = 18 + (int)((millis() / 14) % 188);
        hline(sx, 40, 12, C_CYAN);
    }
    if (sweep_i >= 0 && sweep_i < kBrandCount) {
        const Brand& brand = kBrands[sweep_i];
        text_center(brand.name, 120, 50, C_INK, &fonts::Font2);
        if (note[0] && millis() < note_until) {
            text_center(note, 120, 62, C_AMBER, &fonts::Font0);
        } else {
            char sub[28];
            snprintf(sub, sizeof(sub), "%s  ·  %s", probe_label(), brand.proto_name);
            text_center(sub, 120, 62, flashed ? C_POWER : C_CYAN, &fonts::Font0);
        }
    }
    if (auto_on) {
        uint32_t gap = auto_gap();
        uint32_t gone = millis() - auto_at;
        if (gone > gap) gone = gap;
        int w = (int)((228L * (gap - gone)) / gap);
        fill_r(6, 72, 228, 3, 1, C_LINE);
        if (w > 0) fill_r(6, 72, w, 3, 1, C_CYAN);
    }
    keycap(6, 78, "OK", "fire", true, C_AMBER);
    keycap(84, 78, "G", "remote", false, C_AMBER);
    keycap(162, 78, "S", "save", false, C_AMBER);
    keycap(6, 106, "<", "back", false, C_AMBER);
    keycap(84, 106, ">", "next", false, C_AMBER);
    keycap(162, 106, "A", auto_on ? "stop" : "auto", auto_on, C_CYAN);
    paint_end();
}

static void cap(int x, int y, int w, int h, const char* label, uint16_t ink, bool hot) {
    fill_r(x, y, w, h, 3, C_CARD);
    draw_r(x, y, w, h, 3, hot ? C_POWER : C_LINE);
    text_at(label, x + 7, y + 3, ink, &fonts::Font0);
}

struct Pad {
    int key;
    const char* label;
    int x, y, w, h;
    int row, col;
};

static const Pad kPad[] = {
    {KEY_POWER, "P", 8, 40, 34, 16, 0, 0},
    {KEY_MUTE, "M", 46, 40, 34, 16, 0, 1},
    {KEY_VOL_UP, "+", 84, 40, 34, 16, 0, 2},
    {KEY_VOL_DOWN, "-", 122, 40, 34, 16, 0, 3},
    {KEY_UP, "UP", 84, 58, 34, 14, 1, 1},
    {KEY_LEFT, "L", 46, 74, 34, 14, 2, 0},
    {KEY_OK, "OK", 84, 74, 34, 14, 2, 1},
    {KEY_RIGHT, "R", 122, 74, 34, 14, 2, 2},
    {KEY_DOWN, "DN", 84, 88, 34, 14, 3, 1},
    {KEY_CH_UP, "CH+", 8, 104, 36, 14, 4, 0},
    {KEY_CH_DOWN, "CH-", 48, 104, 36, 14, 4, 1},
    {KEY_SOURCE, "SRC", 88, 104, 36, 14, 4, 2},
    {KEY_MENU, "MENU", 128, 104, 42, 14, 4, 3},
    {KEY_BACK, "BACK", 174, 104, 42, 14, 4, 4},
};
static constexpr int kPadN = (int)(sizeof(kPad) / sizeof(kPad[0]));

static int pad_at(int row, int col) {
    int best = 0;
    int best_d = 99;
    for (int i = 0; i < kPadN; ++i) {
        if (kPad[i].row != row) continue;
        int d = kPad[i].col - col;
        if (d < 0) d = -d;
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

static void move_focus(int drow, int dcol) {
    if (pad_i < 0 || pad_i >= kPadN) pad_i = 6;
    int row = kPad[pad_i].row;
    int col = kPad[pad_i].col;
    if (dcol) {
        int cols[8];
        int n = 0;
        for (int i = 0; i < kPadN && n < 8; ++i) {
            if (kPad[i].row == row) cols[n++] = kPad[i].col;
        }
        int at = 0;
        for (int i = 0; i < n; ++i) if (cols[i] == col) at = i;
        if (n > 0) col = cols[(at + dcol + n) % n];
    }
    if (drow) {
        row += drow;
        if (row < 0) row = 4;
        if (row > 4) row = 0;
    }
    pad_i = pad_at(row, col);
    dirty = true;
    blip(2100, 16);
}

static void focus_key(int key) {
    for (int i = 0; i < kPadN; ++i) {
        if (kPad[i].key == key) {
            pad_i = i;
            break;
        }
    }
}

static void draw_remote() {
    paint_begin();
    header("REMOTE");
    text_at(remote_title(), 8, 23, C_INK, &fonts::Font0);
    text_at(remote_proto(), 130, 23, C_CYAN, &fonts::Font0);
    if (!remote_is_raw()) {
        if (pad_i < 0 || pad_i >= kPadN) pad_i = 6;
        for (int i = 0; i < kPadN; ++i) {
            bool on = i == pad_i;
            bool power = kPad[i].key == KEY_POWER;
            if (on) {
                fill_r(kPad[i].x, kPad[i].y, kPad[i].w, kPad[i].h, 3, power ? C_POWER : C_AMBER);
                text_at(kPad[i].label, kPad[i].x + 6, kPad[i].y + 3, C_BG, &fonts::Font0);
            } else {
                cap(kPad[i].x, kPad[i].y, kPad[i].w, kPad[i].h, kPad[i].label, power ? C_POWER : C_INK, false);
            }
        }
        text_at("G save", 176, 42, C_DIM, &fonts::Font0);
    } else {
        text_at("Raw signal", 8, 48, C_INK, &fonts::Font2);
        cap(8, 78, 100, 22, "SEND", C_AMBER, pad_i == 6);
    }
    footer("OK sends    ` back");
    paint_end();
}

static void draw_name() {
    paint_begin();
    header("SAVE");
    fill_r(8, 40, 224, 28, 4, C_CARD);
    draw_r(8, 40, 224, 28, 4, C_AMBER);
    char shown[16];
    snprintf(shown, sizeof(shown), "%s_", name_buf);
    text_at(shown, 16, 46, C_INK, &fonts::Font2);
    text_at(remote_title(), 8, 78, C_CYAN, &fonts::Font0);
    footer("OK save    ` cancel");
    paint_end();
}

static void draw_list(const char* title, int count, const char* (*label)(int), const char* (*sub)(int), const char* hint) {
    paint_begin();
    header(title);
    if (count <= 0) {
        text_at("Empty", 8, 48, C_DIM, &fonts::Font2);
    } else {
        if (list_sel < 0) list_sel = 0;
        if (list_sel >= count) list_sel = count - 1;
        const int rows = 4;
        int start = list_sel - 1;
        if (start < 0) start = 0;
        if (start > count - rows) start = count - rows;
        if (start < 0) start = 0;
        for (int row = 0; row < rows && start + row < count; ++row) {
            int i = start + row;
            int y = 26 + row * 24;
            bool on = i == list_sel;
            fill_r(8, y, 224, 22, 3, C_CARD);
            draw_r(8, y, 224, 22, 3, on ? C_AMBER : C_LINE);
            text_at(label(i), 14, y + 5, on ? C_AMBER : C_INK, &fonts::Font0);
            if (sub) text_at(sub(i), 140, y + 6, C_CYAN, &fonts::Font0);
        }
    }
    footer(hint);
    paint_end();
}

static const char* fav_label(int i) {
    const Favorite* fav = storage_at(i);
    return fav ? fav->name : "";
}
static const char* fav_sub(int i) {
    const Favorite* fav = storage_at(i);
    return fav ? fav->brand : "";
}
static int here_count() {
    int n = 0;
    for (int i = 0; i < kBrandCount; ++i) {
        if (storage_region() != 0 && storage_region() != 31 && (kRegion[i] & storage_region()) == 0) continue;
        n++;
    }
    return n;
}

static int here_at(int nth) {
    int n = 0;
    for (int i = 0; i < kBrandCount; ++i) {
        if (storage_region() != 0 && storage_region() != 31 && (kRegion[i] & storage_region()) == 0) continue;
        if (n == nth) return i;
        n++;
    }
    return 0;
}

static const char* here_label(int i) { return kBrands[here_at(i)].name; }
static const char* here_sub(int i) { return kBrands[here_at(i)].proto_name; }
static const char* file_label(int i) { return file_name[i]; }
static const char* sig_label(int i) { return sig_name[i]; }
static const char* sig_sub(int i) { return sig_raw[i] ? "RAW" : "IR"; }

static bool same_power(const IrKey& a, const IrKey& b) {
    if (a.proto != b.proto || a.bits != b.bits || a.address != b.address) return false;
    if (a.proto == PROTO_KASEIKYO || a.proto == PROTO_PIONEER) return a.command == b.command;
    return (a.command & 0xFF) == (b.command & 0xFF);
}

static void build_blast() {
    blast_n = 0;
    for (int i = 0; i < kBrandCount && blast_n < (int)(sizeof(blast_idx) / sizeof(blast_idx[0])); ++i) {
        if (storage_region() != 0 && storage_region() != 31 && (kRegion[i] & storage_region()) == 0) continue;
        const IrKey& key = kBrands[i].key[KEY_POWER];
        if (key.proto == PROTO_NONE) continue;
        bool dup = false;
        for (int j = 0; j < blast_n; ++j) {
            if (same_power(key, kBrands[blast_idx[j]].key[KEY_POWER])) {
                dup = true;
                break;
            }
        }
        if (!dup) blast_idx[blast_n++] = i;
    }
}

static void draw_power() {
    paint_begin();
    header("POWER");
    const int bw = 100;
    const int bh = 36;
    const int y0 = 46;
    const int x0 = 16;
    if (blast_on && blast_log_n > 0) text_center(blast_log[blast_log_n - 1], 120, 32, C_POWER, &fonts::Font0);
    brackets(8, 40, 224, 48, blast_on ? C_POWER : C_CYAN);
    const char* lab[2] = {"ON", "OFF"};
    for (int i = 0; i < 2; ++i) {
        int x = x0 + i * (bw + 8);
        bool armed = blast_on && i == blast_arm;
        bool sel = i == blast_sel;
        fill_r(x, y0, bw, bh, 8, armed ? C_POWER : C_CARD);
        draw_r(x, y0, bw, bh, 8, armed ? C_POWER : (sel ? C_AMBER : C_LINE));
        text_center(lab[i], x + bw / 2, y0 + bh / 2, armed ? C_BG : (sel ? C_AMBER : C_INK), &fonts::Font2);
    }
    if (blast_on) {
        fill_r(16, 92, 208, 22, 6, C_POWER);
        text_at("OK", 28, 98, C_BG, &fonts::Font0);
        text_center("STOP", 128, 103, C_BG, &fonts::Font2);
    }
    footer("` back");
    paint_end();
}

static void draw_boot() {
    paint_begin();
    header("");
    const int x = 78;
    const int y = 36;
    const int w = 84;
    const int h = 56;
    draw_r(x, y, w, h, 4, C_CYAN);
    fill_r(x + 7, y + 7, w - 14, 34, 2, C_CARD);
    int span = w - 22;
    int sx = x + 11 + (int)(((millis() - boot_at) % 900) * span / 900);
    vline(sx, y + 10, 28, C_AMBER);
    fill_r(x + w / 2 - 8, y + h - 10, 16, 4, 1, C_DIM);
    fill_r(x + w / 2 - 18, y + h - 6, 36, 3, 1, C_LINE);
    text_center("Meta TV", 120, 112, C_DIM, &fonts::Font0);
    paint_end();
}

static int region_index(uint8_t bits) {
    for (int i = 0; i < 6; ++i) if (kRegionBit[i] == bits) return i;
    return 0;
}

static void draw_region() {
    paint_begin();
    header("REGION");
    const int rows = 4;
    int start = list_sel - 1;
    if (start < 0) start = 0;
    if (start > 6 - rows) start = 6 - rows;
    for (int row = 0; row < rows; ++row) {
        int i = start + row;
        int y = 24 + row * 23;
        bool on = i == list_sel;
        fill_r(8, y, 224, 21, 3, C_CARD);
        draw_r(8, y, 224, 21, 3, on ? C_AMBER : C_LINE);
        text_at(kRegionName[i], 18, y + 3, on ? C_AMBER : C_INK, &fonts::Font2);
    }
    footer("OK");
    paint_end();
}

static const char* kHelp[] = {
    "HOW TO HUNT UNKNOWN TVs",
    "",
    "1. FILTER BY REGION",
    "",
    "Pick your region first",
    "(or ALL) to narrow down",
    "the brand list and find",
    "the TV faster.",
    "",
    "2. SEARCH & LOCK-ON",
    "",
    "Test signal [Tab]:",
    "Switch between POWER,",
    "MUTE, or VOL+.",
    "Tip: use MUTE or VOL+",
    "to test a TV without",
    "turning it off.",
    "",
    "Manual mode [< / >]:",
    "Step through brands",
    "one by one. [OK] fires",
    "the signal.",
    "",
    "Auto sweep [A]:",
    "Cycles brands for you.",
    "Watch the TV. The second",
    "it reacts, press [A]",
    "again to FREEZE on that",
    "brand. Use [< / >] and",
    "[OK] to double-check.",
    "",
    "3. TAKE CONTROL & SAVE",
    "",
    "Once locked on, press",
    "[G] to open the full",
    "TV remote.",
    "",
    "Press [S] anytime to",
    "save the brand to your",
    "favorites.",
    "",
    "4. ON / OFF (BRUTE FORCE)",
    "",
    "Pick ON or OFF. [OK]",
    "spams power codes across",
    "all brands, nonstop.",
    "Press [OK] again to",
    "abort.",
};

static bool help_heading(const char* line) {
    if (!line || !line[0]) return false;
    for (const char* p = line; *p; ++p) {
        if (*p >= 'a' && *p <= 'z') return false;
    }
    return true;
}

static void draw_help() {
    paint_begin();
    header("HELP");
    const int n = (int)(sizeof(kHelp) / sizeof(kHelp[0]));
    const int rows = 8;
    const int pitch = 12;
    if (help_top > n - rows) help_top = n - rows;
    if (help_top < 0) help_top = 0;
    for (int i = 0; i < rows && help_top + i < n; ++i) {
        const char* line = kHelp[help_top + i];
        if (!line[0]) continue;
        text_at(line, 10, 24 + i * pitch, help_heading(line) ? C_AMBER : C_INK, &fonts::Font0);
    }
    if (n > rows) {
        int track = rows * pitch - 4;
        vline(230, 24, track, C_LINE);
        int span = n - rows;
        int y = 24;
        if (span > 0) y += (help_top * (track - 10)) / span;
        fill_r(228, y, 3, 10, 1, C_AMBER);
    }
    footer(help_top + rows < n ? "` back     down" : "` back");
    paint_end();
}

static void draw_current() {
    switch (screen) {
        case SCR_BOOT: draw_boot(); break;
        case SCR_REGION: draw_region(); break;
        case SCR_HOME: draw_home(); break;
        case SCR_SWEEP: draw_sweep(); break;
        case SCR_POWER: draw_power(); break;
        case SCR_REMOTE: draw_remote(); break;
        case SCR_NAME: draw_name(); break;
        case SCR_FAVS: draw_list("FAVS", storage_count(), fav_label, fav_sub, "OK open   Del delete"); break;
        case SCR_BRANDS: draw_list("BRANDS", here_count(), here_label, here_sub, "OK open   Tab SD"); break;
        case SCR_SD: draw_list("SD", file_n, file_label, nullptr, "OK open   ` back"); break;
        case SCR_SIGS: draw_list("FILE", sig_n, sig_label, sig_sub, "OK test   G save"); break;
        case SCR_HELP: draw_help(); break;
    }
}

static uint32_t hex_le(const char* text) {
    uint32_t byte[4] = {0, 0, 0, 0};
    int n = 0;
    const char* p = text;
    while (*p && n < 4) {
        while (*p == ' ') ++p;
        if (!*p) break;
        char* end = nullptr;
        byte[n++] = strtoul(p, &end, 16);
        if (end == p) break;
        p = end;
    }
    return byte[0] | (byte[1] << 8) | (byte[2] << 16) | (byte[3] << 24);
}

static int alias_of(const char* name) {
    char n[24];
    int j = 0;
    for (int i = 0; name[i] && j < (int)sizeof(n) - 1; ++i) {
        char c = name[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c == ' ') c = '_';
        n[j++] = c;
    }
    n[j] = 0;
    if (!strcmp(n, "mute")) return KEY_MUTE;
    if (!strcmp(n, "vol_up") || !strcmp(n, "volume_up")) return KEY_VOL_UP;
    if (!strcmp(n, "vol_dn") || !strcmp(n, "vol_down") || !strcmp(n, "volume_down")) return KEY_VOL_DOWN;
    if (!strcmp(n, "power") || !strcmp(n, "standby")) return KEY_POWER;
    if (!strcmp(n, "ch_next") || !strcmp(n, "ch_up") || !strcmp(n, "channel_up")) return KEY_CH_UP;
    if (!strcmp(n, "ch_prev") || !strcmp(n, "ch_down") || !strcmp(n, "channel_down")) return KEY_CH_DOWN;
    if (!strcmp(n, "source") || !strcmp(n, "sources") || !strcmp(n, "input")) return KEY_SOURCE;
    if (!strcmp(n, "ok") || !strcmp(n, "enter") || !strcmp(n, "select")) return KEY_OK;
    if (!strcmp(n, "up")) return KEY_UP;
    if (!strcmp(n, "down")) return KEY_DOWN;
    if (!strcmp(n, "left")) return KEY_LEFT;
    if (!strcmp(n, "right")) return KEY_RIGHT;
    if (!strcmp(n, "back") || !strcmp(n, "return") || !strcmp(n, "exit")) return KEY_BACK;
    if (!strcmp(n, "menu") || !strcmp(n, "home")) return KEY_MENU;
    return -1;
}

static bool map_proto(const char* proto, uint8_t* id, uint8_t* bits) {
    if (!strcmp(proto, "NEC") || !strcmp(proto, "NECext")) { *id = PROTO_NEC; *bits = 32; return true; }
    if (!strcmp(proto, "Samsung32")) { *id = PROTO_SAMSUNG; *bits = 32; return true; }
    if (!strcmp(proto, "SIRC")) { *id = PROTO_SONY; *bits = 12; return true; }
    if (!strcmp(proto, "SIRC15")) { *id = PROTO_SONY; *bits = 15; return true; }
    if (!strcmp(proto, "SIRC20")) { *id = PROTO_SONY; *bits = 20; return true; }
    if (!strcmp(proto, "RC5") || !strcmp(proto, "RC5X")) { *id = PROTO_RC5; *bits = 13; return true; }
    if (!strcmp(proto, "RC6")) { *id = PROTO_RC6; *bits = 20; return true; }
    if (!strcmp(proto, "Kaseikyo")) { *id = PROTO_KASEIKYO; *bits = 48; return true; }
    if (!strcmp(proto, "RCA")) { *id = PROTO_RCA; *bits = 24; return true; }
    if (!strcmp(proto, "JVC")) { *id = PROTO_JVC; *bits = 16; return true; }
    if (!strcmp(proto, "Pioneer")) { *id = PROTO_PIONEER; *bits = 64; return true; }
    if (!strcmp(proto, "Sharp")) { *id = PROTO_SHARP; *bits = 15; return true; }
    return false;
}

static const char* proto_label(uint8_t id) {
    switch (id) {
        case PROTO_NEC: return "NEC";
        case PROTO_SAMSUNG: return "SAMSUNG";
        case PROTO_SONY: return "SIRC";
        case PROTO_RC5: return "RC5";
        case PROTO_RC6: return "RC6";
        case PROTO_KASEIKYO: return "KASEIKYO";
        case PROTO_RCA: return "RCA";
        case PROTO_JVC: return "JVC";
        case PROTO_PIONEER: return "PIONEER";
        case PROTO_SHARP: return "SHARP";
        default: return "IR";
    }
}

struct Walk {
    int index;
    int seen;
    bool want_send;
    bool want_save;
    int save_raw_index;
    IrKey keys[KEY_COUNT];
    int parsed;
    uint8_t label_proto;
    bool sent;
    uint16_t raw[120];
    uint8_t raw_n;
    uint16_t raw_hz;
    bool got_raw;
};

static void walk_file(const char* path, Walk* walk, bool list_only) {
    File file = SD.open(path, FILE_READ);
    if (!file) return;
    char name[24] = "";
    char type[12] = "";
    char proto[16] = "";
    char address[24] = "";
    char command[24] = "";
    int freq = 38000;
    bool in_sig = false;

    auto flush = [&]() {
        if (!in_sig || !name[0]) return;
        bool parsed = strcmp(type, "parsed") == 0;
        if (list_only && sig_n < kSigMax) {
            strncpy(sig_name[sig_n], name, sizeof(sig_name[0]) - 1);
            sig_name[sig_n][sizeof(sig_name[0]) - 1] = 0;
            sig_raw[sig_n] = parsed ? 0 : 1;
            sig_n++;
        }
        if (walk && walk->seen == walk->index && walk->want_send) {
            if (parsed) {
                uint8_t id = 0, bits = 0;
                if (map_proto(proto, &id, &bits)) {
                    IrKey key = {};
                    key.proto = id;
                    key.bits = bits;
                    key.address = hex_le(address);
                    key.command = (uint16_t)(hex_le(command) & 0xFFFF);
                    ir_send(key);
                    walk->sent = true;
                }
            }
        }
        if (walk && walk->want_save && parsed) {
            int slot = alias_of(name);
            uint8_t id = 0, bits = 0;
            if (slot >= 0 && map_proto(proto, &id, &bits) && walk->keys[slot].proto == PROTO_NONE) {
                walk->keys[slot].proto = id;
                walk->keys[slot].bits = bits;
                walk->keys[slot].address = hex_le(address);
                walk->keys[slot].command = (uint16_t)(hex_le(command) & 0xFFFF);
                walk->parsed++;
                walk->label_proto = id;
            }
        }
        if (walk) walk->seen++;
        name[0] = type[0] = proto[0] = address[0] = command[0] = 0;
        freq = 38000;
    };

    static char line[1400];
    while (file.available()) {
        int n = 0;
        while (file.available()) {
            char c = (char)file.read();
            if (c == '\n' || c == '\r') break;
            if (n < (int)sizeof(line) - 1) line[n++] = c;
        }
        line[n] = 0;
        if (!strncmp(line, "name:", 5)) {
            flush();
            in_sig = true;
            strncpy(name, line + 5, sizeof(name) - 1);
            while (name[0] == ' ') memmove(name, name + 1, strlen(name));
        } else if (!strncmp(line, "type:", 5)) {
            strncpy(type, line + 6, sizeof(type) - 1);
            while (type[0] == ' ') memmove(type, type + 1, strlen(type));
        } else if (!strncmp(line, "protocol:", 9)) {
            strncpy(proto, line + 9, sizeof(proto) - 1);
            while (proto[0] == ' ') memmove(proto, proto + 1, strlen(proto));
        } else if (!strncmp(line, "address:", 8)) {
            strncpy(address, line + 8, sizeof(address) - 1);
        } else if (!strncmp(line, "command:", 8)) {
            strncpy(command, line + 8, sizeof(command) - 1);
        } else if (!strncmp(line, "frequency:", 10)) {
            freq = atoi(line + 10);
        } else if (!strncmp(line, "data:", 5) && walk && walk->want_send && walk->seen == walk->index && strcmp(type, "parsed") != 0) {
            uint16_t buf[160];
            int count = 0;
            char* p = line + 5;
            while (*p && count < 160) {
                while (*p == ' ') ++p;
                if (!*p) break;
                buf[count++] = (uint16_t)atoi(p);
                while (*p && *p != ' ') ++p;
            }
            // El resto de la linea ya se leyo; data de Flipper cabe en una linea larga.
            if (count >= 2) {
                uint16_t khz = freq > 1000 ? (uint16_t)(freq / 1000) : (uint16_t)freq;
                ir_send_raw(buf, (uint16_t)count, khz ? khz : 38);
                walk->sent = true;
            }
        } else if (!strncmp(line, "data:", 5) && walk && walk->want_save && walk->seen == walk->save_raw_index && strcmp(type, "parsed") != 0) {
            char* p = line + 5;
            walk->raw_n = 0;
            walk->raw_hz = freq > 1000 ? (uint16_t)(freq / 1000) : (uint16_t)freq;
            while (*p && walk->raw_n < kRawMax) {
                while (*p == ' ') ++p;
                if (!*p) break;
                walk->raw[walk->raw_n++] = (uint16_t)atoi(p);
                while (*p && *p != ' ') ++p;
            }
            walk->got_raw = walk->raw_n >= 2;
        }
    }
    flush();
    file.close();
}

static void load_sig_names() {
    sig_n = 0;
    walk_file(sig_path, nullptr, true);
}

static void scan_sd() {
    file_n = 0;
    if (strcmp(storage_status(), "SD+NVS") != 0) {
        set_note("No SD");
        return;
    }
    File root = SD.open("/ir");
    if (!root) {
        set_note("No /ir folder");
        return;
    }
    File entry = root.openNextFile();
    while (entry && file_n < kFileMax) {
        const char* raw = entry.name();
        const char* base = raw;
        for (const char* p = raw; *p; ++p) if (*p == '/') base = p + 1;
        if (entry.isDirectory()) {
            char child[64];
            snprintf(child, sizeof(child), "/ir/%s", base);
            File sub = SD.open(child);
            if (sub) {
                File inner = sub.openNextFile();
                while (inner && file_n < kFileMax) {
                    const char* iname = inner.name();
                    const char* ibase = iname;
                    for (const char* p = iname; *p; ++p) if (*p == '/') ibase = p + 1;
                    int len = (int)strlen(ibase);
                    if (!inner.isDirectory() && len > 3 && !strcasecmp(ibase + len - 3, ".ir")) {
                        snprintf(file_path[file_n], 64, "/ir/%s/%s", base, ibase);
                        strncpy(file_name[file_n], ibase, 21);
                        file_name[file_n][21] = 0;
                        file_n++;
                    }
                    inner.close();
                    inner = sub.openNextFile();
                }
                sub.close();
            }
        } else {
            int len = (int)strlen(base);
            if (len > 3 && !strcasecmp(base + len - 3, ".ir")) {
                snprintf(file_path[file_n], 64, "/ir/%s", base);
                strncpy(file_name[file_n], base, 21);
                file_name[file_n][21] = 0;
                file_n++;
            }
        }
        entry.close();
        entry = root.openNextFile();
    }
    root.close();
    if (file_n == 0) set_note("No .ir files");
}

static void save_current_file() {
    Walk walk = {};
    walk.want_save = true;
    walk.save_raw_index = list_sel;
    walk.index = -1;
    walk_file(sig_path, &walk, false);
    const char* base = file_name[0];
    for (int i = 0; i < file_n; ++i) {
        if (!strcmp(file_path[i], sig_path)) base = file_name[i];
    }
    char title[kFavName];
    strncpy(title, base, sizeof(title) - 1);
    title[sizeof(title) - 1] = 0;
    char* dot = strrchr(title, '.');
    if (dot) *dot = 0;
    bool ok = false;
    if (walk.parsed > 0) {
        ok = storage_add_custom(title, title, proto_label(walk.label_proto), walk.keys);
    } else if (walk.got_raw) {
        ok = storage_add_raw(title, walk.raw, walk.raw_n, walk.raw_hz);
    }
    if (ok) {
        set_note("Saved");
        blip(1040, 28);
    } else {
        set_note(walk.parsed == 0 && !walk.got_raw ? "No codes" : "List full");
        blip(220, 30);
    }
}

static void move_list(int dir, int count) {
    if (count <= 0) return;
    int next = list_sel + dir;
    if (next < 0) next = count - 1;
    if (next >= count) next = 0;
    if (next != list_sel) {
        list_sel = next;
        dirty = true;
        blip(2100, 14);
    }
}

static void blast_push(const IrKey& key) {
    char hex[20];
    ir_format(key, hex, sizeof(hex));
    if (blast_log_n < 6) {
        strncpy(blast_log[blast_log_n], hex, 19);
        blast_log[blast_log_n][19] = 0;
        blast_log_n++;
    } else {
        memmove(blast_log[0], blast_log[1], 5 * 20);
        strncpy(blast_log[5], hex, 19);
        blast_log[5][19] = 0;
    }
}

static void blast_step() {
    if (!blast_on || blast_n <= 0) return;
    if (blast_i >= blast_n) blast_i = 0;
    const IrKey& key = kBrands[blast_idx[blast_i]].key[KEY_POWER];
    blast_push(key);
    ir_send(key);
    blast_i++;
    dirty = true;
}

static void handle_power(const Input& in) {
    if (in.edge && (in.esc || in.del)) {
        blast_on = false;
        go(SCR_HOME);
        return;
    }
    if (in.edge && (in.left || in.right || in.up || in.down)) {
        blast_sel ^= 1;
        if (blast_on) blast_arm = blast_sel;
        dirty = true;
        blip(2100, 14);
    }
    if (in.edge && (in.enter || in.space)) {
        if (blast_on) {
            blast_on = false;
            dirty = true;
            blip(980, 18);
        } else {
            build_blast();
            blast_arm = blast_sel;
            blast_i = 0;
            blast_log_n = 0;
            blast_on = blast_n > 0;
            dirty = true;
            blip(1800, 16);
        }
    }
    if (blast_on) blast_step();
}

static void handle_home(const Input& in) {
    if (!in.edge) return;
    if (in.up) {
        home_sel = (home_sel + 4) % 5;
        dirty = true;
        blip(2100, 14);
    } else if (in.down) {
        home_sel = (home_sel + 1) % 5;
        dirty = true;
        blip(2100, 14);
    }
    int pick = -1;
    if (has_char(in, 'r')) {
        region_from_home = true;
        list_sel = region_index(storage_region());
        screen = SCR_REGION;
        dirty = true;
        blip(2100, 14);
        return;
    }
    if (has_char(in, '1')) pick = 0;
    if (has_char(in, '2')) pick = 1;
    if (has_char(in, '3')) pick = 2;
    if (has_char(in, '4')) pick = 3;
    if (has_char(in, '5')) pick = 4;
    if (in.enter) pick = home_sel;
    if (pick == 0) {
        sweep_i = -1;
        last_shot = -1;
        probe = KEY_POWER;
        auto_on = false;
        ensure_sweep();
        go(SCR_SWEEP);
    } else if (pick == 1) {
        blast_on = false;
        blast_i = 0;
        build_blast();
        go(SCR_POWER);
    } else if (pick == 2) {
        go(SCR_FAVS);
    } else if (pick == 3) {
        go(SCR_BRANDS);
    } else if (pick == 4) {
        help_top = 0;
        screen = SCR_HELP;
        dirty = true;
        blip(1500, 14);
    }
}

static void handle_sweep(const Input& in) {
    if (in.edge && (in.esc || in.del)) {
        auto_on = false;
        go(SCR_HOME);
        return;
    }
    if (in.edge && in.tab) {
        if (probe == KEY_POWER) probe = KEY_MUTE;
        else if (probe == KEY_MUTE) probe = KEY_VOL_UP;
        else probe = KEY_POWER;
        ensure_sweep();
        dirty = true;
        blip(1500, 16);
    }
    if (in.edge && (in.left || in.up)) {
        auto_on = false;
        int prev = find_probe(sweep_i < 0 ? 0 : sweep_i, -1);
        if (prev >= 0) sweep_i = prev;
        dirty = true;
        blip(2100, 16);
    } else if (in.edge && (in.right || in.down)) {
        auto_on = false;
        int next = find_probe(sweep_i < 0 ? -1 : sweep_i, 1);
        if (next >= 0) sweep_i = next;
        dirty = true;
        blip(2100, 16);
    }
    if (in.edge && has_char(in, 'a')) {
        auto_on = !auto_on;
        auto_at = millis();
        dirty = true;
        blip(1400, 16);
        if (auto_on) fire_current();
    }
    if (in.edge && (in.enter || in.space)) {
        auto_on = false;
        fire_current();
    }
    if (in.edge && has_char(in, 'g') && sweep_i >= 0) {
        auto_on = false;
        open_catalog(sweep_i);
        return;
    }
    if (in.edge && has_char(in, 's') && sweep_i >= 0 && sweep_i < kBrandCount) {
        auto_on = false;
        rem_fav = -1;
        rem_brand = sweep_i;
        strncpy(name_buf, kBrands[sweep_i].name, sizeof(name_buf) - 1);
        name_buf[sizeof(name_buf) - 1] = 0;
        name_back = SCR_SWEEP;
        screen = SCR_NAME;
        dirty = true;
        blip(1500, 16);
        return;
    }
    if (auto_on && millis() - auto_at >= auto_gap()) {
        auto_at = millis();
        int next = find_probe(sweep_i < 0 ? -1 : sweep_i, 1);
        if (next >= 0) sweep_i = next;
        fire_current();
    }
}

static void type_name(const Input& in) {
    if (in.del && name_buf[0]) {
        name_buf[strlen(name_buf) - 1] = 0;
        dirty = true;
        return;
    }
    if (in.enter) {
        bool ok = false;
        if (rem_fav >= 0) {
            const Favorite* fav = storage_at(rem_fav);
            if (fav && fav->kind == FAV_CUSTOM) ok = storage_add_custom(name_buf, fav->brand, fav->proto_name, fav->key);
            else if (fav && fav->kind == FAV_CATALOG) ok = storage_add_catalog(name_buf, fav->catalog);
            else if (fav && fav->kind == FAV_RAW) ok = storage_add_raw(name_buf, fav->raw, fav->raw_count, fav->raw_hz);
        } else {
            ok = storage_add_catalog(name_buf, rem_brand);
        }
        set_note(ok ? "Saved" : "List full");
        if (ok) blip(1040, 28);
        screen = name_back;
        dirty = true;
        return;
    }
    for (int i = 0; i < in.nch && (int)strlen(name_buf) < kFavName - 1; ++i) {
        char c = in.ch[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-') {
            int n = (int)strlen(name_buf);
            name_buf[n] = c;
            name_buf[n + 1] = 0;
            dirty = true;
        }
    }
}

static void handle_remote(const Input& in) {
    if (in.vol_up || in.vol_down) {
        if (!vol_armed || millis() >= vol_at) {
            send_id(in.vol_up ? KEY_VOL_UP : KEY_VOL_DOWN);
            vol_at = millis() + (vol_armed ? 110 : 280);
            vol_armed = true;
        }
    } else {
        vol_armed = false;
    }
    if (!in.edge) return;
    if (in.esc || in.del) {
        screen = back_to;
        dirty = true;
        blip(2100, 14);
        return;
    }
    if (remote_is_raw()) {
        if (in.enter || in.space) {
            const Favorite* fav = storage_at(rem_fav);
            if (fav && fav->raw_count >= 2) {
                ir_send_raw(fav->raw, fav->raw_count, fav->raw_hz);
                blip(1700, 16);
            }
        }
        return;
    }
    if (in.up) { move_focus(-1, 0); return; }
    if (in.down) { move_focus(1, 0); return; }
    if (in.left) { move_focus(0, -1); return; }
    if (in.right) { move_focus(0, 1); return; }
    if (in.enter || in.space) {
        if (pad_i >= 0 && pad_i < kPadN) send_id(kPad[pad_i].key);
        return;
    }
    if (has_char(in, 'p')) send_id(KEY_POWER);
    else if (has_char(in, 'm')) send_id(KEY_MUTE);
    else if (has_char(in, 'n')) send_id(KEY_CH_UP);
    else if (has_char(in, 'b')) send_id(KEY_CH_DOWN);
    else if (has_char(in, 's')) send_id(KEY_SOURCE);
    else if (has_char(in, 'h')) send_id(KEY_MENU);
    else if (has_char(in, 'x')) send_id(KEY_BACK);
    else if (has_char(in, 'g')) {
        strncpy(name_buf, remote_title(), sizeof(name_buf) - 1);
        name_buf[sizeof(name_buf) - 1] = 0;
        name_back = SCR_REMOTE;
        screen = SCR_NAME;
        dirty = true;
        blip(1500, 14);
    }
}

void ui_begin() {
    C_BG = M5Cardputer.Display.color565(11, 11, 14);
    C_CARD = M5Cardputer.Display.color565(24, 24, 31);
    C_INK = M5Cardputer.Display.color565(244, 244, 242);
    C_DIM = M5Cardputer.Display.color565(142, 142, 153);
    C_AMBER = M5Cardputer.Display.color565(255, 107, 0);
    C_CYAN = M5Cardputer.Display.color565(0, 212, 255);
    C_LINE = M5Cardputer.Display.color565(42, 42, 52);
    C_POWER = M5Cardputer.Display.color565(255, 77, 26);
    canvas.setColorDepth(16);
    canvas_ok = canvas.createSprite(240, 135);
    boot_at = millis();
    dirty = true;
    blip(1800, 18);
}

void ui_loop() {
    Input in;
    poll(in);
    if (in.edge && in.ctrl && has_char(in, 'm')) {
        storage_set_sound(!storage_sound());
        set_note(storage_sound() ? "Sound" : "Muted");
        if (storage_sound()) blip(1400, 18);
        return;
    }
    switch (screen) {
        case SCR_BOOT:
            if (in.edge || millis() - boot_at >= 2600) {
                list_sel = region_index(storage_region());
                region_from_home = false;
                screen = SCR_REGION;
            }
            dirty = true;
            break;
        case SCR_REGION:
            if (in.edge && (in.esc || in.del) && region_from_home) go(SCR_HOME);
            else if (in.edge && (in.up || in.down)) move_list(in.down ? 1 : -1, 6);
            else if (in.edge && (in.enter || in.space)) {
                storage_set_region(kRegionBit[list_sel]);
                screen = SCR_HOME;
                list_sel = 0;
                dirty = true;
                blip(1800, 16);
            }
            break;
        case SCR_HOME: handle_home(in); break;
        case SCR_SWEEP: handle_sweep(in); break;
        case SCR_POWER: handle_power(in); break;
        case SCR_REMOTE: handle_remote(in); break;
        case SCR_NAME:
            if (in.edge && in.esc) {
                screen = name_back;
                dirty = true;
            } else if (in.edge) type_name(in);
            break;
        case SCR_FAVS:
            if (in.edge && (in.esc || (in.del && storage_count() == 0))) go(SCR_HOME);
            else if (in.edge && in.del && storage_count() > 0) {
                storage_remove(list_sel);
                if (list_sel >= storage_count() && list_sel > 0) list_sel--;
                dirty = true;
                blip(400, 20);
            } else if (in.edge && (in.up || in.down)) move_list(in.down ? 1 : -1, storage_count());
            else if (in.edge && in.enter && storage_count() > 0) open_fav(list_sel);
            break;
        case SCR_BRANDS:
            if (in.edge && (in.esc || in.del)) go(SCR_HOME);
            else if (in.edge && in.tab) {
                scan_sd();
                go(SCR_SD);
            } else if (in.edge && (in.up || in.down)) move_list(in.down ? 1 : -1, here_count());
            else if (in.edge && in.enter && here_count() > 0) open_catalog(here_at(list_sel));
            break;
        case SCR_SD:
            if (in.edge && (in.esc || in.del)) go(SCR_BRANDS);
            else if (in.edge && (in.up || in.down)) move_list(in.down ? 1 : -1, file_n);
            else if (in.edge && in.enter && file_n > 0) {
                strncpy(sig_path, file_path[list_sel], sizeof(sig_path) - 1);
                load_sig_names();
                go(SCR_SIGS);
            }
            break;
        case SCR_SIGS:
            if (in.edge && (in.esc || in.del)) go(SCR_SD);
            else if (in.edge && (in.up || in.down)) move_list(in.down ? 1 : -1, sig_n);
            else if (in.edge && in.enter && sig_n > 0) {
                Walk walk = {};
                walk.want_send = true;
                walk.index = list_sel;
                walk_file(sig_path, &walk, false);
                blip(walk.sent ? 1700 : 220, walk.sent ? 16 : 30);
                if (!walk.sent) set_note("Not sent");
            } else if (in.edge && has_char(in, 'g')) save_current_file();
            break;
        case SCR_HELP: {
            const int n = (int)(sizeof(kHelp) / sizeof(kHelp[0]));
            const int rows = 8;
            if (in.edge && (in.esc || in.del)) {
                screen = SCR_HOME;
                dirty = true;
                blip(2100, 14);
            } else if (in.edge && in.down && help_top + rows < n) {
                help_top += 4;
                if (help_top > n - rows) help_top = n - rows;
                if (help_top < 0) help_top = 0;
                dirty = true;
            } else if (in.edge && in.up && help_top > 0) {
                help_top -= 4;
                if (help_top < 0) help_top = 0;
                dirty = true;
            }
            break;
        }
    }
    if (note[0] && millis() > note_until) {
        note[0] = 0;
        dirty = true;
    }
    if (screen == SCR_POWER && blast_on) dirty = true;
    if (screen == SCR_SWEEP && (auto_on || (shot_at && millis() - shot_at < 480))) {
        static uint32_t painted = 0;
        if (millis() - painted > 80) {
            painted = millis();
            dirty = true;
        }
    }
    if (dirty) {
        draw_current();
        dirty = false;
    }
    delay(8);
}
