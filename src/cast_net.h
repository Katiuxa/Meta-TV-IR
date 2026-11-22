#pragma once

#include <stdbool.h>
#include <stdint.h>

// Asks which Wi-Fi is visible, then finds TVs and sends a link.

enum CastView : uint8_t {
    CAST_NETS = 0,
    CAST_JOIN,
    CAST_FAIL,
    CAST_SEEK,
    CAST_LIST,
    CAST_EMPTY,
    CAST_TV,
    CAST_LINKS,
    CAST_SENT,
    CAST_KEY,
    CAST_URL,
    CAST_LABEL,
    CAST_PAD,
};

enum CastResult : uint8_t {
    CAST_STAY = 0,
    CAST_OPEN_IR = 1,
};

struct CastRow {
    char title[24];
    char sub[16];
    char extra[16];
};

void cast_enter();
void cast_leave();
void cast_poll();
bool cast_active();
bool cast_busy();
bool cast_take_touch();

CastView cast_view();
const char* cast_ssid();
const char* cast_detail();
int cast_progress();
int cast_sel();
int cast_count();
void cast_row(int index, CastRow* out);
const char* cast_tv_brand();
const char* cast_tv_name();
const char* cast_tv_ip();

void cast_ask_wifi();
void cast_retry();
void cast_move(int dir);
void cast_move_xy(int dx, int dy);
void cast_choose(int index);
bool cast_back();
CastResult cast_accept();
bool cast_typing();
const char* cast_field();
void cast_type(char c);
void cast_backspace();
bool cast_arm_drop();
bool cast_confirming();
void cast_cancel_drop();
bool cast_drop();
bool cast_edit();
bool cast_editing();
