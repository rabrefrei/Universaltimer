/**
 * @file Universaltimer.ino
 * @brief Master mit FreeRTOS Tasking & UI-Verbesserungen (v3.2)
 */

#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>
#include "TimerProtocol.h"

// ==========================================
// CONFIG & STYLING
// ==========================================
constexpr uint8_t  PREP_TIME_S  = 5;
constexpr uint32_t AUTO_DIMM_MS = 60000;
constexpr uint32_t AUTO_OFF_MS  = 3600000;

const uint8_t slaveAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct Theme { uint32_t main, glow; };
const Theme THEME_SETUP  { 0x00BFFFU, 0x004455U };
const Theme THEME_WORK   { 0x00FF7FU, 0x005522U };
const Theme THEME_PAUSE  { 0xFF4500U, 0x551100U };
const Theme THEME_PREP   { 0xFFD700U, 0x554400U };
const Theme THEME_FINISH { 0xFFFFAAU, 0x554400U }; // Helles Gold für Abschluss

TimerData dataToSend;
M5Canvas canvas(&M5.Display);

enum class AppMode { MAIN_MENU, SPORT_TIMER, STOPWATCH, COUNTDOWN, SCOREBOARD };
volatile AppMode currentAppMode = AppMode::MAIN_MENU;

enum class TimerState { SETUP_ROUNDS, SETUP_WORK, SETUP_PAUSE, PREPARE, RUN_WORK, RUN_PAUSE, FINISHED };
volatile TimerState currentState = TimerState::SETUP_ROUNDS;

// Modus-spezifische Variablen
volatile uint8_t  currentRounds = 3, roundsLeft = 0;
volatile uint16_t currentWorkTime = 60, currentPauseTime = 15;

// Stoppuhr
volatile uint32_t swElapsedMillis = 0;
volatile uint32_t swStartMillis = 0;

// Countdown
volatile uint16_t cdStartTime = 60;
volatile uint16_t cdTimeLeft = 60;

// Spielstand
volatile uint8_t scoreRed = 0, scoreGreen = 0;
volatile bool    scoreDirty = false; // Nur neu zeichnen wenn sich etwas geändert hat

volatile uint16_t timeLeftS = 0, totalInState = 0;
volatile uint32_t lastSecondMillis = 0, lastInteraction = 0;
volatile bool     isDimmed = false, isPaused = false;

volatile bool     needsMenuRedraw = false;
SemaphoreHandle_t sendMutex = nullptr;

struct Zone { int16_t x, y, w, h; bool contains(int16_t tx, int16_t ty) const { return tx>=x && tx<x+w && ty>=y && ty<y+h; } };
constexpr Zone zMenuSport{0,0,160,120}, zMenuStop{160,0,160,120}, zMenuCount{0,120,160,120}, zMenuScore{160,120,160,120};
constexpr Zone zScoreRedUp{0,0,160,80}, zScoreRedDown{0,160,160,80}, zScoreGreenUp{160,0,160,80}, zScoreGreenDown{160,160,160,80};
constexpr Zone zMode{0,160,80,80}, zMinus{80,160,80,80}, zPlus{160,160,80,80}, zStart{240,160,80,80}, zStop{0,160,320,80};

