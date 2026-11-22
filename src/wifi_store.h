#pragma once

#include <stdbool.h>

// Claves escritas en wifi.txt (se graban con el aparato) y las que ya
// entraron alguna vez, guardadas en el propio ESP32.

bool wifi_lookup(const char* ssid, char* pass, int pass_cap);
void wifi_remember(const char* ssid, const char* pass);
const char* wifi_recent_ssid();

int clip_count();
bool clip_at(int index, char* name, int name_cap, char* url, int url_cap);
bool clip_add(const char* name, const char* url);
bool clip_set(int index, const char* name, const char* url);
bool clip_remove(int index);
