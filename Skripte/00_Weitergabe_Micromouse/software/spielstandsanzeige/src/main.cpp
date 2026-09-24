#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <COMMUNICATION_CODES.h>

#include "DisplayStateMachine.h"

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4

// #define PIN_DIN   D7   // GPIO13
// #define PIN_CS    D8   // GPIO4
// #define PIN_CLK   D5   // GPIO14

#define PIN_DIN   D8
#define PIN_CS    D7
#define PIN_CLK   D6

MD_Parola display = MD_Parola(HARDWARE_TYPE, PIN_DIN, PIN_CLK, PIN_CS, MAX_DEVICES);

static const uint16_t GAME_DURATION_SECONDS = 2 * 60 + 30;
static const uint8_t SCORE_DISPLAY_ID = 254;
static uint8_t broadcastAddress[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

// Spiel-interne Control-Codes, die nicht in COMMUNICATION_CODES.h stehen
// (Quelle der Wahrheit: RP2040_Software/src/game/GhostInteraction.h).
static const uint8_t GC_ROUND_START = 21;  // neue Runde laeuft -> Timer weiter
static const uint8_t GC_GAME_OVER   = 22;  // Spielende -> zurueck auf READY

DisplayStateMachine displayStateMachine(display);

// Von der ESP-NOW-Empfangs-Callback gesetzte Flags; in processPendingEvents()
// im normalen Loop-Kontext abgearbeitet.
volatile bool startGameRequested = false;
volatile bool pauseRequested = false;
volatile bool resumeRequested = false;
volatile bool winRequested = false;
volatile bool gameOverRequested = false;
volatile bool coinUpdateRequested = false;
volatile uint8_t coinCollectedRx = 0;
volatile uint8_t coinTotalRx = 0;

static void sendTimerTimeout()
{
    const uint8_t payload[] = {
        OPC_GAME_CONTROL,
        SCORE_DISPLAY_ID,
        GS_TIMER_TIMEOUT
    };

    Serial.println("Timer abgelaufen: GS_TIMER_TIMEOUT wird gesendet");
    for (uint8_t i = 0; i < 3; i++)
    {
        esp_now_send(
            broadcastAddress,
            const_cast<uint8_t *>(payload),
            sizeof(payload));
        delay(15);
    }
}

void onReceive(uint8_t *macAddr, uint8_t *data, uint8_t len)
{
    (void)macAddr;

    if (len < 3 || data[0] != OPC_GAME_CONTROL)
        return;

    // data[1] = TargetId (Broadcast 255), data[2] = Game-Control-Code.
    switch (data[2])
    {
        case GC_START_GAME:
            startGameRequested = true;
            break;
        case GC_CATCHED_PACMAN:
            pauseRequested = true;
            break;
        case GC_ROUND_START:
            resumeRequested = true;
            break;
        case GC_PACMAN_WINS:
            winRequested = true;
            break;
        case GC_GAME_OVER:
        case GC_STOP_GAME:
            gameOverRequested = true;
            break;
        case GC_CURRENT_SCORE:
            if (len >= 5)
            {
                coinCollectedRx = data[3];
                coinTotalRx = data[4];
                coinUpdateRequested = true;
            }
            break;
        default:
            break;
    }
}

static void processPendingEvents()
{
    noInterrupts();
    const bool shouldStartGame = startGameRequested;
    const bool shouldPause = pauseRequested;
    const bool shouldResume = resumeRequested;
    const bool shouldWin = winRequested;
    const bool shouldGameOver = gameOverRequested;
    const bool shouldUpdateCoins = coinUpdateRequested;
    const uint8_t coinsCollected = coinCollectedRx;
    const uint8_t coinsTotal = coinTotalRx;
    startGameRequested = false;
    pauseRequested = false;
    resumeRequested = false;
    winRequested = false;
    gameOverRequested = false;
    coinUpdateRequested = false;
    interrupts();

    // Münzstand zuerst, damit der Balken vor evtl. Zustandswechseln stimmt.
    if (shouldUpdateCoins)
        displayStateMachine.setCoinScore(coinsCollected, coinsTotal);

    if (shouldStartGame &&
        displayStateMachine.state() != DisplayState::COUNTDOWN)
    {
        Serial.println("GC_START_GAME empfangen: Countdown startet");
        displayStateMachine.startCountdown(GAME_DURATION_SECONDS);
    }

    if (shouldPause)
    {
        Serial.println("GC_CATCHED_PACMAN: Timer pausiert");
        displayStateMachine.pauseCountdown();
    }

    if (shouldResume)
    {
        Serial.println("GC_ROUND_START: Timer laeuft weiter");
        displayStateMachine.resumeCountdown();
    }

    if (shouldWin)
    {
        Serial.println("GC_PACMAN_WINS: WIN, Timer steht");
        displayStateMachine.showWin();
    }

    if (shouldGameOver)
    {
        Serial.println("Spielende: zurueck auf READY");
        displayStateMachine.showIdle();
    }
}

void setup()
{
    Serial.begin(115200);
    display.begin();
    display.setIntensity(5);
    display.displayClear();
    displayStateMachine.begin();
    Serial.println("Display bereit.");

    // ESP Now stuff
    WiFi.mode(WIFI_STA);
    Serial.println("Meine MAC: " + WiFi.macAddress());

    if (esp_now_init() != 0) {
        Serial.println("ESP-NOW Init fehlgeschlagen");
        return;
    }

    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    if (esp_now_add_peer(
            broadcastAddress,
            ESP_NOW_ROLE_COMBO,
            0,
            nullptr,
            0) != 0)
    {
        Serial.println("ESP-NOW Broadcast-Peer konnte nicht angelegt werden");
    }
    esp_now_register_recv_cb(onReceive);

}

void loop()
{
    processPendingEvents();
    displayStateMachine.update();
    if (displayStateMachine.takeTimeoutEvent())
        sendTimerTimeout();
    yield();
}
