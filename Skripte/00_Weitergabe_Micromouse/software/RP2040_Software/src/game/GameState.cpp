//
// Created by robo on 05.06.26.
//

#include "GameState.h"

#include "GamePosition.h"
#include "GhostInteraction.h"
#include "GhostGame.h"
#include "PacmanGame.h"
#include "TestDrive.h"

#include "../Globals.h"
#include "../ButtonAdvanced.h"
#include "../UART_L3.h"
#include "../MicromouseMapping.h"
#include "../MicromouseMappingState.h"
#include "../MicromouseSolving.h"
#include "../MicromouseLeds.h"
#include "../MicromouseGraphics.h"
#include "../MicromouseMenu.h"   // menuGetUseCurves()
#include <COMMUNICATION_CODES.h>
#include <Pololu3piPlus2040Buzzer.h>

GAME_STATE currentGameState = GAME_STATE::GS_IDLE_AT_HOME;

using namespace Pololu3piPlus2040;

extern ButtonAdvanced buttonAdvanced;
extern UART_L3 uartL3;

static GAME_ROLES activeRole = GAME_ROLES::ROLE_PACMAN;
static bool gameInitialized = false;
static uint16_t gameCtrlSw = 0;
static bool capturePending = false;
static bool pacmanWon = false;
static bool winRetreatStartPending = false;
static bool timerTimeoutPending = false;
static bool timerTimeoutReturnActive = false;
static uint8_t captureGhostId = 0;
static uint8_t captureX = 0;
static uint8_t captureY = 0;
static uint32_t captureMotionDoneMs = 0;
static bool homingStartReceived = false;
static Buzzer gameBuzzer;

static const uint8_t GAME_BROADCAST_ID = 255;
static const uint32_t CAPTURE_HOLD_MS = 2000;
static const uint32_t RETURN_MESSAGE_INTERVAL_MS = 500;
static const uint32_t RETURNING_MESSAGE_INTERVAL_MS = 200;
static const uint32_t RETURNING_SILENCE_DONE_MS = 500;
static const uint32_t ACTIVE_GHOST_TIMEOUT_MS = 3000;

struct SeenGhost
{
    bool known;
    uint8_t id;
    uint32_t seenMs;
};

static SeenGhost seenGhosts[4];
static uint8_t returnQueue[4];
static uint8_t returnQueueLength = 0;
static uint8_t returnQueueIndex = 0;
static uint8_t returnSequence = 0;
static uint8_t grantedRobotId = 0;
static bool returnGrantActive = false;
static bool ownReturnStarted = false;
static bool ownReturnStartPending = false;
static bool ownAtHome = false;
static uint32_t lastReturnMessageMs = 0;
static uint32_t lastHomingStartMessageMs = 0;
static uint32_t lastReturningMessageMs = 0;
static uint32_t lastReturningTxMs = 0;
static bool pacmanManualResumePending = false;

static void leaveCaptureSequence(GAME_STATE nextState);
static void drawPacmanLobby();
static void drawPacmanManualHomeConfirm();
static void manualHomeReset();
static void startGhostRoundFromIdle(const char *reason);
static bool pacmanCanResumeAfterManualHome(GAME_STATE previousState);
static void resumePacmanAfterManualHome();
static void startOwnReturnHome();
static void startSimultaneousReturnHome();
static void finishPacmanReturnIfReady();
static void startNextRound();
static void finishGame();

static void resetReturnCoordination()
{
    homingStartReceived = false;
    returnGrantActive = false;
    returnQueueLength = 0;
    returnQueueIndex = 0;
    grantedRobotId = 0;
    ownReturnStarted = false;
    ownReturnStartPending = false;
    ownAtHome = false;
    lastReturnMessageMs = 0;
    lastHomingStartMessageMs = 0;
    lastReturningMessageMs = 0;
    lastReturningTxMs = 0;
}

static void buildActiveGhostReturnQueue()
{
    const uint32_t now = millis();
    for (uint8_t i = 0; i < 4; i++) {
        const SeenGhost &ghost = seenGhosts[i];
        if (ghost.known &&
            (uint32_t)(now - ghost.seenMs) <= ACTIVE_GHOST_TIMEOUT_MS)
        {
            returnQueue[returnQueueLength++] = ghost.id;
        }
    }
}

static void drawPacmanWinScreen()
{
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawBox(0, 0, 128, 13);
    u8g2.setDrawColor(0);
    u8g2.drawStr(2, 10, "PACMAN GEWINNT");
    u8g2.setDrawColor(1);
    u8g2.drawStr(2, 31, "Geister homen...");
    u8g2.drawStr(2, 47, "Pacman folgt.");
    oledSendBuffer();
}

static void drawCaptureRetreatError(const char *reason)
{
    if (reason == nullptr || reason[0] == '\0')
        reason = "capture retreat";

    ledShowError();
    ledFlushToHardware();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawBox(0, 0, 128, 13);
    u8g2.setDrawColor(0);
    u8g2.drawStr(2, 10, "RUECKFAHRT FEHLER");
    u8g2.setDrawColor(1);
    u8g2.drawStr(2, 31, reason);
    u8g2.drawStr(2, 55, "Taste = Menue");
    oledSendBuffer();
}

static void drawTimerTimeoutScreen()
{
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawBox(0, 0, 128, 13);
    u8g2.setDrawColor(0);
    u8g2.drawStr(2, 10, "ZEIT ABGELAUFEN");
    u8g2.setDrawColor(1);
    u8g2.drawStr(2, 31, "Geister homen...");
    u8g2.drawStr(2, 47, "Pacman folgt.");
    oledSendBuffer();
}

