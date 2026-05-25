#include "bt_spectrum.h"
#include "bt_remote.h"
#include "ui_theme.h"
#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <RF24.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <math.h>
#include <string>
#include <string.h>

extern RF24 jam1;
extern RF24 jam2;
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// ============================================================================
// Constantes
// ============================================================================

static const uint8_t  BT_FIRST_CH = 2;   // 2402 MHz
static const uint8_t  BT_LAST_CH  = 80;  // 2480 MHz
static const uint8_t  BT_CHANNELS = BT_LAST_CH - BT_FIRST_CH + 1; // 79

static const uint8_t  GRAPH_TOP    = 20;
static const uint8_t  GRAPH_BOTTOM = 52;
static const uint8_t  GRAPH_HEIGHT = GRAPH_BOTTOM - GRAPH_TOP;     // 32

static const uint8_t  SAMPLES_PER_CHANNEL = 28;
static const uint16_t PLL_SETTLE_US = 130;

static const uint8_t  MAX_BLE_UNIQUE   = 16;
static const uint32_t UNIQUE_WINDOW_MS = 30000;
static const uint16_t SCAN_RESTART_MS  = 1250;

// Peak hold + fast decay
static const uint16_t PEAK_HOLD_MS       = 600;
static const uint16_t PEAK_DECAY_STEP_MS = 200;  // 1 unidad cada 200 ms tras el hold

// BLE pulse
static const uint16_t PULSE_MS = 600;
static const uint8_t  PULSE_MAX_R = 3;

// Flash de nuevo emisor
static const uint8_t  NEW_EMITTER_DELTA = 8;
static const uint8_t  FLASH_HAT_FRAMES  = 3;

// Respiracion del piso
static const long     FLOOR_QUIET_THRESHOLD = 30;

// ============================================================================
// Estado RF
// ============================================================================

static uint8_t  rawSamples[BT_CHANNELS];
static uint8_t  displayedSample[BT_CHANNELS];
static uint8_t  peaks[BT_CHANNELS];
static uint32_t peakLastUpdate[BT_CHANNELS];
static uint8_t  flashFrames[BT_CHANNELS];
static uint8_t  channelToXLUT[BT_CHANNELS];

static bool rf1Ok = false;
static bool rf2Ok = false;

static uint16_t      energyHighWater = 100;
static uint32_t      lastEnergyHwDecay = 0;

// ============================================================================
// Estado BLE
// ============================================================================

struct UniqueAddr {
    char     addr[18];
    uint32_t lastSeen;
};

static BLEScan* bleSpectrumScan = nullptr;
static bool     bleScanActive = false;
static volatile bool exitingSpectrum = false;
static uint32_t bleScanStartedAt = 0;

static portMUX_TYPE bleStatsMux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint16_t bleSecondPackets   = 0;
static volatile uint32_t bleTotalPackets    = 0;
static volatile int32_t  bleRssiSum         = 0;
static volatile uint16_t bleRssiSamples     = 0;
static volatile int      bleStrongestSecond = -127;
static volatile int      bleStrongestEver   = -127;
static volatile uint32_t bleLastPacketMs    = 0;
static UniqueAddr        bleUniqueAddrs[MAX_BLE_UNIQUE];
static uint8_t           bleUniqueCount = 0;

static uint16_t viewBlePps             = 0;
static uint32_t viewBleTotal           = 0;
static uint8_t  viewBleUnique          = 0;
static int      viewBleAvgRssi         = -100;
static int      viewBleStrongestEver   = -127;
static uint32_t viewLastPacketMs       = 0;
static uint32_t lastStatsTick          = 0;

// ============================================================================
// LUT y conversiones
// ============================================================================

static void buildChannelLut() {
    for (uint8_t i = 0; i < BT_CHANNELS; i++) {
        channelToXLUT[i] = (uint8_t)map(i, 0, BT_CHANNELS - 1, 1, 126);
    }
}

static inline uint8_t channelToX(uint8_t idx) {
    return idx < BT_CHANNELS ? channelToXLUT[idx] : 0;
}

