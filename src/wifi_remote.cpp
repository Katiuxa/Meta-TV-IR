#include "wifi_remote.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "ir_database.h"

// Local TV remotes. Button names are the published ones for each brand.
// Samsung and LG speak WebSocket. Sony, Roku, Panasonic and Philips speak HTTP.
// The Allow / PIN step is theirs. This device only keeps the token they return.

enum Kind : uint8_t {
    KIND_NONE = 0,
    KIND_SAMSUNG,
    KIND_LG,
    KIND_SONY,
    KIND_ROKU,
    KIND_PANASONIC,
    KIND_PHILIPS,
};

static constexpr char kNv[] = "metatvkey";

static WifiPhase g_phase = WIFI_PHASE_OFF;
static Kind g_kind = KIND_NONE;
static char g_ip[16];
static char g_secret[180];
static char g_pin[10];
static char g_tx[640];
static char g_rx[512];
static char g_path[240];
static uint32_t g_gen = 0;
static uint32_t g_arm_at = 0;
static bool g_started = false;
static bool g_ptr_asked = false;
static bool g_reopen = false;
static bool g_reopen_used = false;

static WiFiClient g_plain;
static WiFiClient g_plain2;
static WiFiClientSecure g_sec;
static WiFiClientSecure g_sec2;
static WiFiClient* g_ctl = nullptr;
static WiFiClient* g_btn = nullptr;

static void copy_cap(char* dst, int cap, const char* src) {
    if (!dst || cap <= 0) return;
    if (!src) src = "";
    int i = 0;
    for (; src[i] && i < cap - 1; ++i) dst[i] = src[i];
    dst[i] = 0;
}

static void set_phase(WifiPhase phase) {
    if (g_phase == phase) return;
    g_phase = phase;
}

static Kind kind_of(const char* brand) {
    if (!brand || !brand[0]) return KIND_NONE;
    if (!strcasecmp(brand, "Samsung")) return KIND_SAMSUNG;
    if (!strcasecmp(brand, "LG")) return KIND_LG;
    if (!strcasecmp(brand, "Sony")) return KIND_SONY;
    if (!strcasecmp(brand, "Roku")) return KIND_ROKU;
    if (!strcasecmp(brand, "Panasonic")) return KIND_PANASONIC;
    if (!strcasecmp(brand, "Philips")) return KIND_PHILIPS;
    return KIND_NONE;
}

static void nvs_id(char* out, int cap, char kind) {
    int a = 0, b = 0, c = 0, d = 0;
    sscanf(g_ip, "%d.%d.%d.%d", &a, &b, &c, &d);
    snprintf(out, cap, "%c%03d%03d%03d%03d", kind, a, b, c, d);
}

static void secret_load(char kind) {
    g_secret[0] = 0;
    char key[16];
    nvs_id(key, sizeof(key), kind);
    Preferences prefs;
    if (!prefs.begin(kNv, true)) return;
    String value = prefs.getString(key, "");
    copy_cap(g_secret, sizeof(g_secret), value.c_str());
    prefs.end();
}

static void secret_save(char kind, const char* value) {
    char key[16];
    nvs_id(key, sizeof(key), kind);
    Preferences prefs;
    if (!prefs.begin(kNv, false)) return;
    if (!value || !value[0]) prefs.remove(key);
    else prefs.putString(key, value);
    prefs.end();
}

static bool grab(const char* json, const char* key, char* dst, int cap) {
    if (dst && cap > 0) dst[0] = 0;
    if (!json || !key) return false;
    char pat[28];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(json, pat);
    if (!p) return false;
    p += strlen(pat);
    while (*p == ' ' || *p == ':') ++p;
    if (*p != '"') return false;
    ++p;
    int i = 0;
    while (*p && *p != '"' && i < cap - 1) {
        dst[i++] = *p++;
    }
    dst[i] = 0;
    return i > 0;
}

static void b64(const uint8_t* in, int n, char* out, int cap) {
    static const char* tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int j = 0;
    for (int i = 0; i < n && j < cap - 1; i += 3) {
        int v = in[i] << 16;
        if (i + 1 < n) v |= in[i + 1] << 8;
        if (i + 2 < n) v |= in[i + 2];
        if (j < cap - 1) out[j++] = tab[(v >> 18) & 63];
        if (j < cap - 1) out[j++] = tab[(v >> 12) & 63];
        if (j < cap - 1) out[j++] = (i + 1 < n) ? tab[(v >> 6) & 63] : '=';
        if (j < cap - 1) out[j++] = (i + 2 < n) ? tab[v & 63] : '=';
    }
    out[j] = 0;
}