static void drawPacmanManualHomeConfirm()
{
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawBox(0, 0, 128, 13);
    u8g2.setDrawColor(0);
    u8g2.drawStr(2, 10, "MANUELL ZURUECK");
    u8g2.setDrawColor(1);
    u8g2.drawStr(2, 29, "Roboter auf Start");
    u8g2.drawStr(2, 43, "setzen.");
    u8g2.drawStr(2, 60, "C = Spiel weiter");
    oledSendBuffer();
}

static LedColor gameRoleColor(GAME_ROLES role)
{
    switch (role) {
        case GAME_ROLES::ROLE_PACMAN: return {180, 140, 0};
        case GAME_ROLES::ROLE_RED:    return {51, 0, 0};
        case GAME_ROLES::ROLE_PINK:   return {51, 4, 29};
        case GAME_ROLES::ROLE_CYAN:   return {0, 20, 51};
        case GAME_ROLES::ROLE_BROWN:  return {51, 16, 0};
        default:                      return LED_OFF;
    }
}

static void playGameStartSound()
{
    gameBuzzer.play("!T220 o5 l16 c e g >c8");
}

static void gameCbButtonPressed(uint8_t buttonId)
{
    switch (buttonId) {
        case 0:
            g_abortRequested = true;
            break;
        case 1: g_btnBPressed = true; break;
        case 2:
            g_abortRequested = true;
            break;
        default: break;
    }
}

static void gameCbButtonReleased(uint8_t buttonId)
{
    (void)buttonId;
}

static void gameCbBtControllerStateChanged(uint8_t stateId)
{
    if (stateId == CONN_SUCCESS) {
        uartL3.subscribeBtController(CONTROLLER_SWITCHES, 0, true);
    }
}

static void gameCbSubscribedReceived(uint8_t length, uint8_t *data)
{
    if (length < 3 || data[0] != CONTROLLER_SWITCHES)
        return;

    gameCtrlSw = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
}

static void gameCbPosReceived(uint8_t senderId, uint8_t encodedX, uint8_t encodedY)
{
    const uint8_t x = gamePositionDecodeX(encodedX);
    const uint8_t y = gamePositionDecodeY(encodedY);
    const int heading = gamePositionDecodeHeading(encodedX, encodedY);
    const GAME_ROLES senderRole = getRoleFromId(senderId);
    if (senderRole >= GAME_ROLES::ROLE_RED &&
        senderRole <= GAME_ROLES::ROLE_BROWN)
    {
        const uint8_t slot =
            (uint8_t)senderRole - (uint8_t)GAME_ROLES::ROLE_RED;
        seenGhosts[slot] = {true, senderId, millis()};
    }

    if (senderRole == GAME_ROLES::ROLE_PACMAN)
        ghostGameObservePacman(senderId, x, y, heading);
    else if (activeRole == GAME_ROLES::ROLE_PACMAN)
        pacmanGameObserveGhost(senderId, x, y);
    else
        ghostGameObservePeer(senderId, x, y, heading);
}

