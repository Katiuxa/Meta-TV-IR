#include "cast_net.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "wifi_remote.h"
#include "wifi_store.h"

static constexpr int kTvMax = 6;
static constexpr int kApMax = 8;
static constexpr uint32_t kSeekFast = 5000;
static constexpr uint32_t kSeekMax = 5000;

enum ActKind : uint8_t { ACT_RICK = 1, ACT_CLIP = 2, ACT_ADD = 3, ACT_IR = 4, ACT_CUSTOM = 5, ACT_WIFI = 6 };

struct Tv {
    char name[28];
    char brand[16];
    char ip[16];
    char id[40];
    char loc[120];
    char dial[80];
    bool uuid;
};

struct Ap {
    char ssid[33];
    int8_t rssi;
    bool known;
};

struct Act {
    char label[24];
    uint8_t kind;
    uint8_t link;
};

static bool g_on = false;
static bool g_touch = false;
static bool g_scanning = false;
static CastView g_view = CAST_NETS;
static char g_ssid[33];
static char g_pass[65];
static char g_detail[48];
static uint32_t g_join_at = 0;
static int g_sel = 0;
static int g_chosen = 0;

static Ap g_ap[kApMax];
static int g_ap_n = 0;
static Tv g_tv[kTvMax];
static int g_tv_n = 0;
static Act g_act[12];
static int g_act_n = 0;
static char g_field[96];
static char g_draft[96];
static bool g_from_links = false;
static int g_scan_try = 0;

static WiFiUDP g_udp;
static bool g_udp_on = false;
static uint32_t g_seek_at = 0;
static uint8_t g_seek_wave = 0;
static int g_fetch_i = 0;
static bool g_fetch_arm = false;

static void touch() { g_touch = true; }

static void detail(const char* text) {
    if (!text) text = "";
    if (!strcmp(g_detail, text)) return;
    strncpy(g_detail, text, sizeof(g_detail) - 1);
    g_detail[sizeof(g_detail) - 1] = 0;
    touch();
}

static void copy_cap(char* dst, int cap, const char* src) {
    if (!dst || cap <= 0) return;
    if (!src) src = "";
    int i = 0;
    for (; src[i] && i < cap - 1; ++i) dst[i] = src[i];
    dst[i] = 0;
}

static void copy_text(char* dst, int cap, const char* src) {
    if (!dst || cap <= 0) return;
    dst[0] = 0;
    if (!src) return;
    while (*src == ' ' || *src == '\n' || *src == '\r' || *src == '\t') ++src;
    int j = 0;
    while (*src && j < cap - 1) {
        char c = *src++;
        if (c == '\r' || c == '\n' || c == '\t') c = ' ';
        if (c == ' ' && (j == 0 || dst[j - 1] == ' ')) continue;
        dst[j++] = c;
    }
    while (j > 0 && dst[j - 1] == ' ') --j;
    dst[j] = 0;
}

static char lower_ch(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
}

static bool contains_key(const char* hay, const char* key) {
    if (!hay || !key || !key[0]) return false;
    int klen = (int)strlen(key);
    for (int i = 0; hay[i]; ++i) {
        int j = 0;
        while (key[j] && hay[i + j] && lower_ch(hay[i + j]) == lower_ch(key[j])) ++j;
        if (j != klen) continue;
        if (klen <= 3) {
            char before = i ? hay[i - 1] : ' ';
            char after = hay[i + klen];
            auto alnum = [](char c) {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
            };
            if (alnum(before) || alnum(after)) continue;
        }
        return true;
    }
    return false;
}

static void guess_brand(char* dst, int cap, const char* a, const char* b, const char* c) {
    if (cap > 0) dst[0] = 0;
    char hay[140];
    snprintf(hay, sizeof(hay), "%s %s %s", a ? a : "", b ? b : "", c ? c : "");
    static const char* keys[] = {
        "samsung", "tizen", "webos", "bravia", "hisense", "vidaa", "panasonic", "viera",
        "philips", "toshiba", "aquos", "sharp", "grundig", "telefunken", "hitachi",
        "xiaomi", "chromecast", "android tv", "google tv", "fire tv", "skyworth",
        "vestel", "sanyo", "nevir", "loewe", "sony", "tcl", "lg", "jvc", "shield", "roku",
    };
    static const char* labs[] = {
        "Samsung", "Samsung", "LG", "Sony", "Hisense", "Hisense", "Panasonic", "Panasonic",
        "Philips", "Toshiba", "Sharp", "Sharp", "Grundig", "Telefunken", "Hitachi",
        "Xiaomi", "Chromecast", "Android TV", "Google TV", "Fire TV", "Skyworth",
        "Vestel", "Sanyo", "Nevir", "Loewe", "Sony", "TCL", "LG", "JVC", "Shield", "Roku",
    };
    int best = -1;
    int bestlen = 0;
    int n = (int)(sizeof(keys) / sizeof(keys[0]));
    for (int i = 0; i < n; ++i) {
        int len = (int)strlen(keys[i]);
        if (len > bestlen && contains_key(hay, keys[i])) {
            best = i;
            bestlen = len;
        }
    }
    if (best >= 0) copy_cap(dst, cap, labs[best]);
}

