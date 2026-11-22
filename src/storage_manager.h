#pragma once

#include <stdint.h>

#include "ir_database.h"

static constexpr int kFavMax = 8;
static constexpr int kFavName = 13;
static constexpr int kRawMax = 48;

enum FavKind : uint8_t {
    FAV_NONE = 0,
    FAV_CATALOG = 1,
    FAV_CUSTOM = 2,
    FAV_RAW = 3,
};

struct Favorite {
    char name[kFavName];
    char brand[kFavName];
    char proto_name[10];
    uint8_t kind;
    uint8_t catalog;
    IrKey key[KEY_COUNT];
    uint16_t raw_hz;
    uint8_t raw_count;
    uint16_t raw[kRawMax];
};

void storage_begin();
int storage_count();
const Favorite* storage_at(int index);
bool storage_add_catalog(const char* name, int brand_index);
bool storage_add_custom(const char* name, const char* brand, const char* proto_name, const IrKey keys[KEY_COUNT]);
bool storage_add_raw(const char* name, const uint16_t* timings, uint8_t count, uint16_t hz);
bool storage_remove(int index);
bool storage_sound();
void storage_set_sound(bool on);
uint8_t storage_region();
void storage_set_region(uint8_t region);
const char* storage_status();
bool storage_sd_ready();
