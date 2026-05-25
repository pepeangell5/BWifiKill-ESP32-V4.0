#include "gui_helper.h"
#include "ui_theme.h"
#include "menu_catalog.h"
#include <U8g2lib.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern int menu_index;
extern int category_index;
extern int category_app_index;
extern bool browsingCategoryApps;
extern float currentPos;
extern int TOTAL_OPTIONS;
extern const char* menu_labels[];
extern bool attack_confirmed;

static void drawPositionRail() {
    int railX = 122;
    int railY = 18;
    int railH = 32;
    int markerH = max(3, railH / TOTAL_OPTIONS);
    int markerY = railY + ((railH - markerH) * menu_index) / max(1, TOTAL_OPTIONS - 1);

    u8g2.drawVLine(railX, railY, railH);
    u8g2.drawBox(railX - 1, markerY, 3, markerH);
}

static void drawCategoryRail(uint8_t total, uint8_t selected) {
    int railX = 122;
    int railY = 18;
    int railH = 32;
    int markerH = max(5, railH / max(1, (int)total));
    int markerY = railY + ((railH - markerH) * selected) / max(1, (int)total - 1);

    u8g2.drawVLine(railX, railY, railH);
    u8g2.drawBox(railX - 1, markerY, 3, markerH);
}

static void drawSelectedLabel() {
    const char* label = menu_labels[menu_index];

    u8g2.setFont(u8g2_font_6x10_tr);
    int labelWidth = u8g2.getStrWidth(label);
    int labelX = (128 - labelWidth) / 2;

    u8g2.setDrawColor(1);
    u8g2.drawBox(max(2, labelX - 4), 53, min(124, labelWidth + 8), 10);
    u8g2.setDrawColor(0);
    u8g2.drawStr(labelX, 61, label);
    u8g2.setDrawColor(1);
}

