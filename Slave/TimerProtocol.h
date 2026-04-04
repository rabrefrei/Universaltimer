/**
 * @file TimerProtocol.h
 * @brief Gemeinsame Protokoll-Definitionen für Master und Slave
 *
 * WICHTIG: Diese Datei muss in Master/ und Slave/ identisch sein!
 */
#pragma once
#include <stdint.h>

// --- Protokoll-States (Feld 'state' in TimerData) ---
constexpr uint8_t STATE_SETUP    = 0;  // Einstellung (vor dem Start)
constexpr uint8_t STATE_PREPARE  = 1;  // Vorbereitungs-Countdown
constexpr uint8_t STATE_WORK     = 2;  // Arbeitsphase / Stoppuhr läuft
constexpr uint8_t STATE_REST     = 3;  // Pausenphase
constexpr uint8_t STATE_FINISHED = 4;  // Timer abgelaufen
constexpr uint8_t STATE_PAUSED   = 5;  // Pausiert
constexpr uint8_t STATE_SCORE    = 6;  // Spielstand-Modus

// --- Setup-Submodes (codiert im 'round'-Feld wenn state == STATE_SETUP) ---
// Ermöglicht dem Slave, Rundenanzahl (Zahl) von Zeiten (MM:SS) zu unterscheiden.
constexpr uint8_t SETUP_SUBMODE_ROUNDS = 0;  // seconds = Rundenanzahl (1–99)
constexpr uint8_t SETUP_SUBMODE_WORK   = 1;  // seconds = Arbeitszeit in Sekunden
constexpr uint8_t SETUP_SUBMODE_PAUSE  = 2;  // seconds = Pausenzeit in Sekunden

// --- Gemeinsame Datenstruktur ---
// __attribute__((packed)) verhindert Compiler-Padding (sonst 6 statt 4 Bytes)
struct __attribute__((packed)) TimerData {
    uint8_t  state;    // STATE_*-Konstante (siehe oben)
    uint16_t seconds;  // Zeitwert, Score (Format XXYY) oder Rundenanzahl
    uint8_t  round;    // Laufend: aktuelle Runde. STATE_SETUP: SETUP_SUBMODE_*
};