static bool hdr_get(const char* pkt, const char* key, char* out, int cap) {
    if (cap > 0) out[0] = 0;
    if (!pkt || !key) return false;
    int klen = (int)strlen(key);
    const char* p = pkt;
    while (*p) {
        while (*p == '\r' || *p == '\n') ++p;
        if (!*p) break;
        const char* line = p;
        while (*p && *p != '\n' && *p != '\r') ++p;
        int len = (int)(p - line);
        if (len > klen + 1 && strncasecmp(line, key, klen) == 0 && line[klen] == ':') {
            const char* v = line + klen + 1;
            int vlen = len - klen - 1;
            while (vlen > 0 && (*v == ' ' || *v == '\t')) {
                ++v;
                --vlen;
            }
            if (vlen >= cap) vlen = cap - 1;
            if (vlen < 0) vlen = 0;
            memcpy(out, v, vlen);
            out[vlen] = 0;
            return vlen > 0;
        }
    }
    return false;
}

static void host_of(const char* url, char* ip, int cap) {
    if (cap > 0) ip[0] = 0;
    if (!url || !url[0]) return;
    const char* p = strstr(url, "://");
    p = p ? p + 3 : url;
    int i = 0;
    while (p[i] && p[i] != ':' && p[i] != '/' && i < cap - 1) {
        ip[i] = p[i];
        ++i;
    }
    ip[i] = 0;
}

static void uuid_of(const char* usn, char* dst, int cap) {
    if (cap > 0) dst[0] = 0;
    if (!usn || strncmp(usn, "uuid:", 5) != 0) return;
    const char* s = usn + 5;
    const char* cut = strstr(s, "::");
    int n = cut ? (int)(cut - s) : (int)strlen(s);
    if (n <= 0) return;
    if (n >= cap) n = cap - 1;
    memcpy(dst, s, n);
    dst[n] = 0;
}

static int find_device(const char* id, bool has_uuid, const char* ip) {
    for (int i = 0; i < g_tv_n; ++i) {
        if (has_uuid && g_tv[i].uuid && !strcmp(g_tv[i].id, id)) return i;
        if (ip && ip[0] && !strcmp(g_tv[i].ip, ip)) {
            if (has_uuid && g_tv[i].uuid && strcmp(g_tv[i].id, id) != 0) continue;
            return i;
        }
    }
    return -1;
}

static bool looks_useful(const char* st, const char* server, const char* dial) {
    if (dial && dial[0]) return true;
    if (st && (strstr(st, "AVTransport") || strstr(st, "MediaRenderer") || strstr(st, "dial"))) return true;
    char brand[16];
    guess_brand(brand, sizeof(brand), server, st, "");
    return brand[0] != 0;
}

static void note_device(const char* ip, const char* loc, const char* server, const char* dial, const char* usn) {
    if (!ip || !ip[0]) return;
    char id[40];
    uuid_of(usn, id, sizeof(id));
    bool has_uuid = id[0] != 0;
    if (!has_uuid) copy_cap(id, sizeof(id), ip);
    int slot = find_device(id, has_uuid, ip);
    if (slot < 0) {
        if (g_tv_n >= kTvMax) return;
        slot = g_tv_n++;
        memset(&g_tv[slot], 0, sizeof(g_tv[slot]));
        copy_cap(g_tv[slot].ip, sizeof(g_tv[slot].ip), ip);
        copy_cap(g_tv[slot].id, sizeof(g_tv[slot].id), id);
        g_tv[slot].uuid = has_uuid;
        char brand[16];
        guess_brand(brand, sizeof(brand), server, "", "");
        if (brand[0]) {
            copy_cap(g_tv[slot].brand, sizeof(g_tv[slot].brand), brand);
            copy_cap(g_tv[slot].name, sizeof(g_tv[slot].name), brand);
        }
        char seen[16];
        if (g_tv_n == 1) copy_cap(seen, sizeof(seen), "1 TV");
        else snprintf(seen, sizeof(seen), "%d TVs", g_tv_n);
        detail(seen);
        touch();
    } else if (has_uuid && !g_tv[slot].uuid) {
        copy_cap(g_tv[slot].id, sizeof(g_tv[slot].id), id);
        g_tv[slot].uuid = true;
    }
    if (loc && loc[0] && !g_tv[slot].loc[0]) copy_cap(g_tv[slot].loc, sizeof(g_tv[slot].loc), loc);
    if (dial && dial[0] && !g_tv[slot].dial[0]) copy_cap(g_tv[slot].dial, sizeof(g_tv[slot].dial), dial);
}

