/**
 * Projekt: Timer_Slave_ESP - XIAO ESP32C3 Version
 * Hardware: Seeed Studio XIAO ESP32C3
 * Display: 86-LED Großanzeige (4 Ziffern + Doppelpunkt)
 */

#include <WiFi.h>
#include <esp_now.h>
#include <FastLED.h>
#include "TimerProtocol.h"

// --- Hardware-Konfiguration XIAO ESP32C3 ---
// D2 ist GPIO 2 am XIAO ESP32C3
constexpr uint8_t LED_PIN = 2;          
constexpr uint16_t NUM_LEDS = 86;        
constexpr uint8_t BRIGHTNESS = 255;
constexpr uint8_t LEDS_PER_DIGIT = 21;
constexpr uint32_t NO_SIGNAL_TIMEOUT_MS = 3000; // 3s ohne Paket = Master nicht verbunden

CRGB leds[NUM_LEDS];

// Mapping: Bit 0=b, 1=a, 2=f, 3=g, 4=c, 5=d, 6=e (3 LEDs pro Segment)
const uint8_t DIGIT_MAP[] = {
    0b01110111, 0b00010001, 0b01101011, 0b00111011, 0b00011101,
    0b00111110, 0b01111110, 0b00010011, 0b01111111, 0b00111111
};

// incoming: wird im ESP-NOW-Callback beschrieben (anderer Task-Kontext)
// Durch volatile bool newData wird sichergestellt, dass loop() den Schreibzugriff sieht.
// Auf dem single-core ESP32C3 reicht das aus — kein echter Parallelzugriff möglich.
TimerData incomingBuffer;
volatile bool newData = false;
uint32_t lastPacketMilli = 0;

// --- Hilfsfunktionen für Display ---
void displayDigit(uint8_t pos, int8_t value, CRGB color) {
    if (pos > 3 || value < 0) return;
    
    // Start-Index berechnen (Ziffern 0, 1, Doppelpunkt, 2, 3)
    uint16_t startIdx = pos * LEDS_PER_DIGIT;
    if (pos >= 2) startIdx += 2; // Doppelpunkt-Offset (2 LEDs)

    uint8_t mask = DIGIT_MAP[value % 10];
    for (uint8_t segIdx = 0; segIdx < 7; segIdx++) {
        if ((mask >> segIdx) & 0x01) {
            for (uint8_t l = 0; l < 3; l++) {
                uint16_t idx = startIdx + (segIdx * 3) + l;
                if (idx < NUM_LEDS) leds[idx] = color;
            }
        }
    }
}

void setColon(bool state, CRGB color) {
    leds[42] = state ? color : CRGB::Black;
    leds[43] = state ? color : CRGB::Black;
}

void renderDisplay(const TimerData& d) {
    FastLED.clear();

    CRGB color = CRGB::White;
    bool colonBlink = true;

    // --- Spielstand-Modus (Sonderbehandlung) ---
    if (d.state == STATE_SCORE) {
        uint8_t scoreRed   = d.seconds / 100;
        uint8_t scoreGreen = d.seconds % 100;

        // Linke Seite: Rot (Immer 2 Stellen)
        displayDigit(3, scoreRed / 10, CRGB::Red);
        displayDigit(2, scoreRed % 10, CRGB::Red);

        // Rechte Seite: Grün (Immer 2 Stellen)
        displayDigit(1, scoreGreen / 10, CRGB::Green);
        displayDigit(0, scoreGreen % 10, CRGB::Green);

        setColon(true, CRGB::White);
    }
    // --- Standard Timer Modi ---
    else {
        switch (d.state) {
            case STATE_SETUP:    color = CRGB::Red;          break; // Rot im Setup
            case STATE_PREPARE:  color = CRGB::Yellow;       break;
            case STATE_WORK:     color = CRGB::Green;        break;
            case STATE_REST:     color = CRGB::Red;          break;
            case STATE_FINISHED: color = CRGB::Purple;       break;
            case STATE_PAUSED:   color = CRGB::Blue;         break;
        }

        if (d.state == STATE_SETUP) {
            // Blinken im 500ms Takt (An/Aus)
            if ((millis() / 500) % 2 == 0) {
                displayDigit(3, 0, color);
                displayDigit(2, 0, color);
                displayDigit(1, 0, color);
                displayDigit(0, 0, color);
                setColon(true, color);
            } else {
                FastLED.clear(); 
            }
        } else {
            uint8_t m = d.seconds / 60;
            uint8_t s = d.seconds % 60;
            
            // Immer alle 4 Stellen anzeigen (MM:SS)
            displayDigit(3, m / 10, color);
            displayDigit(2, m % 10, color);
            displayDigit(1, s / 10, color);
            displayDigit(0, s % 10, color);

            colonBlink = (d.state == STATE_PAUSED) || (d.seconds % 2 == 0);
            setColon(colonBlink, color);
        }
    }
    FastLED.show();
}

// --- ESP-NOW Callback (ESP32 API v3.x) ---
void onDataRecv(const esp_now_recv_info *recv_info, const uint8_t *incomingData, int len) {
    if (len == sizeof(TimerData)) {
        memcpy(&incomingBuffer, incomingData, sizeof(TimerData));
        lastPacketMilli = millis();
        newData = true; // volatile write zuletzt — signalisiert loop() dass Daten bereit sind
    }
}

void setup() {
    Serial.begin(115200);

    // FastLED mit XIAO ESP32C3 Support
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear(true);

    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);

    Serial.println("--- XIAO ESP32C3 SLAVE START ---");
    Serial.print("MAC Adresse: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init fehlgeschlagen");
        return;
    }
    
    esp_now_register_recv_cb(onDataRecv);
    Serial.println("Slave bereit und wartet auf Daten...");
}

void loop() {
    static uint32_t lastRender = 0;
    
    // Sofort rendern bei neuen Daten
    if (newData) {
        newData = false;
        renderDisplay(incomingBuffer);
        lastRender = millis();
    }

    // Im Setup-Modus regelmäßig neu rendern für Blink-Animation —
    // aber nur wenn der Master kürzlich ein Paket gesendet hat.
    // Ohne Master-Signal bleibt die letzte Anzeige eingefroren.
    bool masterConnected = (lastPacketMilli > 0) && (millis() - lastPacketMilli < NO_SIGNAL_TIMEOUT_MS);
    if (incomingBuffer.state == STATE_SETUP && masterConnected) {
        if (millis() - lastRender >= 100) {
            renderDisplay(incomingBuffer);
            lastRender = millis();
        }
    }
}