static void stop_all() {
    g_plain.stop();
    g_plain2.stop();
    g_sec.stop();
    g_sec2.stop();
    g_ctl = nullptr;
    g_btn = nullptr;
}

static WiFiClient* client_for(bool tls, bool second) {
    if (!tls) return second ? &g_plain2 : &g_plain;
    WiFiClientSecure& sec = second ? g_sec2 : g_sec;
    sec.setInsecure();
    return &sec;
}

static bool ws_send_op(WiFiClient& client, uint8_t opcode, const uint8_t* data, int n) {
    if (n < 0) n = 0;
    uint8_t mask[4] = {0x13, 0x57, 0x9B, 0xDF};
    uint8_t hdr[8];
    int h = 0;
    hdr[h++] = 0x80 | opcode;
    if (n < 126) {
        hdr[h++] = (uint8_t)(0x80 | n);
    } else {
        hdr[h++] = 0x80 | 126;
        hdr[h++] = (uint8_t)((n >> 8) & 0xFF);
        hdr[h++] = (uint8_t)(n & 0xFF);
    }
    for (int i = 0; i < 4; ++i) hdr[h++] = mask[i];
    if (client.write(hdr, h) != (size_t)h) return false;
    uint8_t chunk[64];
    int off = 0;
    while (off < n) {
        int m = n - off;
        if (m > (int)sizeof(chunk)) m = (int)sizeof(chunk);
        for (int i = 0; i < m; ++i) chunk[i] = data[off + i] ^ mask[(off + i) & 3];
        if (client.write(chunk, m) != (size_t)m) return false;
        off += m;
    }
    return true;
}

static bool ws_send(WiFiClient& client, const char* text) {
    if (!text) text = "";
    return ws_send_op(client, 0x1, (const uint8_t*)text, (int)strlen(text));
}

static bool read_until_headers(WiFiClient& client) {
    int n = 0;
    uint32_t start = millis();
    while (millis() - start < 1200 && n < (int)sizeof(g_rx) - 1) {
        if (!client.connected() && !client.available()) break;
        if (!client.available()) {
            delay(4);
            continue;
        }
        g_rx[n++] = (char)client.read();
        if (n >= 4 && g_rx[n - 4] == '\r' && g_rx[n - 3] == '\n' && g_rx[n - 2] == '\r' && g_rx[n - 1] == '\n') {
            g_rx[n] = 0;
            return strstr(g_rx, " 101 ") != nullptr;
        }
    }
    return false;
}

static bool ws_open(WiFiClient& client, const char* host, uint16_t port, const char* path) {
    client.stop();
    if (!client.connect(host, port, 1200)) return false;
    int wrote = snprintf(g_tx, sizeof(g_tx),
                         "GET %s HTTP/1.1\r\n"
                         "Host: %s:%u\r\n"
                         "Upgrade: websocket\r\n"
                         "Connection: Upgrade\r\n"
                         "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                         "Sec-WebSocket-Version: 13\r\n"
                         "Origin: http://%s:%u\r\n\r\n",
                         path, host, (unsigned)port, host, (unsigned)port);
    if (wrote <= 0 || wrote >= (int)sizeof(g_tx)) {
        client.stop();
        return false;
    }
    if (client.write((const uint8_t*)g_tx, wrote) != (size_t)wrote) {
        client.stop();
        return false;
    }
    if (!read_until_headers(client)) {
        client.stop();
        return false;
    }
    return true;
}

static int ws_recv(WiFiClient& client, char* out, int cap) {
    if (out && cap > 0) out[0] = 0;
    if (client.available() < 2) {
        if (!client.connected()) return -1;
        return 0;
    }
    int b0 = client.read();
    int b1 = client.read();
    if (b0 < 0 || b1 < 0) return -1;
    int opcode = b0 & 0x0F;
    bool masked = (b1 & 0x80) != 0;
    int len = b1 & 0x7F;
    if (len == 127) return -1;
    if (len == 126) {
        uint32_t start = millis();
        while (client.available() < 2 && millis() - start < 250) delay(2);
        if (client.available() < 2) return -1;
        len = (client.read() << 8) | client.read();
    }
    uint8_t mask[4] = {0, 0, 0, 0};
    if (masked) {
        uint32_t start = millis();
        while (client.available() < 4 && millis() - start < 250) delay(2);
        if (client.available() < 4) return -1;
        for (int i = 0; i < 4; ++i) mask[i] = (uint8_t)client.read();
    }
    int keep = len;
    if (keep > cap - 1) keep = cap - 1;
    int got = 0;
    uint32_t start = millis();
    while (got < len && millis() - start < 400) {
        if (!client.available()) {
            delay(2);
            continue;
        }
        int ch = client.read();
        if (ch < 0) break;
        if (got < keep && out) out[got] = (char)(ch ^ (masked ? mask[got & 3] : 0));
        ++got;
    }
    if (out && cap > 0) out[got < keep ? got : keep] = 0;
    if (opcode == 0x8) return -1;
    if (opcode == 0x9) {
        ws_send_op(client, 0xA, (const uint8_t*)out, keep);
        if (out && cap > 0) out[0] = 0;
        return 0;
    }
    if (opcode != 0x1) {
        if (out && cap > 0) out[0] = 0;
        return 0;
    }
    return keep;
}