static void gameCbControlReceived(uint8_t senderId, uint8_t length, uint8_t *data)
{
    if (length < 1 || data == nullptr)
        return;

    if (data[0] == GC_START_GAME) {
        if (activeRole == GAME_ROLES::ROLE_PACMAN ||
            getRoleFromId(senderId) != GAME_ROLES::ROLE_PACMAN ||
            currentGameState != GAME_STATE::GS_IDLE_AT_HOME)
        {
            return;
        }

        startGhostRoundFromIdle("broadcast");
        return;
    }

    if (data[0] == GS_TIMER_TIMEOUT) {
        if (timerTimeoutReturnActive || timerTimeoutPending)
            return;
        navigationCellRetreatPrepareAllowStationary();
        timerTimeoutPending = true;
        g_abortRequested = true;
        stopMotors();
        return;
    }

    if (data[0] == GC_PACMAN_WINS) {
        if (length < 2 ||
            activeRole == GAME_ROLES::ROLE_PACMAN ||
            getRoleFromId(senderId) != GAME_ROLES::ROLE_PACMAN ||
            currentGameState != GAME_STATE::GS_RUNNING)
        {
            return;
        }

        returnSequence = data[1];
        pacmanWon = true;
        resetReturnCoordination();
        homingStartReceived = true;
        navigationCellRetreatPrepareAllowStationary();
        g_abortRequested = true;
        stopMotors();
        winRetreatStartPending = true;
        currentGameState = GAME_STATE::GS_RETREATING_TO_CELL;
        drawPacmanWinScreen();
        return;
    }

    if (data[0] == GC_GHOST_RESERVE_CELL) {
        if (length >= 4) {
            const bool committed =
                length >= 5 && data[4] == GHOST_RESERVATION_COMMITTED;
            ghostGameObserveReservation(
                senderId, data[1], data[2], data[3], committed);
        }
        return;
    }

    if (data[0] == GC_GHOST_RELEASE_CELL) {
        if (length >= 2)
            ghostGameObserveReservationRelease(senderId, data[1]);
        return;
    }

    if (data[0] == GC_GHOST_REPLAN) {
        if (length >= 3)
            ghostGameObserveReplan(senderId, data[1], data[2]);
        return;
    }

    if (data[0] == GC_HOMING_START) {
        if (length < 2 || activeRole == GAME_ROLES::ROLE_PACMAN ||
            getRoleFromId(senderId) != GAME_ROLES::ROLE_PACMAN ||
            currentGameState == GAME_STATE::GS_RUNNING ||
            currentGameState == GAME_STATE::GS_IDLE_AT_HOME)
        {
            return;
        }

        returnSequence = data[1];
        homingStartReceived = true;
        returnGrantActive = false;
        if (!ownReturnStarted &&
            currentGameState != GAME_STATE::GS_CATCHT &&
            currentGameState != GAME_STATE::GS_RETREATING_TO_CELL)
        {
            startOwnReturnHome();
        }
        return;
    }

    if (data[0] == GC_RETURNING) {
        if (length < 2 ||
            activeRole != GAME_ROLES::ROLE_PACMAN ||
            data[1] != returnSequence ||
            getRoleFromId(senderId) == GAME_ROLES::ROLE_PACMAN)
        {
            return;
        }

        lastReturningMessageMs = millis();
        return;
    }

    if (data[0] == GC_RETURN_GRANT) {
        if (length < 3 || data[2] != (uint8_t)robotId ||
            activeRole == GAME_ROLES::ROLE_PACMAN ||
            !homingStartReceived || data[1] != returnSequence)
        {
            return;
        }

        const bool duplicateGrant =
            returnSequence == data[1] &&
            grantedRobotId == data[2] &&
            (ownReturnStartPending || ownReturnStarted || ownAtHome);
        if (duplicateGrant)
            return;

        returnSequence = data[1];
        grantedRobotId = data[2];
        returnGrantActive = true;
        ownAtHome = false;
        ownReturnStarted = true;
        ownReturnStartPending = true;
        if (currentGameState != GAME_STATE::GS_CATCHT &&
            currentGameState != GAME_STATE::GS_RETREATING_TO_CELL)
            currentGameState = GAME_STATE::GS_RETURNING_TO_HOME;
        return;
    }

    if (data[0] == GC_AT_HOME) {
        if (length < 3 || activeRole != GAME_ROLES::ROLE_PACMAN ||
            data[1] != returnSequence || !returnGrantActive ||
            data[2] != grantedRobotId || senderId != grantedRobotId)
        {
            return;
        }

        returnGrantActive = false;
        returnQueueIndex++;
        lastReturnMessageMs = 0;
        return;
    }

    if (data[0] == GC_ROUND_START) {
        if (length < 2 || activeRole == GAME_ROLES::ROLE_PACMAN)
            return;

        returnSequence = data[1];
        returnGrantActive = false;
        ownReturnStarted = false;
        ownReturnStartPending = false;
        ownAtHome = false;
        homingStartReceived = false;
        ghostGameStartNextRound();
        return;
    }

    if (data[0] == GC_GAME_OVER) {
        stopMotors();
        currentGameState = GAME_STATE::GS_IDLE_AT_HOME;
        rpState = RP_STATE_MENU;
        return;
    }

    if (data[0] != GC_CATCHED_PACMAN)
        return;
    if (currentGameState != GAME_STATE::GS_RUNNING || capturePending)
        return;
    if (length >= 4) {
        captureGhostId = data[1];
        captureX = data[2];
        captureY = data[3];
    } else {
        if (getRoleFromId(senderId) == GAME_ROLES::ROLE_PACMAN)
            return;
        captureGhostId = senderId;
        if (length >= 3) {
            captureX = data[1];
            captureY = data[2];
        }
    }
    navigationCellRetreatPrepareAllowStationary();
    capturePending = true;
    g_abortRequested = true;
    stopMotors();
}

static void gameBackgroundUpdate()
{
    uartL3.update();
    if (activeRole == GAME_ROLES::ROLE_PACMAN)
        pacmanCheckBumpers();
    else
        ghostGameBackgroundUpdate();
}

static void drawCaptureScreen()
{
    const bool isCaptor = (uint8_t)robotId == captureGhostId;

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawBox(0, 0, 128, 13);
    u8g2.setDrawColor(0);
    u8g2.drawStr(2, 10, "PACMAN GEFANGEN");
    u8g2.setDrawColor(1);
    u8g2.drawStr(2, 28, isCaptor ? "Du hast gefangen!" : "Rueckfahrt folgt");

    char buf[28];
    snprintf(buf, sizeof(buf), "Ghost ID: %u", captureGhostId);
    u8g2.drawStr(2, 42, buf);
    snprintf(buf, sizeof(buf), "Position: %u,%u", captureX, captureY);
    u8g2.drawStr(2, 55, buf);
    oledSendBuffer();
}

static void enterCaptureSequence()
{
    stopMotors();
    solvePauseMotion();
    g_abortRequested = false;
    currentGameState = GAME_STATE::GS_CATCHT;
    captureMotionDoneMs = 0;
    resetReturnCoordination();

    if (activeRole == GAME_ROLES::ROLE_PACMAN) {
        returnSequence++;
        pacmanGameBeginCapture(captureGhostId);

        const uint32_t now = millis();
        const GAME_ROLES captorRole = getRoleFromId(captureGhostId);
        if (captorRole >= GAME_ROLES::ROLE_RED &&
            captorRole <= GAME_ROLES::ROLE_BROWN)
        {
            const uint8_t slot =
                (uint8_t)captorRole - (uint8_t)GAME_ROLES::ROLE_RED;
            seenGhosts[slot] = {true, captureGhostId, now};
        }

        for (uint8_t i = 0; i < 4; i++) {
            const SeenGhost &ghost = seenGhosts[i];
            if (ghost.known &&
                ((uint32_t)(now - ghost.seenMs) <= ACTIVE_GHOST_TIMEOUT_MS ||
                 ghost.id == captureGhostId))
            {
                returnQueue[returnQueueLength++] = ghost.id;
            }
        }
    } else {
        ghostGameBeginCapture();
    }

    ledSetBumperIndicators(false, false);
    ledSetAll(activeRole == GAME_ROLES::ROLE_PACMAN
        ? LED_RED
        : gameRoleColor(activeRole));
    ledFlushToHardware();
    drawCaptureScreen();

    if ((uint8_t)robotId == captureGhostId)
        gameBuzzer.play("!T180 o5 l8 c e g >c");
    else
        gameBuzzer.play("!T140 o4 l8 g e c");
}