void sendTimerUpdate() {
    if (sendMutex == nullptr) return;
    if (xSemaphoreTake(sendMutex, pdMS_TO_TICKS(10)) != pdTRUE) return;

    // FINISHED hat Vorrang vor isPaused (verhindert State-5-Override beim Übergang)
    bool finished = (currentState == TimerState::FINISHED);

    if (isPaused && !finished && currentAppMode != AppMode::SCOREBOARD) {
        dataToSend.state = STATE_PAUSED;
    } else if (currentAppMode == AppMode::SPORT_TIMER) {
        if      (currentState < TimerState::PREPARE)        dataToSend.state = STATE_SETUP;
        else if (currentState == TimerState::PREPARE)       dataToSend.state = STATE_PREPARE;
        else if (currentState == TimerState::RUN_WORK)      dataToSend.state = STATE_WORK;
        else if (currentState == TimerState::RUN_PAUSE)     dataToSend.state = STATE_REST;
        else if (currentState == TimerState::FINISHED)      dataToSend.state = STATE_FINISHED;
    } else if (currentAppMode == AppMode::STOPWATCH) {
        dataToSend.state = isPaused ? STATE_PAUSED : STATE_WORK;
    } else if (currentAppMode == AppMode::COUNTDOWN) {
        if      (currentState < TimerState::PREPARE)        dataToSend.state = STATE_SETUP;
        else if (currentState == TimerState::FINISHED)      dataToSend.state = STATE_FINISHED;
        else                                                dataToSend.state = STATE_WORK;
    } else if (currentAppMode == AppMode::SCOREBOARD) {
        dataToSend.state = STATE_SCORE;
    }

    if (currentAppMode == AppMode::SPORT_TIMER) {
        if (currentState < TimerState::PREPARE) {
            if (currentState == TimerState::SETUP_ROUNDS) {
                dataToSend.seconds = (uint16_t)currentRounds;
                dataToSend.round   = SETUP_SUBMODE_ROUNDS;
            } else if (currentState == TimerState::SETUP_WORK) {
                dataToSend.seconds = currentWorkTime;
                dataToSend.round   = SETUP_SUBMODE_WORK;
            } else {
                dataToSend.seconds = currentPauseTime;
                dataToSend.round   = SETUP_SUBMODE_PAUSE;
            }
        } else {
            dataToSend.seconds = timeLeftS;
            dataToSend.round   = (roundsLeft == 0) ? currentRounds : currentRounds - roundsLeft + 1;
        }
    } else if (currentAppMode == AppMode::STOPWATCH) {
        dataToSend.seconds = swElapsedMillis / 1000;
        dataToSend.round   = 0;
    } else if (currentAppMode == AppMode::COUNTDOWN) {
        dataToSend.seconds = (currentState < TimerState::PREPARE) ? cdStartTime : cdTimeLeft;
        dataToSend.round   = 0;
    } else if (currentAppMode == AppMode::SCOREBOARD) {
        dataToSend.seconds = scoreRed * 100 + scoreGreen;
        dataToSend.round   = 0;
    }

    esp_now_send(slaveAddress, (uint8_t*)&dataToSend, sizeof(dataToSend));
    xSemaphoreGive(sendMutex);
}

void drawIconPlus(int16_t x, int16_t y, uint32_t color)  { M5.Display.fillRect(x-15, y-2, 30, 4, color); M5.Display.fillRect(x-2, y-15, 4, 30, color); }
void drawIconMinus(int16_t x, int16_t y, uint32_t color) { M5.Display.fillRect(x-15, y-2, 30, 4, color); }
void drawIconMode(int16_t x, int16_t y, uint32_t color)  { for(int i=0; i<3; i++) M5.Display.fillCircle(x, y - 12 + i*12, 4, color); }
void drawIconPlay(int16_t x, int16_t y, uint32_t color)  { M5.Display.fillTriangle(x-10, y-15, x-10, y+15, x+15, y, color); }
void drawIconPause(int16_t x, int16_t y, uint32_t color) { M5.Display.fillRect(x-10, y-15, 8, 30, color); M5.Display.fillRect(x+2, y-15, 8, 30, color); }
void drawIconReset(int16_t x, int16_t y, uint32_t color) {
    M5.Display.drawArc(x, y, 13, 10, 45, 360, color);          // 315° Bogen
    M5.Display.fillTriangle(x+13, y+8, x+8, y-2, x+18, y-2, color); // Pfeilspitze
}

