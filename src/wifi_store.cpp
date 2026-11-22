#include "wifi_store.h"

#include <Preferences.h>

#include <stdio.h>
#include <string.h>

extern const uint8_t wifi_txt_start[] asm("_binary_wifi_txt_start");
extern const uint8_t wifi_txt_end[] asm("_binary_wifi_txt_end");

static const char* kNs = "metawifi";
static constexpr int kSaved = 4;

static void copy_cap(char* dst, int cap, const char* src) {
    if (!dst || cap <= 0) return;
    if (!src) src = "";
    int i = 0;
    for (; src[i] && i < cap - 1; ++i) dst[i] = src[i];
    dst[i] = 0;
}

static const char* skip_space(const char* p, const char* end) {
    while (p < end && (*p == ' ' || *p == '\t')) ++p;
    return p;
}

static bool embedded_pass(const char* ssid, char* pass, int pass_cap) {
    const char* p = (const char*)wifi_txt_start;
    const char* end = (const char*)wifi_txt_end;
    char cur_ssid[33];
    char cur_pass[65];
    cur_ssid[0] = 0;
    cur_pass[0] = 0;
    bool have = false;
    while (p < end) {
        const char* line = p;
        while (p < end && *p != '\n') ++p;
        const char* eol = p;
        if (p < end && *p == '\n') ++p;
        if (eol > line && eol[-1] == '\r') --eol;
        line = skip_space(line, eol);
        if (line >= eol || *line == '#') continue;
        if ((eol - line) > 5 && !strncmp(line, "ssid=", 5)) {
            if (have && cur_ssid[0] && !strcmp(cur_ssid, ssid)) {
                copy_cap(pass, pass_cap, cur_pass);
                return true;
            }
            const char* v = skip_space(line + 5, eol);
            int n = (int)(eol - v);
            while (n > 0 && (v[n - 1] == ' ' || v[n - 1] == '\t')) --n;
            if (n > 32) n = 32;
            memcpy(cur_ssid, v, n);
            cur_ssid[n] = 0;
            cur_pass[0] = 0;
            have = cur_ssid[0] != 0;
        } else if ((eol - line) > 5 && !strncmp(line, "pass=", 5)) {
            const char* v = skip_space(line + 5, eol);
            int n = (int)(eol - v);
            while (n > 0 && (v[n - 1] == ' ' || v[n - 1] == '\t')) --n;
            if (n > 64) n = 64;
            memcpy(cur_pass, v, n);
            cur_pass[n] = 0;
        }
    }
    if (have && cur_ssid[0] && !strcmp(cur_ssid, ssid)) {
        copy_cap(pass, pass_cap, cur_pass);
        return true;
    }
    return false;
}

static bool nvs_pass(const char* ssid, char* pass, int pass_cap) {
    Preferences prefs;
    if (!prefs.begin(kNs, true)) return false;
    int n = prefs.getUChar("n", 0);
    if (n > kSaved) n = kSaved;
    bool found = false;
    for (int i = 0; i < n; ++i) {
        char key[4];
        snprintf(key, sizeof(key), "s%d", i);
        String s = prefs.getString(key, "");
        if (s == ssid) {
            snprintf(key, sizeof(key), "p%d", i);
            String p = prefs.getString(key, "");
            copy_cap(pass, pass_cap, p.c_str());
            found = true;
            break;
        }
    }
    prefs.end();
    return found;
}

bool wifi_lookup(const char* ssid, char* pass, int pass_cap) {
    if (pass && pass_cap > 0) pass[0] = 0;
    if (!ssid || !ssid[0]) return false;
    if (embedded_pass(ssid, pass, pass_cap)) return true;
    return nvs_pass(ssid, pass, pass_cap);
}