static void sendReturnGrant(bool force)
{
    if (!returnGrantActive)
        return;

    const uint32_t now = millis();
    if (!force &&
        (uint32_t)(now - lastReturnMessageMs) < RETURN_MESSAGE_INTERVAL_MS)
    {
        return;
    }

    const uint8_t payload[] = {
        GC_RETURN_GRANT,
        returnSequence,
        grantedRobotId
    };
    uartL3.sendControlData(GAME_BROADCAST_ID, sizeof(payload), payload);
    lastReturnMessageMs = now;
}

static void sendHomingStart(bool force)
{
    const uint32_t now = millis();
    if (!force &&
        (uint32_t)(now - lastHomingStartMessageMs) <
            RETURN_MESSAGE_INTERVAL_MS)
    {
        return;
    }

    const uint8_t payload[] = {GC_HOMING_START, returnSequence};
    uartL3.sendControlData(GAME_BROADCAST_ID, sizeof(payload), payload);
    lastHomingStartMessageMs = now;
    homingStartReceived = true;
}

static void sendAtHome(bool force)
{
    const uint32_t now = millis();
    if (!force &&
        (uint32_t)(now - lastReturnMessageMs) < RETURN_MESSAGE_INTERVAL_MS)
    {
        return;
    }

    const uint8_t payload[] = {
        GC_AT_HOME,
        returnSequence,
        (uint8_t)robotId
    };
    uartL3.sendControlData(GAME_BROADCAST_ID, sizeof(payload), payload);
    lastReturnMessageMs = now;
}

static void sendReturning(bool force)
{
    const uint32_t now = millis();
    if (!force &&
        (uint32_t)(now - lastReturningTxMs) <
            RETURNING_MESSAGE_INTERVAL_MS)
    {
        return;
    }

    const uint8_t payload[] = {
        GC_RETURNING,
        returnSequence
    };
    uartL3.sendControlData(GAME_BROADCAST_ID, sizeof(payload), payload);
    lastReturningTxMs = now;
}

static void startOwnReturnHome()
{
    if (ownReturnStarted || ownAtHome)
        return;

    ownReturnStarted = true;
    ownAtHome = false;
    currentGameState = GAME_STATE::GS_RETURNING_TO_HOME;

    if (activeRole == GAME_ROLES::ROLE_PACMAN) {
        ownReturnStartPending = false;
        pacmanGameBeginReturnHome();
    } else {
        ownReturnStartPending = true;
        sendReturning(true);
    }
}

static void startSimultaneousReturnHome()
{
    sendHomingStart(true);
    lastReturningMessageMs = millis();
    startOwnReturnHome();
}

static void finishPacmanReturnIfReady()
{
    if (activeRole != GAME_ROLES::ROLE_PACMAN || !ownAtHome)
        return;
    if ((uint32_t)(millis() - lastReturningMessageMs) <=
        RETURNING_SILENCE_DONE_MS)
    {
        return;
    }

    if (timerTimeoutReturnActive) {
        pacmanGameResetAfterTimeout();
        timerTimeoutReturnActive = false;
        finishGame();
    } else if (pacmanWon) {
        finishGame();
    } else if (pacmanGameHasLives()) {
        startNextRound();
    } else {
        finishGame();
    }
}

static void startNextReturnParticipant()
{
    if (returnQueueIndex < returnQueueLength) {
        grantedRobotId = returnQueue[returnQueueIndex];
        returnGrantActive = true;
        lastReturnMessageMs = 0;
        sendReturnGrant(true);
        return;
    }

    if (!ownReturnStarted) {
        ownReturnStarted = true;
        currentGameState = GAME_STATE::GS_RETURNING_TO_HOME;
        pacmanGameBeginReturnHome();
    }
}

static void startNextRound()
{
    const uint8_t payload[] = {GC_ROUND_START, returnSequence};
    for (uint8_t i = 0; i < 3; i++) {
        uartL3.sendControlData(GAME_BROADCAST_ID, sizeof(payload), payload);
        uartL3.update();
        delay(15);
    }

    pacmanGameStartNextRound();
    leaveCaptureSequence(GAME_STATE::GS_RUNNING);
}

static void finishGame()
{
    const uint8_t payload[] = {GC_GAME_OVER, returnSequence};
    for (uint8_t i = 0; i < 3; i++) {
        uartL3.sendControlData(GAME_BROADCAST_ID, sizeof(payload), payload);
        uartL3.update();
        delay(15);
    }

    // Hinweis: Die Victory-Fanfare wird bereits beim Einsammeln der LETZTEN
    // Muenze gespielt (PacmanGame::finishDriveOneCell), nicht hier bei Ankunft
    // an der Startposition. Deshalb hier kein erneuter Win-Sound.

    // Zurueck in die LOBBY (Neustart-Schleife): erneut auf die Xbox-Taste
    // warten. Beendet wird die Schleife ueber eine physische Maus-Taste in der
    // Lobby. Die Geister haben oben GC_GAME_OVER bekommen (→ Menue) und werden
    // beim naechsten Tastendruck per GC_START_GAME re-armed.
    currentGameState = GAME_STATE::GS_LOBBY;
    pacmanManualResumePending = false;
    drawPacmanLobby();
}

// Gemeinsamer Capture-Cleanup. Ein spaeterer Lebens-/Runden-Neustart kann
// dieselbe Austrittsaktion mit GS_RUNNING verwenden, waehrend der aktuelle
// Ablauf danach ins Menue wechselt.
static void leaveCaptureSequence(GAME_STATE nextState)
{
    gameBuzzer.stopPlaying();
    capturePending = false;
    pacmanWon = false;
    winRetreatStartPending = false;
    timerTimeoutPending = false;
    timerTimeoutReturnActive = false;
    captureMotionDoneMs = 0;
    homingStartReceived = false;
    g_abortRequested = false;
    pacmanManualResumePending = false;
    currentGameState = nextState;
}

