#include "storage_manager.h"

#include <M5Cardputer.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Favorite g_favs[kFavMax];
static int g_count = 0;
static bool g_sound = true;
static uint8_t g_region = 0;
static bool g_sd = false;
static bool g_sd_tried = false;
static char g_status[16] = "NVS";

static const char* kDir = "/ir_blade";
static const char* kFile = "/ir_blade/favs.json";

static void copy_fit(char* dst, int cap, const char* src) {
    if (!dst || cap <= 0) return;
    if (!src) src = "";
    int j = 0;
    for (int i = 0; src[i] && j < cap - 1; ++i) {
        char c = src[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_') {
            dst[j++] = c;
        }
    }
    dst[j] = 0;
    if (j == 0) {
        strncpy(dst, "Fav", cap - 1);
        dst[cap - 1] = 0;
    }
}

static void nvs_write() {
    Preferences prefs;
    if (!prefs.begin("irblade", false)) return;
    prefs.putUChar("n", (uint8_t)g_count);
    prefs.putUChar("snd", g_sound ? 1 : 0);
    prefs.putUChar("reg", g_region);
    prefs.putBytes("fav", g_favs, sizeof(g_favs));
    prefs.end();
}

static void nvs_read() {
    Preferences prefs;
    if (!prefs.begin("irblade", true)) return;
    g_sound = prefs.getUChar("snd", 1) != 0;
    g_region = prefs.getUChar("reg", 0);
    g_count = prefs.getUChar("n", 0);
    if (g_count < 0 || g_count > kFavMax) g_count = 0;
    if (prefs.getBytesLength("fav") == sizeof(g_favs)) {
        prefs.getBytes("fav", g_favs, sizeof(g_favs));
    } else {
        g_count = 0;
    }
    prefs.end();
}

static bool pull_str(const char* obj, const char* end, const char* key, char* dst, int cap) {
    char pat[24];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char* p = strstr(obj, pat);
    if (!p || p >= end) {
        if (cap > 0) dst[0] = 0;
        return false;
    }
    p += strlen(pat);
    int i = 0;
    while (p < end && *p && *p != '"' && i < cap - 1) dst[i++] = *p++;
    dst[i] = 0;
    return true;
}

static int pull_int(const char* obj, const char* end, const char* key, int fallback) {
    char pat[24];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char* p = strstr(obj, pat);
    if (!p || p >= end) return fallback;
    return atoi(p + strlen(pat));
}

static void parse_keys(const char* text, IrKey out[KEY_COUNT]) {
    memset(out, 0, sizeof(IrKey) * KEY_COUNT);
    if (!text) return;
    const char* p = text;
    for (int i = 0; i < KEY_COUNT && *p; ++i) {
        const char* semi = strchr(p, ';');
        char token[40];
        int n = semi ? (int)(semi - p) : (int)strlen(p);
        if (n > (int)sizeof(token) - 1) n = (int)sizeof(token) - 1;
        memcpy(token, p, n);
        token[n] = 0;
        if (token[0]) {
            unsigned proto = 0, bits = 0, addr = 0, cmd = 0;
            if (sscanf(token, "%u,%u,%x,%x", &proto, &bits, &addr, &cmd) == 4) {
                out[i].proto = (uint8_t)proto;
                out[i].bits = (uint8_t)bits;
                out[i].address = addr;
                out[i].command = (uint16_t)cmd;
            }
        }
        if (!semi) break;
        p = semi + 1;
    }
}

static void parse_raw(const char* text, Favorite* fav) {
    fav->raw_count = 0;
    if (!text || !text[0]) return;
    const char* p = text;
    while (*p && fav->raw_count < kRawMax) {
        fav->raw[fav->raw_count++] = (uint16_t)atoi(p);
        const char* comma = strchr(p, ',');
        if (!comma) break;
        p = comma + 1;
    }
}

static void json_load() {
    File file = SD.open(kFile, FILE_READ);
    if (!file) return;
    static char buf[7000];
    int n = 0;
    while (file.available() && n < (int)sizeof(buf) - 1) buf[n++] = (char)file.read();
    file.close();
    buf[n] = 0;
    if (n < 8) return;

    Favorite next[kFavMax];
    memset(next, 0, sizeof(next));
    int count = 0;
    const char* cursor = buf;
    while (count < kFavMax) {
        const char* start = strstr(cursor, "{\"name\"");
        if (!start) break;
        const char* next_obj = strstr(start + 6, "{\"name\"");
        const char* end = next_obj ? next_obj : buf + n;
        char name[kFavName];
        char brand[kFavName];
        char plabel[10];
        char keys[420];
        char raw[280];
        pull_str(start, end, "name", name, sizeof(name));
        pull_str(start, end, "brand", brand, sizeof(brand));
        pull_str(start, end, "plabel", plabel, sizeof(plabel));
        pull_str(start, end, "keys", keys, sizeof(keys));
        pull_str(start, end, "raw", raw, sizeof(raw));
        int kind = pull_int(start, end, "kind", 0);
        int idx = pull_int(start, end, "idx", 0);
        int hz = pull_int(start, end, "hz", 38);
        if (name[0] && kind >= FAV_CATALOG && kind <= FAV_RAW) {
            Favorite& fav = next[count];
            copy_fit(fav.name, sizeof(fav.name), name);
            copy_fit(fav.brand, sizeof(fav.brand), brand);
            strncpy(fav.proto_name, plabel, sizeof(fav.proto_name) - 1);
            fav.kind = (uint8_t)kind;
            fav.catalog = (uint8_t)idx;
            fav.raw_hz = (uint16_t)hz;
            parse_keys(keys, fav.key);
            parse_raw(raw, &fav);
            if (fav.kind == FAV_CATALOG && fav.catalog >= kBrandCount) {
                cursor = end;
                continue;
            }
            count++;
        }
        cursor = end;
    }
    memcpy(g_favs, next, sizeof(g_favs));
    g_count = count;
    nvs_write();
}