void wifi_remember(const char* ssid, const char* pass) {
    if (!ssid || !ssid[0]) return;
    if (!pass) pass = "";
    char ssids[kSaved][33];
    char passes[kSaved][65];
    int n = 0;
    Preferences prefs;
    if (!prefs.begin(kNs, false)) return;
    int have = prefs.getUChar("n", 0);
    if (have > kSaved) have = kSaved;
    for (int i = 0; i < have; ++i) {
        char key[4];
        snprintf(key, sizeof(key), "s%d", i);
        String s = prefs.getString(key, "");
        if (!s.length() || s == ssid) continue;
        snprintf(key, sizeof(key), "p%d", i);
        String p = prefs.getString(key, "");
        copy_cap(ssids[n], sizeof(ssids[n]), s.c_str());
        copy_cap(passes[n], sizeof(passes[n]), p.c_str());
        if (++n >= kSaved - 1) break;
    }
    prefs.putString("s0", ssid);
    prefs.putString("p0", pass);
    for (int i = 0; i < n; ++i) {
        char key[4];
        snprintf(key, sizeof(key), "s%d", i + 1);
        prefs.putString(key, ssids[i]);
        snprintf(key, sizeof(key), "p%d", i + 1);
        prefs.putString(key, passes[i]);
    }
    prefs.putUChar("n", (uint8_t)(n + 1));
    prefs.end();
}

static const char* kClipNs = "metatube";
static constexpr int kClipMax = 8;

struct SavedClip {
    char name[22];
    char url[96];
};

static SavedClip g_clip[kClipMax];
static int g_clip_n = -1;

static void clip_read() {
    if (g_clip_n >= 0) return;
    g_clip_n = 0;
    Preferences prefs;
    if (!prefs.begin(kClipNs, true)) return;
    int n = prefs.getUChar("n", 0);
    if (n > kClipMax) n = kClipMax;
    for (int i = 0; i < n; ++i) {
        char key[4];
        snprintf(key, sizeof(key), "c%d", i);
        String name = prefs.getString(key, "");
        snprintf(key, sizeof(key), "u%d", i);
        String url = prefs.getString(key, "");
        if (!name.length() || !url.length()) continue;
        copy_cap(g_clip[g_clip_n].name, sizeof(g_clip[g_clip_n].name), name.c_str());
        copy_cap(g_clip[g_clip_n].url, sizeof(g_clip[g_clip_n].url), url.c_str());
        ++g_clip_n;
    }
    prefs.end();
}

static void clip_write() {
    Preferences prefs;
    if (!prefs.begin(kClipNs, false)) return;
    prefs.putUChar("n", (uint8_t)g_clip_n);
    for (int i = 0; i < g_clip_n; ++i) {
        char key[4];
        snprintf(key, sizeof(key), "c%d", i);
        prefs.putString(key, g_clip[i].name);
        snprintf(key, sizeof(key), "u%d", i);
        prefs.putString(key, g_clip[i].url);
    }
    prefs.end();
}

int clip_count() {
    clip_read();
    return g_clip_n < 0 ? 0 : g_clip_n;
}

bool clip_at(int index, char* name, int name_cap, char* url, int url_cap) {
    clip_read();
    if (index < 0 || index >= g_clip_n) return false;
    copy_cap(name, name_cap, g_clip[index].name);
    copy_cap(url, url_cap, g_clip[index].url);
    return true;
}

bool clip_add(const char* name, const char* url) {
    clip_read();
    if (!url || !url[0] || g_clip_n >= kClipMax) return false;
    if (!name || !name[0]) name = "Video";
    copy_cap(g_clip[g_clip_n].name, sizeof(g_clip[g_clip_n].name), name);
    copy_cap(g_clip[g_clip_n].url, sizeof(g_clip[g_clip_n].url), url);
    ++g_clip_n;
    clip_write();
    return true;
}

bool clip_set(int index, const char* name, const char* url) {
    clip_read();
    if (index < 0 || index >= g_clip_n || !url || !url[0]) return false;
    if (!name || !name[0]) name = "Video";
    copy_cap(g_clip[index].name, sizeof(g_clip[index].name), name);
    copy_cap(g_clip[index].url, sizeof(g_clip[index].url), url);
    clip_write();
    return true;
}

bool clip_remove(int index) {
    clip_read();
    if (index < 0 || index >= g_clip_n) return false;
    for (int i = index; i < g_clip_n - 1; ++i) g_clip[i] = g_clip[i + 1];
    --g_clip_n;
    clip_write();
    return true;
}

const char* wifi_recent_ssid() {
    static char recent[33];
    recent[0] = 0;
    Preferences prefs;
    if (!prefs.begin(kNs, true)) return recent;
    if (prefs.getUChar("n", 0) > 0) {
        String s = prefs.getString("s0", "");
        copy_cap(recent, sizeof(recent), s.c_str());
    }
    prefs.end();
    return recent;
}