static int http_req(const char* url, bool post, const char* ctype, const char* body,
                    const char* h1n, const char* h1v, const char* h2n, const char* h2v,
                    char* body_out, int body_cap, char* cookie_out, int cookie_cap) {
    if (body_out && body_cap > 0) body_out[0] = 0;
    if (cookie_out && cookie_cap > 0) cookie_out[0] = 0;
    HTTPClient http;
    http.setReuse(false);
    http.setConnectTimeout(900);
    http.setTimeout(1000);
    if (!http.begin(url)) return -1;
    if (ctype && ctype[0]) http.addHeader("Content-Type", ctype);
    if (h1n && h1v && h1v[0]) http.addHeader(h1n, h1v);
    if (h2n && h2v && h2v[0]) http.addHeader(h2n, h2v);
    if (cookie_out && cookie_cap > 0) {
        const char* keys[] = {"Set-Cookie"};
        http.collectHeaders(keys, 1);
    }
    int code;
    if (!post) code = http.GET();
    else code = http.POST((uint8_t*)(body ? body : ""), body ? strlen(body) : 0);
    if (code > 0 && cookie_out && cookie_cap > 0) {
        String cookie = http.header("Set-Cookie");
        copy_cap(cookie_out, cookie_cap, cookie.c_str());
        char* cut = strchr(cookie_out, ';');
        if (cut) *cut = 0;
    }
    if (code > 0 && body_out && body_cap > 0) {
        String text = http.getString();
        copy_cap(body_out, body_cap, text.c_str());
    }
    http.end();
    return code;
}