static void json_save() {
    if (!g_sd) return;
    if (!SD.exists(kDir)) SD.mkdir(kDir);
    File file = SD.open(kFile, FILE_WRITE);
    if (!file) return;
    file.print("{\"v\":1,\"favs\":[");
    for (int i = 0; i < g_count; ++i) {
        if (i) file.print(',');
        const Favorite& fav = g_favs[i];
        file.printf("{\"name\":\"%s\",\"brand\":\"%s\",\"plabel\":\"%s\",\"kind\":%u,\"idx\":%u,\"hz\":%u,\"keys\":\"",
                    fav.name, fav.brand, fav.proto_name, fav.kind, fav.catalog, fav.raw_hz);
        for (int k = 0; k < KEY_COUNT; ++k) {
            if (k) file.print(';');
            if (fav.key[k].proto == PROTO_NONE) continue;
            file.printf("%u,%u,%lX,%X", fav.key[k].proto, fav.key[k].bits,
                        (unsigned long)fav.key[k].address, fav.key[k].command);
        }
        file.print("\",\"raw\":\"");
        for (int r = 0; r < fav.raw_count && r < kRawMax; ++r) {
            if (r) file.print(',');
            file.printf("%u", fav.raw[r]);
        }
        file.print("\"}");
    }
    file.print("]}");
    file.close();
}

static bool ensure_sd() {
    if (g_sd_tried) return g_sd;
    g_sd_tried = true;
    SPI.begin(40, 39, 14, 12);
    g_sd = SD.begin(12, SPI, 4000000);
    if (!g_sd) {
        strncpy(g_status, "NVS", sizeof(g_status) - 1);
        return false;
    }
    strncpy(g_status, "SD+NVS", sizeof(g_status) - 1);
    if (SD.exists(kFile)) json_load();
    else json_save();
    return true;
}

void storage_begin() {
    memset(g_favs, 0, sizeof(g_favs));
    nvs_read();
}

int storage_count() {
    ensure_sd();
    return g_count;
}

const Favorite* storage_at(int index) {
    if (index < 0 || index >= g_count) return nullptr;
    return &g_favs[index];
}

static bool push_fav(Favorite fav) {
    ensure_sd();
    if (g_count >= kFavMax) return false;
    g_favs[g_count++] = fav;
    nvs_write();
    json_save();
    return true;
}

bool storage_add_catalog(const char* name, int brand_index) {
    if (brand_index < 0 || brand_index >= kBrandCount) return false;
    Favorite fav;
    memset(&fav, 0, sizeof(fav));
    copy_fit(fav.name, sizeof(fav.name), name);
    copy_fit(fav.brand, sizeof(fav.brand), kBrands[brand_index].name);
    strncpy(fav.proto_name, kBrands[brand_index].proto_name, sizeof(fav.proto_name) - 1);
    fav.kind = FAV_CATALOG;
    fav.catalog = (uint8_t)brand_index;
    fav.raw_hz = 38;
    return push_fav(fav);
}

bool storage_add_custom(const char* name, const char* brand, const char* proto_name, const IrKey keys[KEY_COUNT]) {
    Favorite fav;
    memset(&fav, 0, sizeof(fav));
    copy_fit(fav.name, sizeof(fav.name), name);
    copy_fit(fav.brand, sizeof(fav.brand), brand);
    strncpy(fav.proto_name, proto_name ? proto_name : "IR", sizeof(fav.proto_name) - 1);
    fav.kind = FAV_CUSTOM;
    fav.raw_hz = 38;
    if (keys) memcpy(fav.key, keys, sizeof(fav.key));
    return push_fav(fav);
}

bool storage_add_raw(const char* name, const uint16_t* timings, uint8_t count, uint16_t hz) {
    if (!timings || count < 2) return false;
    Favorite fav;
    memset(&fav, 0, sizeof(fav));
    copy_fit(fav.name, sizeof(fav.name), name);
    strncpy(fav.brand, "RAW", sizeof(fav.brand) - 1);
    strncpy(fav.proto_name, "RAW", sizeof(fav.proto_name) - 1);
    fav.kind = FAV_RAW;
    fav.raw_hz = hz ? hz : 38;
    fav.raw_count = count > kRawMax ? kRawMax : count;
    memcpy(fav.raw, timings, fav.raw_count * sizeof(uint16_t));
    return push_fav(fav);
}

bool storage_remove(int index) {
    if (index < 0 || index >= g_count) return false;
    for (int i = index; i < g_count - 1; ++i) g_favs[i] = g_favs[i + 1];
    g_count--;
    memset(&g_favs[g_count], 0, sizeof(g_favs[g_count]));
    nvs_write();
    ensure_sd();
    json_save();
    return true;
}

bool storage_sound() { return g_sound; }

void storage_set_sound(bool on) {
    g_sound = on;
    nvs_write();
}

uint8_t storage_region() { return g_region; }

void storage_set_region(uint8_t region) {
    g_region = region;
    nvs_write();
}

const char* storage_status() {
    ensure_sd();
    return g_status;
}
