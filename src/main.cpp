#include <M5Cardputer.h>
#include <WiFi.h>

#include "ir_engine.h"
#include "storage_manager.h"
#include "ui_manager.h"

void setup() {
    WiFi.mode(WIFI_OFF);
    auto cfg = M5.config();
    cfg.fallback_board = m5::board_t::board_M5CardputerADV;
    cfg.internal_spk = true;
    cfg.internal_mic = false;
    cfg.internal_imu = false;
    cfg.internal_rtc = false;
    cfg.clear_display = true;
    M5Cardputer.begin(cfg, true);
    // Panel nativo 135x240 con offset_rotation=1 (M5GFX de la ADV).
    // Rotacion 0 -> 240x135, teclado abajo e IR hacia la tele.
    M5Cardputer.Display.setRotation(0);
    if (M5Cardputer.Display.width() < M5Cardputer.Display.height()) {
        M5Cardputer.Display.setRotation(1);
    }
    M5Cardputer.Display.setBrightness(190);
    M5Cardputer.Speaker.setVolume(64);
    M5Cardputer.Speaker.begin();
    ir_begin();
    storage_begin();
    ui_begin();
}

void loop() {
    M5Cardputer.update();
    ui_loop();
}