static const char* map_key(int key) {
    switch (g_kind) {
        case KIND_SAMSUNG:
            switch (key) {
                case KEY_POWER: return "KEY_POWER";
                case KEY_MUTE: return "KEY_MUTE";
                case KEY_VOL_UP: return "KEY_VOLUP";
                case KEY_VOL_DOWN: return "KEY_VOLDOWN";
                case KEY_CH_UP: return "KEY_CHUP";
                case KEY_CH_DOWN: return "KEY_CHDOWN";
                case KEY_SOURCE: return "KEY_SOURCE";
                case KEY_OK: return "KEY_ENTER";
                case KEY_UP: return "KEY_UP";
                case KEY_DOWN: return "KEY_DOWN";
                case KEY_LEFT: return "KEY_LEFT";
                case KEY_RIGHT: return "KEY_RIGHT";
                case KEY_BACK: return "KEY_RETURN";
                case KEY_MENU: return "KEY_HOME";
                default: return nullptr;
            }
        case KIND_LG:
            switch (key) {
                case KEY_POWER: return "POWER";
                case KEY_MUTE: return "MUTE";
                case KEY_VOL_UP: return "VOLUMEUP";
                case KEY_VOL_DOWN: return "VOLUMEDOWN";
                case KEY_CH_UP: return "CHANNELUP";
                case KEY_CH_DOWN: return "CHANNELDOWN";
                case KEY_SOURCE: return "INPUT_HUB";
                case KEY_OK: return "ENTER";
                case KEY_UP: return "UP";
                case KEY_DOWN: return "DOWN";
                case KEY_LEFT: return "LEFT";
                case KEY_RIGHT: return "RIGHT";
                case KEY_BACK: return "BACK";
                case KEY_MENU: return "HOME";
                default: return nullptr;
            }
        case KIND_ROKU:
            switch (key) {
                case KEY_MUTE: return "VolumeMute";
                case KEY_VOL_UP: return "VolumeUp";
                case KEY_VOL_DOWN: return "VolumeDown";
                case KEY_CH_UP: return "ChannelUp";
                case KEY_CH_DOWN: return "ChannelDown";
                case KEY_OK: return "Select";
                case KEY_UP: return "Up";
                case KEY_DOWN: return "Down";
                case KEY_LEFT: return "Left";
                case KEY_RIGHT: return "Right";
                case KEY_BACK: return "Back";
                case KEY_MENU: return "Home";
                default: return nullptr;
            }
        case KIND_PANASONIC:
            switch (key) {
                case KEY_POWER: return "NRC_POWER-ONOFF";
                case KEY_MUTE: return "NRC_MUTE-ONOFF";
                case KEY_VOL_UP: return "NRC_VOLUP-ONOFF";
                case KEY_VOL_DOWN: return "NRC_VOLDOWN-ONOFF";
                case KEY_CH_UP: return "NRC_CH_UP-ONOFF";
                case KEY_CH_DOWN: return "NRC_CH_DOWN-ONOFF";
                case KEY_SOURCE: return "NRC_CHG_INPUT-ONOFF";
                case KEY_OK: return "NRC_ENTER-ONOFF";
                case KEY_UP: return "NRC_UP-ONOFF";
                case KEY_DOWN: return "NRC_DOWN-ONOFF";
                case KEY_LEFT: return "NRC_LEFT-ONOFF";
                case KEY_RIGHT: return "NRC_RIGHT-ONOFF";
                case KEY_BACK: return "NRC_RETURN-ONOFF";
                case KEY_MENU: return "NRC_HOME-ONOFF";
                default: return nullptr;
            }
        case KIND_PHILIPS:
            switch (key) {
                case KEY_POWER: return "Standby";
                case KEY_MUTE: return "Mute";
                case KEY_VOL_UP: return "VolumeUp";
                case KEY_VOL_DOWN: return "VolumeDown";
                case KEY_CH_UP: return "ChannelStepUp";
                case KEY_CH_DOWN: return "ChannelStepDown";
                case KEY_SOURCE: return "Source";
                case KEY_OK: return "Confirm";
                case KEY_UP: return "CursorUp";
                case KEY_DOWN: return "CursorDown";
                case KEY_LEFT: return "CursorLeft";
                case KEY_RIGHT: return "CursorRight";
                case KEY_BACK: return "Back";
                case KEY_MENU: return "Home";
                default: return nullptr;
            }
        case KIND_SONY:
            switch (key) {
                case KEY_POWER: return "AAAAAQAAAAEAAAAVAw==";
                case KEY_SOURCE: return "AAAAAQAAAAEAAAAlAw==";
                case KEY_UP: return "AAAAAQAAAAEAAAB0Aw==";
                case KEY_DOWN: return "AAAAAQAAAAEAAAB1Aw==";
                case KEY_RIGHT: return "AAAAAQAAAAEAAAAzAw==";
                case KEY_LEFT: return "AAAAAQAAAAEAAAA0Aw==";
                case KEY_OK: return "AAAAAQAAAAEAAABlAw==";
                case KEY_BACK: return "AAAAAgAAAJcAAAAjAw==";
                case KEY_MENU: return "AAAAAQAAAAEAAABgAw==";
                case KEY_VOL_UP: return "AAAAAQAAAAEAAAASAw==";
                case KEY_VOL_DOWN: return "AAAAAQAAAAEAAAATAw==";
                case KEY_MUTE: return "AAAAAQAAAAEAAAAUAw==";
                case KEY_CH_UP: return "AAAAAQAAAAEAAAAQAw==";
                case KEY_CH_DOWN: return "AAAAAQAAAAEAAAARAw==";
                default: return nullptr;
            }
        default:
            return nullptr;
    }
}

static void append_query(char* dst, int cap, const char* src) {
    int n = (int)strlen(dst);
    for (int i = 0; src && src[i] && n < cap - 1; ++i) {
        unsigned char c = (unsigned char)src[i];
        bool raw = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        if (raw) {
            dst[n++] = (char)c;
            dst[n] = 0;
        } else if (n < cap - 4) {
            snprintf(dst + n, cap - n, "%%%02X", c);
            n = (int)strlen(dst);
        }
    }
}

static bool samsung_open(bool tls, uint16_t port) {
    g_path[0] = 0;
    snprintf(g_path, sizeof(g_path), "/api/v2/channels/samsung.remote.control?name=TWV0YVRW");
    if (g_secret[0]) {
        strncat(g_path, "&token=", sizeof(g_path) - strlen(g_path) - 1);
        append_query(g_path, sizeof(g_path), g_secret);
    }
    WiFiClient* client = client_for(tls, false);
    if (!ws_open(*client, g_ip, port, g_path)) return false;
    g_ctl = client;
    g_btn = client;
    return true;
}

