#include "menu_catalog.h"

static const uint8_t WIFI_APPS[] = {
    0,  // WIFI SCANNER
    1,  // WIFI RADAR
    2,  // CHANNEL SCAN
    5,  // PACKET MONITOR
    6,  // MODO CENTINELA
    14, // IP SCANNER
    16  // WEB DASHBOARD
};

static const uint8_t RF_APPS[] = {
    3,  // ANALIZADOR
    23, // RF HEATMAP
    24, // CH ADVISOR
    25, // NRF LINK
    26, // NRF CHAT
    27, // BT/WIFI COEX
    28  // DUAL NRF SCOPE
};

static const uint8_t BLUETOOTH_APPS[] = {
    4,  // BT SCANNER
    21, // BT ANALYZER
    22, // BT SPECTRUM
    17  // BT REMOTE
};

static const uint8_t ILLEGAL_APPS[] = {
    7,  // JAMMER CANAL
    8,  // BARRIDO TOTAL
    9,  // BT JAMMER
    10, // BEACON SPAM
    11, // BLE SPAM (POP)
    12, // MODO HIBRIDO
    13, // EVIL PORTAL
    29  // DEAUTHER
};

static const uint8_t GAMES_APPS[] = {
    19  // ARCADE
};

static const uint8_t SYSTEM_APPS[] = {
    15, // CONTROL ESCLAVO
    18, // LEER LOGS
    20  // ABOUT
};

static const MenuCategory CATEGORIES[] = {
    { "WIFI",      "scanner/radar/red", MENU_ICON_WIFI,      WIFI_APPS,      sizeof(WIFI_APPS) },
    { "RF TOOLS",  "nRF24 spectrum",    MENU_ICON_RF,        RF_APPS,        sizeof(RF_APPS) },
    { "BLUETOOTH", "scan/remoto",       MENU_ICON_BLUETOOTH, BLUETOOTH_APPS, sizeof(BLUETOOTH_APPS) },
    { "ILEGAL",    "zona de alerta",    MENU_ICON_WARNING,   ILLEGAL_APPS,   sizeof(ILLEGAL_APPS) },
    { "GAMES",     "arcade",            MENU_ICON_GAMES,     GAMES_APPS,     sizeof(GAMES_APPS) },
    { "SISTEMA",   "tools/info",        MENU_ICON_SYSTEM,    SYSTEM_APPS,    sizeof(SYSTEM_APPS) }
};

uint8_t menuCategoryCount() {
    return sizeof(CATEGORIES) / sizeof(CATEGORIES[0]);
}

const MenuCategory& menuCategoryAt(uint8_t index) {
    if (index >= menuCategoryCount()) index = 0;
    return CATEGORIES[index];
}

uint8_t menuCategoryAppIndex(uint8_t category, uint8_t position) {
    const MenuCategory& cat = menuCategoryAt(category);
    if (position >= cat.count) position = 0;
    return cat.apps[position];
}