static inline uint8_t scaleSample(uint8_t sample) {
    uint8_t s = sample > SAMPLES_PER_CHANNEL ? SAMPLES_PER_CHANNEL : sample;
    return (uint16_t)s * GRAPH_HEIGHT / SAMPLES_PER_CHANNEL;
}

// ============================================================================
// Registro de direcciones BLE unicas
// ============================================================================

static bool rememberBleAddressUnsafe(const char* addr, uint32_t now) {
    if (!addr || !addr[0]) return false;
    for (uint8_t i = 0; i < bleUniqueCount; i++) {
        if (strcasecmp(bleUniqueAddrs[i].addr, addr) == 0) {
            bleUniqueAddrs[i].lastSeen = now;
            return true;
        }
    }
    if (bleUniqueCount >= MAX_BLE_UNIQUE) return false;
    strncpy(bleUniqueAddrs[bleUniqueCount].addr, addr,
            sizeof(bleUniqueAddrs[0].addr) - 1);
    bleUniqueAddrs[bleUniqueCount].addr[sizeof(bleUniqueAddrs[0].addr) - 1] = 0;
    bleUniqueAddrs[bleUniqueCount].lastSeen = now;
    bleUniqueCount++;
    return true;
}

static void purgeUniqueAddrs() {
    uint32_t now = millis();
    portENTER_CRITICAL(&bleStatsMux);
    uint8_t writeIdx = 0;
    for (uint8_t i = 0; i < bleUniqueCount; i++) {
        if (now - bleUniqueAddrs[i].lastSeen <= UNIQUE_WINDOW_MS) {
            if (writeIdx != i) bleUniqueAddrs[writeIdx] = bleUniqueAddrs[i];
            writeIdx++;
        }
    }
    bleUniqueCount = writeIdx;
    portEXIT_CRITICAL(&bleStatsMux);
}

class BtSpectrumBleCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice device) override {
        if (exitingSpectrum) return;

        const std::string addr = device.getAddress().toString();
        const int rssi = device.getRSSI();
        const uint32_t now = millis();

        portENTER_CRITICAL(&bleStatsMux);
        if (bleSecondPackets < 0xFFFE) bleSecondPackets++;
        if (bleTotalPackets  < 0xFFFFFFFEu) bleTotalPackets++;
        bleRssiSum += rssi;
        if (bleRssiSamples < 1000) bleRssiSamples++;
        if (rssi > bleStrongestSecond) bleStrongestSecond = rssi;
        if (rssi > bleStrongestEver)   bleStrongestEver   = rssi;
        bleLastPacketMs = now;
        rememberBleAddressUnsafe(addr.c_str(), now);
        portEXIT_CRITICAL(&bleStatsMux);
    }
};

static BtSpectrumBleCallbacks bleCallbacks;

// ============================================================================
// Estadisticas periodicas
// ============================================================================

static void resetBleStats() {
    portENTER_CRITICAL(&bleStatsMux);
    bleSecondPackets   = 0;
    bleTotalPackets    = 0;
    bleRssiSum         = 0;
    bleRssiSamples     = 0;
    bleStrongestSecond = -127;
    bleStrongestEver   = -127;
    bleLastPacketMs    = 0;
    bleUniqueCount     = 0;
    for (uint8_t i = 0; i < MAX_BLE_UNIQUE; i++) {
        bleUniqueAddrs[i].addr[0] = 0;
        bleUniqueAddrs[i].lastSeen = 0;
    }
    portEXIT_CRITICAL(&bleStatsMux);

    viewBlePps           = 0;
    viewBleTotal         = 0;
    viewBleUnique        = 0;
    viewBleAvgRssi       = -100;
    viewBleStrongestEver = -127;
    viewLastPacketMs     = 0;
    lastStatsTick        = millis();
}