void drawMenu() {
    if (currentAppMode == AppMode::MAIN_MENU) return;
    M5.Display.startWrite();
    if (currentAppMode == AppMode::SCOREBOARD) {
        M5.Display.fillScreen(BLACK);
    } else {
        M5.Display.fillRect(0, 160, 320, 80, 0x050505U);
        if (currentAppMode == AppMode::SPORT_TIMER) {
            if (currentState < TimerState::PREPARE) {
                drawIconMode(40, 190, WHITE);
                drawIconMinus(120, 190, WHITE);
                drawIconPlus(200, 190, WHITE);
                drawIconPlay(280, 190, (currentState == TimerState::SETUP_ROUNDS) ? THEME_SETUP.main : WHITE);
            } else {
                if (isPaused || currentState == TimerState::FINISHED) drawIconPlay(160, 190, WHITE);
                else drawIconPause(160, 190, WHITE);
            }
        } else if (currentAppMode == AppMode::STOPWATCH) {
            if (!isPaused && swStartMillis != 0) {
                drawIconPause(160, 190, WHITE);
            } else {
                drawIconPlay(160, 190, WHITE);
                if (isPaused && swElapsedMillis > 0)
                    drawIconReset(40, 190, WHITE); // Reset-Icon links, nur wenn Zeit vorhanden
            }
        } else if (currentAppMode == AppMode::COUNTDOWN) {
            if (currentState < TimerState::PREPARE) {
                drawIconMinus(106, 190, WHITE);
                drawIconPlus(213, 190, WHITE);
                drawIconPlay(280, 190, WHITE);
            } else {
                if (isPaused || currentState == TimerState::FINISHED) drawIconPlay(160, 190, WHITE);
                else drawIconPause(160, 190, WHITE);
            }
        }
    }
    M5.Display.endWrite();
}

void drawMainMenu() {
    M5.Display.startWrite();
    M5.Display.fillScreen(BLACK);

    M5.Display.fillRect(0, 0, 159, 119, 0x000033U);
    M5.Display.drawRect(0, 0, 159, 119, 0x0088FFU);
    M5.Display.setTextColor(0x0088FFU);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString("SPORT", 80, 80);
    M5.Display.fillCircle(80, 40, 15, 0x0088FFU);
    M5.Display.fillTriangle(76, 32, 76, 48, 88, 40, 0x000033U);

    M5.Display.fillRect(161, 0, 159, 119, 0x003300U);
    M5.Display.drawRect(161, 0, 159, 119, 0x00FF00U);
    M5.Display.setTextColor(0x00FF00U);
    M5.Display.drawString("STOPPUHR", 240, 80);
    M5.Display.drawCircle(240, 40, 15, 0x00FF00U);
    M5.Display.fillRect(239, 30, 3, 12, 0x00FF00U);

    M5.Display.fillRect(0, 121, 159, 119, 0x330000U);
    M5.Display.drawRect(0, 121, 159, 119, 0xFF4400U);
    M5.Display.setTextColor(0xFF4400U);
    M5.Display.drawString("COUNTDOWN", 80, 201);
    M5.Display.drawTriangle(80-10, 161-10, 80+10, 161-10, 80, 161, 0xFF4400U);
    M5.Display.drawTriangle(80-10, 161+10, 80+10, 161+10, 80, 161, 0xFF4400U);

    M5.Display.fillRect(161, 121, 159, 119, 0x221100U);
    M5.Display.drawRect(161, 121, 159, 119, WHITE);
    M5.Display.setTextColor(WHITE);
    M5.Display.drawString("SPIELSTAND", 240, 201);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextColor(RED);
    M5.Display.drawString("X", 225, 161);
    M5.Display.setTextColor(GREEN);
    M5.Display.drawString("Y", 255, 161);
    M5.Display.setTextColor(WHITE);
    M5.Display.drawString(":", 240, 161);

    M5.Display.endWrite();
}

void handleMainMenuTouch(const m5::touch_detail_t& t) {
    if (!t.wasClicked()) return;

    M5.Speaker.tone(1000, 50);
    M5.Display.fillScreen(BLACK);

    if (zMenuSport.contains(t.x, t.y)) {
        currentAppMode = AppMode::SPORT_TIMER;
        currentState = TimerState::SETUP_ROUNDS;
    } else if (zMenuStop.contains(t.x, t.y)) {
        currentAppMode = AppMode::STOPWATCH;
        swElapsedMillis = 0; swStartMillis = 0; isPaused = true;
    } else if (zMenuCount.contains(t.x, t.y)) {
        currentAppMode = AppMode::COUNTDOWN;
        currentState = TimerState::SETUP_WORK; cdTimeLeft = cdStartTime; isPaused = true;
    } else if (zMenuScore.contains(t.x, t.y)) {
        currentAppMode = AppMode::SCOREBOARD;
        scoreRed = 0; scoreGreen = 0;
        scoreDirty = true; // Erstes Zeichnen anstoßen
    }
    drawMenu();
    sendTimerUpdate(); // Sofortiges Update beim Modus-Wechsel
}