static void parse_ssdp(const char* pkt) {
    if (strncmp(pkt, "HTTP/1.1", 8) != 0 && strncmp(pkt, "NOTIFY", 6) != 0) return;
    char loc[120], st[80], server[80], dial[80], usn[80];
    loc[0] = st[0] = server[0] = dial[0] = usn[0] = 0;
    hdr_get(pkt, "LOCATION", loc, sizeof(loc));
    hdr_get(pkt, "ST", st, sizeof(st));
    if (!st[0]) hdr_get(pkt, "NT", st, sizeof(st));
    hdr_get(pkt, "SERVER", server, sizeof(server));
    hdr_get(pkt, "USN", usn, sizeof(usn));
    hdr_get(pkt, "Application-URL", dial, sizeof(dial));
    char blob[180];
    snprintf(blob, sizeof(blob), "%s %s", st, usn);
    if (!looks_useful(blob, server, dial)) return;
    const char* from = loc[0] ? loc : dial;
    char ip[16];
    host_of(from, ip, sizeof(ip));
    if (!ip[0]) return;
    note_device(ip, loc, server, dial, usn);
}

static void stop_udp() {
    if (!g_udp_on) return;
    g_udp.stop();
    g_udp_on = false;
}

struct XmlGrab {
    uint8_t st;
    bool name_done;
    char buf[88];
    uint8_t n;
    uint8_t cap;
    char friendly[40];
    char maker[24];
    char model[24];
    int bytes;
    bool stop;
};

static XmlGrab g_xml;

static bool tag_is(const char* tag, const char* want) {
    const char* s = tag;
    if (*s == '/') ++s;
    const char* colon = strrchr(s, ':');
    if (colon) s = colon + 1;
    return strcasecmp(s, want) == 0;
}

static void xml_finish_text() {
    g_xml.buf[g_xml.n] = 0;
    if (g_xml.cap == 1 && !g_xml.friendly[0]) copy_text(g_xml.friendly, sizeof(g_xml.friendly), g_xml.buf);
    else if (g_xml.cap == 2 && !g_xml.maker[0]) copy_text(g_xml.maker, sizeof(g_xml.maker), g_xml.buf);
    else if (g_xml.cap == 3 && !g_xml.model[0]) copy_text(g_xml.model, sizeof(g_xml.model), g_xml.buf);
    g_xml.cap = 0;
    g_xml.n = 0;
}

static void xml_finish_tag() {
    g_xml.buf[g_xml.n] = 0;
    g_xml.cap = 0;
    if (g_xml.buf[0] != '/') {
        if (tag_is(g_xml.buf, "friendlyName")) g_xml.cap = 1;
        else if (tag_is(g_xml.buf, "manufacturer")) g_xml.cap = 2;
        else if (tag_is(g_xml.buf, "modelName")) g_xml.cap = 3;
    }
    g_xml.n = 0;
    g_xml.st = g_xml.cap ? 2 : 0;
}

static void xml_byte(char c) {
    if (g_xml.st == 0) {
        if (c == '<') {
            g_xml.st = 1;
            g_xml.n = 0;
            g_xml.name_done = false;
        }
        return;
    }
    if (g_xml.st == 1) {
        if (c == '>') {
            xml_finish_tag();
            return;
        }
        if (!g_xml.name_done) {
            if (c == ' ' || c == '\t' || (c == '/' && g_xml.n > 0)) g_xml.name_done = true;
            else if (g_xml.n < 60) g_xml.buf[g_xml.n++] = c;
        }
        return;
    }
    if (c == '<') {
        xml_finish_text();
        g_xml.st = 1;
        g_xml.n = 0;
        g_xml.name_done = false;
        return;
    }
    if (g_xml.n < sizeof(g_xml.buf) - 1) g_xml.buf[g_xml.n++] = c;
}

class XmlFeed : public Stream {
public:
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
    size_t write(uint8_t c) override {
        if (!g_xml.stop) xml_byte((char)c);
        if (++g_xml.bytes > 24000) g_xml.stop = true;
        if (g_xml.friendly[0] && g_xml.maker[0]) g_xml.stop = true;
        return 1;
    }
};