// --- COLECCIÓN DE ICONOS ÚNICOS ---
void drawBolt(int x, int y) { u8g2.drawLine(x+22, y+2, x+10, y+16); u8g2.drawLine(x+10, y+16, x+24, y+16); u8g2.drawLine(x+24, y+16, x+12, y+30); }
void drawFlame(int x, int y) { u8g2.drawTriangle(x+16, y+2, x+8, y+20, x+24, y+20); u8g2.drawDisc(x+16, y+22, 8); u8g2.setDrawColor(0); u8g2.drawDisc(x+16, y+24, 4); u8g2.setDrawColor(1); }
void drawBan(int x, int y) { u8g2.drawCircle(x+16, y+16, 13); u8g2.drawLine(x+7, y+25, x+25, y+7); }
void drawBTJam(int x, int y) { u8g2.drawLine(x+12, y+6, x+12, y+26); u8g2.drawLine(x+12, y+6, x+20, y+11); u8g2.drawLine(x+20, y+11, x+12, y+16); u8g2.drawLine(x+12, y+16, x+20, y+21); u8g2.drawLine(x+20, y+21, x+12, y+26); u8g2.drawLine(x+6, y+6, x+26, y+26); }
void drawRadar(int x, int y) { u8g2.drawCircle(x+16, y+16, 14); u8g2.drawCircle(x+16, y+16, 7); u8g2.drawLine(x+16, y+2, x+16, y+30); u8g2.drawLine(x+2, y+16, x+30, y+16); u8g2.drawLine(x+16, y+16, x+28, y+5); }
void drawSearch(int x, int y) { u8g2.drawCircle(x+14, y+12, 9); u8g2.drawLine(x+20, y+19, x+28, y+28); }
void drawMegaphone(int x, int y) { u8g2.drawTriangle(x+8, y+16, x+24, y+6, x+24, y+26); u8g2.drawBox(x+4, y+14, 6, 6); }
void drawLock(int x, int y) { u8g2.drawFrame(x+8, y+14, 16, 13); u8g2.drawCircle(x+16, y+14, 7, U8G2_DRAW_UPPER_RIGHT|U8G2_DRAW_UPPER_LEFT); u8g2.drawDisc(x+16, y+21, 2); }
void drawAlien(int x, int y) { u8g2.drawBox(x+6, y+10, 20, 10); u8g2.drawBox(x+4, y+12, 24, 6); u8g2.drawBox(x+8, y+22, 4, 4); u8g2.drawBox(x+20, y+22, 4, 4); u8g2.setDrawColor(0); u8g2.drawBox(x+9, y+13, 4, 4); u8g2.drawBox(x+19, y+13, 4, 4); u8g2.setDrawColor(1); }
void drawFolder(int x, int y) { u8g2.drawFrame(x+4, y+8, 24, 18); u8g2.drawBox(x+4, y+4, 10, 4); }
void drawInfo(int x, int y) { u8g2.drawCircle(x+16, y+16, 14); u8g2.drawBox(x+15, y+12, 2, 10); u8g2.drawDisc(x+16, y+8, 2); }
void drawSnakeIcon(int x, int y) { u8g2.drawFrame(x+6, y+6, 20, 20); u8g2.drawBox(x+10, y+10, 4, 4); u8g2.drawBox(x+14, y+10, 4, 4); u8g2.drawBox(x+14, y+14, 4, 4); u8g2.drawDisc(x+22, y+22, 2); }
void drawMonitorIcon(int x, int y) { u8g2.drawFrame(x+4, y+6, 24, 20); u8g2.drawLine(x+6, y+20, x+12, y+10); u8g2.drawLine(x+12, y+10, x+18, y+24); u8g2.drawLine(x+18, y+24, x+24, y+14); }
void drawScanIcon(int x, int y) { u8g2.drawCircle(x+14, y+12, 7); u8g2.drawLine(x+18, y+18, x+24, y+24); u8g2.drawHLine(x+4, y+28, 24); }
void drawControlIcon(int x, int y) { u8g2.drawFrame(x+6, y+10, 20, 12); u8g2.drawBox(x+14, y+22, 4, 4); u8g2.drawCircle(x+16, y+16, 3); }
void drawRemoteIcon(int x, int y) { u8g2.drawFrame(x+10, y+4, 12, 24); u8g2.drawBox(x+13, y+7, 6, 4); u8g2.drawBox(x+13, y+13, 2, 2); u8g2.drawBox(x+17, y+13, 2, 2); u8g2.drawBox(x+13, y+17, 6, 2); u8g2.drawDisc(x+16, y+23, 2); }
void drawBluetoothIcon(int x, int y) { u8g2.drawLine(x+15, y+4, x+15, y+28); u8g2.drawLine(x+15, y+4, x+24, y+11); u8g2.drawLine(x+24, y+11, x+15, y+17); u8g2.drawLine(x+15, y+17, x+24, y+23); u8g2.drawLine(x+24, y+23, x+15, y+28); u8g2.drawLine(x+7, y+10, x+23, y+22); u8g2.drawLine(x+7, y+22, x+23, y+10); }
void drawBtAnalyzerIcon(int x, int y) { u8g2.drawLine(x+7, y+4, x+7, y+28); u8g2.drawLine(x+7, y+4, x+15, y+10); u8g2.drawLine(x+15, y+10, x+7, y+16); u8g2.drawLine(x+7, y+16, x+15, y+22); u8g2.drawLine(x+15, y+22, x+7, y+28); u8g2.drawFrame(x+19, y+8, 10, 18); u8g2.drawLine(x+20, y+23, x+22, y+19); u8g2.drawLine(x+22, y+19, x+24, y+22); u8g2.drawLine(x+24, y+22, x+28, y+12); }
void drawBtSpectrumIcon(int x, int y) { u8g2.drawLine(x+5, y+5, x+5, y+28); u8g2.drawLine(x+5, y+5, x+13, y+11); u8g2.drawLine(x+13, y+11, x+5, y+17); u8g2.drawLine(x+5, y+17, x+13, y+23); u8g2.drawLine(x+13, y+23, x+5, y+28); for (int i = 0; i < 6; i++) { int h = 4 + ((i * 5) % 15); u8g2.drawBox(x+16+i*2, y+27-h, 1, h); } u8g2.drawHLine(x+15, y+28, 14); }
void drawRfHeatmapIcon(int x, int y) { u8g2.drawFrame(x+4, y+5, 24, 22); for (int row = 0; row < 4; row++) { for (int col = 0; col < 6; col++) { if (((row * 3 + col * 2) % 5) < 3) u8g2.drawBox(x+7+col*3, y+8+row*4, 2, 2); else u8g2.drawPixel(x+8+col*3, y+9+row*4); } } }
void drawChannelAdvisorIcon(int x, int y) { u8g2.drawFrame(x+5, y+7, 22, 16); u8g2.drawLine(x+8, y+20, x+12, y+14); u8g2.drawLine(x+12, y+14, x+17, y+17); u8g2.drawLine(x+17, y+17, x+24, y+10); u8g2.drawDisc(x+24, y+10, 2); u8g2.drawLine(x+11, y+26, x+21, y+26); u8g2.drawLine(x+16, y+23, x+16, y+28); }
void drawNrfLinkIcon(int x, int y) { u8g2.drawFrame(x+4, y+9, 9, 13); u8g2.drawFrame(x+20, y+9, 9, 13); u8g2.drawLine(x+8, y+9, x+8, y+4); u8g2.drawLine(x+24, y+9, x+24, y+4); u8g2.drawDisc(x+8, y+24, 1); u8g2.drawDisc(x+24, y+24, 1); u8g2.drawCircle(x+16, y+15, 3); u8g2.drawCircle(x+16, y+15, 7); u8g2.drawLine(x+13, y+15, x+8, y+15); u8g2.drawLine(x+19, y+15, x+24, y+15); }
void drawNrfChatIcon(int x, int y) { u8g2.drawRFrame(x+4, y+7, 24, 15, 3); u8g2.drawTriangle(x+10, y+22, x+15, y+22, x+10, y+27); u8g2.drawHLine(x+8, y+12, 16); u8g2.drawHLine(x+8, y+16, 11); u8g2.drawCircle(x+24, y+25, 3); u8g2.drawCircle(x+24, y+25, 6, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_LOWER_RIGHT); }
void drawCoexIcon(int x, int y) { u8g2.drawFrame(x+3, y+7, 26, 18); u8g2.drawHLine(x+6, y+13, 20); u8g2.drawHLine(x+6, y+19, 20); u8g2.drawCircle(x+10, y+13, 2); u8g2.drawCircle(x+18, y+19, 2); u8g2.drawVLine(x+9, y+5, 23); u8g2.drawVLine(x+21, y+5, 23); u8g2.drawLine(x+5, y+27, x+27, y+5); }
void drawDualNrfScopeIcon(int x, int y) { u8g2.drawFrame(x+3, y+6, 26, 21); u8g2.drawHLine(x+5, y+16, 22); u8g2.drawLine(x+5, y+12, x+9, y+9); u8g2.drawLine(x+9, y+9, x+14, y+14); u8g2.drawLine(x+14, y+14, x+20, y+8); u8g2.drawLine(x+20, y+8, x+27, y+13); u8g2.drawLine(x+5, y+23, x+10, y+18); u8g2.drawLine(x+10, y+18, x+16, y+24); u8g2.drawLine(x+16, y+24, x+22, y+19); u8g2.drawLine(x+22, y+19, x+27, y+22); }
void drawSystemIcon(int x, int y) { u8g2.drawCircle(x+16, y+16, 11); u8g2.drawDisc(x+16, y+16, 4); u8g2.drawHLine(x+2, y+15, 7); u8g2.drawHLine(x+23, y+15, 7); u8g2.drawVLine(x+15, y+2, 7); u8g2.drawVLine(x+15, y+23, 7); }
void drawSkullIcon(int x, int y) {
    u8g2.drawCircle(x+16, y+13, 11);
    u8g2.drawBox(x+8, y+18, 16, 8);
    u8g2.setDrawColor(0);
    u8g2.drawDisc(x+12, y+13, 3);
    u8g2.drawDisc(x+20, y+13, 3);
    u8g2.drawTriangle(x+16, y+16, x+13, y+21, x+19, y+21);
    u8g2.setDrawColor(1);
    u8g2.drawVLine(x+11, y+23, 5);
    u8g2.drawVLine(x+16, y+22, 6);
    u8g2.drawVLine(x+21, y+23, 5);
    u8g2.drawHLine(x+9, y+27, 14);
}