static void updateBlePacketStats() {
    uint32_t now = millis();

    if (now - lastStatsTick < 1000) {
        // Cada frame refresca los counters de "ahora mismo"
        portENTER_CRITICAL(&bleStatsMux);
        viewBleTotal         = bleTotalPackets;
        viewBleUnique        = bleUniqueCount;
        viewLastPacketMs     = bleLastPacketMs;
        viewBleStrongestEver = bleStrongestEver;
        portEXIT_CRITICAL(&bleStatsMux);
        return;
    }
    lastStatsTick = now;

    // Cada segundo computa pps y promedio, y resetea el acumulador
    portENTER_CRITICAL(&bleStatsMux);
    viewBlePps           = bleSecondPackets;
    viewBleTotal         = bleTotalPackets;
    viewBleUnique        = bleUniqueCount;
    viewBleStrongestEver = bleStrongestEver;
    viewLastPacketMs     = bleLastPacketMs;
    if (bleRssiSamples > 0) {
        int32_t sum  = bleRssiSum;
        int32_t cnt  = (int32_t)bleRssiSamples;
        int32_t half = cnt / 2;
        // Redondeo correcto para negativos
        viewBleAvgRssi = sum < 0 ? (int)((sum - half) / cnt)
                                 : (int)((sum + half) / cnt);
    } else {
        viewBleAvgRssi = -100;
    }
    bleSecondPackets   = 0;
    bleRssiSum         = 0;
    bleRssiSamples     = 0;
    bleStrongestSecond = -127;
    portEXIT_CRITICAL(&bleStatsMux);
}

static void onBleSpectrumScanComplete(BLEScanResults) {
    bleScanActive = false;
    BLEScan* s = bleSpectrumScan;  // copia local: evita race con btSpectrumExit
    if (s && !exitingSpectrum) s->clearResults();
}

static void startBlePacketScan() {
    if (!bleSpectrumScan || bleScanActive || exitingSpectrum) return;
    bleScanActive = true;
    bleScanStartedAt = millis();
    bleSpectrumScan->start(1, onBleSpectrumScanComplete, false);
}

static void serviceBlePacketScan() {
    if (!bleSpectrumScan || exitingSpectrum) return;
    if (!bleScanActive) { startBlePacketScan(); return; }
    if (millis() - bleScanStartedAt > SCAN_RESTART_MS) {
        bleSpectrumScan->stop();
        bleSpectrumScan->clearResults();
        bleScanActive = false;
    }
}

// ============================================================================
// Lectura RF
// ============================================================================

static uint8_t sampleRadio(RF24& radio, uint8_t channel) {
    radio.setChannel(channel);
    delayMicroseconds(PLL_SETTLE_US);
    uint8_t hits = 0;
    for (uint8_t i = 0; i < SAMPLES_PER_CHANNEL; i++) {
        if (radio.testCarrier()) hits++;
    }
    return hits;
}

// ============================================================================
// Marcadores BLE adv y WiFi
// ============================================================================

struct ChannelMarker {
    uint8_t btChannel;   // canal absoluto (2..80)
    const char* label;
};

// BLE adv: 37 (2402), 38 (2426), 39 (2480)
static const ChannelMarker BLE_MARKERS[] = {
    { 2,  "37" },
    { 26, "38" },
    { 80, "39" },
};

// WiFi 2.4 GHz: ch 1 (2412 -> BT 12), ch 6 (2437 -> BT 37), ch 11 (2462 -> BT 62)
static const ChannelMarker WIFI_MARKERS[] = {
    { 12, "W1"  },
    { 37, "W6"  },
    { 62, "W11" },
};

static uint8_t strongestBleAdvBar() {
    uint8_t best = 0;
    uint8_t bestVal = 0;
    for (uint8_t i = 0; i < 3; i++) {
        uint8_t idx = BLE_MARKERS[i].btChannel - BT_FIRST_CH;
        if (displayedSample[idx] > bestVal) {
            bestVal = displayedSample[idx];
            best = i;
        }
    }
    return best;
}

// ============================================================================
// Dibujo: piso y grid
// ============================================================================

static void drawFloor(long totalEnergy) {
    int wave = 0;
    if (totalEnergy < FLOOR_QUIET_THRESHOLD) {
        // Sutil "respiracion" de +/-1 px cuando no pasa nada
        float t = millis() * 0.003f;
        wave = (int)(1.5f * sinf(t));
    }
    for (uint8_t x = 0; x < 128; x += 2) {
        u8g2.drawPixel(x, GRAPH_BOTTOM + wave);
    }
}