static void lg_send_register() {
    if (!g_ctl) return;
    if (g_secret[0]) {
        snprintf(g_tx, sizeof(g_tx),
                 "{\"type\":\"register\",\"id\":\"reg0\",\"payload\":{\"forcePairing\":false,\"pairingType\":\"PROMPT\",\"client-key\":\"%s\",\"manifest\":{\"manifestVersion\":1,\"appVersion\":\"1.1\",\"permissions\":[\"LAUNCH\",\"CONTROL_AUDIO\",\"CONTROL_INPUT_TV\",\"CONTROL_POWER\",\"CONTROL_MOUSE_AND_KEYBOARD\",\"CONTROL_INPUT_TEXT\",\"READ_POWER_STATE\"]}}}",
                 g_secret);
    } else {
        snprintf(g_tx, sizeof(g_tx),
                 "{\"type\":\"register\",\"id\":\"reg0\",\"payload\":{\"forcePairing\":false,\"pairingType\":\"PROMPT\",\"manifest\":{\"manifestVersion\":1,\"appVersion\":\"1.1\",\"permissions\":[\"LAUNCH\",\"CONTROL_AUDIO\",\"CONTROL_INPUT_TV\",\"CONTROL_POWER\",\"CONTROL_MOUSE_AND_KEYBOARD\",\"CONTROL_INPUT_TEXT\",\"READ_POWER_STATE\"]}}}");
    }
    ws_send(*g_ctl, g_tx);
}

static bool parse_ws_url(const char* url, bool* tls, char* host, int host_cap, uint16_t* port, char* path, int path_cap) {
    *tls = false;
    *port = 3000;
    if (!strncmp(url, "wss://", 6)) {
        *tls = true;
        *port = 3001;
        url += 6;
    } else if (!strncmp(url, "ws://", 5)) {
        url += 5;
    } else return false;
    const char* slash = strchr(url, '/');
    const char* colon = strchr(url, ':');
    if (colon && (!slash || colon < slash)) {
        int n = (int)(colon - url);
        if (n >= host_cap) n = host_cap - 1;
        memcpy(host, url, n);
        host[n] = 0;
        *port = (uint16_t)atoi(colon + 1);
    } else {
        int n = slash ? (int)(slash - url) : (int)strlen(url);
        if (n >= host_cap) n = host_cap - 1;
        memcpy(host, url, n);
        host[n] = 0;
    }
    if (slash) copy_cap(path, path_cap, slash);
    else copy_cap(path, path_cap, "/");
    return host[0] != 0;
}

static bool lg_open_pointer(const char* socket_path) {
    bool tls = false;
    char host[48];
    char path[180];
    uint16_t port = 3000;
    if (!parse_ws_url(socket_path, &tls, host, sizeof(host), &port, path, sizeof(path))) return false;
    WiFiClient* client = client_for(tls, true);
    if (!ws_open(*client, host, port, path)) return false;
    g_btn = client;
    return true;
}

static void on_text(const char* msg) {
    if (!msg || !msg[0]) return;
    if (g_kind == KIND_SAMSUNG) {
        if (strstr(msg, "unauthorized")) {
            if (g_secret[0] && !g_reopen_used) {
                g_secret[0] = 0;
                secret_save('s', "");
                g_reopen = true;
                g_reopen_used = true;
            }
            set_phase(WIFI_PHASE_ALLOW);
            return;
        }
        char token[96];
        if (grab(msg, "token", token, sizeof(token))) {
            copy_cap(g_secret, sizeof(g_secret), token);
            secret_save('s', g_secret);
            set_phase(WIFI_PHASE_READY);
            return;
        }
        if (strstr(msg, "ms.channel.connect")) set_phase(WIFI_PHASE_READY);
        return;
    }
    if (g_kind != KIND_LG) return;
    char path[180];
    if (grab(msg, "socketPath", path, sizeof(path))) {
        if (lg_open_pointer(path)) set_phase(WIFI_PHASE_READY);
        else set_phase(WIFI_PHASE_DOWN);
        return;
    }
    char key[80];
    if (grab(msg, "client-key", key, sizeof(key))) {
        copy_cap(g_secret, sizeof(g_secret), key);
        secret_save('l', g_secret);
        if (!g_ptr_asked && g_ctl) {
            g_ptr_asked = true;
            ws_send(*g_ctl, "{\"type\":\"request\",\"id\":\"ptr\",\"uri\":\"ssap://com.webos.service.networkinput/getPointerInputSocket\"}");
        }
        if (g_phase != WIFI_PHASE_READY) set_phase(WIFI_PHASE_WAIT);
        return;
    }
    if (strstr(msg, "\"PIN\"")) set_phase(WIFI_PHASE_PIN);
}