static void fetch_one(int index) {
    Tv& tv = g_tv[index];
    if (!tv.loc[0] || !strncmp(tv.loc, "https://", 8)) return;
    memset(&g_xml, 0, sizeof(g_xml));
    HTTPClient http;
    http.setReuse(false);
    http.setConnectTimeout(1200);
    http.setTimeout(1500);
    http.useHTTP10(true);
    if (!http.begin(tv.loc)) return;
    if (http.GET() == HTTP_CODE_OK) {
        XmlFeed feed;
        http.writeToStream(&feed);
    }
    http.end();
    if (g_xml.friendly[0]) copy_text(tv.name, sizeof(tv.name), g_xml.friendly);
    char brand[16];
    guess_brand(brand, sizeof(brand), g_xml.friendly, g_xml.maker, g_xml.model);
    if (!brand[0]) guess_brand(brand, sizeof(brand), tv.brand, tv.name, "");
    if (brand[0]) copy_cap(tv.brand, sizeof(tv.brand), brand);
    if (!tv.name[0] && tv.brand[0]) copy_cap(tv.name, sizeof(tv.name), tv.brand);
}

static void open_tv(int index);

static void show_results() {
    int w = 0;
    for (int i = 0; i < g_tv_n; ++i) {
        if (!g_tv[i].dial[0] && !g_tv[i].brand[0]) continue;
        if (!g_tv[i].name[0]) copy_cap(g_tv[i].name, sizeof(g_tv[i].name), g_tv[i].brand[0] ? g_tv[i].brand : "TV");
        if (w != i) g_tv[w] = g_tv[i];
        ++w;
    }
    g_tv_n = w;
    g_sel = 0;
    g_chosen = 0;
    if (g_tv_n <= 0) {
        g_view = CAST_EMPTY;
        detail("Same Wi-Fi as the TV");
    } else {
        g_view = CAST_LIST;
        char buf[24];
        if (g_tv_n == 1) copy_cap(buf, sizeof(buf), "1 TV");
        else snprintf(buf, sizeof(buf), "%d TVs", g_tv_n);
        detail(buf);
    }
    touch();
}

static void start_seek() {
    stop_udp();
    memset(g_tv, 0, sizeof(g_tv));
    g_tv_n = 0;
    g_fetch_i = 0;
    g_fetch_arm = false;
    g_udp_on = g_udp.beginMulticast(IPAddress(239, 255, 255, 250), 1900) != 0;
    if (!g_udp_on) g_udp_on = g_udp.begin(1900) != 0;
    g_seek_at = millis();
    g_seek_wave = 1;
    g_view = CAST_SEEK;
    detail("Searching");
    touch();
}

static void start_join() {
    stop_udp();
    WiFi.setSleep(WIFI_PS_NONE);
    WiFi.mode(WIFI_STA);
    if (g_pass[0]) WiFi.begin(g_ssid, g_pass);
    else WiFi.begin(g_ssid);
    g_join_at = millis();
    g_view = CAST_JOIN;
    detail("");
    touch();
}

static void fill_acts() {
    g_act_n = 0;
    copy_cap(g_act[g_act_n].label, sizeof(g_act[0].label), "Wi-Fi Remote");
    g_act[g_act_n].kind = ACT_WIFI;
    g_act[g_act_n].link = 0;
    ++g_act_n;
    copy_cap(g_act[g_act_n].label, sizeof(g_act[0].label), "Rickroll Attack");
    g_act[g_act_n].kind = ACT_RICK;
    g_act[g_act_n].link = 0;
    ++g_act_n;
    copy_cap(g_act[g_act_n].label, sizeof(g_act[0].label), "YouTube Attack");
    g_act[g_act_n].kind = ACT_CUSTOM;
    g_act[g_act_n].link = 0;
    ++g_act_n;
    copy_cap(g_act[g_act_n].label, sizeof(g_act[0].label), "IR remote");
    g_act[g_act_n].kind = ACT_IR;
    g_act[g_act_n].link = 0;
    ++g_act_n;
}

static void fill_links() {
    g_act_n = 0;
    int clips = clip_count();
    for (int i = 0; i < clips && g_act_n < 10; ++i) {
        char name[24];
        char url[96];
        if (!clip_at(i, name, sizeof(name), url, sizeof(url))) continue;
        copy_cap(g_act[g_act_n].label, sizeof(g_act[0].label), name);
        g_act[g_act_n].kind = ACT_CLIP;
        g_act[g_act_n].link = (uint8_t)i;
        ++g_act_n;
    }
    copy_cap(g_act[g_act_n].label, sizeof(g_act[0].label), "Add link");
    g_act[g_act_n].kind = ACT_ADD;
    g_act[g_act_n].link = 0;
    ++g_act_n;
}

