#pragma once

#include <stdbool.h>
#include <stdint.h>

// Network remote for the TV that NETWORK already found.
// Buttons use the published local API of that brand.
// Infrared stays available for the on-screen Allow / OK.

enum WifiPhase : uint8_t {
    WIFI_PHASE_OFF = 0,
    WIFI_PHASE_WAIT,
    WIFI_PHASE_ALLOW,
    WIFI_PHASE_PIN,
    WIFI_PHASE_READY,
    WIFI_PHASE_IR,
    WIFI_PHASE_DOWN,
};

void wifi_remote_begin(const char* ip, const char* brand);
void wifi_remote_end();
bool wifi_remote_poll();

WifiPhase wifi_remote_phase();
const char* wifi_remote_status();
const char* wifi_remote_pin();

bool wifi_remote_send(int key_id);
void wifi_remote_type(char c);
void wifi_remote_backspace();
bool wifi_remote_submit_pin();