static bool pump_socket(WiFiClient* client) {
    if (!client) return false;
    bool changed = false;
    if (!client->connected() && !client->available()) {
        if (g_phase == WIFI_PHASE_READY || g_phase == WIFI_PHASE_ALLOW || g_phase == WIFI_PHASE_PIN) {
            set_phase(WIFI_PHASE_DOWN);
            changed = true;
        }
        return changed;
    }
    for (int n = 0; n < 4; ++n) {
        int got = ws_recv(*client, g_rx, sizeof(g_rx));
        if (got < 0) {
            if (g_phase == WIFI_PHASE_READY || g_phase == WIFI_PHASE_ALLOW || g_phase == WIFI_PHASE_PIN) {
                set_phase(WIFI_PHASE_DOWN);
                changed = true;
            }
            break;
        }
        if (got == 0 && !g_rx[0]) break;
        WifiPhase before = g_phase;
        on_text(g_rx);
        if (g_phase != before) changed = true;
        if (client->available() < 2) break;
    }
    return changed;
}

static bool alive(const char* url) {
    int code = http_req(url, false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr, 0);
    return code > 0;
}

static void connect_sony(uint32_t gen) {
    secret_load('y');
    if (g_secret[0]) {
        char url[48];
        snprintf(url, sizeof(url), "http://%s/sony/system", g_ip);
        int code = http_req(url, true, "application/json",
                            "{\"method\":\"getPowerStatus\",\"id\":1,\"params\":[],\"version\":\"1.0\"}",
                            "Cookie", g_secret, nullptr, nullptr, nullptr, 0, nullptr, 0);
        if (g_gen != gen) return;
        if (code == 200) {
            set_phase(WIFI_PHASE_READY);
            return;
        }
        g_secret[0] = 0;
        secret_save('y', "");
    }
    char url[56];
    snprintf(url, sizeof(url), "http://%s/sony/accessControl", g_ip);
    char cookie[180];
    cookie[0] = 0;
    int code = http_req(url, true, "application/json",
                        "{\"method\":\"actRegister\",\"id\":8,\"version\":\"1.0\",\"params\":[{\"clientid\":\"MetaTV:1\",\"nickname\":\"META TV\",\"level\":\"private\"},[{\"function\":\"WOL\",\"value\":\"yes\"}]]}",
                        nullptr, nullptr, nullptr, nullptr, nullptr, 0, cookie, sizeof(cookie));
    if (g_gen != gen) return;
    if (code < 0) {
        set_phase(WIFI_PHASE_DOWN);
        return;
    }
    if (cookie[0]) {
        copy_cap(g_secret, sizeof(g_secret), cookie);
        secret_save('y', g_secret);
        set_phase(WIFI_PHASE_READY);
        return;
    }
    g_pin[0] = 0;
    set_phase(WIFI_PHASE_PIN);
}

static void connect_now(uint32_t gen) {
    char url[80];
    if (g_kind == KIND_SAMSUNG) {
        secret_load('s');
        bool ok = samsung_open(false, 8001);
        if (!ok && g_gen == gen) ok = samsung_open(true, 8002);
        if (g_gen != gen) {
            stop_all();
            return;
        }
        set_phase(ok ? WIFI_PHASE_ALLOW : WIFI_PHASE_DOWN);
        return;
    }
    if (g_kind == KIND_LG) {
        secret_load('l');
        WiFiClient* client = client_for(false, false);
        bool ok = ws_open(*client, g_ip, 3000, "/");
        if (!ok && g_gen == gen) {
            client = client_for(true, false);
            ok = ws_open(*client, g_ip, 3001, "/");
        }
        if (g_gen != gen) {
            stop_all();
            return;
        }
        if (!ok) {
            set_phase(WIFI_PHASE_DOWN);
            return;
        }
        g_ctl = client;
        g_ptr_asked = false;
        lg_send_register();
        set_phase(WIFI_PHASE_ALLOW);
        return;
    }
    if (g_kind == KIND_SONY) {
        connect_sony(gen);
        return;
    }
    if (g_kind == KIND_ROKU) {
        snprintf(url, sizeof(url), "http://%s:8060/query/device-info", g_ip);
    } else if (g_kind == KIND_PANASONIC) {
        snprintf(url, sizeof(url), "http://%s:55000/nrc/ddd.xml", g_ip);
    } else if (g_kind == KIND_PHILIPS) {
        snprintf(url, sizeof(url), "http://%s:1925/system", g_ip);
    } else {
        set_phase(WIFI_PHASE_IR);
        return;
    }
    bool ok = alive(url);
    if (!ok && g_kind == KIND_PHILIPS && g_gen == gen) {
        snprintf(url, sizeof(url), "http://%s:1925/6/system", g_ip);
        ok = alive(url);
    }
    if (g_gen != gen) return;
    set_phase(ok ? WIFI_PHASE_READY : WIFI_PHASE_DOWN);
}

