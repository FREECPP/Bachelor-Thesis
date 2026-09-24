#include "PacmanGame.h"
#include "GamePosition.h"

#include "GameState.h"
#include "GameBumperConfig.h"
#include "RouteMotionExecutor.h"

#include "../MicromouseSolving.h"
#include "../MicromouseMappingState.h"
#include "../MicromouseMapping.h"
#include "../MicromouseLeds.h"
#include "../MicromouseGraphics.h"
#include "../Globals.h"
//#include "GameState.h"
//#include "GameComm.h"
#include <COMMUNICATION_CODES.h>
#include <Pololu3piPlus2040Buzzer.h>
#include <Pololu3piPlus2040BumpSensors.h>
#include <Arduino.h>
#include <math.h>
#include "../ButtonAdvanced.h"
#include "Turbo.h"

#include "../UART_L3.h"

extern UART_L3 uartL3;
extern ButtonAdvanced buttonAdvanced;

using namespace Pololu3piPlus2040;

// ============================================================
// Zustandsmaschine
// ============================================================

enum PacState
{
    PAC_WAIT,    // Warte auf Controller-Eingabe
    PAC_MOVING,  // Roboter faehrt gerade (wurde bereits gestartet)
    PAC_DONE     // Spiel beendet
};

static PacState g_state;
static int      g_pacX, g_pacY, g_pacHeading;
static bool     g_coins[MAZE_W][MAZE_H];
static int      g_lives;
static int      g_coinCount;
static int      g_totalCoins;
static bool     g_robotRolling;  // true: Roboter nicht an Kreuzung gestoppt
// true: 90°-Abbiegungen werden als fliessender Bogen gefahren (Maus rollt
// durch die Kreuzung) statt anzuhalten und im Stand zu drehen. Wird aus der
// Menue-Einstellung "Kurvenfahrt" gespeist (pacmanSetUseCurves).
static bool     g_pacUseCurves = true;
static int      g_moveDir;
static int      g_requestedDir;
static int      g_activeCellDir;
static bool     g_activeCellCenterStop;
static int      g_lastSentPacX;
static int      g_lastSentPacY;
static int      g_lastSentPacHeading;
static uint32_t g_lastPosTxMs;
static bool g_pacDrivingCell;
static BumpSensors g_pacBumpers;
static Buzzer g_pacBuzzer;
static bool g_pacBumpersReady = false;
static bool g_bumperConfirmationActive = false;
static uint32_t g_bumperPressedSinceMs = 0;
static uint8_t g_bumperCandidateGhostId = 0;
static uint8_t g_bumperCandidateX = 0;
static uint8_t g_bumperCandidateY = 0;
static uint32_t g_driveStartMs = 0;
static uint32_t g_hudLastUpdateMs = 0;
static uint16_t g_prevSwitches = 0;  // fuer X-Tasten-Flankenerkennung (Turbo)

struct ObservedGhost
{
    bool known;
    uint8_t id;
    uint8_t x;
    uint8_t y;
    uint32_t seenMs;
};

static ObservedGhost g_observedGhosts[4];

static const LedColor PAC_LED_YELLOW = {180, 140, 0};
static const uint32_t PAC_POS_TX_INTERVAL_MS = 400;
static const uint32_t PAC_GHOST_POSITION_TIMEOUT_MS = 2000;
static const uint32_t PAC_RETURN_REPLAN_MS = 300;
static const uint32_t PAC_RETURN_START_SETTLE_MS = 500;
static const uint8_t PAC_BROADCAST_ID = 255;
static bool g_returnPlanActive = false;
static bool g_returnHomeReached = false;
static uint32_t g_nextReturnPlanMs = 0;

enum PacCaptureMotionState
{
    PAC_CAPTURE_MOTION_IDLE,
    PAC_CAPTURE_MOTION_RETREATING,
    PAC_CAPTURE_MOTION_DONE,
    PAC_CAPTURE_MOTION_ERROR
};

enum PacReturnMotionState
{
    PAC_RETURN_MOTION_IDLE,
    PAC_RETURN_MOTION_PLANNING,
    PAC_RETURN_MOTION_DRIVING,
    PAC_RETURN_MOTION_ALIGNING,
    PAC_RETURN_MOTION_DONE
};

static PacCaptureMotionState g_captureMotionState = PAC_CAPTURE_MOTION_IDLE;
static PacReturnMotionState g_returnMotionState = PAC_RETURN_MOTION_IDLE;
static const char *g_captureMotionLastError = "";

static void drawPacmanHUD();

// ============================================================
// Hilfsfunktionen
// ============================================================

static void playCoinCollectSound()
{
    g_pacBuzzer.play("!T260 o6 l16 c e g >c");
    uartL3.sendControllerRumble(100, 255, 255);
}

static bool isNoCoin(int x, int y)
{
    return (x == startPosX      && y == startPosY);
}

static int switchesToDir(uint16_t sw)
{
    if (sw & CSW_D_PAD_UP)    return NORTH;
    if (sw & CSW_D_PAD_DOWN)  return SOUTH;
    if (sw & CSW_D_PAD_RIGHT) return EAST;
    if (sw & CSW_D_PAD_LEFT)  return WEST;
    return -1;
}

static bool canMove(int x, int y, int dir)
{
    int nx = x + dxDir(dir);
    int ny = y + dyDir(dir);
    // Erlaube Bewegung, wenn Wand unbekannt oder bekannt offen
    return inBounds(nx, ny)
        && !(maze[x][y].known[dir] && maze[x][y].wall[dir]);
}

static void latchRequestedDirection(uint16_t sw)
{
    int dir = switchesToDir(sw);
    if (dir >= 0)
        g_requestedDir = dir;
}