static void drawGrid() {
    // Lineas horizontales punteadas a 25/50/75 %
    uint8_t levels[3] = {
        (uint8_t)(GRAPH_BOTTOM - (GRAPH_HEIGHT * 1 / 4)),
        (uint8_t)(GRAPH_BOTTOM - (GRAPH_HEIGHT * 2 / 4)),
        (uint8_t)(GRAPH_BOTTOM - (GRAPH_HEIGHT * 3 / 4)),
    };
    for (uint8_t r = 0; r < 3; r++) {
        for (uint8_t x = 0; x < 128; x += 6) {
            u8g2.drawPixel(x, levels[r]);
        }
    }
}

// ============================================================================
// Dibujo: marcadores
// ============================================================================

static void drawMarkerLines() {
    // BLE adv: linea vertical punteada cada 4 px en toda la altura del grafico
    for (uint8_t i = 0; i < 3; i++) {
        uint8_t x = channelToX(BLE_MARKERS[i].btChannel - BT_FIRST_CH);
        for (uint8_t y = GRAPH_TOP; y < GRAPH_BOTTOM; y += 4) {
            u8g2.drawPixel(x, y);
        }
    }
    // WiFi: linea vertical punteada cada 5 px, solo en mitad inferior
    for (uint8_t i = 0; i < 3; i++) {
        uint8_t x = channelToX(WIFI_MARKERS[i].btChannel - BT_FIRST_CH);
        for (uint8_t y = GRAPH_TOP + GRAPH_HEIGHT / 2; y < GRAPH_BOTTOM; y += 5) {
            u8g2.drawPixel(x, y);
        }
    }
}

static void drawBlePulse() {
    if (viewLastPacketMs == 0) return;
    uint32_t age = millis() - viewLastPacketMs;
    if (age >= PULSE_MS) return;
    // Radio: PULSE_MAX_R -> 0 lineal
    int r = (int)PULSE_MAX_R - (int)((age * (uint32_t)PULSE_MAX_R) / PULSE_MS);
    if (r <= 0) return;
    uint8_t best = strongestBleAdvBar();
    uint8_t x = channelToX(BLE_MARKERS[best].btChannel - BT_FIRST_CH);
    u8g2.drawCircle(x, GRAPH_TOP + 4, r);
}

static void drawMarkerLabels() {
    u8g2.setFont(u8g2_font_4x6_tf);
    // BLE
    for (uint8_t i = 0; i < 3; i++) {
        uint8_t x = channelToX(BLE_MARKERS[i].btChannel - BT_FIRST_CH);
        int labelX = constrain((int)x - 3, 0, 119);
        u8g2.drawStr(labelX, GRAPH_BOTTOM + 7, BLE_MARKERS[i].label);
    }
    // WiFi
    for (uint8_t i = 0; i < 3; i++) {
        uint8_t x = channelToX(WIFI_MARKERS[i].btChannel - BT_FIRST_CH);
        int width = u8g2.getStrWidth(WIFI_MARKERS[i].label);
        int labelX = constrain((int)x - width / 2, 0, 128 - width);
        u8g2.drawStr(labelX, GRAPH_BOTTOM + 7, WIFI_MARKERS[i].label);
    }
}

// ============================================================================
// Dibujo: barras
// ============================================================================

static void drawSpectrumBar(uint8_t idx) {
    uint8_t x = channelToX(idx);
    uint8_t sample = displayedSample[idx];
    uint8_t barHeight = scaleSample(sample);

    if (barHeight > 0) {
        u8g2.drawVLine(x, GRAPH_BOTTOM - barHeight, barHeight);
    }

    // Pico (solo si esta por encima del bar height visible)
    uint8_t peakHeight = scaleSample(peaks[idx]);
    if (peakHeight > 0 && peakHeight > barHeight) {
        u8g2.drawPixel(x, GRAPH_BOTTOM - peakHeight);
    }

    // Flash hat para nuevos emisores
    if (flashFrames[idx] > 0 && barHeight > 0) {
        int hatY = (int)(GRAPH_BOTTOM - barHeight) - 2;
        if (hatY >= GRAPH_TOP) {
            int hatX = (int)x - 1;
            if (hatX < 0) hatX = 0;
            u8g2.drawHLine(hatX, hatY, 3);
        }
    }
}