static void open_links() {
    fill_links();
    g_sel = 0;
    g_view = CAST_LINKS;
    detail(clip_count() > 0 ? "" : "No videos yet");
    touch();
}

static bool id_chars(const char* s, int n) {
    if (n < 6 || n > 16) return false;
    for (int i = 0; i < n; ++i) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    return true;
}

static bool youtube_id(const char* url, char* dst, int cap) {
    dst[0] = 0;
    if (!url || !url[0]) return false;
    const char* marks[] = {"youtu.be/", "shorts/", "embed/", "live/", "v="};
    const char* p = nullptr;
    for (int m = 0; m < 5 && !p; ++m) {
        const char* hit = strstr(url, marks[m]);
        if (hit) p = hit + strlen(marks[m]);
    }
    if (!p) p = url;
    int i = 0;
    while (p[i] && p[i] != '&' && p[i] != '?' && p[i] != '/' && i < cap - 1 && i < 16) {
        dst[i] = p[i];
        ++i;
    }
    dst[i] = 0;
    return id_chars(dst, i);
}

static bool send_youtube(const char* video_url) {
    if (g_chosen < 0 || g_chosen >= g_tv_n || !g_tv[g_chosen].dial[0]) return false;
    char url[110];
    copy_cap(url, sizeof(url), g_tv[g_chosen].dial);
    int n = (int)strlen(url);
    if (n > 0 && url[n - 1] != '/' && n < (int)sizeof(url) - 1) {
        url[n++] = '/';
        url[n] = 0;
    }
    strncat(url, "YouTube", sizeof(url) - strlen(url) - 1);
    char body[24];
    body[0] = 0;
    char id[20];
    if (video_url && youtube_id(video_url, id, sizeof(id))) snprintf(body, sizeof(body), "v=%s", id);
    HTTPClient http;
    http.setReuse(false);
    http.setConnectTimeout(1500);
    http.setTimeout(2000);
    if (!http.begin(url)) return false;
    http.addHeader("Content-Type", "text/plain");
    http.addHeader("Origin", "https://www.youtube.com");
    int code = http.POST((uint8_t*)body, strlen(body));
    http.end();
    return code == HTTP_CODE_OK || code == HTTP_CODE_CREATED;
}

static void open_tv(int index) {
    g_chosen = index;
    g_sel = 0;
    fill_acts();
    g_view = CAST_TV;
    detail("");
    touch();
}

static int ap_rank(const Ap& ap) {
    const char* recent = wifi_recent_ssid();
    if (recent && recent[0] && !strcmp(ap.ssid, recent)) return 0;
    if (ap.known) return 1;
    return 2;
}

static void sort_aps() {
    for (int i = 0; i < g_ap_n; ++i) {
        for (int j = i + 1; j < g_ap_n; ++j) {
            bool swap = false;
            int ri = ap_rank(g_ap[i]);
            int rj = ap_rank(g_ap[j]);
            if (rj < ri) swap = true;
            else if (rj == ri && g_ap[j].rssi > g_ap[i].rssi) swap = true;
            if (swap) {
                Ap tmp = g_ap[i];
                g_ap[i] = g_ap[j];
                g_ap[j] = tmp;
            }
        }
    }
    g_sel = 0;
}

static void take_scan() {
    int n = WiFi.scanComplete();
    if (n < 0) n = 0;
    g_ap_n = 0;
    for (int i = 0; i < n && g_ap_n < kApMax; ++i) {
        String s = WiFi.SSID(i);
        if (!s.length() || s.length() > 32) continue;
        int slot = -1;
        for (int j = 0; j < g_ap_n; ++j) {
            if (!strcmp(g_ap[j].ssid, s.c_str())) slot = j;
        }
        int8_t rssi = (int8_t)WiFi.RSSI(i);
        if (slot >= 0) {
            if (rssi > g_ap[slot].rssi) g_ap[slot].rssi = rssi;
            continue;
        }
        Ap& ap = g_ap[g_ap_n++];
        copy_cap(ap.ssid, sizeof(ap.ssid), s.c_str());
        ap.rssi = rssi;
        char pass[65];
        ap.known = wifi_lookup(ap.ssid, pass, sizeof(pass));
    }
    WiFi.scanDelete();
    if (g_ap_n == 0 && g_scan_try < 1) {
        g_scan_try++;
        g_scanning = true;
        if (WiFi.scanNetworks(true) == WIFI_SCAN_FAILED) {
            g_scanning = false;
            detail("No networks");
        } else {
            detail("Scanning");
        }
        touch();
        return;
    }
    sort_aps();
    g_scanning = false;
    if (g_ap_n == 0) detail("No networks");
    else if (g_ap[0].known) detail("Saved. Press OK");
    else detail("Pick a network");
    touch();
}