static int chooseDirectionAtNode(bool requireControllerStart)
{   
    
    if (requireControllerStart && g_requestedDir < 0)
        return -1;

    if (g_requestedDir >= 0 && canMove(g_pacX, g_pacY, g_requestedDir))
        return g_requestedDir;

    if (g_moveDir >= 0 && canMove(g_pacX, g_pacY, g_moveDir))
        return g_moveDir;

    g_moveDir = -1;
    return -1;
   
}

static void requestCenterStopForQueuedTurn()
{
    if (!g_pacDrivingCell || g_requestedDir < 0 || g_requestedDir == g_activeCellDir)
        return;

    // 90°-Abbiegung als fliessende Kurve: NICHT abbremsen und NICHT anhalten –
    // die Maus rollt mit Tempo durch die Kreuzung in den Pivot. Einen evtl. aus
    // dem Geradeaus-Lookahead gesetzten Center-Stop deshalb aktiv loeschen,
    // sobald eine 90°-Abbiegung gequeued ist. Nur 180° (oder deaktivierte
    // Boegen) erzwingen weiterhin den Halt (Punktdrehung aus dem Stand).
    const int turnDiff = ((g_requestedDir - g_activeCellDir) % 4 + 4) % 4;
    const bool curveTurn = g_pacUseCurves && (turnDiff == 1 || turnDiff == 3);
    if (curveTurn)
    {
        g_activeCellCenterStop = false;
        navigationDriveCellSetCenterStop(false);
        return;
    }

    const int nextX = g_pacX + dxDir(g_activeCellDir);
    const int nextY = g_pacY + dyDir(g_activeCellDir);
    if (!inBounds(nextX, nextY))
        return;

    if (canMove(nextX, nextY, g_requestedDir))
    {
        g_activeCellCenterStop = true;
        navigationDriveCellRequestCenterStop();
    }
}

static void sendPacmanPosition(bool force)
{
    const uint32_t now = millis();
    if (!force &&
        g_pacX == g_lastSentPacX &&
        g_pacY == g_lastSentPacY &&
        g_pacHeading == g_lastSentPacHeading &&
        (uint32_t)(now - g_lastPosTxMs) < PAC_POS_TX_INTERVAL_MS)
    {
        return;
    }

    g_lastSentPacX = g_pacX;
    g_lastSentPacY = g_pacY;
    g_lastSentPacHeading = g_pacHeading;
    g_lastPosTxMs = now;
    // GameComm is not rebuilt yet. Keep Pacman position broadcasts on UART_L3
    // so the old direct dependency on GameComm stays out of this module.
    uartL3.sendPosData(PAC_BROADCAST_ID,
                       gamePositionEncodeX((uint8_t)g_pacX, g_pacHeading),
                       gamePositionEncodeY((uint8_t)g_pacY, g_pacHeading));
}

// Aktuellen Muenzstand (gesammelt + gesamt) an die externe Spielstandsanzeige
// broadcasten. Das Scoreboard fuellt daraus den senkrechten LED-Balken.
static void sendPacmanScore()
{
    const uint8_t payload[] = {
        GC_CURRENT_SCORE,
        (uint8_t)g_coinCount,
        (uint8_t)g_totalCoins
    };
    uartL3.sendControlData(PAC_BROADCAST_ID, sizeof(payload), payload);
}

static bool observedGhostFresh(const ObservedGhost &ghost, uint32_t now)
{
    return ghost.known &&
           (uint32_t)(now - ghost.seenMs) <=
               PAC_GHOST_POSITION_TIMEOUT_MS;
}

static bool observedGhostAt(int x, int y, uint32_t now)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        const ObservedGhost &ghost = g_observedGhosts[i];
        if (observedGhostFresh(ghost, now) &&
            ghost.x == x && ghost.y == y)
        {
            return true;
        }
    }
    return false;
}

static void applyObservedGhostBlocks(uint32_t now)
{
    solveClearDynamicObstacles();
    for (uint8_t i = 0; i < 4; i++)
    {
        const ObservedGhost &ghost = g_observedGhosts[i];
        if (observedGhostFresh(ghost, now))
            solveBlockCell(ghost.x, ghost.y);
    }
}

// ============================================================
// Display
// ============================================================

// Zeichnet einen grossen, gut sichtbaren Pfeil von (cx, cy) in Richtung (nx, ny).
static void drawCompassArrow(int cx, int cy, float nx, float ny, int r)
{
    (void)r;

    float len = sqrtf(nx * nx + ny * ny);
    if (len < 0.01f)
        return;

    nx /= len;
    ny /= len;

    const float px = -ny;
    const float py =  nx;

    const float arrowLen = 25.0f;
    const int tipX  = cx + (int)(nx * arrowLen);
    const int tipY  = cy + (int)(ny * arrowLen);
    const int baseX = tipX - (int)(nx * 18.0f);
    const int baseY = tipY - (int)(ny * 18.0f);
    const int tailX = cx - (int)(nx * 20.0f);
    const int tailY = cy - (int)(ny * 20.0f);

    for (int w = -4; w <= 4; w++)
    {
        const int ox = (int)(px * (float)w);
        const int oy = (int)(py * (float)w);
        u8g2.drawLine(tailX + ox, tailY + oy, baseX + ox, baseY + oy);
    }

    for (int w = -15; w <= 15; w++)
    {
        const int bx = baseX + (int)(px * (float)w);
        const int by = baseY + (int)(py * (float)w);
        u8g2.drawLine(tipX, tipY, bx, by);
    }
}

static float headingDeltaCells(int fromHeading, int toHeading)
{
    int diff = (toHeading - fromHeading + 4) % 4;
    if (diff == 3)
        return -1.0f;
    return (float)diff;
}

static void headingVector(float headingCells, float &x, float &y)
{
    const float angle = (3.14159265f * 0.5f) -
                        headingCells * (3.14159265f * 0.5f);
    x = cosf(angle);
    y = sinf(angle);
}