static bool pacmanCanResumeAfterManualHome(GAME_STATE previousState)
{
    if (activeRole != GAME_ROLES::ROLE_PACMAN)
        return false;
    if (pacmanWon || timerTimeoutPending || timerTimeoutReturnActive)
        return false;

    return previousState == GAME_STATE::GS_CATCHT ||
           previousState == GAME_STATE::GS_RETREATING_TO_CELL ||
           previousState == GAME_STATE::GS_WAITING_TO_RETURN ||
           previousState == GAME_STATE::GS_RETURNING_TO_HOME;
}

static void manualHomeReset()
{
    const GAME_STATE previousState = currentGameState;
    const bool pacmanResume =
        pacmanCanResumeAfterManualHome(previousState);

    gameBuzzer.stopPlaying();
    stopMotors();
    navigationDriveCellAbort();
    navigationCellRetreatAbort();
    solveAbortRun();
    capturePending = false;
    pacmanWon = false;
    winRetreatStartPending = false;
    timerTimeoutPending = false;
    timerTimeoutReturnActive = false;
    captureMotionDoneMs = 0;
    g_abortRequested = false;
    g_btnAPressed = false;
    g_btnBPressed = false;
    g_btnCPressed = false;
    resetReturnCoordination();

    if (activeRole == GAME_ROLES::ROLE_PACMAN) {
        if (pacmanResume) {
            pacmanGameStartNextRound();
            pacmanManualResumePending = true;
            currentGameState = GAME_STATE::GS_MANUAL_HOME_CONFIRM;
            drawPacmanManualHomeConfirm();
            Serial.println("[Game] manual home reset: pacman resume pending");
        } else {
            pacmanGameManualHomeReset();
            pacmanManualResumePending = false;
            currentGameState = GAME_STATE::GS_LOBBY;
            drawPacmanLobby();
            Serial.println("[Game] manual home reset: pacman lobby");
        }
    } else {
        ghostGameManualHomeReset();
        pacmanManualResumePending = false;
        currentGameState = GAME_STATE::GS_IDLE_AT_HOME;
        Serial.println("[Game] manual home reset: ghost idle");
    }
}

static void resumePacmanAfterManualHome()
{
    if (!pacmanManualResumePending)
        return;

    pacmanManualResumePending = false;
    if (pacmanGameHasLives()) {
        startNextRound();
    } else {
        leaveCaptureSequence(GAME_STATE::GS_LOBBY);
        finishGame();
    }
    Serial.println("[Game] pacman manual home confirmed");
}

static void startGhostRoundFromIdle(const char *reason)
{
    resetReturnCoordination();
    capturePending = false;
    pacmanWon = false;
    winRetreatStartPending = false;
    timerTimeoutPending = false;
    timerTimeoutReturnActive = false;
    captureMotionDoneMs = 0;
    pacmanManualResumePending = false;
    g_abortRequested = false;
    g_btnAPressed = false;
    g_btnBPressed = false;
    g_btnCPressed = false;
    ghostGameStartNextRound();
    Serial.print("[Game] ghost start from idle: ");
    Serial.println(reason);
}

static bool startPacmanGame()
{
    if (!mapReady) {
        Serial.println("Game start failed: no map ready");
        return false;
    }

    // Fahrmodus aus der Menue-Einstellung "Kurvenfahrt" (Kurve/Punkt).
    // Steuert sowohl den solvingLoop-Pfad (Heim-/Restart-Fahrten) als auch
    // das In-Game-Abbiegen der Pacman-Maus.
    const bool useCurves = menuGetUseCurves();
    setSolveSmoothTurns(useCurves);
    pacmanSetUseCurves(useCurves);

    buildGraphFromMaze();
    calibrateLineSensors();
    ledSetAll(gameRoleColor(activeRole));
    ledFlushToHardware();
    playGameStartSound();

    if (!solvingResetToStart(startPosX, startPosY, PAC_START_HEADING)) {
        Serial.println("Game start failed: solver reset failed");
        return false;
    }

    pacmanGameInit();
    currentGameState = GAME_STATE::GS_RUNNING;
    return true;
}

// Lobby-Anzeige der Pacman-Maus: vorfuehrsicher auf den Tastendruck warten.
static void drawPacmanLobby()
{
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawBox(0, 0, 128, 13);
    u8g2.setDrawColor(0);
    u8g2.drawStr(2, 10, "PAC-MAN");
    u8g2.setDrawColor(1);
    const char *l1 = "Xbox-Taste druecken";
    u8g2.drawStr((128 - u8g2.getStrWidth(l1)) / 2, 32, l1);
    const char *l2 = "zum Starten";
    u8g2.drawStr((128 - u8g2.getStrWidth(l2)) / 2, 45, l2);
    u8g2.drawLine(0, 52, 128, 52);
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(2, 62, "Maus-Taste = Menue");
    oledSendBuffer();
}

// Startet eine frische Pacman-Runde aus der Lobby: holt/re-armed die Geister
// per Broadcast und setzt den Spielzustand sauber zurueck. startPacmanGame()
// erledigt Kalibrierung + Init und setzt GS_RUNNING.
static void startPacmanRound()
{
    // Geister (im Menue) in den Spielmodus holen bzw. nach einer Runde re-armen.
    const uint8_t payload[] = {GC_START_GAME};
    for (uint8_t i = 0; i < 3; i++) {
        uartL3.sendControlData(GAME_BROADCAST_ID, sizeof(payload), payload);
        uartL3.update();
        delay(15);
    }

    // Sauberer Zustand fuer die neue Runde.
    pacmanWon = false;
    winRetreatStartPending = false;
    timerTimeoutPending = false;
    timerTimeoutReturnActive = false;
    capturePending = false;
    resetReturnCoordination();

    startPacmanGame();   // Kalibrierung + Init, setzt GS_RUNNING
}