void handleTouch() {
    auto t = M5.Touch.getDetail();
    if (t.isPressed()) lastInteraction = millis();
    if (isDimmed && t.wasPressed()) { M5.Display.setBrightness(200); isDimmed = false; return; }
    if (!isDimmed && (millis() - lastInteraction > AUTO_DIMM_MS) && (currentAppMode == AppMode::MAIN_MENU || (currentAppMode == AppMode::SPORT_TIMER && currentState < TimerState::PREPARE))) {
        M5.Display.setBrightness(5); isDimmed = true;
    }
    if (isDimmed) return;

    static uint32_t lastResetMillis = 0;
    if (millis() - lastResetMillis < 400) return;

    if (currentAppMode == AppMode::MAIN_MENU) {
        handleMainMenuTouch(t);
        return;
    }

    if (currentAppMode == AppMode::SCOREBOARD) {
        bool changed = false;
        if (t.wasClicked()) {  // wasClicked() feuert nur bei kurzem Tippen, nicht bei Long-Press
            if      (zScoreRedUp.contains(t.x, t.y))    { if (scoreRed < 99) scoreRed++;     changed = true; }
            else if (zScoreRedDown.contains(t.x, t.y))  { if (scoreRed > 0) scoreRed--;      changed = true; }
            else if (zScoreGreenUp.contains(t.x, t.y))  { if (scoreGreen < 99) scoreGreen++; changed = true; }
            else if (zScoreGreenDown.contains(t.x, t.y)){ if (scoreGreen > 0) scoreGreen--;  changed = true; }
        }
        if (changed) { M5.Speaker.tone(1000, 20); scoreDirty = true; sendTimerUpdate(); }
    }

    // "Back to Menu" via Long Press
    if (t.wasHold() && (t.y < 160 || currentAppMode == AppMode::SCOREBOARD)) {
        currentAppMode = AppMode::MAIN_MENU;
        lastResetMillis = millis();
        M5.Speaker.tone(400, 200);
        drawMainMenu();
        return;
    }

    static uint32_t lastRep = 0;
    bool rep = t.isHolding() && (millis() - lastRep > 100);
    if (rep) lastRep = millis();

    if (currentAppMode == AppMode::SPORT_TIMER) {
        if (currentState < TimerState::PREPARE) {
            bool changed = false;
            if (t.wasClicked() && zMode.contains(t.x, t.y)) {
                currentState = (TimerState)(((int)currentState + 1) % 3);
                M5.Speaker.tone(800, 40); drawMenu(); changed = true;
            } else if ((t.wasPressed() || rep) && zMinus.contains(t.x, t.y)) {
                if (currentState == TimerState::SETUP_ROUNDS && currentRounds > 1) currentRounds--;
                else if (currentState == TimerState::SETUP_WORK && currentWorkTime > 5) currentWorkTime -= 5;
                else if (currentState == TimerState::SETUP_PAUSE && currentPauseTime > 5) currentPauseTime -= 5;
                changed = true;
            } else if ((t.wasPressed() || rep) && zPlus.contains(t.x, t.y)) {
                if (currentState == TimerState::SETUP_ROUNDS && currentRounds < 99) currentRounds++;
                else if (currentState == TimerState::SETUP_WORK && currentWorkTime < 3600) currentWorkTime += 5;
                else if (currentState == TimerState::SETUP_PAUSE && currentPauseTime < 3600) currentPauseTime += 5;
                changed = true;
            } else if (t.wasClicked() && zStart.contains(t.x, t.y)) {
                currentState = TimerState::PREPARE; timeLeftS = PREP_TIME_S; totalInState = PREP_TIME_S;
                roundsLeft = currentRounds; lastSecondMillis = millis(); isPaused = false;
                M5.Speaker.tone(1000, 200); drawMenu(); changed = true;
            }
            if (changed) { if (t.wasPressed() || rep) M5.Speaker.tone(1000, 20); sendTimerUpdate(); }
        } else if (zStop.contains(t.x, t.y)) {
            if (t.wasClicked()) {
                if (currentState == TimerState::FINISHED) {
                    currentState = TimerState::PREPARE; timeLeftS = PREP_TIME_S; totalInState = PREP_TIME_S;
                    roundsLeft = currentRounds; lastSecondMillis = millis(); isPaused = false;
                    M5.Speaker.tone(1000, 200); drawMenu();
                } else {
                    isPaused = !isPaused;
                    if (!isPaused) lastSecondMillis = millis();
                    M5.Speaker.tone(isPaused ? 600 : 900, 100);
                    drawMenu();
                }
                sendTimerUpdate();
            } else if (t.wasHold()) {
                currentState = TimerState::SETUP_ROUNDS; isPaused = false;
                lastResetMillis = millis();
                M5.Speaker.tone(400, 500); drawMenu(); sendTimerUpdate();
            }
        }
    } else if (currentAppMode == AppMode::STOPWATCH) {
        if (t.wasClicked() && zMode.contains(t.x, t.y) && isPaused && swElapsedMillis > 0) {
            // Reset-Button (links): Stoppuhr auf Null
            swElapsedMillis = 0; swStartMillis = 0;
            M5.Speaker.tone(400, 300); drawMenu();
        } else if (zStop.contains(t.x, t.y)) {
            if (t.wasClicked()) {
                if (swStartMillis == 0) { swStartMillis = millis() - swElapsedMillis; isPaused = false; }
                else { isPaused = !isPaused; if (!isPaused) swStartMillis = millis() - swElapsedMillis; }
                M5.Speaker.tone(isPaused ? 600 : 900, 40);
                drawMenu();
            } else if (t.wasHold()) {
                swElapsedMillis = 0; swStartMillis = 0; isPaused = true;
                lastResetMillis = millis();
                M5.Speaker.tone(400, 500); drawMenu();
            }
        }
    } else if (currentAppMode == AppMode::COUNTDOWN) {
        if (currentState < TimerState::PREPARE) {
            bool changed = false;
            if ((t.wasPressed() || rep) && (zMinus.contains(t.x, t.y) || zMode.contains(t.x, t.y))) {
                if (cdStartTime > 10) cdStartTime -= 10;
                changed = true;
            } else if ((t.wasPressed() || rep) && zPlus.contains(t.x, t.y)) {
                if (cdStartTime < 36000) cdStartTime += 10;
                changed = true;
            } else if (t.wasClicked() && zStart.contains(t.x, t.y)) {
                currentState = TimerState::PREPARE; cdTimeLeft = cdStartTime;
                timeLeftS = cdStartTime; totalInState = cdStartTime;
                lastSecondMillis = millis(); isPaused = false;
                M5.Speaker.tone(1000, 200); drawMenu();
            }
            if (changed) { cdTimeLeft = cdStartTime; M5.Speaker.tone(1000, 20); }
        } else if (zStop.contains(t.x, t.y)) {
            if (t.wasClicked()) {
                isPaused = !isPaused; if (!isPaused) lastSecondMillis = millis();
                M5.Speaker.tone(isPaused ? 600 : 900, 100); drawMenu();
            } else if (t.wasHold()) {
                currentState = TimerState::SETUP_WORK; isPaused = true; cdTimeLeft = cdStartTime;
                lastResetMillis = millis();
                M5.Speaker.tone(400, 500); drawMenu();
            }
        }
    }
}