static bool sony_send(const char* code) {
    char url[48];
    snprintf(url, sizeof(url), "http://%s/sony/ircc", g_ip);
    snprintf(g_tx, sizeof(g_tx),
             "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:X_SendIRCC xmlns:u=\"urn:schemas-sony-com:service:IRCC:1\"><IRCCCode>%s</IRCCCode></u:X_SendIRCC></s:Body></s:Envelope>",
             code);
    int code_http = http_req(url, true, "text/xml; charset=UTF-8", g_tx,
                             "SOAPACTION", "\"urn:schemas-sony-com:service:IRCC:1#X_SendIRCC\"",
                             g_secret[0] ? "Cookie" : nullptr, g_secret[0] ? g_secret : nullptr,
                             nullptr, 0, nullptr, 0);
    if (code_http == 200) return true;
    if (code_http == 401 || code_http == 403) {
        g_secret[0] = 0;
        secret_save('y', "");
        g_pin[0] = 0;
        set_phase(WIFI_PHASE_PIN);
    }
    return false;
}

static bool http_key(const char* name) {
    char url[80];
    if (g_kind == KIND_ROKU) {
        const char* key = name;
        if (!name) {
            snprintf(url, sizeof(url), "http://%s:8060/query/device-info", g_ip);
            int code = http_req(url, false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, g_rx, sizeof(g_rx), nullptr, 0);
            key = (code == 200 && strstr(g_rx, "PowerOn")) ? "PowerOff" : "PowerOn";
        }
        snprintf(url, sizeof(url), "http://%s:8060/keypress/%s", g_ip, key);
        int code = http_req(url, true, nullptr, "", nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr, 0);
        return code >= 200 && code < 300;
    }
    if (g_kind == KIND_PANASONIC) {
        snprintf(url, sizeof(url), "http://%s:55000/nrc/control_0", g_ip);
        snprintf(g_tx, sizeof(g_tx),
                 "<?xml version=\"1.0\" encoding=\"utf-8\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:X_SendKey xmlns:u=\"urn:panasonic-com:service:p00NetworkControl:1\"><X_KeyEvent>%s</X_KeyEvent></u:X_SendKey></s:Body></s:Envelope>",
                 name);
        int code = http_req(url, true, "text/xml; charset=\"utf-8\"", g_tx,
                            "SOAPACTION", "\"urn:panasonic-com:service:p00NetworkControl:1#X_SendKey\"",
                            nullptr, nullptr, nullptr, 0, nullptr, 0);
        return code >= 200 && code < 300;
    }
    if (g_kind == KIND_PHILIPS) {
        snprintf(url, sizeof(url), "http://%s:1925/6/input/key", g_ip);
        snprintf(g_tx, sizeof(g_tx), "{\"key\":\"%s\"}", name);
        int code = http_req(url, true, "application/json", g_tx, nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr, 0);
        if (code < 0) {
            snprintf(url, sizeof(url), "http://%s:1925/1/input/key", g_ip);
            code = http_req(url, true, "application/json", g_tx, nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr, 0);
        }
        return code >= 200 && code < 300;
    }
    return false;
}

void wifi_remote_begin(const char* ip, const char* brand) {
    wifi_remote_end();
    copy_cap(g_ip, sizeof(g_ip), ip);
    g_pin[0] = 0;
    g_secret[0] = 0;
    g_ptr_asked = false;
    g_reopen = false;
    g_reopen_used = false;
    g_started = false;
    g_arm_at = millis();
    g_kind = kind_of(brand);
    if (!g_ip[0] || g_kind == KIND_NONE) {
        set_phase(WIFI_PHASE_IR);
        g_started = true;
        return;
    }
    set_phase(WIFI_PHASE_WAIT);
}

void wifi_remote_end() {
    ++g_gen;
    stop_all();
    g_started = true;
    g_kind = KIND_NONE;
    g_phase = WIFI_PHASE_OFF;
}

bool wifi_remote_poll() {
    if (g_phase == WIFI_PHASE_OFF || g_phase == WIFI_PHASE_IR) return false;
    bool changed = false;
    if (!g_started && g_phase == WIFI_PHASE_WAIT) {
        if (millis() - g_arm_at < 60) return false;
        g_started = true;
        uint32_t gen = g_gen;
        connect_now(gen);
        if (g_gen != gen) return false;
        changed = true;
    }
    if (g_ctl) changed = pump_socket(g_ctl) || changed;
    if (g_btn && g_btn != g_ctl) changed = pump_socket(g_btn) || changed;
    if (g_reopen && g_kind == KIND_SAMSUNG && g_phase != WIFI_PHASE_OFF) {
        g_reopen = false;
        uint32_t gen = g_gen;
        stop_all();
        bool ok = samsung_open(false, 8001);
        if (!ok && g_gen == gen) ok = samsung_open(true, 8002);
        if (g_gen == gen) set_phase(ok ? WIFI_PHASE_ALLOW : WIFI_PHASE_DOWN);
        changed = true;
    }
    return changed;
}

