#ifndef DEAUTHER_H
#define DEAUTHER_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  DEAUTHER · desconecta clientes de redes WiFi usando deauth frames
//  · Scan APs → Select AP → [Scan clients | Broadcast | Rambo mode]
//  · Uso educativo/pentesting en redes propias
//  · USO EN REDES AJENAS ES ILEGAL
// ═══════════════════════════════════════════════════════════════════════════

void runDeauther();

#endif
