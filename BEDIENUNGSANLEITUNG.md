# Bedienungsanleitung – Universaltimer

## Inhaltsverzeichnis

1. [Systemübersicht](#systemübersicht)
2. [Erstinbetriebnahme](#erstinbetriebnahme)
3. [Hauptmenü](#hauptmenü)
4. [Modus 1: Sport-Timer](#modus-1-sport-timer)
5. [Modus 2: Stoppuhr](#modus-2-stoppuhr)
6. [Modus 3: Countdown](#modus-3-countdown)
7. [Modus 4: Spielstand](#modus-4-spielstand)
8. [Slave-Display (LED-Großanzeige)](#slave-display-led-großanzeige)
9. [Energieverwaltung](#energieverwaltung)
10. [Technische Daten](#technische-daten)

---

## Systemübersicht

Der Universaltimer besteht aus zwei Geräten, die drahtlos miteinander kommunizieren:

| Komponente | Gerät | Funktion |
|---|---|---|
| **Master** | M5Stack Core2 | Touchscreen-Steuereinheit |
| **Slave** | XIAO ESP32C3 + LEDs | LED-Großanzeige (86 LEDs) |

Die beiden Geräte verbinden sich automatisch über **ESP-NOW** – eine direkte WLAN-Verbindung ohne Router, mit einer Reichweite von ca. 40 m (in Gebäuden) bis 100 m (Freigelände). Es ist keine App oder Internetverbindung erforderlich.

---

## Erstinbetriebnahme

1. **Großanzeige (Slave) einschalten**: Mitgeliefertes Netzteil mit Strom anschließen. Die LED-Anzeige leuchtet kurz auf, dann blinkt sie rot – das Gerät sucht den Master.
2. **Fernbedienung (Master) einschalten**: Das Hauptmenü erscheint automatisch. Die Fernbedienung kann auch ohne Großanzeige verwendet werden.
3. **Verbindungsaufbau**: Der Slave erkennt den Master automatisch (keine Konfiguration nötig). Nach erfolgreichem Verbindungsaufbau zeigt der Slave die aktuelle Einstellung an.

> **Hinweis**: Slave und Master müssen einmalig mit der korrekten MAC-Adresse konfiguriert sein (Firmware-seitig hinterlegt). Bei Austausch eines Gerätes muss die Firmware entsprechend angepasst werden.

---

## Hauptmenü

Nach dem Einschalten erscheint das Hauptmenü mit vier Kacheln:

```
┌──────────────┬──────────────┐
│   SPORT      │  STOPPUHR    │
│  (blau)      │   (grün)     │
├──────────────┼──────────────┤
│  COUNTDOWN   │  SPIELSTAND  │
│  (orange)    │   (weiß)     │
└──────────────┴──────────────┘
```

Tippe auf die gewünschte Kachel, um den entsprechenden Modus zu öffnen.

---

## Modus 1: Sport-Timer

Der Sport-Timer ist für **Intervall-Training** (z. B. HIIT, CrossFit, Tabata) ausgelegt. Er zählt automatisch Arbeits- und Pausenphasen über mehrere Runden.

### Einrichten

Nach dem Tippen auf **SPORT** erscheint die Einstellungsansicht. Die drei Parameter werden nacheinander konfiguriert:

#### Schritt 1: Rundenanzahl

- Anzeige: **RUNDEN** (rot)
- Einstellbereich: **1–99 Runden**
- Steuerung: Tippe **−** oder **+** zum Anpassen
- Weiter: Tippe **MODE** (drei Punkte) oder **PLAY** um direkt zu starten

#### Schritt 2: Arbeitszeit

- Anzeige: **ARBEIT** (grün)
- Einstellbereich: **5–3600 Sekunden**
- Steuerung: **−** / **+** Buttons
- Weiter: **MODE** tippen

#### Schritt 3: Pausenzeit

- Anzeige: **PAUSE** (orange)
- Einstellbereich: **5–3600 Sekunden**
- Steuerung: **−** / **+** Buttons
- Starten: **PLAY** (Dreieck) tippen

### Ablauf nach dem Start

```
BEREIT (5 Sek. Countdown, gelb)
   ↓
ARBEIT (grün, zählt herunter)
   ↓
PAUSE (rot, zählt herunter)
   ↓
... Wiederholung für jede Runde ...
   ↓
FERTIG! (lila auf Slave, goldgelb auf Master)
```

Das Display zeigt oben links die aktuelle Runde (z. B. **RUNDE 2 / 5**) und in der Mitte die verbleibende Zeit.

### Bedienung während des Laufs

| Aktion | Geste |
|---|---|
| Pause | Tippe den mittig-unten Button |
| Fortsetzen | Tippe erneut |
| Zurück zum Setup | Langer Druck (2 Sek.) auf den Button |
| Zurück zum Menü | Tippe **< MENU** oben links |

### Signaltöne

| Ereignis | Ton |
|---|---|
| 5 Sek. vor Phasenwechsel | Mehrere kurze Warntöne (1500 Hz) |
| Arbeitsphase beginnt | Hoher Ton (1200 Hz) |
| Pausenphase beginnt | Tiefer Ton (800 Hz) |
| Timer beendet | Zweistufiger Abschlusston |

### Beispiel: Tabata-Training

```
Runden:    8
Arbeitszeit: 20 Sekunden
Pausenzeit:  10 Sekunden
Gesamtzeit:  4 Minuten
```

---

## Modus 2: Stoppuhr

Die Stoppuhr misst Zeiten **hochgenau** bis auf Hundertstel-Sekunden.

### Bedienung

1. Tippe im Hauptmenü auf **STOPPUHR**
2. Anzeige zeigt **00:00.00** (MM:SS.hh)
3. Tippe **PLAY** → Zeitmessung startet (grün)
4. Tippe **PAUSE** → Zeit stoppt (grau, "GESTOPPT")
5. Im gestoppten Zustand: Tippe **RESET** (links) → zurück auf 00:00.00

### Slave-Anzeige

Das LED-Display zeigt die Zeit im Format **MM:SS**. Wenn die Zeit gestoppt ist, blinkt der Doppelpunkt.

### Zurück zum Menü

Tippe **< MENU** oben links auf dem Touchscreen.

---

## Modus 3: Countdown

Der Countdown zählt von einer eingestellten Zeit auf null herunter.

### Einrichten

1. Tippe im Hauptmenü auf **COUNTDOWN**
2. Standardzeit: **01:00** (60 Sekunden)
3. Tippe **−** oder **+** zum Anpassen (Schritte: 10 Sekunden; Bereich: 10 s – 36.000 s / 10 h)

### Ablauf nach dem Start

```
BEREIT (5 Sek. Countdown, gelb)
   ↓
Countdown läuft (orange)
   ↓
ZEIT UM! (rot, bei Null)
```

### Bedienung während des Laufs

| Aktion | Geste |
|---|---|
| Pause | Mittig-unten Button tippen |
| Fortsetzen | Erneut tippen |
| Zurück zum Setup | Langer Druck auf Button |

---

## Modus 4: Spielstand

Der Spielstand-Modus zeigt Punkte für zwei Teams oder Spieler an – ohne Zeitkomponente.

### Bedienung

1. Tippe im Hauptmenü auf **SPIELSTAND**
2. Bildschirm zeigt zwei Scores nebeneinander:
   - **Links (Rot)**: Team 1 / Spieler 1
   - **Rechts (Grün)**: Team 2 / Spieler 2
3. Tippe die **Pfeil-nach-oben** Buttons um Punkte zu erhöhen
4. Tippe die **Pfeil-nach-unten** Buttons um Punkte zu verringern
5. Bereich: **0–99** Punkte pro Seite

### Slave-Anzeige

Das LED-Display zeigt den Spielstand im Format **RR:GG**, wobei die linken zwei Ziffern in Rot (Team 1) und die rechten zwei Ziffern in Grün (Team 2) leuchten. Der Doppelpunkt in der Mitte ist weiß.

---

## Slave-Display (LED-Großanzeige)

### Farben pro Betriebszustand

| Zustand | Farbe auf dem Slave |
|---|---|
| Einstellmodus (Setup) | Rot, blinkend |
| Vorbereitung (5-Sek.) | Gelb |
| Arbeitsphase | Grün |
| Pausenphase | Rot |
| Timer-Pause (manuell) | Blau, Doppelpunkt blinkt |
| Fertig / Abgeschlossen | Lila |
| Spielstand (Team 1) | Rot |
| Spielstand (Team 2) | Grün |

### Kein Signal

Empfängt der Slave länger als **3 Sekunden** kein Signal vom Master, bleibt die zuletzt empfangene Anzeige stehen. Das Gerät zeigt keine Fehlermeldung an.

---

## Energieverwaltung

### Master (M5Core2)

| Funktion | Einstellung |
|---|---|
| **Auto-Dimm** | Nach 60 Sek. Inaktivität → Helligkeit wird auf 5 % reduziert |
| **Auto-Off** | Nach 60 Min. im Hauptmenü → Gerät schaltet sich aus |
| **Akkuanzeige** | Oben rechts (Prozentwert) |
| **Ladestatus** | Oben rechts "LADEN" wenn per USB verbunden |

Ein Tippen auf den Touchscreen deaktiviert den Auto-Dimm-Modus sofort wieder.

### Slave 

Der Slave hat keine eigene Energieverwaltung. Er läuft, solange er mit Strom versorgt wird. Die WS2812B-LEDs können bei voller Helligkeit bis zu **~4 A** verbrauchen – auf ausreichende Stromversorgung achten.

---

## Technische Daten

### Master

| Parameter | Wert |
|---|---|
| Gerät | M5Stack Core2 |
| Prozessor | ESP32 (Dual Core, 240 MHz) |
| Display | 2" IPS Touchscreen, 320×240 px |
| Drahtlos | ESP-NOW (2,4 GHz) |
| Akku | 390 mAh (intern) |

### Slave

| Parameter | Wert |
|---|---|
| Gerät | ESP32C3 |
| Prozessor | ESP32-C3 (Single Core, 160 MHz) |
| Drahtlos | ESP-NOW (2,4 GHz) |
| LEDs | 86× WS2812B RGB (GPIO 2) |
| Stromversorgung | 5 V via Netzteil Achtung, die USB-C Buchse ist nur zur Programmierung |

### Verbindung

| Parameter | Wert |
|---|---|
| Protokoll | ESP-NOW |
| Reichweite (innen) | ~40 m |
| Reichweite (außen) | ~100 m |
| Latenz | < 5 ms |
| Kein Router nötig | Ja (direkte Peer-to-Peer-Verbindung) |