static void drawCheeseIcon(int x, int y, bool filled)
{
    if (filled)
    {
        u8g2.drawTriangle(x, y, x + 18, y + 7, x, y + 15);
        u8g2.setDrawColor(0);
        u8g2.drawDisc(x + 5, y + 5, 2);
        u8g2.drawDisc(x + 7, y + 11, 1);
        u8g2.drawDisc(x + 13, y + 7, 1);
        u8g2.setDrawColor(1);
        return;
    }

    u8g2.drawLine(x, y, x + 18, y + 7);
    u8g2.drawLine(x + 18, y + 7, x, y + 15);
    u8g2.drawLine(x, y + 15, x, y);
    u8g2.drawCircle(x + 5, y + 5, 2);
    u8g2.drawCircle(x + 7, y + 11, 1);
    u8g2.drawCircle(x + 13, y + 7, 1);
}

static void drawCheeseProgress()
{
    const int xs[6] = {1, 1, 1, 109, 109, 109};
    const int ys[6] = {2, 24, 47, 2, 24, 47};
    for (int i = 0; i < 6; i++)
        drawCheeseIcon(xs[i], ys[i], i < g_coinCount);
}

static void drawPacmanHUD()
{
    u8g2.clearBuffer();

    // --- Lebensanzeige oben, ohne Balken ---
    u8g2.setFont(u8g2_font_5x7_tf);
    char buf[24];
    snprintf(buf, sizeof(buf), "L:%d", g_lives);
    u8g2.drawStr(58, 7, buf);

    // --- Grosser Richtungspfeil unter dem Header ---
    const int CX = 64;
    const int CY = 37;
    const int CR = 24;

    drawCheeseProgress();

    float pacFx = (float)g_pacX;
    float pacFy = (float)g_pacY;
    float displayHeading = (float)g_pacHeading;
    if (g_pacDrivingCell && g_activeCellDir >= 0)
    {
        float progress = navigationDriveCellProgress();
        pacFx += (float)dxDir(g_activeCellDir) * progress;
        pacFy += (float)dyDir(g_activeCellDir) * progress;
        if (navigationDriveCellIsTurning())
        {
            displayHeading =
                (float)g_pacHeading +
                headingDeltaCells(g_pacHeading, g_activeCellDir) *
                navigationDriveCellTurnProgress();
        }
        else
        {
            displayHeading = (float)g_activeCellDir;
        }
    }

    int nearX = -1, nearY = -1;
    float bestSq = 1e9f;
    for (int x = 0; x < MAZE_W; x++)
        for (int y = 0; y < MAZE_H; y++)
            if (g_coins[x][y])
            {
                float ddx = (float)x - pacFx;
                float ddy = (float)y - pacFy;
                float d = ddx * ddx + ddy * ddy;
                if (d < bestSq) { bestSq = d; nearX = x; nearY = y; }
            }

    if (nearX < 0)
    {
        u8g2.setFont(u8g2_font_7x13B_tf);
        const char* msg = "DONE!";
        int w = u8g2.getStrWidth(msg);
        u8g2.drawStr(CX - w / 2, CY + 5, msg);
    }
    else if (bestSq > 0.01f)
    {
        float fdx = (float)nearX - pacFx;
        float fdy = (float)nearY - pacFy;
        float len = sqrtf(fdx * fdx + fdy * fdy);
        float ux = fdx / len;
        float uy = fdy / len;

        float fwdX, fwdY;
        headingVector(displayHeading, fwdX, fwdY);
        float dispFwd = ux * fwdX + uy * fwdY;
        float dispRgt = ux * fwdY + uy * (-fwdX);
        float nx =  dispRgt;
        float ny = -dispFwd;
        drawCompassArrow(CX, CY, nx, ny, CR);
    }
    else
    {
        u8g2.setFont(u8g2_font_7x13B_tf);
        u8g2.drawStr(CX - 3, CY + 5, "!");
    }

    // Turbo: schwindender Balken am unteren Rand.
    if (turboIsActive())
    {
        int w = (int)(128.0f * turboRemainingFraction());
        if (w < 1) w = 1;
        if (w > 128) w = 128;
        u8g2.drawBox(0, 61, w, 3);
    }

    oledSendBuffer();
    return;

    if (g_pacDrivingCell && g_driveStartMs > 0)
    {
        // === Waehrend Fahrt: Pfeil rotiert kontinuierlich ===
        // 1.5 Umdrehungen pro Sekunde
        uint32_t elapsed = (uint32_t)(millis() - g_driveStartMs);
        float angle = (float)elapsed * (2.0f * 3.14159265f * 1.5f) / 1000.0f;
        float nx =  sinf(angle);
        float ny = -cosf(angle);
        drawCompassArrow(CX, CY, nx, ny, CR);
    }
    else
    {
        // === Am Node: Pfeil zeigt zur naechsten Muenze ===
        int nearX = -1, nearY = -1;
        float bestSq = 1e9f;
        for (int x = 0; x < MAZE_W; x++)
            for (int y = 0; y < MAZE_H; y++)
                if (g_coins[x][y])
                {
                    float ddx = (float)x - (float)g_pacX;
                    float ddy = (float)y - (float)g_pacY;
                    float d = ddx * ddx + ddy * ddy;
                    if (d < bestSq) { bestSq = d; nearX = x; nearY = y; }
                }

        if (nearX < 0)
        {
            // Alle Muenzen eingesammelt
            u8g2.setFont(u8g2_font_7x13B_tf);
            const char* msg = "DONE!";
            int w = u8g2.getStrWidth(msg);
            u8g2.drawStr(CX - w / 2, CY + 5, msg);
        }
        else if (bestSq > 0.1f)
        {
            // Richtung zur naechsten Muenze berechnen
            float fdx = (float)nearX - (float)g_pacX;
            float fdy = (float)nearY - (float)g_pacY;
            float len = sqrtf(fdx * fdx + fdy * fdy);
            float ux = fdx / len;
            float uy = fdy / len;

            // Weltrichtung → Displayrichtung (relativ zur Blickrichtung)
            float fwdX = (float)dxDir(g_pacHeading);
            float fwdY = (float)dyDir(g_pacHeading);
            float dispFwd = ux * fwdX + uy * fwdY;
            float dispRgt = ux * fwdY + uy * (-fwdX);
            float nx =  dispRgt;
            float ny = -dispFwd;
            drawCompassArrow(CX, CY, nx, ny, CR);
        }
        else
        {
            // Pacman steht auf der Muenze (sehr nah) – Pfeil geradeaus
            float fwdX = (float)dxDir(g_pacHeading);
            float fwdY = (float)dyDir(g_pacHeading);
            float dispFwd = 1.0f;
            float dispRgt = 0.0f;
            (void)fwdX; (void)fwdY; (void)dispFwd; (void)dispRgt;
            // Kurz "!" zeigen
            u8g2.setFont(u8g2_font_7x13B_tf);
            u8g2.drawStr(CX - 3, CY + 5, "!");
        }
    }

    oledSendBuffer();
}