static void poll_seek() {
    if (!g_udp_on && g_seek_wave < 3) {
        g_view = CAST_EMPTY;
        detail("Radio is off");
        touch();
        return;
    }
    if (g_seek_wave >= 3) return;
    int budget = 24;
    while (budget-- > 0) {
        int n = g_udp.parsePacket();
        if (n <= 0) break;
        char buf[768];
        if (n > (int)sizeof(buf) - 1) n = (int)sizeof(buf) - 1;
        int got = g_udp.read(buf, n);
        if (got < 0) got = 0;
        buf[got] = 0;
        parse_ssdp(buf);
    }
    uint32_t gone = millis() - g_seek_at;
    if ((g_tv_n > 0 && gone > kSeekFast) || gone > kSeekMax) {
        stop_udp();
        g_seek_wave = 3;
        g_fetch_i = 0;
        g_fetch_arm = false;
        if (g_tv_n <= 0) {
            g_view = CAST_EMPTY;
            detail("Same Wi-Fi as the TV");
        } else {
            char line[28];
            snprintf(line, sizeof(line), "Reading 1/%d", g_tv_n);
            detail(line);
        }
        touch();
    }
}

static const Tv* focus_tv() {
    int i = (g_view == CAST_LIST) ? g_sel : g_chosen;
    if (i < 0 || i >= g_tv_n) return nullptr;
    return &g_tv[i];
}

void cast_ask_wifi() {
    if (!g_on) return;
    stop_udp();
    WiFi.scanDelete();
    WiFi.disconnect(false);
    WiFi.mode(WIFI_STA);
    delay(100);
    g_ap_n = 0;
    g_sel = 0;
    g_view = CAST_NETS;
    g_scan_try = 0;
    g_scanning = true;
    if (WiFi.scanNetworks(true) == WIFI_SCAN_FAILED) {
        g_scanning = false;
        detail("No networks");
        touch();
        return;
    }
    detail("Scanning");
    touch();
}

void cast_enter() {
    g_on = true;
    WiFi.persistent(false);
    WiFi.setSleep(WIFI_PS_NONE);
    g_ssid[0] = 0;
    g_pass[0] = 0;
    g_field[0] = 0;
    cast_ask_wifi();
}

void cast_leave() {
    wifi_remote_end();
    stop_udp();
    WiFi.scanDelete();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    g_on = false;
    g_scanning = false;
    g_view = CAST_NETS;
    g_tv_n = 0;
    g_ap_n = 0;
    delay(40);
}

void cast_poll() {
    if (!g_on) return;
    if (g_view == CAST_PAD) {
        if (wifi_remote_poll()) touch();
        return;
    }
    if (g_view == CAST_NETS && g_scanning) {
        if (WiFi.scanComplete() != -1) take_scan();
        return;
    }
    if (g_view == CAST_JOIN) {
        if (WiFi.status() == WL_CONNECTED) {
            wifi_remember(g_ssid, g_pass);
            start_seek();
            return;
        }
        if (millis() - g_join_at > 15000) {
            g_view = CAST_FAIL;
            detail("No reply");
            touch();
        }
        return;
    }
    if (g_view == CAST_SEEK) {
        poll_seek();
        if (g_view == CAST_SEEK && g_seek_wave >= 3 && g_tv_n > 0) {
            if (!g_fetch_arm) {
                char buf[28];
                snprintf(buf, sizeof(buf), "Reading %d/%d", g_fetch_i + 1, g_tv_n);
                detail(buf);
                g_fetch_arm = true;
                return;
            }
            fetch_one(g_fetch_i);
            g_fetch_arm = false;
            ++g_fetch_i;
            if (g_fetch_i >= g_tv_n) show_results();
        }
    }
}

bool cast_active() { return g_on; }

bool cast_busy() {
    return g_on && (g_scanning || g_view == CAST_JOIN || g_view == CAST_SEEK);
}

bool cast_take_touch() {
    bool was = g_touch;
    g_touch = false;
    return was;
}

CastView cast_view() { return g_view; }
const char* cast_ssid() { return g_ssid; }
const char* cast_detail() { return g_detail; }

int cast_progress() {
    if (g_view == CAST_JOIN) {
        uint32_t gone = millis() - g_join_at;
        if (gone > 15000) return 100;
        return (int)(gone * 100UL / 15000UL);
    }
    if (g_view == CAST_SEEK && g_udp_on) {
        uint32_t gone = millis() - g_seek_at;
        if (gone > kSeekMax) return 100;
        return (int)(gone * 100UL / kSeekMax);
    }
    return -1;
}

