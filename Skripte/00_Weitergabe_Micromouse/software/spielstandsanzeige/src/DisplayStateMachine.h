#pragma once

#include <Arduino.h>
#include <MD_Parola.h>

enum class DisplayState : uint8_t
{
    IDLE,
    COUNTDOWN,
    PAUSED,
    WIN,
    FINISHED,
};

class DisplayStateMachine
{
public:
    explicit DisplayStateMachine(MD_Parola &display);

    void begin();
    void update();

    // Spielablauf-Steuerung (wird aus den empfangenen Funk-Events gespeist).
    void startCountdown(uint16_t durationSeconds);
    void pauseCountdown();   // Pacman gefangen -> Restzeit einfrieren
    void resumeCountdown();  // neue Runde laeuft -> weiterzaehlen
    void showWin();          // alle Muenzen gesammelt -> "WIN", Timer steht
    void showIdle();         // Spielende/Reset -> zurueck auf READY

    // Muenzstand fuer den senkrechten LED-Balken (links).
    void setCoinScore(uint8_t collected, uint8_t total);

    bool takeTimeoutEvent();
    DisplayState state() const;

private:
    void transitionTo(DisplayState nextState);
    void renderIdle();
    void renderCountdown();
    void renderWin();
    void renderFinished();
    void showText(const char *text);
    void drawCoinBar();

    MD_Parola &display;
    DisplayState currentState = DisplayState::IDLE;
    uint16_t remainingSeconds = 0;
    uint16_t lastRenderedSeconds = UINT16_MAX;
    uint32_t nextTickMs = 0;
    bool timeoutEventPending = false;
    uint8_t coinCollected = 0;
    uint8_t coinTotal = 0;
    uint32_t pauseBlinkNextMs = 0;  // Blink-Takt der eingefrorenen Zeit (PAUSED)
    bool pauseBlinkOn = true;
    char displayBuffer[8] = {};
};