// Gibt true -> Quit (Menue), false -> Neustart
static bool showVictoryScreen()
{
    stopMotors();
    g_robotRolling = false;
    ledSetAll(LED_WHITE);
    ledFlushToHardware();

    Buzzer buzzer;
    buzzer.play("!T230 o5 l16 c e g >c e g >c e g >c8 r16 <g >c <g >c <g >c8 r16 <e g >c e g >c4");

    u8g2.clearBuffer();
    u8g2.drawBox(0, 0, 128, 15);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_9x18B_tf);
    const char* title = "VICTORY!";
    u8g2.drawStr((128 - u8g2.getStrWidth(title)) / 2, 13, title);
    u8g2.setDrawColor(1);

    u8g2.setFont(u8g2_font_6x10_tf);
    char buf[28];
    snprintf(buf, sizeof(buf), "Muenzen: %d/%d", g_coinCount, g_totalCoins);
    u8g2.drawStr((128 - u8g2.getStrWidth(buf)) / 2, 32, buf);

    u8g2.drawLine(0, 42, 128, 42);
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(4, 52, "A = Menu");
    u8g2.drawStr(4, 62, "C = Neu starten");
    oledSendBuffer();

    while (buzzer.isPlaying()) delay(10);

    while (true)
    {
        // menuPollButtons();
        buttonAdvanced.update();
        if (gameGetPressedA()) {
            g_abortRequested = false;
            return true;   // Menue
        }
        if (gameGetPressedC()) {
            g_abortRequested = false;
            return false;  // Neustart
        }
        delay(20);
    }
}

// ============================================================
// Kernlogik: Eine Zelle fahren (nicht-blockierende Linienfahrt)
//
// beginDriveOneCell() startet Drehung und Linienfahrt. Danach ruft die
// Pacman-Loop updateDriveOneCell() zyklisch, bis die Zelle fertig ist.
// Gibt zurueck:
//   0  -> Weiterfahren (g_state bleibt PAC_MOVING)
//  -1  -> Fehler / Linienverlust (-> PAC_WAIT)
//   1  -> Victory + Quit gewaehlt (-> PAC_DONE)
//   2  -> Victory + Neustart (pacmanGameInit wird aussen gerufen)
// ============================================================
static void beginDriveOneCell(int dir)
{
    // Serial.print("PAC driveOneCell dir="); Serial.print(dir);
    // Serial.print(" pos=("); Serial.print(g_pacX); Serial.print(","); Serial.print(g_pacY);
    // Serial.print(") hdg="); Serial.println(g_pacHeading);

    const int turnDiff = ((dir - g_pacHeading) % 4 + 4) % 4;
    const bool needsTurn = (turnDiff != 0);
    const bool is90Turn  = (turnDiff == 1 || turnDiff == 3);

    // Fliessender Bogen (Pivot um das innere Rad) bei JEDER 90°-Abbiegung –
    // auch an L-Ecken. Grund: Die Kreuzung wird ueber die vorderen 3 Sensoren
    // erkannt, die der Radachse vorauslaufen. Im Pivot-Moment steht der
    // Drehpunkt (Raeder) also noch leicht vor der Ecke, die Nase schwenkt frei
    // (seit dem Innenrad-Halt kein Vorwaertsschwung mehr). Das ist dieselbe
    // Position wie an +/T-Kreuzungen, wo der Pivot bereits sauber laeuft.
    // Nur 180°-Wenden bleiben Punktdrehung (is90Turn == false).
    const bool useCurve = g_pacUseCurves && needsTurn && is90Turn;

    // Nur vor PUNKTdrehungen (180°) aktiv bremsen, damit die Drehung sauber aus
    // dem Stand startet. Bei der 90°-KURVENFAHRT NICHT bremsen: die Maus rollt
    // mit Tempo in den Pivot → fliessende Kurve (Bogen), kein Anhalten – genau
    // wie der Geist. (Wunsch: nicht abbremsen, mit der Kurve abbiegen.)
    if (needsTurn && g_robotRolling && !useCurve)
    {
        motorsActiveBrake();
        g_robotRolling = false;
    }

    // Lookahead: ist die uebernaechste Zelle ebenfalls frei?
    int nx = g_pacX + dxDir(dir);
    int ny = g_pacY + dyDir(dir);
    bool nextAlsoClear = canMove(nx, ny, dir);
    bool cAS = !nextAlsoClear;

    g_driveStartMs = millis();
    g_pacDrivingCell = true;
    g_activeCellDir = dir;
    g_activeCellCenterStop = cAS;
    navigationDriveCellSetAllowCurve(useCurve);
    navigationDriveCellBegin(dir, cAS);
}