int cast_sel() { return g_sel; }

int cast_count() {
    if (g_view == CAST_NETS) return g_ap_n;
    if (g_view == CAST_LIST) return g_tv_n;
    if (g_view == CAST_TV || g_view == CAST_LINKS) return g_act_n;
    return 0;
}

void cast_row(int index, CastRow* out) {
    if (!out) return;
    out->title[0] = out->sub[0] = out->extra[0] = 0;
    if (g_view == CAST_NETS) {
        if (index < 0 || index >= g_ap_n) return;
        copy_cap(out->title, sizeof(out->title), g_ap[index].ssid);
        if (g_ap[index].known) copy_cap(out->sub, sizeof(out->sub), "saved");
        return;
    }
    if (g_view == CAST_LIST) {
        if (index < 0 || index >= g_tv_n) return;
        copy_cap(out->title, sizeof(out->title), g_tv[index].name);
        if (g_tv[index].brand[0] && strcasecmp(g_tv[index].brand, g_tv[index].name) != 0) {
            copy_cap(out->sub, sizeof(out->sub), g_tv[index].brand);
        }
        copy_cap(out->extra, sizeof(out->extra), g_tv[index].ip);
        return;
    }
    if (g_view == CAST_TV || g_view == CAST_LINKS) {
        if (index < 0 || index >= g_act_n) return;
        copy_cap(out->title, sizeof(out->title), g_act[index].label);
    }
}

const char* cast_tv_brand() {
    const Tv* tv = focus_tv();
    return tv ? tv->brand : "";
}
const char* cast_tv_name() {
    const Tv* tv = focus_tv();
    return tv ? tv->name : "";
}
const char* cast_tv_ip() {
    const Tv* tv = focus_tv();
    return tv ? tv->ip : "";
}

void cast_retry() {
    if (!g_on) return;
    if (g_view == CAST_FAIL) {
        start_join();
        return;
    }
    if (g_view == CAST_NETS) {
        cast_ask_wifi();
        return;
    }
    if (WiFi.status() == WL_CONNECTED) start_seek();
    else cast_ask_wifi();
}

void cast_move_xy(int dx, int dy) {
    if (!g_on || g_view != CAST_TV || g_act_n <= 0) {
        if (dy) cast_move(dy);
        else if (dx) cast_move(dx);
        return;
    }
    int cols = 2;
    int rows = (g_act_n + cols - 1) / cols;
    int col = g_sel % cols;
    int row = g_sel / cols;
    if (dx) col = (col + dx + cols) % cols;
    if (dy) row = (row + dy + rows) % rows;
    int next = row * cols + col;
    if (next >= g_act_n) next = g_act_n - 1;
    if (next != g_sel) {
        g_sel = next;
        touch();
    }
}

void cast_choose(int index) {
    int n = cast_count();
    if (index < 0 || index >= n || index == g_sel) return;
    g_sel = index;
    touch();
}

void cast_move(int dir) {
    int n = cast_count();
    if (n <= 0) return;
    int next = g_sel + dir;
    if (next < 0) next = n - 1;
    if (next >= n) next = 0;
    if (next != g_sel) {
        g_sel = next;
        touch();
    }
}

bool cast_typing() {
    return g_view == CAST_KEY || g_view == CAST_URL || g_view == CAST_LABEL;
}

const char* cast_field() { return g_field; }

void cast_type(char c) {
    if (!cast_typing()) return;
    if (c < 32 || c > 126) return;
    int cap = g_view == CAST_KEY ? 64 : g_view == CAST_LABEL ? 20 : 90;
    int n = (int)strlen(g_field);
    if (n >= cap) return;
    g_field[n] = c;
    g_field[n + 1] = 0;
    touch();
}

void cast_backspace() {
    int n = (int)strlen(g_field);
    if (n <= 0) return;
    g_field[n - 1] = 0;
    touch();
}

bool cast_drop() {
    if (g_view != CAST_LINKS || g_sel < 0 || g_sel >= g_act_n) return false;
    if (g_act[g_sel].kind != ACT_CLIP) return false;
    if (!clip_remove(g_act[g_sel].link)) return false;
    int stay = g_sel;
    fill_links();
    if (stay >= g_act_n) stay = g_act_n - 1;
    if (stay < 0) stay = 0;
    g_sel = stay;
    detail(clip_count() > 0 ? "" : "No videos yet");
    touch();
    return true;
}