void gameActive() {
    gameBuzzer.stopPlaying();
    navigationDriveCellAbort();

    buttonAdvanced.setCallbackPressed(gameCbButtonPressed);
    buttonAdvanced.setCallbackReleased(gameCbButtonReleased);
    buttonAdvanced.setCallbackHold(nullptr);

    uartL3.registerCbReceiveMazeData(nullptr);
    uartL3.registerCbReceiveBtControllerData(gameCbSubscribedReceived);
    uartL3.registerCbConnectBtControllerState(gameCbBtControllerStateChanged);
    uartL3.registerCbReceivePosData(gameCbPosReceived);
    uartL3.registerCbConnectNowState(nullptr);
    uartL3.registerCbError(nullptr);
    uartL3.registerCbReceiveControlData(gameCbControlReceived);
    uartL3.subscribeBtController(CONTROLLER_SWITCHES, 0, true);
    mappingRegisterBackgroundUpdate(gameBackgroundUpdate);

    g_btnAPressed = false;
    g_btnBPressed = false;
    g_btnCPressed = false;
    g_abortRequested = false;
    buttonAdvanced.clearEvents();
    gameCtrlSw = 0;
    capturePending = false;
    pacmanWon = false;
    winRetreatStartPending = false;
    timerTimeoutPending = false;
    timerTimeoutReturnActive = false;
    captureGhostId = 0;
    captureX = 0;
    captureY = 0;
    captureMotionDoneMs = 0;
    pacmanManualResumePending = false;
    resetReturnCoordination();
    for (uint8_t i = 0; i < 4; i++)
        seenGhosts[i] = {};
    activeRole = getRoleFromId((uint8_t)robotId);
    currentGameState = GAME_STATE::GS_IDLE_AT_HOME;

    switch (activeRole) {
        case GAME_ROLES::ROLE_PACMAN:
            // Pacman startet NICHT sofort, sondern in die Lobby: erst auf den
            // Xbox-Tastendruck warten (vorfuehrsicher). Der eigentliche
            // Spielstart (inkl. Geister-Broadcast) passiert in startPacmanRound.
            if (mapReady) {
                gameInitialized = true;
                currentGameState = GAME_STATE::GS_LOBBY;
                drawPacmanLobby();
            } else {
                gameInitialized = false;
            }
            break;
        case GAME_ROLES::ROLE_RED:
        case GAME_ROLES::ROLE_PINK:
        case GAME_ROLES::ROLE_CYAN:
        case GAME_ROLES::ROLE_BROWN:
            gameInitialized = ghostGameInit(activeRole);
            break;
        case GAME_ROLES::TESTDRIVE:
            gameInitialized = true;
            currentGameState = GAME_STATE::GS_RUNNING;
            break;
    }

    if (gameInitialized) {
        if (activeRole != GAME_ROLES::ROLE_PACMAN) {
            ledSetAll(gameRoleColor(activeRole));
            ledFlushToHardware();
            playGameStartSound();
        }
    }
}

void gameLeave()
{
    stopMotors();
    navigationCellRetreatAbort();
    solveAbortRun();
    leaveCaptureSequence(GAME_STATE::GS_IDLE_AT_HOME);
    ledShowIdle();
    buttonAdvanced.clearEvents();
    gameInitialized = false;
    ghostGameLeave();
}