static int finishDriveOneCell()
{
    g_pacDrivingCell = false;

    g_robotRolling = !g_activeCellCenterStop;
    g_pacX += dxDir(g_activeCellDir);
    g_pacY += dyDir(g_activeCellDir);
    g_pacHeading = g_activeCellDir;
    g_moveDir = g_activeCellDir;
    g_activeCellDir = -1;

    // Muenze einsammeln
    if (g_coins[g_pacX][g_pacY])
    {
        g_coins[g_pacX][g_pacY] = false;
        g_coinCount++;
        playCoinCollectSound();
        sendPacmanScore();

        if (g_coinCount >= g_totalCoins)
        {
            // Win-Fanfare GENAU jetzt (letzte Muenze), nicht erst bei Ankunft
            // an der Startposition. Nicht-blockierend: spielt im Hintergrund
            // weiter, waehrend die koordinierte Heimfahrt laeuft.
            g_pacBuzzer.play(
                "!T230 o5 l16 c e g >c e g >c e g >c8 r16 "
                "<g >c <g >c <g >c8 r16 <e g >c e g >c4");
            sendPacmanPosition(true);
            drawPacmanHUD();
            g_state = PAC_WAIT;
            gameReportPacmanWin();
            return 0;
        }

        flashColor(LED_WHITE, 180);
        ledFlushToHardware();
    }
    else if (!turboOwnsLeds())
    {
        ledSetAll(PAC_LED_YELLOW);
        ledFlushToHardware();
    }
    sendPacmanPosition(true);
    drawPacmanHUD();
    return 0;
}

static int updateDriveOneCell()
{
    requestCenterStopForQueuedTurn();

    LineFollowRunStatus status = navigationDriveCellStep();
    if (status == LINE_FOLLOW_RUN_RUNNING)
        return 0;
    if (status == LINE_FOLLOW_RUN_DONE)
        return finishDriveOneCell();

    Serial.println("PAC turn/drive FAILED");
    g_pacDrivingCell = false;
    g_robotRolling = false;
    g_activeCellDir = -1;
    stopMotors();
    return -1;
}

// ============================================================
// Rueckfahrt zum Startknoten und Neustart
// ============================================================

static void handleRestart()
{
    // Solver ueber aktuelle physische Position informieren, dann zurueckfahren
    solveSetPose(g_pacX, g_pacY, g_pacHeading);
    if (startSolving(startPosX, startPosY))
    {
        ledSetAll(PAC_LED_YELLOW);
        ledFlushToHardware();
        while (!solvingLoop());
    }
    pacmanGameInit();
}

static bool consumeQuitRequest()
{
    if (!gameGetPressedA() && !gameGetPressedC())
        return false;

    stopMotors();
    g_robotRolling = false;
    g_pacDrivingCell = false;
    g_moveDir = -1;
    g_activeCellDir = -1;
    g_abortRequested = false;
    ledShowIdle();
    g_state = PAC_DONE;
    return true;
}

static bool handleDriveResult(int result)
{
    if (consumeQuitRequest())
        return true;
    if (result == 1)
    {
        ledShowIdle();
        g_state = PAC_DONE;
        return true;
    }
    if (result == 2)
    {
        handleRestart();
        return false;
    }
    if (result < 0)
    {
        g_state = PAC_WAIT;
        drawPacmanHUD();
    }
    return false;
}

// ============================================================
// HUD-Aktualisierung waehrend einer Fahrzelle (aufgerufen aus
// menuBackgroundUpdate, damit der Pfeil zwischen Nodes mitlaeuft).
// ============================================================
void pacmanUpdateHUDDuringDrive()
{
    if (!g_pacDrivingCell) return;
    const uint32_t now = millis();
    if ((uint32_t)(now - g_hudLastUpdateMs) < 70) return;
    g_hudLastUpdateMs = now;
    drawPacmanHUD();
}

// ============================================================
// Pacman-seitige Bumper-Erkennung (aufgerufen aus menuBackgroundUpdate
// nur wenn currentGameRole() == PACMAN)
// ============================================================
void pacmanCheckBumpers()
{
    if (!g_pacBumpersReady || currentGameState != GAME_STATE::GS_RUNNING)
    {
        ledSetBumperIndicators(false, false);
        g_bumperConfirmationActive = false;
        return;
    }

    // Die letzte sicher erreichte Knotenposition auch waehrend Drehung und
    // Zellfahrt erneuern, damit Ghosts keine alten Koordinaten verwenden.
    sendPacmanPosition(false);

    g_pacBumpers.read();
    const bool leftBumperPressed = g_pacBumpers.leftIsPressed();
    const bool rightBumperPressed = g_pacBumpers.rightIsPressed();
    const bool bumperPressed = leftBumperPressed || rightBumperPressed;
    ledSetBumperIndicators(leftBumperPressed, rightBumperPressed);

    int nextX = g_pacX;
    int nextY = g_pacY;
    if (g_pacDrivingCell && g_activeCellDir >= 0)
    {
        nextX += dxDir(g_activeCellDir);
        nextY += dyDir(g_activeCellDir);
    }

    const ObservedGhost *captureGhost = nullptr;
    uint8_t captureX = 0;
    uint8_t captureY = 0;
    const uint32_t now = millis();
    for (uint8_t i = 0; i < 4; i++)
    {
        const ObservedGhost &ghost = g_observedGhosts[i];
        if (!ghost.known ||
            (uint32_t)(now - ghost.seenMs) >
                PAC_GHOST_POSITION_TIMEOUT_MS)
            continue;

        const bool atCurrentCell = ghost.x == g_pacX && ghost.y == g_pacY;
        const bool atDrivenCell =
            g_pacDrivingCell &&
            !navigationDriveCellIsTurning() &&
            ghost.x == nextX && ghost.y == nextY;
        if (!atCurrentCell && !atDrivenCell)
            continue;

        captureGhost = &ghost;
        captureX = atDrivenCell ? (uint8_t)nextX : (uint8_t)g_pacX;
        captureY = atDrivenCell ? (uint8_t)nextY : (uint8_t)g_pacY;
        break;
    }

    if (captureGhost == nullptr)
    {
        g_bumperConfirmationActive = false;
        return;
    }

    if (!bumperPressed)
    {
        g_bumperConfirmationActive = false;
        return;
    }

    if (!g_bumperConfirmationActive ||
        captureGhost->id != g_bumperCandidateGhostId ||
        captureX != g_bumperCandidateX ||
        captureY != g_bumperCandidateY)
    {
        g_bumperConfirmationActive = true;
        g_bumperPressedSinceMs = now;
        g_bumperCandidateGhostId = captureGhost->id;
        g_bumperCandidateX = captureX;
        g_bumperCandidateY = captureY;
        return;
    }

    if ((uint32_t)(now - g_bumperPressedSinceMs) < GAME_BUMPER_CONFIRM_MS)
        return;

    g_bumperConfirmationActive = false;
    Serial.print("[Capture] pacman confirmed bumper+ghost coordinates ghost=");
    Serial.print(captureGhost->id);
    Serial.print(" cell=");
    Serial.print(captureX);
    Serial.print(',');
    Serial.println(captureY);
    gameReportCapture(captureGhost->id, captureX, captureY);
}