void render() {
    if (currentAppMode == AppMode::MAIN_MENU) return;

    // --- Spielstand: direkt auf Display, nur bei Änderung neu zeichnen ---
    if (currentAppMode == AppMode::SCOREBOARD) {
        if (!scoreDirty) return;
        scoreDirty = false;

        M5.Display.startWrite();
        M5.Display.fillScreen(BLACK);

        // Pfeile oben (Touchzone y=0..80)
        M5.Display.fillTriangle( 80, 15,  50, 65, 110, 65, RED);
        M5.Display.fillTriangle(240, 15, 210, 65, 270, 65, GREEN);

        // Scores zentriert (y=120)
        M5.Display.setFont(&fonts::Font8);
        M5.Display.setTextDatum(middle_center);
        M5.Display.setTextColor(RED);
        M5.Display.drawNumber(scoreRed, 80, 120);
        M5.Display.setTextColor(WHITE);
        M5.Display.drawString(":", 160, 120);
        M5.Display.setTextColor(GREEN);
        M5.Display.drawNumber(scoreGreen, 240, 120);

        // Pfeile unten (Touchzone y=160..240)
        M5.Display.fillTriangle( 80, 225,  50, 175, 110, 175, RED);
        M5.Display.fillTriangle(240, 225, 210, 175, 270, 175, GREEN);

        // Menü-Hinweis
        M5.Display.setFont(&fonts::Font2);
        M5.Display.setTextColor(0x2266AAU);
        M5.Display.setTextDatum(top_left);
        M5.Display.drawString("< MENU", 3, 2);

        M5.Display.endWrite();
        return;
    }

    canvas.fillSprite(BLACK);
    Theme th = THEME_SETUP;
    const char* label = "";
    bool setupMode = currentState < TimerState::PREPARE;

    if (currentAppMode == AppMode::SPORT_TIMER) {
        if (setupMode) {
            // Farben spiegeln die jeweilige Phase wider
            if      (currentState == TimerState::SETUP_ROUNDS) { label = "RUNDEN"; th = THEME_SETUP; }
            else if (currentState == TimerState::SETUP_WORK)   { label = "ARBEIT"; th = THEME_WORK;  }
            else                                                { label = "PAUSE";  th = THEME_PAUSE; }
        } else {
            if      (isPaused)                             { label = "PAUSIERT"; th = {0x666666U, 0x111111U}; }
            else if (currentState == TimerState::PREPARE)  { label = "BEREIT";   th = THEME_PREP;   }
            else if (currentState == TimerState::RUN_WORK) { label = "LOS!";     th = THEME_WORK;   }
            else if (currentState == TimerState::RUN_PAUSE){ label = "PAUSE";    th = THEME_PAUSE;  }
            else if (currentState == TimerState::FINISHED) { label = "FERTIG!";  th = THEME_FINISH; }
        }
    } else if (currentAppMode == AppMode::STOPWATCH) {
        label = isPaused ? "GESTOPPT" : "STOPPUHR";
        th = isPaused ? Theme{0x888888U, 0x222222U} : Theme{0x00FF00U, 0x004400U};
        setupMode = false;
    } else if (currentAppMode == AppMode::COUNTDOWN) {
        if (setupMode) { label = "COUNTDOWN"; th = THEME_SETUP; }
        else {
            if      (isPaused)                             { label = "PAUSIERT"; th = {0x666666U, 0x111111U}; }
            else if (currentState == TimerState::FINISHED) { label = "ZEIT UM!"; th = THEME_PAUSE; }
            else                                           { label = "COUNTDOWN"; th = Theme{0xFF4400U, 0x441100U}; }
        }
    }

    int cx = 160, cy = 80;

    if (!setupMode && currentAppMode != AppMode::STOPWATCH) {
        int r = 77;
        canvas.drawArc(cx, cy, r, r-3, 0, 360, 0x101010U); // Hintergrund-Ring

        if (currentState != TimerState::FINISHED) {
            // Fortschritts-Arc: beginnt bei 12 Uhr (270°), läuft im Uhrzeigersinn
            float vLeft  = (currentAppMode == AppMode::SPORT_TIMER) ? (float)timeLeftS  : (float)cdTimeLeft;
            float vTotal = (currentAppMode == AppMode::SPORT_TIMER) ? (float)totalInState : (float)cdStartTime;
            float preciseLeft = isPaused ? vLeft : (vLeft - ((millis() - lastSecondMillis) / 1000.0f));
            float progress = preciseLeft / vTotal;
            canvas.drawArc(cx, cy, r+2, r-5, 270, 270 + 360.0f * progress, th.glow);
            canvas.drawArc(cx, cy, r,   r-3, 270, 270 + 360.0f * progress, th.main);
        } else {
            // Voller Ring als Abschluss-Feedback
            canvas.drawArc(cx, cy, r+2, r-5, 0, 360, th.glow);
            canvas.drawArc(cx, cy, r,   r-3, 0, 360, th.main);
        }
    } else if (currentAppMode != AppMode::STOPWATCH) {
        canvas.setTextDatum(top_center);
        canvas.setFont(&fonts::Font4);
        canvas.setTextColor(0x444444U);
        canvas.drawString("EINSTELLUNGEN", 160, 5);
    }

    canvas.setTextDatum(middle_center);
    canvas.setTextColor(th.main);
    canvas.setFont(&fonts::Font4);
    canvas.drawString(label, cx, (currentAppMode == AppMode::STOPWATCH) ? 22 : (setupMode ? 60 : 45));

    canvas.setTextColor(WHITE);
    canvas.setFont(&fonts::Font7);
    int valueY = setupMode ? 105 : cy + 10;

    if (currentAppMode == AppMode::SPORT_TIMER) {
        uint16_t v = setupMode
            ? (currentState == TimerState::SETUP_ROUNDS ? (uint16_t)currentRounds
               : (currentState == TimerState::SETUP_WORK ? currentWorkTime : currentPauseTime))
            : timeLeftS;
        if (currentState == TimerState::SETUP_ROUNDS) {
            canvas.drawNumber(v, cx, valueY);
        } else {
            char buf[10];
            snprintf(buf, sizeof(buf), "%02d:%02d", v/60, v%60);
            canvas.drawString(buf, cx, valueY);
        }
    } else if (currentAppMode == AppMode::STOPWATCH) {
        uint32_t s  = swElapsedMillis / 1000;
        uint32_t cs = (swElapsedMillis % 1000) / 10;
        char buf[10], csBuf[8];
        snprintf(buf,   sizeof(buf),   "%02d:%02d", (uint16_t)(s / 60), (uint16_t)(s % 60));
        snprintf(csBuf, sizeof(csBuf), ".%02u", (uint8_t)cs);
        canvas.setFont(&fonts::Font8);
        canvas.drawString(buf, cx, cy + 15);    // Haupt-Zeit, etwas nach unten gerückt
        canvas.setFont(&fonts::Font4);
        canvas.setTextColor(0x888888U);
        canvas.drawString(csBuf, cx, cy + 67);  // Hundertstel darunter
    } else if (currentAppMode == AppMode::COUNTDOWN) {
        uint16_t v = setupMode ? cdStartTime : cdTimeLeft;
        char buf[10];
        snprintf(buf, sizeof(buf), "%02d:%02d", v/60, v%60);
        canvas.drawString(buf, cx, valueY);
    }

    // Rundenanzeige im Canvas (Sport-Timer, laufend, nicht FINISHED)
    if (currentAppMode == AppMode::SPORT_TIMER && !setupMode && currentState != TimerState::FINISHED) {
        canvas.setFont(&fonts::Font2);
        canvas.setTextColor(0x666666U);
        canvas.setTextDatum(bottom_center);
        char rndBuf[20];
        snprintf(rndBuf, sizeof(rndBuf), "RUNDE %d / %d", currentRounds - roundsLeft + 1, currentRounds);
        canvas.drawString(rndBuf, cx, 156);
    }

    // Dezenter Menü-Hinweis (Long Press = zurück)
    canvas.setFont(&fonts::Font2);
    canvas.setTextColor(0x2266AAU);
    canvas.setTextDatum(top_left);
    canvas.drawString("< MENU", 3, 2);

    // Akkuanzeige
    int batLevel = M5.Power.getBatteryLevel();
    bool isCharging = M5.Power.isCharging();
    canvas.setTextDatum(top_right);
    canvas.setFont(&fonts::Font0);
    if (isCharging) {
        canvas.setTextColor(0x00FF00U);
        canvas.drawString("LADEN", 317, 3);
    } else {
        canvas.setTextColor(batLevel <= 20 ? 0xFF0000U : 0x888888U);
        char batBuf[8];
        snprintf(batBuf, sizeof(batBuf), "%d%%", batLevel);
        canvas.drawString(batBuf, 317, 3);
    }

    canvas.pushSprite(0, 0);
}