void gameLoop() {
    if (!gameInitialized) {
        if (gameGetPressedA() || gameGetPressedC())
            rpState = RP_STATE_MENU;
        return;
    }

    uartL3.update();

    if (gameGetPressedB()) {
        manualHomeReset();
        return;
    }

    // LOBBY (nur Pacman): vorfuehrsicher auf den Xbox-Tastendruck warten.
    // Eine physische Maus-Taste beendet die Schleife (zurueck ins Menue) –
    // orientiert an der Menuefuehrung. Sonst keine Spiel-Logik ausfuehren.
    if (currentGameState == GAME_STATE::GS_LOBBY) {
        if (gameGetPressedA() || gameGetPressedC()) {
            rpState = RP_STATE_MENU;
            return;
        }

        static uint16_t s_lobbyPrevSw = 0;
        const uint16_t sw = gameGetCtrlSwitches();
        // DEBUG (temporaer): zeigt jede empfangene Tasten-Kombi. Guide-Taste =
        // CSW_BUTTON_HOME = Bit 0x0400. Kommt beim Druck KEINE Aenderung an, gibt
        // der ESP32 die Taste nicht weiter (alte Firmware / Bluepad32 faengt sie).
        if (sw != s_lobbyPrevSw) {
            Serial.print("[Lobby] sw=0x");
            Serial.println(sw, HEX);
        }
        const bool homeEdge =
            (sw & CSW_BUTTON_HOME) && !(s_lobbyPrevSw & CSW_BUTTON_HOME);
        s_lobbyPrevSw = sw;
        if (homeEdge) {
            startPacmanRound();   // Broadcast + frische Runde, → GS_RUNNING
            return;
        }

        static uint32_t s_lobbyDrawMs = 0;
        if ((uint32_t)(millis() - s_lobbyDrawMs) > 250) {
            s_lobbyDrawMs = millis();
            drawPacmanLobby();
        }
        return;
    }

    if (currentGameState == GAME_STATE::GS_MANUAL_HOME_CONFIRM) {
        if (gameGetPressedA()) {
            leaveCaptureSequence(GAME_STATE::GS_IDLE_AT_HOME);
            pacmanManualResumePending = false;
            rpState = RP_STATE_MENU;
            return;
        }
        if (gameGetPressedC()) {
            resumePacmanAfterManualHome();
            return;
        }
        return;
    }

    if (timerTimeoutPending) {
        timerTimeoutPending = false;
        navigationDriveCellAbort();
        solveAbortRun();
        stopMotors();

        // Passender Signalton zum Timerende - nur Pacman spielt ihn ab.
        if (activeRole == GAME_ROLES::ROLE_PACMAN)
            gameBuzzer.play("!T100 o5 g4 e4 c4 o4 g2");

        timerTimeoutReturnActive = true;
        resetReturnCoordination();
        capturePending = false;
        pacmanWon = false;
        winRetreatStartPending = true;

        if (activeRole == GAME_ROLES::ROLE_PACMAN) {
            returnSequence++;
            buildActiveGhostReturnQueue();
        }

        currentGameState = GAME_STATE::GS_RETREATING_TO_CELL;
        drawTimerTimeoutScreen();
        Serial.println("[Game] timer timeout: coordinated homing started");
        return;
    }

    if (capturePending && currentGameState != GAME_STATE::GS_CATCHT) {
        capturePending = false;
        enterCaptureSequence();
    }

    if (currentGameState == GAME_STATE::GS_RETREATING_TO_CELL) {
        if (winRetreatStartPending) {
            winRetreatStartPending = false;
            if (activeRole == GAME_ROLES::ROLE_PACMAN)
                pacmanGameBeginTimeoutRetreat();
            else
                ghostGameBeginCapture();
        }

        if (gameGetPressedA() || gameGetPressedC()) {
            rpState = RP_STATE_MENU;
            return;
        }

        const bool retreatDone = activeRole == GAME_ROLES::ROLE_PACMAN
            ? pacmanGameCaptureLoop()
            : ghostGameCaptureLoop();
        if (!retreatDone &&
            (activeRole == GAME_ROLES::ROLE_PACMAN
                ? pacmanGameCaptureFailed()
                : ghostGameCaptureFailed()))
        {
            drawCaptureRetreatError(
                activeRole == GAME_ROLES::ROLE_PACMAN
                    ? pacmanGameCaptureLastError()
                    : ghostGameCaptureLastError());
            currentGameState = GAME_STATE::GS_ERROR;
            return;
        }
        if (!retreatDone)
            return;

        if (timerTimeoutReturnActive)
            drawTimerTimeoutScreen();
        else
            drawPacmanWinScreen();

        if (activeRole == GAME_ROLES::ROLE_PACMAN) {
            ledSetAll(gameRoleColor(activeRole));
            ledFlushToHardware();
            startSimultaneousReturnHome();
        } else {
            if (homingStartReceived)
                startOwnReturnHome();
            else
                currentGameState = GAME_STATE::GS_WAITING_TO_RETURN;
        }
        return;
    }

    if (currentGameState == GAME_STATE::GS_CATCHT) {
        bool captureMotionDone = true;
        if (activeRole == GAME_ROLES::ROLE_PACMAN)
            captureMotionDone = pacmanGameCaptureLoop();
        else
            captureMotionDone = ghostGameCaptureLoop();

        if (!captureMotionDone &&
            (activeRole == GAME_ROLES::ROLE_PACMAN
                ? pacmanGameCaptureFailed()
                : ghostGameCaptureFailed()))
        {
            drawCaptureRetreatError(
                activeRole == GAME_ROLES::ROLE_PACMAN
                    ? pacmanGameCaptureLastError()
                    : ghostGameCaptureLastError());
            currentGameState = GAME_STATE::GS_ERROR;
            return;
        }

        if (captureMotionDone && captureMotionDoneMs == 0)
            captureMotionDoneMs = millis();

        if (gameGetPressedA() || gameGetPressedC()) {
            leaveCaptureSequence(GAME_STATE::GS_IDLE_AT_HOME);
            rpState = RP_STATE_MENU;
            return;
        }

        if (!captureMotionDone || captureMotionDoneMs == 0 ||
            (uint32_t)(millis() - captureMotionDoneMs) < CAPTURE_HOLD_MS)
            return;

        if (activeRole == GAME_ROLES::ROLE_PACMAN) {
            ledSetAll(gameRoleColor(activeRole));
            ledFlushToHardware();
            startSimultaneousReturnHome();
        } else if (homingStartReceived) {
            startOwnReturnHome();
        }
        return;
    }

    if (currentGameState == GAME_STATE::GS_IDLE_AT_HOME) {
        if (activeRole != GAME_ROLES::ROLE_PACMAN && gameGetPressedC())
            startGhostRoundFromIdle("button C");
        return;
    }

    if (currentGameState == GAME_STATE::GS_ERROR) {
        if (gameGetPressedA() || gameGetPressedB() || gameGetPressedC())
            rpState = RP_STATE_MENU;
        return;
    }

    if (currentGameState == GAME_STATE::GS_WAITING_TO_RETURN) {
        if (gameGetPressedA() || gameGetPressedC()) {
            rpState = RP_STATE_MENU;
            return;
        }

        if (activeRole == GAME_ROLES::ROLE_PACMAN) {
            pacmanGameBroadcastPosition();
            sendHomingStart(false);
            finishPacmanReturnIfReady();
        } else {
            if (homingStartReceived && !ownReturnStarted && !ownAtHome) {
                startOwnReturnHome();
                return;
            }
            ghostGameBackgroundUpdate();
            ghostGameBroadcastPosition();
        }
        return;
    }

    if (currentGameState == GAME_STATE::GS_RETURNING_TO_HOME) {
        if (gameGetPressedA() || gameGetPressedC()) {
            rpState = RP_STATE_MENU;
            return;
        }

        if (activeRole == GAME_ROLES::ROLE_PACMAN) {
            if (pacmanGameReturnHomeLoop()) {
                ownAtHome = true;
                ownReturnStarted = false;
                currentGameState = GAME_STATE::GS_WAITING_TO_RETURN;
                finishPacmanReturnIfReady();
            }
        } else {
            if (ownReturnStartPending) {
                ownReturnStartPending = false;
                ghostGameBeginReturnHome();
            }

            sendReturning(false);
            if (ghostGameReturnHomeLoop()) {
                if (timerTimeoutReturnActive)
                    ghostGameResetAfterTimeout();
                ownAtHome = true;
                ownReturnStarted = false;
                currentGameState = GAME_STATE::GS_WAITING_TO_RETURN;
                lastReturningTxMs = 0;
            }
        }
        return;
    }

    switch (activeRole) {
        case GAME_ROLES::ROLE_PACMAN:
            pacmanCheckBumpers();
            if (pacmanGameLoop())
                rpState = RP_STATE_MENU;
            return;
        case GAME_ROLES::ROLE_RED:
        case GAME_ROLES::ROLE_PINK:
        case GAME_ROLES::ROLE_CYAN:
        case GAME_ROLES::ROLE_BROWN:
            if (ghostGameLoop())
                rpState = RP_STATE_MENU;
            return;
        case GAME_ROLES::TESTDRIVE:
                if (testLoop())
                rpState = RP_STATE_MENU;
            return;
    }
}

