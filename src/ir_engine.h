#pragma once

#include <stdint.h>

#include "ir_database.h"

// Emisor IR de fabrica de la Cardputer ADV: GPIO44 (m5-docs, pinmap "IR TX").
// La ADV no trae demodulador. IRrecv queda enlazado pero no se activa,
// para no ocupar el Grove (GPIO1) ni robar tiempo al teclado.

void ir_begin();
bool ir_send(const IrKey& key);
bool ir_send_raw(const uint16_t* timings, uint16_t count, uint16_t khz);

// Texto corto del codigo que se va a emitir (direccion / comando / trama).
void ir_format(const IrKey& key, char* out, int cap);
