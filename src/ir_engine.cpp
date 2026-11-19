#include "ir_engine.h"

#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>

#include <stdio.h>
#include <string.h>

static IRsend ir_tx(IR_BLADE_TX_PIN);
// Sin demodulador a bordo. El objeto enlaza IRrecv; setTolerance no arranca el RMT.
static IRrecv ir_rx(1);

static bool rc5_flip = false;
static bool rc6_flip = false;

static uint64_t reverse_bits(uint64_t value, int bits) {
    uint64_t out = 0;
    for (int i = 0; i < bits; ++i) {
        if (value & (1ULL << i)) out |= 1ULL << (bits - 1 - i);
    }
    return out;
}

// Trama de 48 bits lista para sendPanasonic64 (MSB = primer bit en el aire).
// El empaquetado sigue el encoder Kaseikyo de Flipper (vendor, genero, comando, paridad).
static uint64_t pack_kaseikyo(uint32_t address, uint16_t command) {
    const uint8_t id = (address >> 24) & 0x03;
    const uint16_t vendor = (address >> 8) & 0xFFFF;
    const uint8_t genre1 = (address >> 4) & 0x0F;
    const uint8_t genre2 = address & 0x0F;
    const uint8_t d0 = vendor & 0xFF;
    const uint8_t d1 = vendor >> 8;
    uint8_t parity = d0 ^ d1;
    parity = (parity & 0x0F) ^ (parity >> 4);
    const uint8_t d2 = (parity & 0x0F) | (genre1 << 4);
    const uint8_t d3 = (genre2 & 0x0F) | ((command & 0x0F) << 4);
    const uint8_t d4 = (id << 6) | ((command >> 4) & 0x3F);
    const uint8_t d5 = d2 ^ d3 ^ d4;
    const uint64_t lsb = (uint64_t)d0 | ((uint64_t)d1 << 8) | ((uint64_t)d2 << 16) |
                         ((uint64_t)d3 << 24) | ((uint64_t)d4 << 32) | ((uint64_t)d5 << 40);
    return reverse_bits(lsb, 48);
}

// RCA 24 bits, LSB primero: addr4 + cmd8 + ~addr4 + ~cmd8 (Flipper infrared_decoder_rca).
static uint32_t pack_rca(uint32_t address, uint16_t command) {
    const uint8_t addr = address & 0x0F;
    const uint8_t cmd = command & 0xFF;
    const uint8_t addr_inv = (~addr) & 0x0F;
    const uint8_t cmd_inv = (~cmd) & 0xFF;
    return (uint32_t)addr | ((uint32_t)cmd << 4) | ((uint32_t)addr_inv << 12) | ((uint32_t)cmd_inv << 16);
}

void ir_begin() {
    ir_tx.begin();
    ir_rx.setTolerance(25);
}

static bool encode_frame(const IrKey& key, uint64_t* frame, uint16_t* bits, bool apply_toggle) {
    *frame = 0;
    *bits = key.bits ? key.bits : 32;
    switch (key.proto) {
        case PROTO_NEC:
            *frame = ir_tx.encodeNEC(static_cast<uint16_t>(key.address), key.command & 0xFF);
            *bits = 32;
            return true;
        case PROTO_SAMSUNG:
            *frame = ir_tx.encodeSAMSUNG(key.address & 0xFF, key.command & 0xFF);
            *bits = 32;
            return true;
        case PROTO_SONY:
            *frame = ir_tx.encodeSony(key.bits ? key.bits : 12, key.command & 0x7F, key.address & 0xFF);
            *bits = key.bits ? key.bits : 12;
            return *frame != 0;
        case PROTO_RC5: {
            uint64_t data = ir_tx.encodeRC5(key.address & 0x1F, key.command & 0x7F);
            if (apply_toggle && rc5_flip) data = ir_tx.toggleRC5(data);
            if (apply_toggle) rc5_flip = !rc5_flip;
            *frame = data;
            *bits = kRC5XBits;
            return true;
        }
        case PROTO_RC6: {
            uint64_t data = ir_tx.encodeRC6(key.address, key.command & 0xFF);
            if (apply_toggle && rc6_flip) data = ir_tx.toggleRC6(data);
            if (apply_toggle) rc6_flip = !rc6_flip;
            *frame = data;
            *bits = kRC6Mode0Bits;
            return true;
        }
        case PROTO_KASEIKYO:
            *frame = pack_kaseikyo(key.address, key.command);
            *bits = 48;
            return true;
        case PROTO_RCA:
            *frame = pack_rca(key.address, key.command);
            *bits = 24;
            return true;
        case PROTO_JVC:
            *frame = ir_tx.encodeJVC(key.address & 0xFF, key.command & 0xFF);
            *bits = 16;
            return true;
        case PROTO_PIONEER:
            *frame = ir_tx.encodePioneer(key.address & 0xFFFF, key.command);
            *bits = 64;
            return true;
        case PROTO_SHARP:
            *frame = ir_tx.encodeSharp(key.address & 0xFFFF, key.command & 0xFF);
            *bits = kSharpBits;
            return true;
        default:
            return false;
    }
}

bool ir_send(const IrKey& key) {
    if (key.proto == PROTO_NONE) return false;
    uint64_t frame = 0;
    uint16_t bits = 0;
    if (!encode_frame(key, &frame, &bits, true)) return false;
    switch (key.proto) {
        case PROTO_NEC:
            ir_tx.sendNEC(frame, bits);
            return true;
        case PROTO_SAMSUNG:
            ir_tx.sendSAMSUNG(frame, bits);
            return true;
        case PROTO_SONY:
            ir_tx.sendSony(frame, bits, kSonyMinRepeat);
            return true;
        case PROTO_RC5:
            ir_tx.sendRC5(frame, bits);
            return true;
        case PROTO_RC6:
            ir_tx.sendRC6(frame, bits);
            return true;
        case PROTO_KASEIKYO:
            ir_tx.sendPanasonic64(frame, bits);
            return true;
        case PROTO_RCA:
            ir_tx.sendGeneric(4000, 4000, 500, 2000, 500, 1000, 500, 40000, frame, bits, 38, false, 0, 33);
            return true;
        case PROTO_JVC:
            ir_tx.sendJVC(frame, bits, 1);
            return true;
        case PROTO_PIONEER:
            ir_tx.sendPioneer(frame, bits);
            return true;
        case PROTO_SHARP:
            ir_tx.sendSharpRaw(frame, bits);
            return true;
        default:
            return false;
    }
}

bool ir_send_raw(const uint16_t* timings, uint16_t count, uint16_t khz) {
    if (!timings || count < 2) return false;
    ir_tx.sendRaw(timings, count, khz ? khz : 38);
    return true;
}

void ir_format(const IrKey& key, char* out, int cap) {
    if (!out || cap < 8) return;
    if (key.proto == PROTO_NONE) {
        snprintf(out, cap, "no code");
        return;
    }
    uint64_t frame = 0;
    uint16_t bits = 0;
    if (!encode_frame(key, &frame, &bits, false)) {
        snprintf(out, cap, "----");
        return;
    }
    if (bits <= 16) snprintf(out, cap, "%04X", (unsigned)(frame & 0xFFFF));
    else if (bits <= 32) snprintf(out, cap, "%08X", (unsigned)frame);
    else snprintf(out, cap, "%04X%08X", (unsigned)(frame >> 32), (unsigned)frame);
}