// ============================================================
// Oeffentliche API
// ============================================================

void pacmanSetUseCurves(bool enable)
{
    g_pacUseCurves = enable;
    // Kurve → vordere 3 Sensoren, Punkt → 4 von 5 Sensoren.
    setIntersectionUseFront3(enable);
}

void pacmanGameInit()
{
    navigationDriveCellAbort();
    g_pacX       = solveCurrentX();
    g_pacY       = solveCurrentY();
    g_pacHeading = solveCurrentHeading();
    g_lives      = PAC_LIVES;
    g_coinCount  = 0;
    g_totalCoins = 0;
    g_state      = PAC_WAIT;
    g_robotRolling = false;
    g_moveDir = -1;
    g_requestedDir = -1;
    g_activeCellDir = -1;
    g_activeCellCenterStop = false;
    g_lastSentPacX = -1;
    g_lastSentPacY = -1;
    g_lastSentPacHeading = -1;
    g_lastPosTxMs = 0;
    g_pacDrivingCell = false;
    g_returnPlanActive = false;
    g_returnHomeReached = false;
    g_nextReturnPlanMs = 0;
    g_captureMotionState = PAC_CAPTURE_MOTION_IDLE;
    g_returnMotionState = PAC_RETURN_MOTION_IDLE;
    g_btnAPressed  = false;
    g_btnCPressed  = false;
    g_prevSwitches = 0;
    turboReset();
    g_pacBumpers.marginPercentage = GAME_BUMPER_MARGIN_PERCENT;
    g_pacBumpers.calibrate();
    g_pacBumpersReady = true;
    g_bumperConfirmationActive = false;
    g_bumperPressedSinceMs = 0;
    for (uint8_t i = 0; i < 4; i++)
        g_observedGhosts[i] = {false, 0, 0, 0, 0};

    // Mapping-Layer-Heading fuer die nicht-blockierenden Dreh-Stepper setzen.
    mappingSetPose(g_pacX, g_pacY, g_pacHeading);

    for (int x = 0; x < MAZE_W; x++)
    for (int y = 0; y < MAZE_H; y++)
        g_coins[x][y] = false;

const int coinNodes[6][2] = {
    {1, 0},
    {1, 3},
    {3, 3},
    {6, 0},
    {7, 1},
    {7, 4}
};

for (int i = 0; i < 6; i++)
{
    int x = coinNodes[i][0];
    int y = coinNodes[i][1];

    if (inBounds(x, y) && !isNoCoin(x, y))
    {
        g_coins[x][y] = true;
        g_totalCoins++;
    }
}

    ledSetAll(PAC_LED_YELLOW);
    drawPacmanHUD();

    sendPacmanPosition(true);
    sendPacmanScore();
    uartL3.update();
}

bool pacmanGameLoop()
{
    if (consumeQuitRequest())
        return true;

    // Aktueller Switch-Stand aus dem GameState-Layer (poll auch UART intern)
    uint16_t sw = gameGetCtrlSwitches();
    latchRequestedDirection(sw);

    // Turbo: X-Taste flankengetriggert auswerten, dann Turbo-Zustand
    // (Timer, LED-Puls, Geschwindigkeitsfaktor) fortschreiben.
    const bool xRisingEdge =
        (sw & CSW_BUTTON_X) && !(g_prevSwitches & CSW_BUTTON_X);
    g_prevSwitches = sw;
    turboHandleButtonEdge(xRisingEdge);
    turboUpdate();

    switch (g_state)
    {
    // ----------------------------------------------------------
    // PAC_WAIT: Warten. Wenn eine Controller-Richtung gelatcht ist und
    // der Weg frei ist, wird die naechste Zellenfahrt gestartet.
    // ----------------------------------------------------------
    case PAC_WAIT:
    {
        if (consumeQuitRequest())
            return true;

        int dir = chooseDirectionAtNode(true);
        if (dir < 0)
        {
            static uint32_t s_lastDbg = 0;
            if (millis() - s_lastDbg > 500) {
                s_lastDbg = millis();
                Serial.print("PAC_WAIT: sw=0x"); Serial.print(sw, HEX);
                Serial.print(" dir="); Serial.print(dir);
                if (g_requestedDir >= 0) {
                    Serial.print(" requested="); Serial.print(g_requestedDir);
                    Serial.print(" known="); Serial.print(maze[g_pacX][g_pacY].known[g_requestedDir]);
                    Serial.print(" wall=");  Serial.print(maze[g_pacX][g_pacY].wall[g_requestedDir]);
                }
                Serial.println();
            }
            if (!turboOwnsLeds())
                ledSetAll(PAC_LED_YELLOW);
            drawPacmanHUD();
            return false;
        }

        // Sofort losfahren - kein Warten auf naechsten Loop-Aufruf
        g_state = PAC_MOVING;
        beginDriveOneCell(dir);
        return false;
    }

    // ----------------------------------------------------------
    // PAC_MOVING: Eine aktive Zelle wird schrittweise aktualisiert.
    // Zwischen den Zellen wird die gelatchte Richtung weitergefahren,
    // bis eine Wand oder eine neue gueltige Controller-Richtung kommt.
    // ----------------------------------------------------------
    case PAC_MOVING:
    {
        if (consumeQuitRequest())
            return true;

        if (g_pacDrivingCell)
        {
            int result = updateDriveOneCell();
            if (result == 0)
                pacmanUpdateHUDDuringDrive();
            return handleDriveResult(result);
        }

        int dir = chooseDirectionAtNode(false);
        if (dir < 0)
        {
            if (g_robotRolling) { stopMotors(); g_robotRolling = false; }
            if (!turboOwnsLeds())
                ledSetAll(PAC_LED_YELLOW);
            g_state = PAC_WAIT;
            drawPacmanHUD();
            return false;
        }

        beginDriveOneCell(dir);
        return false;  // bleibt PAC_MOVING
    }

    // ----------------------------------------------------------
    case PAC_DONE:
        return true;
    }

    return true;
}