// ============================================================================
// Dibujo: energy bar y header
// ============================================================================

static void drawEnergyBar(long totalEnergy) {
    if (totalEnergy > (long)energyHighWater) energyHighWater = (uint16_t)totalEnergy;
    // Decay 10% cada 10 s para reajustarse a entornos que se enfrian
    uint32_t now = millis();
    if (now - lastEnergyHwDecay > 10000) {
        if (energyHighWater > 100) {
            energyHighWater = (uint16_t)((uint32_t)energyHighWater * 9u / 10u);
            if (energyHighWater < 100) energyHighWater = 100;
        }
        lastEnergyHwDecay = now;
    }

    long barH = (totalEnergy * (long)GRAPH_HEIGHT) / (long)energyHighWater;
    if (barH < 0) barH = 0;
    if (barH > (long)GRAPH_HEIGHT) barH = GRAPH_HEIGHT;
    if (barH > 0) {
        u8g2.drawVLine(127, GRAPH_BOTTOM - (uint8_t)barH, (uint8_t)barH);
    }
}

static void drawSpectrumHeader(long totalEnergy) {
    char status[12];
    uint16_t pps = viewBlePps > 99 ? 99 : viewBlePps;
    snprintf(status, sizeof(status), "%02u/s", pps);
    UiTheme::drawHeader(u8g2, "BT SPECTRUM", status);

    u8g2.setFont(u8g2_font_4x6_tf);

    char energyLine[20];
    snprintf(energyLine, sizeof(energyLine), "E%ld U%02u",
             totalEnergy, viewBleUnique);

    char rssiLine[20];
    if (viewBlePps > 0 || viewBleTotal > 0) {
        snprintf(rssiLine, sizeof(rssiLine), "%d/%d",
                 viewBleAvgRssi, viewBleStrongestEver);
    } else {
        snprintf(rssiLine, sizeof(rssiLine), "BLE LISTEN");
    }

    // Avisos de radios desconectadas (sustituyen al energyLine)
    const char* leftText;
    if (!rf1Ok && !rf2Ok)      leftText = "RF OFF";
    else if (!rf1Ok)           leftText = "RF1 OFF";
    else if (!rf2Ok)           leftText = "RF2 OFF";
    else                       leftText = energyLine;
    u8g2.drawStr(2, GRAPH_TOP - 1, leftText);

    // Indicador de scan BLE activo (punto junto al texto izquierdo)
    if (bleScanActive && (millis() / 250) % 2 == 0) {
        int dotX = 2 + u8g2.getStrWidth(leftText) + 3;
        if (dotX < 122) u8g2.drawDisc(dotX, GRAPH_TOP - 3, 1);
    }

    // RSSI a la derecha
    int rssiX = 122 - u8g2.getStrWidth(rssiLine);
    if (rssiX < 62) rssiX = 62;
    u8g2.drawStr(rssiX, GRAPH_TOP - 1, rssiLine);
}

// ============================================================================
// Animaciones por canal: smoothing, peaks, flash
// ============================================================================

static void updateChannelAnimations() {
    uint32_t now = millis();
    for (uint8_t i = 0; i < BT_CHANNELS; i++) {
        uint8_t sample = rawSamples[i];

        // Detecta nuevo emisor ANTES de actualizar displayedSample
        bool newEmitter = (sample > displayedSample[i] + NEW_EMITTER_DELTA);

        // Smoothing asimetrico: subida instant, bajada exponencial alpha=1/4
        if (sample > displayedSample[i]) {
            displayedSample[i] = sample;
        } else {
            displayedSample[i] = (uint8_t)
                (((uint16_t)displayedSample[i] * 3u + sample) / 4u);
        }

        // Flash hat: decrementa primero, despues set
        if (flashFrames[i] > 0) flashFrames[i]--;
        if (newEmitter) flashFrames[i] = FLASH_HAT_FRAMES;

        // Peak hold + fast decay
        if (sample > peaks[i]) {
            peaks[i] = sample;
            peakLastUpdate[i] = now;
        } else {
            uint32_t elapsed = now - peakLastUpdate[i];
            if (elapsed > (uint32_t)PEAK_HOLD_MS + PEAK_DECAY_STEP_MS) {
                if (peaks[i] > 0) peaks[i]--;
                // Resetea el "reloj" al final del hold para que la proxima
                // bajada llegue tras otro PEAK_DECAY_STEP_MS
                peakLastUpdate[i] = now - PEAK_HOLD_MS;
            }
        }
    }
}