WifiPhase wifi_remote_phase() { return g_phase; }

const char* wifi_remote_status() {
    switch (g_phase) {
        case WIFI_PHASE_WAIT: return "Connecting";
        case WIFI_PHASE_ALLOW: return "Allow on the TV";
        case WIFI_PHASE_PIN: return "Type the TV pin";
        case WIFI_PHASE_READY: return "Ready";
        case WIFI_PHASE_IR: return "This TV uses IR";
        case WIFI_PHASE_DOWN: return "Keys use IR";
        default: return "";
    }
}

const char* wifi_remote_pin() { return g_pin; }

bool wifi_remote_send(int key_id) {
    if (g_phase != WIFI_PHASE_READY) return false;
    if (g_kind == KIND_ROKU && key_id == KEY_POWER) return http_key(nullptr);
    const char* name = map_key(key_id);
    if (!name) return false;
    if (g_kind == KIND_SAMSUNG) {
        if (!g_btn) return false;
        snprintf(g_tx, sizeof(g_tx),
                 "{\"method\":\"ms.remote.control\",\"params\":{\"Cmd\":\"Click\",\"DataOfCmd\":\"%s\",\"Option\":\"false\",\"TypeOfRemote\":\"SendRemoteKey\"}}",
                 name);
        if (ws_send(*g_btn, g_tx)) return true;
        set_phase(WIFI_PHASE_DOWN);
        return false;
    }
    if (g_kind == KIND_LG) {
        if (!g_btn) return false;
        snprintf(g_tx, sizeof(g_tx), "type:button\nname:%s\n\n", name);
        if (ws_send(*g_btn, g_tx)) return true;
        set_phase(WIFI_PHASE_DOWN);
        return false;
    }
    if (g_kind == KIND_SONY) return sony_send(name);
    return http_key(name);
}

void wifi_remote_type(char c) {
    if (g_phase != WIFI_PHASE_PIN) return;
    if (c < '0' || c > '9') return;
    int n = (int)strlen(g_pin);
    if (n >= 8) return;
    g_pin[n] = c;
    g_pin[n + 1] = 0;
}

void wifi_remote_backspace() {
    int n = (int)strlen(g_pin);
    if (n <= 0) return;
    g_pin[n - 1] = 0;
}

bool wifi_remote_submit_pin() {
    if (g_phase != WIFI_PHASE_PIN) return false;
    if ((int)strlen(g_pin) < 4) return false;
    if (g_kind == KIND_LG) {
        if (!g_ctl) return false;
        snprintf(g_tx, sizeof(g_tx),
                 "{\"type\":\"request\",\"id\":\"pin\",\"uri\":\"ssap://pairing/setPin\",\"payload\":{\"pin\":\"%s\"}}",
                 g_pin);
        g_pin[0] = 0;
        ws_send(*g_ctl, g_tx);
        set_phase(WIFI_PHASE_ALLOW);
        return true;
    }
    if (g_kind != KIND_SONY) return false;
    uint8_t raw[12];
    int n = 0;
    raw[n++] = ':';
    for (int i = 0; g_pin[i] && n < (int)sizeof(raw); ++i) raw[n++] = (uint8_t)g_pin[i];
    char auth[24];
    b64(raw, n, auth, sizeof(auth));
    char header[40];
    snprintf(header, sizeof(header), "Basic %s", auth);
    char url[56];
    snprintf(url, sizeof(url), "http://%s/sony/accessControl", g_ip);
    char cookie[180];
    cookie[0] = 0;
    int code = http_req(url, true, "application/json",
                        "{\"method\":\"actRegister\",\"id\":8,\"version\":\"1.0\",\"params\":[{\"clientid\":\"MetaTV:1\",\"nickname\":\"META TV\",\"level\":\"private\"},[{\"function\":\"WOL\",\"value\":\"yes\"}]]}",
                        "Authorization", header, nullptr, nullptr, nullptr, 0, cookie, sizeof(cookie));
    g_pin[0] = 0;
    if (cookie[0] && code > 0) {
        copy_cap(g_secret, sizeof(g_secret), cookie);
        secret_save('y', g_secret);
        set_phase(WIFI_PHASE_READY);
        return true;
    }
    set_phase(WIFI_PHASE_PIN);
    return false;
}
