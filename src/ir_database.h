#pragma once

#include <stdint.h>
#include <pgmspace.h>

// Catalogo IR-Blade.
// Codigos de mando publicados en Flipper-IRDB (logickworkshop), convertidos
// a address/command logicos. La trama de 32/48 bits la calcula IRremoteESP8266
// (encodeNEC, encodeSAMSUNG, encodeSony, encodeRC5, encodeRC6, Kaseikyo).
//
// Orden: cuota de mercado. No hay codigos inventados.
// Thomson usa el mismo RCA que TCL (addr 0x0F, mute 0xFC): una sola entrada.
// El .ir de Blaupunkt del IRDB es el mismo NECext que Sharp Aquos.
// El .ir de Haier L42C1180 repite el NEC de LG (addr 0x04).
// Xiaomi, OK, TD Systems, Schneider, Cecotec, Engel y Saivod no tienen
// un mando de tele completo y publicado en ese archivo; se importan por SD.

enum Proto : uint8_t {
    PROTO_NONE = 0,
    PROTO_NEC = 1,
    PROTO_SAMSUNG = 2,
    PROTO_SONY = 3,
    PROTO_RC5 = 4,
    PROTO_RC6 = 5,
    PROTO_KASEIKYO = 6,
    PROTO_RCA = 7,
    PROTO_JVC = 8,
    PROTO_PIONEER = 9,
    PROTO_SHARP = 10,
};

enum KeyId : uint8_t {
    KEY_MUTE = 0,
    KEY_VOL_UP,
    KEY_VOL_DOWN,
    KEY_POWER,
    KEY_CH_UP,
    KEY_CH_DOWN,
    KEY_SOURCE,
    KEY_OK,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_BACK,
    KEY_MENU,
    KEY_COUNT
};

struct IrKey {
    uint8_t proto;
    uint8_t bits;
    uint32_t address;
    uint16_t command;
};

struct Brand {
    const char* name;
    const char* proto_name;
    IrKey key[KEY_COUNT];
};