// ============================================================================
// Ciclo del modulo
// ============================================================================

void btSpectrumEnter() {
    WiFi.mode(WIFI_OFF);
    exitingSpectrum = false;

    BLEDevice::init("");
    bleSpectrumScan = BLEDevice::getScan();
    bleSpectrumScan->setActiveScan(false);
    bleSpectrumScan->setInterval(80);
    bleSpectrumScan->setWindow(70);
    bleSpectrumScan->setAdvertisedDeviceCallbacks(&bleCallbacks, true);
    bleScanActive = false;
    resetBleStats();

    rf1Ok = jam1.begin() && jam1.isChipConnected();
    if (rf1Ok) {
        jam1.setAutoAck(false);
        jam1.setDataRate(RF24_2MBPS);
        jam1.setPALevel(RF24_PA_MAX);
        jam1.startListening();
    }
    rf2Ok = jam2.begin() && jam2.isChipConnected();
    if (rf2Ok) {
        jam2.setAutoAck(false);
        jam2.setDataRate(RF24_2MBPS);
        jam2.setPALevel(RF24_PA_MAX);
        jam2.startListening();
    }

    buildChannelLut();

    memset(rawSamples,       0, sizeof(rawSamples));
    memset(displayedSample,  0, sizeof(displayedSample));
    memset(peaks,            0, sizeof(peaks));
    memset(flashFrames,      0, sizeof(flashFrames));
    for (uint8_t i = 0; i < BT_CHANNELS; i++) peakLastUpdate[i] = millis();

    energyHighWater   = 100;
    lastEnergyHwDecay = millis();
}

void btSpectrumExit() {
    exitingSpectrum = true;
    BLEScan* scan = bleSpectrumScan;
    bleSpectrumScan = nullptr;
    if (scan) {
        scan->stop();
        scan->clearResults();
    }
    bleScanActive = false;
    if (!btRemoteBleActive()) {
        BLEDevice::deinit(false);
    }

    if (rf1Ok) jam1.stopListening();
    if (rf2Ok) jam2.stopListening();
    rf1Ok = false;
    rf2Ok = false;

    memset(rawSamples,      0, sizeof(rawSamples));
    memset(displayedSample, 0, sizeof(displayedSample));
    memset(peaks,           0, sizeof(peaks));
    memset(flashFrames,     0, sizeof(flashFrames));
    resetBleStats();
    exitingSpectrum = false;
}

void btSpectrumLoop() {
    serviceBlePacketScan();
    updateBlePacketStats();
    purgeUniqueAddrs();

    // 1. Muestreo RF
    for (uint8_t slot = 0; slot < 40; slot++) {
        uint8_t ch1 = BT_FIRST_CH + slot;
        uint8_t idx1 = slot;
        rawSamples[idx1] = rf1Ok ? sampleRadio(jam1, ch1) : 0;

        uint8_t ch2 = BT_FIRST_CH + 40 + slot;
        if (ch2 <= BT_LAST_CH) {
            uint8_t idx2 = 40 + slot;
            rawSamples[idx2] = rf2Ok ? sampleRadio(jam2, ch2) : 0;
        }
    }

    // 2. Animaciones por canal
    updateChannelAnimations();

    // 3. Energia total a partir de los valores suavizados (mismo dato que
    //    se muestra en pantalla, para consistencia visual)
    long totalEnergy = 0;
    for (uint8_t i = 0; i < BT_CHANNELS; i++) totalEnergy += displayedSample[i];

    // 4. Render
    u8g2.clearBuffer();
    drawGrid();
    drawFloor(totalEnergy);
    drawMarkerLines();
    for (uint8_t i = 0; i < BT_CHANNELS; i++) drawSpectrumBar(i);
    drawBlePulse();
    drawMarkerLabels();
    drawEnergyBar(totalEnergy);
    drawSpectrumHeader(totalEnergy);
    u8g2.sendBuffer();
}