// --- ICONOS ANIMADOS DE CATEGORIA ---

// WiFi: ondas concentricas que se propagan en cascada (4 fases)
void drawWifiCategoryIcon(int x, int y, uint8_t frame) {
    uint8_t step = (frame / 6) % 4;
    u8g2.drawDisc(x+16, y+22, 2);
    if (step >= 1) u8g2.drawCircle(x+16, y+22, 6,  U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    if (step >= 2) u8g2.drawCircle(x+16, y+22, 10, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    if (step >= 3) u8g2.drawCircle(x+16, y+22, 14, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
}

// RF: barrido radar con estela de la posicion anterior
void drawRfCategoryIcon(int x, int y, uint8_t frame) {
    static const int8_t sx[] = {0, 8, 12, 7, 0, -7, -12, -8};
    static const int8_t sy[] = {-12, -9, 0, 8, 12, 8, 0, -9};
    uint8_t pos  = (frame / 4) & 7;
    uint8_t tail = (pos - 1) & 7;

    u8g2.drawCircle(x+16, y+16, 14);
    u8g2.drawDisc(x+16, y+16, 2);
    u8g2.drawPixel(x+16+sx[tail]/2, y+16+sy[tail]/2);
    u8g2.drawLine(x+16, y+16, x+16+sx[pos], y+16+sy[pos]);
    u8g2.drawDisc(x+16+sx[pos], y+16+sy[pos], 2);
}

// Bluetooth: simbolo BT estatico + ondas de emparejamiento saliendo a la derecha
void drawBluetoothCategoryIcon(int x, int y, uint8_t frame) {
    drawBluetoothIcon(x, y);
    uint8_t step = (frame / 8) % 3;
    if (step >= 1) u8g2.drawCircle(x+26, y+16, 2, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_LOWER_RIGHT);
    if (step >= 2) u8g2.drawCircle(x+26, y+16, 4, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_LOWER_RIGHT);
}

// Warning / ILEGAL: simbolo de prohibido (circulo tachado) con onda de alerta expansiva
void drawWarningCategoryIcon(int x, int y, uint8_t frame) {
    // simbolo de prohibido base (siempre visible)
    u8g2.drawCircle(x+16, y+16, 11);
    u8g2.drawLine(x+8, y+24, x+24, y+8);

    // onda de alerta expansiva: aparece, crece y descansa
    uint8_t pulse = (frame / 5) % 4;
    if (pulse == 1) u8g2.drawCircle(x+16, y+16, 13);
    if (pulse == 2) u8g2.drawCircle(x+16, y+16, 15);
}

// System: engranaje con particula recorriendo el perimetro
void drawSystemCategoryIcon(int x, int y, uint8_t frame) {
    static const int8_t ox[] = {11, 8, 0, -8, -11, -8, 0, 8};
    static const int8_t oy[] = {0, 8, 11, 8, 0, -8, -11, -8};
    uint8_t pos = (frame / 6) & 7;

    u8g2.drawCircle(x+16, y+16, 11);
    u8g2.drawDisc(x+16, y+16, 4);
    u8g2.drawHLine(x+2, y+15, 7);
    u8g2.drawHLine(x+23, y+15, 7);
    u8g2.drawVLine(x+15, y+2, 7);
    u8g2.drawVLine(x+15, y+23, 7);
    u8g2.drawDisc(x+16+ox[pos], y+16+oy[pos], 1);
}

void drawGamesCategoryIcon(int x, int y, uint8_t frame) {
    uint8_t blink = (frame / 8) & 1;
    u8g2.drawRFrame(x+3, y+11, 26, 14, 4);
    u8g2.drawDisc(x+10, y+18, 2);
    u8g2.drawHLine(x+7, y+18, 6);
    u8g2.drawVLine(x+10, y+15, 6);
    u8g2.drawDisc(x+21, y+16, 1 + blink);
    u8g2.drawDisc(x+25, y+20, 1);
}

// --- NUEVOS DISEÑOS EXCLUSIVOS ---
void drawPortalIcon(int x, int y) { // EVIL PORTAL: Puerta con remolino
    u8g2.drawFrame(x+8, y+4, 16, 24);
    u8g2.drawCircle(x+16, y+14, 4);
    u8g2.drawCircle(x+16, y+14, 2);
    u8g2.drawHLine(x+4, y+28, 24);
}
void drawDashboardIcon(int x, int y) { // WEB DASHBOARD: Monitor de PC
    u8g2.drawFrame(x+4, y+6, 24, 16);
    u8g2.drawBox(x+12, y+22, 8, 2);
    u8g2.drawHLine(x+8, y+24, 16);
    u8g2.drawPixel(x+6, y+8); // Detalle pantalla
}
void drawIPScannerIcon(int x, int y) { // IP SCANNER: Lupa con "red"
    u8g2.drawCircle(x+12, y+12, 8);
    u8g2.drawLine(x+12, y+8, x+12, y+16);
    u8g2.drawLine(x+8, y+12, x+16, y+12);
    u8g2.drawLine(x+18, y+18, x+26, y+26);
}

void drawWifiRadarIcon(int x, int y) {
    u8g2.drawCircle(x+16, y+17, 4);
    u8g2.drawCircle(x+16, y+17, 10);
    u8g2.drawCircle(x+16, y+17, 15);
    u8g2.drawLine(x+16, y+17, x+27, y+7);
    u8g2.drawDisc(x+25, y+8, 2);
}

void drawChannelScanIcon(int x, int y) {
    u8g2.drawFrame(x+4, y+5, 24, 22);
    for (int i = 0; i < 5; i++) {
        int h = 5 + ((i * 4) % 15);
        u8g2.drawBox(x + 7 + i * 4, y + 25 - h, 3, h);
    }
    u8g2.drawHLine(x+6, y+28, 20);
}

void drawCentinelaIcon(int x, int y) {
    if (attack_confirmed && (millis() / 250) % 2 == 0) return;
    u8g2.drawFrame(x+8, y+4, 16, 18);
    u8g2.drawTriangle(x+8, y+22, x+24, y+22, x+16, y+30);
    u8g2.drawCircle(x+16, y+14, 4);
    u8g2.drawDisc(x+16, y+14, 1);
    u8g2.drawLine(x+4, y+10, x+8, y+10);
    u8g2.drawLine(x+24, y+10, x+28, y+10);
}

// --- DIBUJO DEL MENÚ PRINCIPAL ---
static void drawMenuIcon(int index, int x, int y) {
    switch(index) {
        case 0:  drawScanIcon(x, y); break;
        case 1:  drawWifiRadarIcon(x, y); break;
        case 2:  drawChannelScanIcon(x, y); break;
        case 3:  drawBolt(x, y); break;
        case 4:  drawSearch(x, y); break;
        case 5:  drawMonitorIcon(x, y); break;
        case 6:  drawCentinelaIcon(x, y); break;
        case 7:  drawBan(x, y); break;
        case 8:  drawRadar(x, y); break;
        case 9:  drawBTJam(x, y); break;
        case 10: drawMegaphone(x, y); break;
        case 11: drawFlame(x, y); break;
        case 12: drawAlien(x, y); break;
        case 13: drawPortalIcon(x, y); break;
        case 14: drawIPScannerIcon(x, y); break;
        case 15: drawControlIcon(x, y); break;
        case 16: drawDashboardIcon(x, y); break;
        case 17: drawRemoteIcon(x, y); break;
        case 18: drawFolder(x, y); break;
        case 19: drawSnakeIcon(x, y); break;
        case 20: drawInfo(x, y); break;
        case 21: drawBtAnalyzerIcon(x, y); break;
        case 22: drawBtSpectrumIcon(x, y); break;
        case 23: drawRfHeatmapIcon(x, y); break;
        case 24: drawChannelAdvisorIcon(x, y); break;
        case 25: drawNrfLinkIcon(x, y); break;
        case 26: drawNrfChatIcon(x, y); break;
        case 27: drawCoexIcon(x, y); break;
        case 28: drawDualNrfScopeIcon(x, y); break;
        case 29: drawSkullIcon(x, y); break;
    }
}

static void drawCategoryIcon(uint8_t icon, int x, int y) {
    uint8_t frame = (millis() / 80) & 0xFF;
    switch (icon) {
        case MENU_ICON_WIFI:
            drawWifiCategoryIcon(x, y, frame);
            break;
        case MENU_ICON_RF:
            drawRfCategoryIcon(x, y, frame);
            break;
        case MENU_ICON_BLUETOOTH:
            drawBluetoothCategoryIcon(x, y, frame);
            break;
        case MENU_ICON_WARNING:
            drawWarningCategoryIcon(x, y, frame);
            break;
        case MENU_ICON_GAMES:
            drawGamesCategoryIcon(x, y, frame);
            break;
        case MENU_ICON_SYSTEM:
            drawSystemCategoryIcon(x, y, frame);
            break;
        default:
            drawInfo(x, y);
            break;
    }
}

static void drawCategoryScreen() {
    uint8_t total = menuCategoryCount();
    const MenuCategory& cat = menuCategoryAt(category_index);

    u8g2.clearBuffer();
    char status[8];
    snprintf(status, sizeof(status), "%02d/%02d", category_index + 1, total);
    UiTheme::drawHeader(u8g2, "BWifiKill v4.0", status);

    drawCategoryIcon(cat.icon, 48, 18);
    drawCategoryRail(total, category_index);

    const char* label = cat.name;
    u8g2.setFont(u8g2_font_6x10_tr);
    int labelWidth = u8g2.getStrWidth(label);
    int labelX = (128 - labelWidth) / 2;
    u8g2.drawBox(max(2, labelX - 4), 53, min(124, labelWidth + 8), 10);
    u8g2.setDrawColor(0);
    u8g2.drawStr(labelX, 61, label);
    u8g2.setDrawColor(1);

    u8g2.drawRFrame(0, 0, 128, 64, 5);
    u8g2.sendBuffer();
}

static void drawCategoryAppList() {
    const MenuCategory& cat = menuCategoryAt(category_index);
    int appIdx = menuCategoryAppIndex(category_index, category_app_index);

    u8g2.clearBuffer();
    char status[8];
    snprintf(status, sizeof(status), "%02d/%02d", category_app_index + 1, cat.count);
    UiTheme::drawHeader(u8g2, cat.name, status);

    drawMenuIcon(appIdx, 48, 18);
    drawCategoryRail(cat.count, category_app_index);

    const char* label = menu_labels[appIdx];
    u8g2.setFont(u8g2_font_6x10_tr);
    int labelWidth = u8g2.getStrWidth(label);
    int labelX = (128 - labelWidth) / 2;
    u8g2.drawBox(max(2, labelX - 4), 53, min(124, labelWidth + 8), 10);
    u8g2.setDrawColor(0);
    u8g2.drawStr(labelX, 61, label);
    u8g2.setDrawColor(1);

    u8g2.drawRFrame(0, 0, 128, 64, 5);
    u8g2.sendBuffer();
}

void drawBruceMenu() {
    if (!browsingCategoryApps) {
        drawCategoryScreen();
        return;
    }

    drawCategoryAppList();
}
