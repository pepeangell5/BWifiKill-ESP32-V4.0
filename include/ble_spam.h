#ifndef BLE_SPAM_H
#define BLE_SPAM_H

#include <Arduino.h>

// BLE SPAM: transmite advertisements BLE falsos.
// Protocolos: Apple Continuity, Samsung, Microsoft Swift Pair y Google.
// CHAOS MODE rota entre los protocolos para pruebas controladas.
// Uso educativo/demo: ejecutar solo en laboratorio propio o autorizado.

void runBLESpam();
void bleSpamEnter();
void bleSpamLoop();
void bleSpamExit();

#endif
