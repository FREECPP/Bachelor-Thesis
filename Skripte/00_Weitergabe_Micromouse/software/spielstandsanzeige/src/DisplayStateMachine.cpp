#include "DisplayStateMachine.h"

#include <MD_MAX72xx.h>
#include <stdio.h>

// ----------------------------------------------------------------------------
// Muenz-Balken (senkrecht, ganz links auf der Matrix).
// Die Matrix ist 8 Pixel hoch; wir nutzen 6 Reihen (1..6) als 6 "LEDs" und
// fuellen von unten nach oben. COIN_BAR_COLUMN ist die Spalte ganz links -
// falls der Balken auf der falschen Seite erscheint, hier auf die letzte
// Spalte (8 * MAX_DEVICES - 1) aendern.
// ----------------------------------------------------------------------------
static const uint8_t COIN_BAR_LEDS = 6;
static const uint8_t COIN_BAR_TOP_ROW = 1;   // belegt Reihen 1..6 von 0..7
static const uint8_t COIN_BAR_COLUMN = 0;    // Spalte ganz links

// Blink-Intervall der pausierten Restzeit (an/aus je 500 ms ~ 1 Hz Blinken).
static const uint32_t PAUSE_BLINK_INTERVAL_MS = 500;

DisplayStateMachine::DisplayStateMachine(MD_Parola &display)
    : display(display)
{
}

void DisplayStateMachine::begin()
{
    transitionTo(DisplayState::IDLE);
}

void DisplayStateMachine::update()
{
    display.displayAnimate();

    // Der Münzbalken wird nach jeder Animation neu gezeichnet, damit ihn ein
    // Text-Refresh (der die linke Spalte mit leert) nicht dauerhaft loescht.
    if (currentState != DisplayState::IDLE)
        drawCoinBar();

    // Im Pausenzustand blinkt die eingefrorene Restzeit (~2 Hz).
    if (currentState == DisplayState::PAUSED)
    {
        const uint32_t now = millis();
        if ((int32_t)(now - pauseBlinkNextMs) >= 0)
        {
            pauseBlinkNextMs = now + PAUSE_BLINK_INTERVAL_MS;
            pauseBlinkOn = !pauseBlinkOn;
            if (pauseBlinkOn)
                renderCountdown();   // eingefrorene Zeit zeigen
            else
                showText("");        // ausblenden (Münzbalken bleibt)
        }
        return;
    }

    if (currentState != DisplayState::COUNTDOWN)
        return;

    const uint32_t now = millis();
    while (remainingSeconds > 0 &&
           (int32_t)(now - nextTickMs) >= 0)
    {
        remainingSeconds--;
        nextTickMs += 1000;
    }

    if (remainingSeconds == 0)
    {
        timeoutEventPending = true;
        transitionTo(DisplayState::FINISHED);
        return;
    }

    if (remainingSeconds != lastRenderedSeconds)
    {
        lastRenderedSeconds = remainingSeconds;
        renderCountdown();
    }
}

void DisplayStateMachine::startCountdown(uint16_t durationSeconds)
{
    remainingSeconds = durationSeconds;
    lastRenderedSeconds = UINT16_MAX;
    nextTickMs = millis() + 1000;
    timeoutEventPending = durationSeconds == 0;
    coinCollected = 0;  // frischer Münzbalken zu Spielbeginn
    transitionTo(
        durationSeconds == 0 ? DisplayState::FINISHED
                             : DisplayState::COUNTDOWN);
}

void DisplayStateMachine::pauseCountdown()
{
    // Nur aus einem laufenden Countdown heraus einfrieren. Restzeit bleibt in
    // remainingSeconds stehen; update() zaehlt nur im COUNTDOWN-Zustand.
    if (currentState != DisplayState::COUNTDOWN)
        return;
    pauseBlinkOn = true;
    pauseBlinkNextMs = millis() + PAUSE_BLINK_INTERVAL_MS;
    transitionTo(DisplayState::PAUSED);
}

void DisplayStateMachine::resumeCountdown()
{
    if (currentState != DisplayState::PAUSED)
        return;
    nextTickMs = millis() + 1000;
    lastRenderedSeconds = UINT16_MAX;
    transitionTo(DisplayState::COUNTDOWN);
}

void DisplayStateMachine::showWin()
{
    // Alle Muenzen gesammelt: Balken voll, Timer steht bis zum naechsten Start.
    if (coinTotal > 0)
        coinCollected = coinTotal;
    transitionTo(DisplayState::WIN);
}

void DisplayStateMachine::showIdle()
{
    coinCollected = 0;
    transitionTo(DisplayState::IDLE);
}

void DisplayStateMachine::setCoinScore(uint8_t collected, uint8_t total)
{
    coinTotal = total;
    coinCollected = collected > total ? total : collected;
    if (currentState != DisplayState::IDLE)
        drawCoinBar();
}

bool DisplayStateMachine::takeTimeoutEvent()
{
    const bool pending = timeoutEventPending;
    timeoutEventPending = false;
    return pending;
}

DisplayState DisplayStateMachine::state() const
{
    return currentState;
}

void DisplayStateMachine::transitionTo(DisplayState nextState)
{
    currentState = nextState;

    switch (currentState)
    {
        case DisplayState::IDLE:
            renderIdle();
            break;
        case DisplayState::COUNTDOWN:
            renderCountdown();
            break;
        case DisplayState::PAUSED:
            // Eingefrorene Restzeit weiter anzeigen (kein eigener Text).
            renderCountdown();
            break;
        case DisplayState::WIN:
            renderWin();
            break;
        case DisplayState::FINISHED:
            renderFinished();
            break;
    }
}

void DisplayStateMachine::renderIdle()
{
    showText("READY");
}

void DisplayStateMachine::renderCountdown()
{
    const uint16_t minutes = remainingSeconds / 60;
    const uint16_t seconds = remainingSeconds % 60;
    snprintf(
        displayBuffer,
        sizeof(displayBuffer),
        "%u:%02u",
        minutes,
        seconds);
    showText(displayBuffer);
}

void DisplayStateMachine::renderWin()
{
    showText("WIN");
}

void DisplayStateMachine::renderFinished()
{
    showText("0:00");
}

void DisplayStateMachine::showText(const char *text)
{
    display.displayClear();
    display.displayText(
        text,
        PA_CENTER,
        0,
        0,
        PA_PRINT,
        PA_NO_EFFECT);
    display.displayReset();
    // displayClear() loescht auch den Münzbalken -> direkt neu zeichnen.
    drawCoinBar();
}

void DisplayStateMachine::drawCoinBar()
{
    if (currentState == DisplayState::IDLE)
        return;

    MD_MAX72XX *mx = display.getGraphicObject();
    if (mx == nullptr)
        return;

    uint8_t litCount = 0;
    if (coinTotal > 0)
    {
        const uint16_t scaled =
            ((uint16_t)coinCollected * COIN_BAR_LEDS + coinTotal / 2) /
            coinTotal;
        litCount = scaled > COIN_BAR_LEDS ? COIN_BAR_LEDS : (uint8_t)scaled;
    }

    // Von unten (Reihe 6) nach oben fuellen.
    for (uint8_t i = 0; i < COIN_BAR_LEDS; i++)
    {
        const uint8_t row = COIN_BAR_TOP_ROW + i;
        const bool on = i >= (uint8_t)(COIN_BAR_LEDS - litCount);
        mx->setPoint(row, COIN_BAR_COLUMN, on);
    }
}
