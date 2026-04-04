# Gemini CLI Configuration: XIAO ESP32C3 Slave

## Hardware: Seeed Studio XIAO ESP32C3
- **LED-Pin:** GPIO 2 (D2).
- **Display:** 86-LED Großanzeige (4 Digits á 21 LEDs + 2 LEDs Doppelpunkt).
- **USB:** Natives USB-C für Flashen und Seriellen Monitor.

## Architektur
- **ESP-NOW v3.x API:** Nutzt `esp_now_recv_info` Struktur.
- **FastLED:** Volle Unterstützung für ESP32-C3 (RMT-Treiber).
- **Modi:** Unterstützt Sport-Timer, Stoppuhr, Countdown und Scoreboard (Status 6).

## Flashen
1. In der Arduino IDE "XIAO_ESP32C3" auswählen.
2. `LED_PIN` ist auf 2 (D2) voreingestellt.