void gameReportCapture(uint8_t ghostId, uint8_t x, uint8_t y)
{
    if (getRoleFromId(ghostId) == GAME_ROLES::ROLE_PACMAN ||
        currentGameState != GAME_STATE::GS_RUNNING ||
        capturePending)
    {
        return;
    }

    captureGhostId = ghostId;
    captureX = x;
    captureY = y;
    navigationCellRetreatPrepareAllowStationary();
    capturePending = true;
    g_abortRequested = true;
    stopMotors();

    const uint8_t payload[] = {GC_CATCHED_PACMAN, ghostId, x, y};
    for (uint8_t i = 0; i < 3; i++) {
        uartL3.sendControlData(GAME_BROADCAST_ID, sizeof(payload), payload);
        uartL3.update();
        delay(15);
    }
}

void gameReportPacmanWin()
{
    if (activeRole != GAME_ROLES::ROLE_PACMAN ||
        currentGameState != GAME_STATE::GS_RUNNING ||
        pacmanWon)
    {
        return;
    }

    stopMotors();
    solvePauseMotion();
    navigationDriveCellAbort();
    pacmanWon = true;
    returnSequence++;
    resetReturnCoordination();
    buildActiveGhostReturnQueue();
    drawPacmanWinScreen();

    const uint8_t payload[] = {GC_PACMAN_WINS, returnSequence};
    for (uint8_t i = 0; i < 3; i++) {
        uartL3.sendControlData(GAME_BROADCAST_ID, sizeof(payload), payload);
        uartL3.update();
        delay(15);
    }

    startSimultaneousReturnHome();
}

bool gameGetPressedA()
{
    bool v = buttonAdvanced.wasPressed(0);
    g_btnAPressed = false;
    return v;
}

bool gameGetPressedB()
{
    bool v = buttonAdvanced.wasPressed(1) || g_btnBPressed;
    g_btnBPressed = false;
    return v;
}

bool gameGetPressedC()
{
    bool v = buttonAdvanced.wasPressed(2);
    g_btnCPressed = false;
    return v;
}


uint16_t gameGetCtrlSwitches()
{
    uartL3.update();
    return gameCtrlSw;
}

GAME_ROLES getRoleFromId(uint8_t id) {
    id = id & 0b11100000;
    switch (id) {
        case ROBOT_ID_BITS_PACMAN:
            return GAME_ROLES::ROLE_PACMAN;
        case ROBOT_ID_BITS_RED:
            return GAME_ROLES::ROLE_RED;
        case ROBOT_ID_BITS_PINK:
            return GAME_ROLES::ROLE_PINK;
        case ROBOT_ID_BITS_CYAN:
            return GAME_ROLES::ROLE_CYAN;
        case ROBOT_ID_BITS_BROWN:
            return GAME_ROLES::ROLE_BROWN;
        case ROBOT_ID_BITS_TESTDRIVE:
            return GAME_ROLES::TESTDRIVE;
        default:
            Serial.println("Unknown game role. Defaulting to pacman");
            return GAME_ROLES::ROLE_PACMAN;
    }
}


void setOwnIdRole(GAME_ROLES newRole, uint8_t id) {
    robotId = id & 0b00011111;
    switch (newRole) {
        case GAME_ROLES::ROLE_PACMAN:
            robotId |= ROBOT_ID_BITS_PACMAN;
            return;
        case GAME_ROLES::ROLE_RED:
            robotId |= ROBOT_ID_BITS_RED;
            return;
        case GAME_ROLES::ROLE_PINK:
            robotId |= ROBOT_ID_BITS_PINK;
            return;
        case GAME_ROLES::ROLE_CYAN:
            robotId |= ROBOT_ID_BITS_CYAN;
            return;
        case GAME_ROLES::ROLE_BROWN:
            robotId |= ROBOT_ID_BITS_BROWN;
            return;
        case GAME_ROLES::TESTDRIVE:
            robotId |= ROBOT_ID_BITS_TESTDRIVE;
            return;
        default:
            return;
    }
}