static const Brand kBrands[] PROGMEM = {
    // LG  <- TVs/LG/LG_32LF650V.ir
    {"LG", "NEC",
     {
        {PROTO_NEC, 32, 0x4, 0x9}, // MUTE
        {PROTO_NEC, 32, 0x4, 0x2}, // VOL_UP
        {PROTO_NEC, 32, 0x4, 0x3}, // VOL_DOWN
        {PROTO_NEC, 32, 0x4, 0x8}, // POWER
        {PROTO_NEC, 32, 0x4, 0x0}, // CH_UP
        {PROTO_NEC, 32, 0x4, 0x1}, // CH_DOWN
        {PROTO_NEC, 32, 0x4, 0xB}, // SOURCE
        {PROTO_NEC, 32, 0x4, 0x44}, // OK
        {PROTO_NEC, 32, 0x4, 0x40}, // UP
        {PROTO_NEC, 32, 0x4, 0x41}, // DOWN
        {PROTO_NEC, 32, 0x4, 0x7}, // LEFT
        {PROTO_NEC, 32, 0x4, 0x6}, // RIGHT
        {PROTO_NEC, 32, 0x4, 0x28}, // BACK
        {PROTO_NEC, 32, 0x4, 0x43}, // MENU
     }},
    // Samsung  <- TVs/Samsung/Samsung.ir
    {"Samsung", "SAMSUNG",
     {
        {PROTO_SAMSUNG, 32, 0x7, 0xF}, // MUTE
        {PROTO_SAMSUNG, 32, 0x7, 0x7}, // VOL_UP
        {PROTO_SAMSUNG, 32, 0x7, 0xB}, // VOL_DOWN
        {PROTO_SAMSUNG, 32, 0x7, 0x2}, // POWER
        {PROTO_SAMSUNG, 32, 0x7, 0x12}, // CH_UP  (Samsung.ir duplica Ch_next=Ch_prev 0x10; BN59 real es 0x12 -> trama E0E048B7)
        {PROTO_SAMSUNG, 32, 0x7, 0x10}, // CH_DOWN
        {PROTO_SAMSUNG, 32, 0x7, 0x1}, // SOURCE
        {PROTO_SAMSUNG, 32, 0x7, 0x68}, // OK
        {PROTO_SAMSUNG, 32, 0x7, 0x60}, // UP
        {PROTO_SAMSUNG, 32, 0x7, 0x61}, // DOWN
        {PROTO_SAMSUNG, 32, 0x7, 0x65}, // LEFT
        {PROTO_SAMSUNG, 32, 0x7, 0x62}, // RIGHT
        {PROTO_SAMSUNG, 32, 0x7, 0x58}, // BACK
        {PROTO_SAMSUNG, 32, 0x7, 0x1A}, // MENU
     }},
    // Sony  <- TVs/Sony/Sony_KD.ir, TVs/Sony/Sony_Bravia.ir, TVs/Sony/SonyTV.ir, TVs/Sony/Sony_Bravia_KD-55XF80xx-49XF80xx-43XF80xx.ir
    {"Sony", "SIRC",
     {
        {PROTO_SONY, 12, 0x1, 0x14}, // MUTE
        {PROTO_SONY, 12, 0x1, 0x12}, // VOL_UP
        {PROTO_SONY, 12, 0x1, 0x13}, // VOL_DOWN
        {PROTO_SONY, 12, 0x1, 0x15}, // POWER
        {PROTO_SONY, 12, 0x1, 0x10}, // CH_UP
        {PROTO_SONY, 12, 0x1, 0x11}, // CH_DOWN
        {PROTO_SONY, 12, 0x1, 0x25}, // SOURCE
        {PROTO_SONY, 12, 0x1, 0x65}, // OK
        {PROTO_SONY, 12, 0x1, 0x74}, // UP
        {PROTO_SONY, 12, 0x1, 0x75}, // DOWN
        {PROTO_SONY, 12, 0x1, 0x34}, // LEFT
        {PROTO_SONY, 12, 0x1, 0x33}, // RIGHT
        {PROTO_SONY, 15, 0x97, 0x23}, // BACK
        {PROTO_SONY, 12, 0x1, 0x60}, // MENU
     }},
    // Philips  <- TVs/Philips/Philips_TV_Universal.ir, TVs/Philips/PhilipsTV.ir
    {"Philips", "RC6",
     {
        {PROTO_RC6, 20, 0x0, 0xD}, // MUTE
        {PROTO_RC6, 20, 0x0, 0x10}, // VOL_UP
        {PROTO_RC6, 20, 0x0, 0x11}, // VOL_DOWN
        {PROTO_RC6, 20, 0x0, 0xC}, // POWER
        {PROTO_RC6, 20, 0x0, 0x20}, // CH_UP
        {PROTO_RC6, 20, 0x0, 0x21}, // CH_DOWN
        {PROTO_RC6, 20, 0x0, 0x38}, // SOURCE
        {PROTO_RC6, 20, 0x0, 0x5C}, // OK
        {PROTO_RC6, 20, 0x0, 0x58}, // UP
        {PROTO_RC6, 20, 0x0, 0x59}, // DOWN
        {PROTO_RC6, 20, 0x0, 0x5A}, // LEFT
        {PROTO_RC6, 20, 0x0, 0x5B}, // RIGHT
        {PROTO_RC6, 20, 0x0, 0xA}, // BACK
        {PROTO_RC6, 20, 0x0, 0x57}, // MENU
     }},
    // Hisense  <- TVs/Hisense/Hisense_55E7KQ.ir
    {"Hisense", "NEC",
     {
        {PROTO_NEC, 32, 0xBF00, 0xF10E}, // MUTE
        {PROTO_NEC, 32, 0xBF00, 0xBB44}, // VOL_UP
        {PROTO_NEC, 32, 0xBF00, 0xBC43}, // VOL_DOWN
        {PROTO_NEC, 32, 0xBF00, 0xF20D}, // POWER
        {PROTO_NEC, 32, 0xBF00, 0xB54A}, // CH_UP
        {PROTO_NEC, 32, 0xBF00, 0xB44B}, // CH_DOWN
        {PROTO_NEC, 32, 0xBF00, 0xED12}, // SOURCE
        {PROTO_NEC, 32, 0xBF00, 0xEA15}, // OK
        {PROTO_NEC, 32, 0xBF00, 0xE916}, // UP
        {PROTO_NEC, 32, 0xBF00, 0xE817}, // DOWN
        {PROTO_NEC, 32, 0xBF00, 0xE619}, // LEFT
        {PROTO_NEC, 32, 0xBF00, 0xE718}, // RIGHT
        {PROTO_NEC, 32, 0xBF00, 0xB748}, // BACK
        {PROTO_NEC, 32, 0xBF00, 0xEB14}, // MENU
     }},
    // TCL  <- TVs/TCL/TCL_65C635K.ir
    {"TCL", "RCA",
     {
        {PROTO_RCA, 24, 0xF, 0xFC}, // MUTE
        {PROTO_RCA, 24, 0xF, 0xF4}, // VOL_UP
        {PROTO_RCA, 24, 0xF, 0x74}, // VOL_DOWN
        {PROTO_RCA, 24, 0xF, 0x54}, // POWER
        {PROTO_RCA, 24, 0xF, 0xB4}, // CH_UP
        {PROTO_RCA, 24, 0xF, 0x34}, // CH_DOWN
        {PROTO_RCA, 24, 0xF, 0xC5}, // SOURCE
        {PROTO_RCA, 24, 0xF, 0x2F}, // OK
        {PROTO_RCA, 24, 0xF, 0x9A}, // UP
        {PROTO_RCA, 24, 0xF, 0x1A}, // DOWN
        {PROTO_RCA, 24, 0xF, 0x6A}, // LEFT
        {PROTO_RCA, 24, 0xF, 0xEA}, // RIGHT
        {PROTO_RCA, 24, 0xF, 0xE4}, // BACK
        {PROTO_RCA, 24, 0xF, 0x10}, // MENU
     }},
    // Panasonic  <- TVs/Panasonic/Panasonic_58JX800_Series.ir, TVs/Panasonic/N2QAYB001109.ir, TVs/Panasonic/Panasonic_N2QAYB000705.ir
    {"Panasonic", "KASEIKYO",
     {
        {PROTO_KASEIKYO, 48, 0x200280, 0x320}, // MUTE
        {PROTO_KASEIKYO, 48, 0x200280, 0x200}, // VOL_UP
        {PROTO_KASEIKYO, 48, 0x200280, 0x210}, // VOL_DOWN
        {PROTO_KASEIKYO, 48, 0x200280, 0x3D0}, // POWER
        {PROTO_KASEIKYO, 48, 0x200280, 0x340}, // CH_UP
        {PROTO_KASEIKYO, 48, 0x200280, 0x350}, // CH_DOWN
        {PROTO_KASEIKYO, 48, 0x200280, 0x50}, // SOURCE
        {PROTO_KASEIKYO, 48, 0x1200280, 0x90}, // OK
        {PROTO_KASEIKYO, 48, 0x1200280, 0xA0}, // UP
        {PROTO_KASEIKYO, 48, 0x1200280, 0xB0}, // DOWN
        {PROTO_KASEIKYO, 48, 0x1200280, 0xE0}, // LEFT
        {PROTO_KASEIKYO, 48, 0x1200280, 0xF0}, // RIGHT
        {PROTO_KASEIKYO, 48, 0x2200289, 0x1A0}, // BACK
        {PROTO_KASEIKYO, 48, 0x1200280, 0x120}, // MENU
     }},
    // Toshiba  <- TVs/Toshiba/Toshiba_50C350LC.ir, TVs/Toshiba/Toshiba_43LF421U21.ir
    {"Toshiba", "NEC",
     {
        {PROTO_NEC, 32, 0x7D02, 0xB34C}, // MUTE
        {PROTO_NEC, 32, 0x7D02, 0xF30C}, // VOL_UP
        {PROTO_NEC, 32, 0x7D02, 0xE619}, // VOL_DOWN
        {PROTO_NEC, 32, 0x7D02, 0xB946}, // POWER
        {PROTO_NEC, 32, 0x7D02, 0xF00F}, // CH_UP
        {PROTO_NEC, 32, 0x7D02, 0xA55A}, // CH_DOWN
        {0, 0, 0, 0}, // SOURCE missing
        {PROTO_NEC, 32, 0x7D02, 0xB54A}, // OK
        {PROTO_NEC, 32, 0x7D02, 0xB748}, // UP
        {PROTO_NEC, 32, 0x7D02, 0xB24D}, // DOWN
        {PROTO_NEC, 32, 0x7D02, 0xB14E}, // LEFT
        {PROTO_NEC, 32, 0x7D02, 0xB649}, // RIGHT
        {PROTO_NEC, 32, 0x7D02, 0xF20D}, // BACK
        {PROTO_NEC, 32, 0x7D02, 0x609F}, // MENU
     }},
    // Sharp  <- TVs/Sharp/Sharp_Aquos_32BG3E.ir
    {"Sharp", "NEC",
     {
        {PROTO_NEC, 32, 0x7F00, 0xAF50}, // MUTE
        {PROTO_NEC, 32, 0x7F00, 0xE11E}, // VOL_UP
        {PROTO_NEC, 32, 0x7F00, 0xA05F}, // VOL_DOWN
        {PROTO_NEC, 32, 0x7F00, 0xF50A}, // POWER
        {PROTO_NEC, 32, 0x7F00, 0xE01F}, // CH_UP
        {PROTO_NEC, 32, 0x7F00, 0xA35C}, // CH_DOWN
        {PROTO_NEC, 32, 0x7F00, 0xAC53}, // SOURCE
        {PROTO_NEC, 32, 0x7F00, 0xA55A}, // OK
        {PROTO_NEC, 32, 0x7F00, 0xA15E}, // UP
        {PROTO_NEC, 32, 0x7F00, 0xA956}, // DOWN
        {PROTO_NEC, 32, 0x7F00, 0xA45B}, // LEFT
        {PROTO_NEC, 32, 0x7F00, 0xA758}, // RIGHT
        {PROTO_NEC, 32, 0x7F00, 0xB14E}, // BACK
        {PROTO_NEC, 32, 0x7F00, 0xA857}, // MENU
     }},
    // Grundig  <- TVs/Grundig/GRUNDIG.ir, TVs/Grundig/GRUNDIG_UNKNOWN.ir
    {"Grundig", "RC5",
     {
        {PROTO_RC5, 13, 0x0, 0xD}, // MUTE
        {PROTO_RC5, 13, 0x0, 0x10}, // VOL_UP
        {PROTO_RC5, 13, 0x0, 0x11}, // VOL_DOWN
        {PROTO_RC5, 13, 0x0, 0xC}, // POWER
        {PROTO_RC5, 13, 0x0, 0x20}, // CH_UP
        {PROTO_RC5, 13, 0x0, 0x21}, // CH_DOWN
        {PROTO_RC5, 13, 0x0, 0x38}, // SOURCE
        {PROTO_RC5, 13, 0x0, 0x35}, // OK
        {PROTO_RC5, 13, 0x0, 0x16}, // UP
        {PROTO_RC5, 13, 0x0, 0x17}, // DOWN
        {PROTO_RC5, 13, 0x0, 0x13}, // LEFT
        {PROTO_RC5, 13, 0x0, 0x12}, // RIGHT
        {0, 0, 0, 0}, // BACK missing
        {0, 0, 0, 0}, // MENU missing
     }},
    // JVC  <- TVs/JVC/JVC_LT-49HW97U.ir
    {"JVC", "SAMSUNG",
     {
        {PROTO_SAMSUNG, 32, 0xE, 0xD}, // MUTE
        {PROTO_SAMSUNG, 32, 0xE, 0x14}, // VOL_UP
        {PROTO_SAMSUNG, 32, 0xE, 0x15}, // VOL_DOWN
        {PROTO_SAMSUNG, 32, 0xE, 0xC}, // POWER
        {PROTO_SAMSUNG, 32, 0xE, 0x12}, // CH_UP
        {PROTO_SAMSUNG, 32, 0xE, 0x13}, // CH_DOWN
        {PROTO_SAMSUNG, 32, 0xE, 0xF}, // SOURCE
        {PROTO_SAMSUNG, 32, 0xE, 0x46}, // OK
        {PROTO_SAMSUNG, 32, 0xE, 0x42}, // UP
        {PROTO_SAMSUNG, 32, 0xE, 0x43}, // DOWN
        {PROTO_SAMSUNG, 32, 0xE, 0x44}, // LEFT
        {PROTO_SAMSUNG, 32, 0xE, 0x45}, // RIGHT
        {PROTO_SAMSUNG, 32, 0xE, 0xB5}, // BACK
        {PROTO_SAMSUNG, 32, 0xE, 0x11}, // MENU
     }},
    // Hitachi  <- TVs/Hitachi/Hitachi_SmartTV_43140.ir
    {"Hitachi", "RC5",
     {
        {PROTO_RC5, 13, 0x3, 0xD}, // MUTE
        {PROTO_RC5, 13, 0x3, 0x10}, // VOL_UP
        {PROTO_RC5, 13, 0x3, 0x11}, // VOL_DOWN
        {PROTO_RC5, 13, 0x3, 0xC}, // POWER
        {PROTO_RC5, 13, 0x3, 0x20}, // CH_UP
        {PROTO_RC5, 13, 0x3, 0x21}, // CH_DOWN
        {PROTO_RC5, 13, 0x3, 0x38}, // SOURCE
        {PROTO_RC5, 13, 0x3, 0x35}, // OK
        {PROTO_RC5, 13, 0x3, 0x14}, // UP
        {PROTO_RC5, 13, 0x3, 0x13}, // DOWN
        {PROTO_RC5, 13, 0x3, 0x15}, // LEFT
        {PROTO_RC5, 13, 0x3, 0x16}, // RIGHT
        {PROTO_RC5, 13, 0x3, 0xA}, // BACK
        {PROTO_RC5, 13, 0x3, 0x30}, // MENU
     }},
    // Hitachi NEC  <- TVs/Hitachi/Hitachi_CLE-1031.ir
    {"Hitachi NEC", "NEC",
     {
        {PROTO_NEC, 32, 0x50, 0x10}, // MUTE
        {PROTO_NEC, 32, 0x50, 0x1A}, // VOL_UP
        {PROTO_NEC, 32, 0x50, 0x1E}, // VOL_DOWN
        {PROTO_NEC, 32, 0x50, 0x12}, // POWER
        {PROTO_NEC, 32, 0x50, 0x1B}, // CH_UP
        {PROTO_NEC, 32, 0x50, 0x1F}, // CH_DOWN
        {PROTO_NEC, 32, 0x50, 0x14}, // SOURCE
        {PROTO_NEC, 32, 0x50, 0xA}, // OK
        {PROTO_NEC, 32, 0x50, 0x19}, // UP
        {PROTO_NEC, 32, 0x50, 0x1D}, // DOWN
        {PROTO_NEC, 32, 0x50, 0x46}, // LEFT
        {PROTO_NEC, 32, 0x50, 0x47}, // RIGHT
        {PROTO_NEC, 32, 0x50, 0x40}, // BACK
        {PROTO_NEC, 32, 0x50, 0x49}, // MENU
     }},
    // Nevir  <- TVs/Nevir/NEVIR_NVR-7409-39HD-N.ir
    {"Nevir", "NEC",
     {
        {PROTO_NEC, 32, 0x1, 0x17}, // MUTE
        {PROTO_NEC, 32, 0x1, 0xE}, // VOL_UP
        {PROTO_NEC, 32, 0x1, 0xF}, // VOL_DOWN
        {PROTO_NEC, 32, 0x1, 0x10}, // POWER
        {PROTO_NEC, 32, 0x1, 0xC}, // CH_UP
        {PROTO_NEC, 32, 0x1, 0xD}, // CH_DOWN
        {PROTO_NEC, 32, 0x1, 0x12}, // SOURCE
        {PROTO_NEC, 32, 0x1, 0x1D}, // OK
        {PROTO_NEC, 32, 0x1, 0x19}, // UP
        {PROTO_NEC, 32, 0x1, 0x1A}, // DOWN
        {PROTO_NEC, 32, 0x1, 0x1B}, // LEFT
        {PROTO_NEC, 32, 0x1, 0x1C}, // RIGHT
        {PROTO_NEC, 32, 0x1, 0x16}, // BACK
        {PROTO_NEC, 32, 0x1, 0x15}, // MENU
     }},
    // Telefunken  <- TVs/Telefunken/Telefunken_D40F294R4CW.ir
    {"Telefunken", "RC5",
     {
        {PROTO_RC5, 13, 0x1, 0xD}, // MUTE
        {PROTO_RC5, 13, 0x1, 0x10}, // VOL_UP
        {PROTO_RC5, 13, 0x1, 0x11}, // VOL_DOWN
        {PROTO_RC5, 13, 0x1, 0xC}, // POWER
        {PROTO_RC5, 13, 0x1, 0x20}, // CH_UP
        {PROTO_RC5, 13, 0x1, 0x21}, // CH_DOWN
        {PROTO_RC5, 13, 0x1, 0x38}, // SOURCE
        {PROTO_RC5, 13, 0x1, 0x35}, // OK
        {PROTO_RC5, 13, 0x1, 0x14}, // UP
        {PROTO_RC5, 13, 0x1, 0x13}, // DOWN
        {PROTO_RC5, 13, 0x1, 0x15}, // LEFT
        {PROTO_RC5, 13, 0x1, 0x16}, // RIGHT
        {PROTO_RC5, 13, 0x1, 0xA}, // BACK
        {PROTO_RC5, 13, 0x1, 0x30}, // MENU
     }},
    // Sanyo  <- TVs/Sanyo/Sanyo_FW40R49FC.ir, TVs/Sanyo/Sanyo_DP26640.ir, TVs/Sanyo/Sanyo_ds32224.ir
    {"Sanyo", "NEC",
     {
        {PROTO_NEC, 32, 0xC7EA, 0xDF20}, // MUTE
        {PROTO_NEC, 32, 0xC7EA, 0xF00F}, // VOL_UP
        {PROTO_NEC, 32, 0xC7EA, 0xEF10}, // VOL_DOWN
        {PROTO_NEC, 32, 0xC7EA, 0xE817}, // POWER
        {PROTO_NONE, 0, 0, 0}, // CH_UP  (no esta en Sanyo_FW40R49FC)
        {PROTO_NONE, 0, 0, 0}, // CH_DOWN
        {PROTO_NONE, 0, 0, 0}, // SOURCE
        {PROTO_NEC, 32, 0xC7EA, 0xD52A}, // OK
        {PROTO_NEC, 32, 0xC7EA, 0xE619}, // UP
        {PROTO_NEC, 32, 0xC7EA, 0xCC33}, // DOWN
        {PROTO_NEC, 32, 0xC7EA, 0xE11E}, // LEFT
        {PROTO_NEC, 32, 0xC7EA, 0xD22D}, // RIGHT
        {PROTO_NEC, 32, 0xC7EA, 0x9966}, // BACK
        {PROTO_NEC, 32, 0xC7EA, 0xFC03}, // MENU
     }},
    // NEC  <- TVs/NEC/NEC_E425.ir
    {"NEC", "NEC",
     {
        {PROTO_NEC, 32, 0x38, 0x4}, // MUTE
        {PROTO_NEC, 32, 0x38, 0xC}, // VOL_UP
        {PROTO_NEC, 32, 0x38, 0x10}, // VOL_DOWN
        {PROTO_NEC, 32, 0x38, 0x1}, // POWER
        {PROTO_NEC, 32, 0x38, 0x18}, // CH_UP
        {PROTO_NEC, 32, 0x38, 0x1C}, // CH_DOWN
        {PROTO_NEC, 32, 0x38, 0x5}, // SOURCE
        {PROTO_NEC, 32, 0x38, 0xD}, // OK
        {PROTO_NEC, 32, 0x38, 0xB}, // UP
        {PROTO_NEC, 32, 0x38, 0xF}, // DOWN
        {PROTO_NEC, 32, 0x38, 0x49}, // LEFT
        {PROTO_NEC, 32, 0x38, 0x4A}, // RIGHT
        {PROTO_NEC, 32, 0x38, 0x7}, // BACK
        {PROTO_NEC, 32, 0x38, 0xA}, // MENU
     }},
    // Toshiba CT  <- TVs/Toshiba/Toshiba_CT-90325.ir
    {"Toshiba CT", "NEC",
     {
        {PROTO_NEC, 32, 0x40, 0x10}, // MUTE
        {PROTO_NEC, 32, 0x40, 0x1A}, // VOL_UP
        {PROTO_NEC, 32, 0x40, 0x1E}, // VOL_DOWN
        {PROTO_NEC, 32, 0x40, 0x12}, // POWER
        {PROTO_NEC, 32, 0x40, 0x1B}, // CH_UP
        {PROTO_NEC, 32, 0x40, 0x1F}, // CH_DOWN
        {PROTO_NEC, 32, 0x40, 0xF}, // SOURCE
        {PROTO_NEC, 32, 0x40, 0x89}, // OK
        {PROTO_NEC, 32, 0x40, 0x82}, // UP
        {PROTO_NEC, 32, 0x40, 0x83}, // DOWN
        {PROTO_NEC, 32, 0x40, 0x1D}, // LEFT
        {PROTO_NEC, 32, 0x40, 0x19}, // RIGHT
        {0, 0, 0, 0}, // BACK missing
        {0, 0, 0, 0}, // MENU missing
     }},
#include "ir_extra.inc"
#include "ir_more.inc"
#include "ir_more2.inc"
#include "ir_irdb.inc"
};

static constexpr int kBrandCount = (int)(sizeof(kBrands) / sizeof(kBrands[0]));

#include "ir_regions.inc"
static_assert(sizeof(kRegion) == kBrandCount, "region map");