void timerTask(void* pvParameters) {
    while (true) {
        if (currentAppMode == AppMode::SPORT_TIMER) {
            if (currentState >= TimerState::PREPARE && currentState != TimerState::FINISHED && !isPaused) {
                if (millis() - lastSecondMillis >= 1000) {
                    lastSecondMillis += 1000;
                    if (timeLeftS > 0) {
                        timeLeftS--;
                        if (timeLeftS <= 3 && timeLeftS > 0) M5.Speaker.tone(1500, 150);
                        sendTimerUpdate();
                    }
                    if (timeLeftS == 0) {
                        if (currentState == TimerState::PREPARE || currentState == TimerState::RUN_PAUSE) {
                            currentState = TimerState::RUN_WORK; timeLeftS = currentWorkTime; totalInState = currentWorkTime;
                            M5.Speaker.tone(800, 800);
                        } else {
                            if (--roundsLeft > 0) {
                                currentState = TimerState::RUN_PAUSE; timeLeftS = currentPauseTime; totalInState = currentPauseTime;
                                M5.Speaker.tone(1200, 500);
                            } else {
                                currentState = TimerState::FINISHED;
                                M5.Speaker.tone(1000, 1500);
                                needsMenuRedraw = true;
                            }
                        }
                        sendTimerUpdate();
                    }
                }
            }
        } else if (currentAppMode == AppMode::STOPWATCH) {
            if (!isPaused && swStartMillis != 0) {
                uint32_t oldS = swElapsedMillis / 1000;
                swElapsedMillis = millis() - swStartMillis;
                if (swElapsedMillis / 1000 != oldS) sendTimerUpdate();
            }
        } else if (currentAppMode == AppMode::COUNTDOWN) {
            if (currentState >= TimerState::PREPARE && currentState != TimerState::FINISHED && !isPaused) {
                if (millis() - lastSecondMillis >= 1000) {
                    lastSecondMillis += 1000;
                    if (cdTimeLeft > 0) {
                        cdTimeLeft--;
                        if (cdTimeLeft <= 3 && cdTimeLeft > 0) M5.Speaker.tone(1500, 150);
                        sendTimerUpdate();
                    }
                    if (cdTimeLeft == 0) {
                        currentState = TimerState::FINISHED;
                        M5.Speaker.tone(1000, 1500);
                        sendTimerUpdate();
                        needsMenuRedraw = true;
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));

        // Periodisches Update (alle 1s) im Setup/Pause, damit der Slave aktiv bleibt
        static uint32_t lastKeepAlive = 0;
        if (millis() - lastKeepAlive > 1000) {
            lastKeepAlive = millis();
            bool inSetup = (currentAppMode == AppMode::SPORT_TIMER || currentAppMode == AppMode::COUNTDOWN) && (currentState < TimerState::PREPARE);
            if (inSetup || isPaused || currentAppMode == AppMode::SCOREBOARD) {
                sendTimerUpdate();
            }
        }
    }
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setBrightness(200);
    canvas.createSprite(320, 160);

    WiFi.mode(WIFI_STA);
    if (esp_now_init() == ESP_OK) {
        esp_now_peer_info_t peerInfo;
        memset(&peerInfo, 0, sizeof(peerInfo));
        memcpy(peerInfo.peer_addr, slaveAddress, 6);
        peerInfo.channel = 1;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
    }

    sendMutex = xSemaphoreCreateMutex();

    drawMainMenu();
    lastInteraction = millis();

    xTaskCreatePinnedToCore(timerTask, "TimerTask", 4096, NULL, 1, NULL, 1);
}

void loop() {
    M5.update();
    handleTouch();
    if (needsMenuRedraw) { drawMenu(); needsMenuRedraw = false; }
    if (currentAppMode == AppMode::MAIN_MENU && (millis() - lastInteraction > AUTO_OFF_MS)) M5.Power.powerOff();
    render();
}
