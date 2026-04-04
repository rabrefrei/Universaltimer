# Universaltimer

Ein drahtloses Sport-Timer-System auf Basis von ESP-NOW, bestehend aus einem M5Stack Core2 als Touchscreen-Controller (Master) und einem XIAO ESP32C3 mit 86-LED RGB-Großanzeige (Slave).

## Features

- **4 Modi**: Sport-Timer (Intervalltraining), Stoppuhr, Countdown, Spielstand
- **Drahtlose Synchronisation** über ESP-NOW – kein Router, kein Internet erforderlich
- **Große LED-Anzeige**: 86× WS2812B RGB-LEDs als 7-Segment-Display (4 Ziffern + Doppelpunkt)
- **Touchscreen-Bedienung** am M5Core2
- **Farbcodiertes Feedback**: Jeder Betriebszustand hat eine eigene Farbe (grün = Arbeit, rot = Pause, gelb = Vorbereitung, ...)
- **Akustische Signale**: Warntöne 5 Sekunden vor Phasenwechseln
- **Energieverwaltung**: Auto-Dimm und Auto-Off am Master

## Modi

| Modus | Beschreibung |
|---|---|
| **Sport-Timer** | Intervalltraining: Runden, Arbeitszeit und Pausenzeit einstellbar (1–99 Runden, 5–3600 s) |
| **Stoppuhr** | Hochpräzise Zeitmessung bis auf Hundertstel-Sekunden (MM:SS.hh) |
| **Countdown** | Herunterzählen von einer eingestellten Zeit (10 s – 10 h) |
| **Spielstand** | Punkteanzeige für zwei Teams (0–99), ohne Zeitkomponente |

## Hardware

### Master – M5Stack Core2

- ESP32 Dual Core, 240 MHz
- 2" IPS Touchscreen (320×240 px, kapazitiv)
- Interner 390 mAh Akku
- ESP-NOW (2,4 GHz)

### Slave – XIAO ESP32C3 + LED-Panel

- ESP32-C3 Single Core, 160 MHz
- 86× WS2812B RGB-LEDs (4-stelliges 7-Segment-Display)
- Stromversorgung: 5 V via USB-C
- ESP-NOW (2,4 GHz)

### Verbindung

Die Kommunikation läuft über **ESP-NOW** – ein direktes Peer-to-Peer-Protokoll im 2,4-GHz-Band. Reichweite ca. 40 m in Gebäuden, bis zu 100 m im Freigelände, Latenz unter 5 ms.

## Projektstruktur

```
Universaltimer/
├── Master/
│   ├── Master.ino          # Hauptcode für M5Core2 (Touchscreen, Timer-Logik, ESP-NOW)
│   └── TimerProtocol.h     # Gemeinsames Kommunikationsprotokoll
├── Slave/
│   ├── Slave.ino           # Code für XIAO ESP32C3 (LED-Anzeige, ESP-NOW Empfänger)
│   └── TimerProtocol.h     # Gemeinsames Kommunikationsprotokoll
└── TimerProtocol.h         # Protokolldefinition (root-Kopie)
```

## Kommunikationsprotokoll

Master und Slave tauschen eine kompakte 4-Byte-Struktur aus:

```cpp
struct TimerData {
    uint8_t  state;    // Betriebszustand (0–6)
    uint16_t seconds;  // Aktuelle Zeit in Sekunden oder Punktestand
    uint8_t  round;    // Aktuelle Runde oder Setup-Schritt
};
```

| State | Bedeutung |
|---|---|
| 0 | Setup (Einstellung) |
| 1 | Vorbereitung (5-Sek.-Countdown) |
| 2 | Arbeitsphase läuft |
| 3 | Pausenphase läuft |
| 4 | Timer beendet |
| 5 | Pausiert (manuell) |
| 6 | Spielstand-Modus |

## Verwendete Bibliotheken

### Master
- [M5Unified](https://github.com/m5stack/M5Unified) – M5Stack Hardware-Abstraktion
- `WiFi` / `esp_now` – ESP-NOW Kommunikation (ESP-IDF / Arduino)

### Slave
- [FastLED](https://github.com/FastLED/FastLED) – WS2812B LED-Steuerung
- `WiFi` / `esp_now` – ESP-NOW Kommunikation

## Aufbauen & Flashen

### Voraussetzungen

- [Arduino IDE](https://www.arduino.cc/en/software) (empfohlen: 2.x)
- Board-Paket: **M5Stack** (für Master) → in der Arduino IDE über den Boards Manager installieren
- Board-Paket: **Seeed XIAO ESP32C3** (für Slave) → Boards Manager, Seeed Studio ESP32 Paket
- Bibliotheken: `M5Unified`, `FastLED` (über den Arduino Bibliotheksmanager)

### Master flashen

1. `Master/Master.ino` in der Arduino IDE öffnen
2. Board: **M5Stack Core2** auswählen
3. MAC-Adresse des Slave in `Master.ino` eintragen (Variable `slaveMac`)
4. Flashen

### Slave flashen

1. `Slave/Slave.ino` in der Arduino IDE öffnen
2. Board: **XIAO ESP32C3** auswählen
3. Flashen

> Die MAC-Adresse des Slave kann über den seriellen Monitor nach dem ersten Flashen ausgelesen werden (`Serial.println(WiFi.macAddress())`).

## Typische Anwendungen

- **HIIT / CrossFit**: Sport-Timer mit individuellen Intervallen und Rundenzähler
- **Yoga / Meditation**: Einfacher Countdown ohne Ablenkung
- **Sportveranstaltungen**: Sichtbare Großanzeige für Zuschauer und Athleten
- **Spielstände**: Live-Scoreboard für Tischtennis, Darts, etc.