static void beginPacmanRetreat(bool loseLife)
{
    g_abortRequested = false;
    const NavigationRetreatBeginStatus retreatStatus =
        navigationCellRetreatBegin();
    routeMotionAbort();
    stopMotors();

    g_pacDrivingCell = false;
    g_robotRolling = false;
    g_moveDir = -1;
    g_requestedDir = -1;
    g_activeCellDir = -1;
    g_state = PAC_WAIT;
    if (retreatStatus == NAV_RETREAT_BEGIN_STARTED)
        g_captureMotionState = PAC_CAPTURE_MOTION_RETREATING;
    else if (retreatStatus == NAV_RETREAT_BEGIN_STATIONARY)
    {
        g_pacHeading = mappingGetHeading();
        solveSetPose(g_pacX, g_pacY, g_pacHeading);
        g_captureMotionState = PAC_CAPTURE_MOTION_DONE;
    }
    else
    {
        g_captureMotionState = PAC_CAPTURE_MOTION_ERROR;
        g_captureMotionLastError = navigationCellRetreatLastError();
        Serial.print("[Pacman] capture retreat begin failed: ");
        Serial.println(g_captureMotionLastError);
    }

    turboCancel();

    if (loseLife && g_lives > 0)
        g_lives--;

    drawPacmanHUD();
}

void pacmanGameBeginCapture(uint8_t ghostId)
{
    (void)ghostId;
    beginPacmanRetreat(true);
}

void pacmanGameBeginTimeoutRetreat()
{
    beginPacmanRetreat(false);
}

bool pacmanGameCaptureLoop()
{
    if (g_captureMotionState == PAC_CAPTURE_MOTION_DONE ||
        g_captureMotionState == PAC_CAPTURE_MOTION_IDLE)
    {
        return true;
    }

    if (g_captureMotionState == PAC_CAPTURE_MOTION_ERROR)
        return false;

    const LineFollowRunStatus status = navigationCellRetreatStep();
    if (status == LINE_FOLLOW_RUN_RUNNING)
        return false;
    if (status != LINE_FOLLOW_RUN_DONE)
    {
        stopMotors();
        g_captureMotionState = PAC_CAPTURE_MOTION_ERROR;
        g_captureMotionLastError = navigationCellRetreatLastError();
        Serial.print("[Pacman] capture retreat failed: ");
        Serial.println(g_captureMotionLastError);
        return false;
    }

    g_pacHeading = mappingGetHeading();
    solveSetPose(g_pacX, g_pacY, g_pacHeading);
    stopMotors();
    sendPacmanPosition(true);
    g_captureMotionState = PAC_CAPTURE_MOTION_DONE;
    return true;
}

bool pacmanGameCaptureFailed()
{
    return g_captureMotionState == PAC_CAPTURE_MOTION_ERROR;
}

const char *pacmanGameCaptureLastError()
{
    return g_captureMotionLastError;
}

bool pacmanGameHasLives()
{
    return g_lives > 0;
}

void pacmanGameBeginReturnHome()
{
    navigationDriveCellAbort();
    routeMotionReset();
    stopMotors();
    ledSetAll(PAC_LED_YELLOW);
    ledFlushToHardware();

    g_returnPlanActive = false;
    g_returnHomeReached = false;
    g_nextReturnPlanMs = millis() + PAC_RETURN_START_SETTLE_MS;
    g_returnMotionState = PAC_RETURN_MOTION_PLANNING;
    g_state = PAC_WAIT;
    solveSetPose(g_pacX, g_pacY, g_pacHeading);
}