bool cast_back() {
    if (g_view == CAST_LABEL) {
        copy_cap(g_field, sizeof(g_field), g_draft);
        g_view = CAST_URL;
        touch();
        return true;
    }
    if (g_view == CAST_URL) {
        open_links();
        return true;
    }
    if (g_view == CAST_LINKS) {
        open_tv(g_chosen);
        g_sel = 2;
        touch();
        return true;
    }
    if (g_view == CAST_PAD) {
        wifi_remote_end();
        open_tv(g_chosen);
        g_sel = 0;
        touch();
        return true;
    }
    if (g_view == CAST_KEY) {
        g_field[0] = 0;
        g_view = CAST_NETS;
        touch();
        return true;
    }
    if (g_view == CAST_SENT) {
        if (g_from_links) open_links();
        else open_tv(g_chosen);
        return true;
    }
    if (g_view == CAST_TV) {
        g_sel = g_chosen;
        g_view = g_tv_n > 0 ? CAST_LIST : CAST_EMPTY;
        touch();
        return true;
    }
    if (g_view != CAST_NETS) {
        cast_ask_wifi();
        return true;
    }
    return false;
}

static void send_act() {
    const char* url = "";
    char stored[96];
    stored[0] = 0;
    if (g_act[g_sel].kind == ACT_RICK) {
        url = "https://www.youtube.com/watch?v=dQw4w9WgXcQ";
        g_from_links = false;
    } else if (g_act[g_sel].kind == ACT_CLIP) {
        g_from_links = true;
        char name[24];
        if (clip_at(g_act[g_sel].link, name, sizeof(name), stored, sizeof(stored))) url = stored;
    } else return;
    bool ok = send_youtube(url);
    g_view = CAST_SENT;
    detail(ok ? "Video sent" : "TV did not open");
    touch();
}

CastResult cast_accept() {
    if (!g_on) return CAST_STAY;
    if (g_view == CAST_KEY) {
        copy_cap(g_pass, sizeof(g_pass), g_field);
        g_field[0] = 0;
        start_join();
        return CAST_STAY;
    }
    if (g_view == CAST_URL) {
        if (!g_field[0]) return CAST_STAY;
        copy_cap(g_draft, sizeof(g_draft), g_field);
        g_field[0] = 0;
        g_view = CAST_LABEL;
        touch();
        return CAST_STAY;
    }
    if (g_view == CAST_LABEL) {
        const char* name = g_field[0] ? g_field : "Video";
        bool ok = clip_add(name, g_draft);
        g_field[0] = 0;
        g_draft[0] = 0;
        if (!ok) {
            open_links();
            detail("List full");
            touch();
            return CAST_STAY;
        }
        open_links();
        g_sel = clip_count() - 1;
        if (g_sel < 0) g_sel = 0;
        touch();
        return CAST_STAY;
    }
    if (g_view == CAST_NETS) {
        if (g_scanning || g_sel < 0 || g_sel >= g_ap_n) return CAST_STAY;
        const Ap& ap = g_ap[g_sel];
        copy_cap(g_ssid, sizeof(g_ssid), ap.ssid);
        if (!wifi_lookup(ap.ssid, g_pass, sizeof(g_pass))) {
            g_pass[0] = 0;
            g_field[0] = 0;
            g_view = CAST_KEY;
            touch();
            return CAST_STAY;
        }
        start_join();
        return CAST_STAY;
    }
    if (g_view == CAST_FAIL) {
        start_join();
        return CAST_STAY;
    }
    if (g_view == CAST_LIST) {
        if (g_tv_n <= 0) return CAST_STAY;
        open_tv(g_sel);
        return CAST_STAY;
    }
    if (g_view == CAST_EMPTY) {
        if (WiFi.status() == WL_CONNECTED) start_seek();
        return CAST_STAY;
    }
    if (g_view == CAST_TV) {
        if (g_sel < 0 || g_sel >= g_act_n) return CAST_STAY;
        if (g_act[g_sel].kind == ACT_IR) return CAST_OPEN_IR;
        if (g_act[g_sel].kind == ACT_WIFI) {
            const Tv* tv = focus_tv();
            if (!tv || !tv->ip[0]) return CAST_STAY;
            g_view = CAST_PAD;
            wifi_remote_begin(tv->ip, tv->brand);
            touch();
            return CAST_STAY;
        }
        if (g_act[g_sel].kind == ACT_CUSTOM) {
            open_links();
            return CAST_STAY;
        }
        send_act();
        return CAST_STAY;
    }
    if (g_view == CAST_LINKS) {
        if (g_sel < 0 || g_sel >= g_act_n) return CAST_STAY;
        if (g_act[g_sel].kind == ACT_ADD) {
            g_field[0] = 0;
            g_draft[0] = 0;
            g_view = CAST_URL;
            touch();
            return CAST_STAY;
        }
        send_act();
    }
    return CAST_STAY;
}