bool pacmanGameReturnHomeLoop()
{
    if (g_returnHomeReached)
        return true;

    sendPacmanPosition(false);

    if (g_returnMotionState == PAC_RETURN_MOTION_ALIGNING)
    {
        const TurnRunStatus status = turnStep();
        if (status == TURN_RUN_RUNNING)
            return false;
        if (status != TURN_RUN_DONE)
        {
            g_returnMotionState = PAC_RETURN_MOTION_PLANNING;
            g_nextReturnPlanMs = millis() + PAC_RETURN_REPLAN_MS;
            return false;
        }

        g_pacHeading = PAC_START_HEADING;
        solveSetPose(g_pacX, g_pacY, g_pacHeading);
        mappingSetPose(g_pacX, g_pacY, g_pacHeading);
        sendPacmanPosition(true);
        g_returnHomeReached = true;
        g_returnMotionState = PAC_RETURN_MOTION_DONE;
        return true;
    }

    if (g_returnMotionState == PAC_RETURN_MOTION_PLANNING)
    {
        if ((int32_t)(millis() - g_nextReturnPlanMs) < 0)
            return false;

        if (g_pacX == startPosX && g_pacY == startPosY)
        {
            mappingSetPose(g_pacX, g_pacY, g_pacHeading);
            turnBegin(PAC_START_HEADING);
            if (turnIsActive())
            {
                g_returnMotionState = PAC_RETURN_MOTION_ALIGNING;
                return false;
            }

            g_pacHeading = PAC_START_HEADING;
            solveSetPose(g_pacX, g_pacY, g_pacHeading);
            sendPacmanPosition(true);
            g_returnHomeReached = true;
            g_returnMotionState = PAC_RETURN_MOTION_DONE;
            return true;
        }

        const uint32_t now = millis();
        if (observedGhostAt(startPosX, startPosY, now))
        {
            g_nextReturnPlanMs = now + PAC_RETURN_REPLAN_MS;
            return false;
        }

        applyObservedGhostBlocks(now);
        SolveRoutePlan route = {};
        if (!solveCreateRoutePlan(startPosX, startPosY, route) ||
            !routeMotionLoad(route))
        {
            solveClearDynamicObstacles();
            g_nextReturnPlanMs = now + PAC_RETURN_REPLAN_MS;
            return false;
        }

        solveClearDynamicObstacles();
        g_returnPlanActive = routeMotionIsActive();
        if (!g_returnPlanActive)
            return false;
        g_returnMotionState = PAC_RETURN_MOTION_DRIVING;
    }

    if (!routeMotionIsDrivingCell())
    {
        int nextX;
        int nextY;
        const uint32_t now = millis();
        if (routeMotionNextCell(nextX, nextY) &&
            observedGhostAt(nextX, nextY, now))
        {
            Serial.print("[PacmanHome] next cell occupied, replanning cell=");
            Serial.print(nextX);
            Serial.print(',');
            Serial.println(nextY);
            routeMotionAbort();
            g_returnPlanActive = false;
            g_returnMotionState = PAC_RETURN_MOTION_PLANNING;
            g_nextReturnPlanMs = now + PAC_RETURN_REPLAN_MS;
            return false;
        }
    }

    const RouteMotionStatus status = routeMotionStep();
    if (status == ROUTE_MOTION_ERROR)
    {
        routeMotionAbort();
        g_returnPlanActive = false;
        g_returnMotionState = PAC_RETURN_MOTION_PLANNING;
        g_nextReturnPlanMs = millis() + PAC_RETURN_REPLAN_MS;
        return false;
    }

    if (status == ROUTE_MOTION_CELL_REACHED ||
        status == ROUTE_MOTION_DONE)
    {
        g_pacX = solveCurrentX();
        g_pacY = solveCurrentY();
        g_pacHeading = solveCurrentHeading();
        sendPacmanPosition(true);
    }

    if (status == ROUTE_MOTION_DONE)
    {
        g_returnPlanActive = false;
        mappingSetPose(g_pacX, g_pacY, g_pacHeading);
        turnBegin(PAC_START_HEADING);
        if (turnIsActive())
        {
            g_returnMotionState = PAC_RETURN_MOTION_ALIGNING;
            return false;
        }

        g_pacHeading = PAC_START_HEADING;
        solveSetPose(g_pacX, g_pacY, g_pacHeading);
        sendPacmanPosition(true);
        g_returnHomeReached = true;
        g_returnMotionState = PAC_RETURN_MOTION_DONE;
        return true;
    }

    return false;
}

void pacmanGameStartNextRound()
{
    navigationDriveCellAbort();
    routeMotionAbort();
    g_pacX = startPosX;
    g_pacY = startPosY;
    g_pacHeading = PAC_START_HEADING;
    g_state = PAC_WAIT;
    g_robotRolling = false;
    g_moveDir = -1;
    g_requestedDir = -1;
    g_activeCellDir = -1;
    g_activeCellCenterStop = false;
    g_pacDrivingCell = false;
    g_returnPlanActive = false;
    g_returnHomeReached = false;
    g_nextReturnPlanMs = 0;
    g_captureMotionState = PAC_CAPTURE_MOTION_IDLE;
    g_returnMotionState = PAC_RETURN_MOTION_IDLE;
    g_captureMotionLastError = "";
    g_abortRequested = false;
    g_btnAPressed = false;
    g_btnCPressed = false;
    g_prevSwitches = 0;
    turboReset();
    solveSetPose(g_pacX, g_pacY, g_pacHeading);
    mappingSetPose(g_pacX, g_pacY, g_pacHeading);
    ledSetAll(PAC_LED_YELLOW);
    ledFlushToHardware();
    drawPacmanHUD();
    sendPacmanPosition(true);
    uartL3.update();
}

void pacmanGameResetAfterTimeout()
{
    navigationDriveCellAbort();
    routeMotionAbort();
    stopMotors();
    solveSetPose(startPosX, startPosY, PAC_START_HEADING);
    mappingSetPose(startPosX, startPosY, PAC_START_HEADING);
    pacmanGameInit();
}

void pacmanGameManualHomeReset()
{
    navigationDriveCellAbort();
    routeMotionAbort();
    navigationCellRetreatAbort();
    stopMotors();
    solveSetPose(startPosX, startPosY, PAC_START_HEADING);
    mappingSetPose(startPosX, startPosY, PAC_START_HEADING);
    pacmanGameInit();
}

void pacmanGameBroadcastPosition()
{
    sendPacmanPosition(false);
}

void pacmanGameObserveGhost(uint8_t senderId, uint8_t x, uint8_t y)
{
    const GAME_ROLES role = getRoleFromId(senderId);
    if (role == GAME_ROLES::ROLE_PACMAN || !inBounds(x, y))
        return;

    const uint8_t slot = (uint8_t)role - (uint8_t)GAME_ROLES::ROLE_RED;
    if (slot >= 4)
        return;

    g_observedGhosts[slot] = {true, senderId, x, y, millis()};
}
