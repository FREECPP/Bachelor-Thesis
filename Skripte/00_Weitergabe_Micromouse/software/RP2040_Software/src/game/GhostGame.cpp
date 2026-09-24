#include "GhostGame.h"

#include "GamePosition.h"
#include "GhostInteraction.h"
#include "GhostTargets.h"
#include "GameBumperConfig.h"
#include "PacmanGame.h"
#include "RouteMotionExecutor.h"

#include "../Globals.h"
#include "../MicromouseLeds.h"
#include "../MicromouseGraphics.h"
#include "../MicromouseMapping.h"
#include "../MicromouseMappingState.h"
#include "../MicromouseMenu.h"   // menuGetUseCurves()
#include "../MicromouseSolving.h"
#include "../UART_L3.h"
#include <Pololu3piPlus2040BumpSensors.h>

extern UART_L3 uartL3;

using namespace Pololu3piPlus2040;

static GAME_ROLES s_role = GAME_ROLES::ROLE_RED;
static bool s_active = false;
static bool s_pacmanKnown = false;
static uint8_t s_pacmanX = 0;
static uint8_t s_pacmanY = 0;
static uint32_t s_lastPacmanSeenMs = 0;
static bool s_routeActive = false;
static bool s_errorWaiting = false;
static bool s_waitingForPeer = false;
static bool s_reservationActive = false;
static uint8_t s_reservationX = 0;
static uint8_t s_reservationY = 0;
static uint8_t s_reservationSequence = 0;
static uint32_t s_lastReservationTxMs = 0;
static bool s_reservationPending = false;
static bool s_reservationCommitted = false;
static bool s_replanRequested = false;
static uint32_t s_reservationReadyMs = 0;
static uint32_t s_nextPeerReplanMs = 0;
static int s_activeTargetX = -1;
static int s_activeTargetY = -1;
static int s_stagedFromX = -1;
static int s_stagedFromY = -1;
static int s_stagedTargetX = -1;
static int s_stagedTargetY = -1;
static int s_lastSentX = -1;
static int s_lastSentY = -1;
static int s_lastSentHeading = -1;
static uint32_t s_lastPosTxMs = 0;
static uint32_t s_lastHudMs = 0;
static BumpSensors s_bumpers;
static bool s_bumpersReady = false;
static bool s_returningHome = false;
static bool s_homeReached = false;
static bool s_bumperConfirmationActive = false;
static uint32_t s_bumperPressedSinceMs = 0;
static uint8_t s_bumperCandidateX = 0;
static uint8_t s_bumperCandidateY = 0;

enum GhostReturnMotionState
{
  GHOST_RETURN_MOTION_IDLE,
  GHOST_RETURN_MOTION_PLANNING,
  GHOST_RETURN_MOTION_DRIVING,
  GHOST_RETURN_MOTION_ALIGNING,
  GHOST_RETURN_MOTION_DONE,
  GHOST_RETURN_MOTION_ERROR
};

static GhostReturnMotionState s_returnMotionState = GHOST_RETURN_MOTION_IDLE;

enum GhostCaptureMotionState
{
  GHOST_CAPTURE_MOTION_IDLE,
  GHOST_CAPTURE_MOTION_RETREATING,
  GHOST_CAPTURE_MOTION_DONE,
  GHOST_CAPTURE_MOTION_ERROR
};

static GhostCaptureMotionState s_captureMotionState =
  GHOST_CAPTURE_MOTION_IDLE;
static const char *s_captureMotionLastError = "";
static int s_captureStartX = 0;
static int s_captureStartY = 0;

static const uint8_t GAME_BROADCAST_ID = 255;
static const uint32_t GHOST_POS_TX_INTERVAL_MS = 400;
static const uint32_t PACMAN_POSITION_TIMEOUT_MS = 2000;
static const uint32_t GHOST_RESERVATION_TX_INTERVAL_MS = 100;
static const uint32_t GHOST_RESERVATION_VETO_MS = 350;
static const uint32_t GHOST_RESERVATION_COMMIT_MS = 150;
static const uint32_t GHOST_PEER_REPLAN_INTERVAL_MS = 250;
static const uint32_t GHOST_HUD_INTERVAL_MS = 150;

static LedColor ghostRoleColor(GAME_ROLES role)
{
  switch (role) {
    case GAME_ROLES::ROLE_RED:   return {51, 0, 0};
    case GAME_ROLES::ROLE_PINK:  return {51, 4, 29};
    case GAME_ROLES::ROLE_CYAN:  return {0, 20, 51};
    case GAME_ROLES::ROLE_BROWN: return {51, 16, 0};
    default:                     return LED_OFF;
  }
}

static void showGhostRoleLeds()
{
  ledSetBumperIndicators(false, false);
  ledSetAll(ghostRoleColor(s_role));
  ledFlushToHardware();
}

static void ghostStartPose(GAME_ROLES role, int &x, int &y, int &heading)
{
  x = startPosX;
  y = startPosY;

  if (role == GAME_ROLES::ROLE_RED)
  {
    heading = SHADOW_START_HEADING;
    return;
  }

  if (role == GAME_ROLES::ROLE_PINK)
  {
    heading = SPEEDY_START_HEADING;
    return;
  }

  heading = EAST;
}

static const char *ghostRoleName()
{
  switch (s_role)
  {
    case GAME_ROLES::ROLE_RED:   return "GEIST ROT";
    case GAME_ROLES::ROLE_PINK:  return "GEIST PINK";
    case GAME_ROLES::ROLE_CYAN:  return "GEIST CYAN";
    case GAME_ROLES::ROLE_BROWN: return "GEIST BRAUN";
    default:                     return "GEIST";
  }
}

static void drawGhostDriveScreen(bool force)
{
  const uint32_t now = millis();
  if (!force &&
      (uint32_t)(now - s_lastHudMs) < GHOST_HUD_INTERVAL_MS)
  {
    return;
  }
  s_lastHudMs = now;

  int nextX;
  int nextY;
  const bool nextKnown = routeMotionNextCell(nextX, nextY);
  char line[24];

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawStr(2, 10, ghostRoleName());
  u8g2.setDrawColor(1);

  snprintf(line, sizeof(line), "Position: %d,%d",
           solveCurrentX(), solveCurrentY());
  u8g2.drawStr(2, 24, line);

  snprintf(line, sizeof(line), "Ausrichtung: %s",
           dirName(solveCurrentHeading()));
  u8g2.drawStr(2, 36, line);

  if (nextKnown)
    snprintf(line, sizeof(line), "Naechste: %d,%d", nextX, nextY);
  else
    snprintf(line, sizeof(line), "Naechste: --");
  u8g2.drawStr(2, 48, line);

  if (inBounds(s_activeTargetX, s_activeTargetY))
    snprintf(line, sizeof(line), "Ziel: %d,%d",
             s_activeTargetX, s_activeTargetY);
  else
    snprintf(line, sizeof(line), "Ziel: --");
  u8g2.drawStr(2, 60, line);
  oledSendBuffer();
}

bool ghostGameInit(GAME_ROLES role)
{
  if (!mapReady)
  {
    Serial.println("Ghost game start failed: no map ready");
    return false;
  }

  int startX;
  int startY;
  int startHeading;
  ghostStartPose(role, startX, startY, startHeading);

  // Der Ghost-Planer liefert vollständige Zellrouten. Der separate
  // RouteMotionExecutor arbeitet diese nicht-blockierend ab.
  // Die Jagd nutzt nicht solveSmoothTurn90 (das gehoert zum Loesen-Loop),
  // daher bleibt SOLVE_USE_SMOOTH_TURNS hier aus. Der fliessende Bogen der
  // Jagd wird stattdessen direkt im Executor aktiviert – gespeist aus der
  // Menue-Einstellung "Kurvenfahrt", konsistent mit Pacman/Loesen.
  setSolveSmoothTurns(false);
  routeMotionSetAllowCurves(menuGetUseCurves());
  buildGraphFromMaze();
  calibrateLineSensors();

  if (!solvingResetToStart(startX, startY, startHeading))
  {
    Serial.println("Ghost game start failed: solver reset failed");
    return false;
  }

  s_role = role;
  s_active = true;
  s_pacmanKnown = false;
  s_lastPacmanSeenMs = 0;
  s_routeActive = false;
  s_errorWaiting = false;
  s_waitingForPeer = false;
  s_reservationActive = false;
  s_reservationSequence = 0;
  s_lastReservationTxMs = 0;
  s_reservationPending = false;
  s_reservationCommitted = false;
  s_replanRequested = false;
  s_reservationReadyMs = 0;
  s_nextPeerReplanMs = 0;
  s_activeTargetX = -1;
  s_activeTargetY = -1;
  s_stagedFromX = -1;
  s_stagedFromY = -1;
  s_stagedTargetX = -1;
  s_stagedTargetY = -1;
  s_lastSentX = -1;
  s_lastSentY = -1;
  s_lastSentHeading = -1;
  s_lastPosTxMs = 0;
  s_lastHudMs = 0;
  routeMotionReset();
  ghostTargetsReset(role);
  ghostInteractionReset((uint8_t)robotId);
  s_bumpers.marginPercentage = GAME_BUMPER_MARGIN_PERCENT;
  s_bumpers.calibrate();
  s_bumpersReady = true;
  s_returningHome = false;
  s_homeReached = false;
  s_captureMotionState = GHOST_CAPTURE_MOTION_IDLE;
  s_returnMotionState = GHOST_RETURN_MOTION_IDLE;
  s_bumperConfirmationActive = false;
  s_bumperPressedSinceMs = 0;
  showGhostRoleLeds();
  currentGameState = GAME_STATE::GS_RUNNING;
  uartL3.sendPosData(GAME_BROADCAST_ID,
                     gamePositionEncodeX((uint8_t)solveCurrentX(), solveCurrentHeading()),
                     gamePositionEncodeY((uint8_t)solveCurrentY(), solveCurrentHeading()));
  drawGhostDriveScreen(true);
  return true;
}

static void sendActiveReservation(bool force)
{
  if (!s_reservationActive)
    return;

  const uint32_t now = millis();
  if (!force &&
      (uint32_t)(now - s_lastReservationTxMs) <
        GHOST_RESERVATION_TX_INTERVAL_MS)
  {
    return;
  }

  const uint8_t payload[] = {
    GC_GHOST_RESERVE_CELL,
    s_reservationX,
    s_reservationY,
    s_reservationSequence,
    s_reservationCommitted
      ? GHOST_RESERVATION_COMMITTED
      : GHOST_RESERVATION_PENDING
  };
  uartL3.sendControlData(GAME_BROADCAST_ID, sizeof(payload), payload);
  s_lastReservationTxMs = now;
}

static void releaseReservation()
{
  if (!s_reservationActive)
    return;

  const uint8_t payload[] = {
    GC_GHOST_RELEASE_CELL,
    s_reservationSequence
  };
  uartL3.sendControlData(GAME_BROADCAST_ID, sizeof(payload), payload);
  s_reservationActive = false;
  s_reservationPending = false;
  s_reservationCommitted = false;
  s_replanRequested = false;
}

static void reserveNextCell(int x, int y, bool waitForVeto)
{
  releaseReservation();
  s_reservationSequence++;
  s_reservationX = (uint8_t)x;
  s_reservationY = (uint8_t)y;
  s_reservationActive = true;
  s_reservationPending = waitForVeto;
  s_reservationCommitted = !waitForVeto;
  s_replanRequested = false;
  s_reservationReadyMs =
    waitForVeto ? millis() + GHOST_RESERVATION_VETO_MS : millis();
  s_lastReservationTxMs = 0;
  sendActiveReservation(true);
}

static void commitReservation(uint32_t now)
{
  s_reservationPending = false;
  s_reservationCommitted = true;
  s_reservationReadyMs = now + GHOST_RESERVATION_COMMIT_MS;
  sendActiveReservation(true);
}

static void sendReplanRequest(uint8_t ownerId, uint8_t sequence)
{
  const uint8_t payload[] = {
    GC_GHOST_REPLAN,
    ownerId,
    sequence
  };
  uartL3.sendControlData(GAME_BROADCAST_ID, sizeof(payload), payload);
}

// Veto-Fenster nur dort noetig, wo es echte Kollisionsgegner geben kann: an
// einer Entscheidungszelle UND wenn ueberhaupt ein Peer aktiv ist. Ohne
// aktiven Peer (Einzel-Geist) committet die Reservierung sofort → kein Halt.
// Sobald ein zweiter Geist aktiv ist, greift wieder das volle Veto.
static bool reservationNeedsVeto(int x, int y)
{
  return solveIsDecisionCell(x, y) &&
         ghostInteractionAnyPeerActive(millis());
}

// Pro Fahrschritt: Haelt der Executor vor Entscheidungszellen an? Nur wenn ein
// Peer aktiv ist. Ohne Peer rollt der Geist fluessig durch die Kreuzung.
// (Spaeter ersetzt durch die vorausschauend-committe Reservierung, siehe TODO.)
static void updateDecisionStopGate()
{
  routeMotionSetStopBeforeDecisionCells(ghostInteractionAnyPeerActive(millis()));
}

static void waitForPeer()
{
  routeMotionAbort();
  releaseReservation();
  s_routeActive = false;
  s_waitingForPeer = true;
  s_nextPeerReplanMs = millis() + GHOST_PEER_REPLAN_INTERVAL_MS;
}

static void sendGhostPosition(bool force)
{
  const int x = solveCurrentX();
  const int y = solveCurrentY();
  const int heading = solveCurrentHeading();
  const uint32_t now = millis();

  if (!force &&
      x == s_lastSentX &&
      y == s_lastSentY &&
      heading == s_lastSentHeading &&
      (uint32_t)(now - s_lastPosTxMs) < GHOST_POS_TX_INTERVAL_MS)
  {
    return;
  }

  s_lastSentX = x;
  s_lastSentY = y;
  s_lastSentHeading = heading;
  s_lastPosTxMs = now;
  uartL3.sendPosData(GAME_BROADCAST_ID,
                     gamePositionEncodeX((uint8_t)x, heading),
                     gamePositionEncodeY((uint8_t)y, heading));
}

// reserveDecisionLookahead: true = vorausschauend die naechste Entscheidungs-
// zelle reservieren (Jagd, passt zur Look-ahead-Logik in ghostGameLoop).
// false = nur die unmittelbar naechste Zelle reservieren (Heimfahrt, deren
// Schritt-Logik in ghostGameReturnHomeLoop genau das erwartet).
static bool startGhostTarget(int targetX, int targetY,
                             bool reserveDecisionLookahead = true)
{
  if (!inBounds(targetX, targetY))
  {
    Serial.print("[Ghost] reject target oob=");
    Serial.print(targetX);
    Serial.print(',');
    Serial.println(targetY);
    return false;
  }

  if (targetX == solveCurrentX() && targetY == solveCurrentY())
  {
    Serial.print("[Ghost] target already reached=");
    Serial.print(targetX);
    Serial.print(',');
    Serial.println(targetY);
    routeMotionAbort();
    s_routeActive = false;
    s_activeTargetX = targetX;
    s_activeTargetY = targetY;
    releaseReservation();
    s_waitingForPeer = false;
    sendGhostPosition(true);
    return true;
  }

  routeMotionAbort();
  releaseReservation();
  s_activeTargetX = targetX;
  s_activeTargetY = targetY;
  s_stagedFromX = -1;
  s_stagedFromY = -1;
  s_stagedTargetX = -1;
  s_stagedTargetY = -1;
  Serial.print("[Ghost] replan pos=");
  Serial.print(solveCurrentX());
  Serial.print(',');
  Serial.print(solveCurrentY());
  Serial.print(" heading=");
  Serial.print(solveCurrentHeading());
  Serial.print(" pacman=");
  Serial.print(s_pacmanX);
  Serial.print(',');
  Serial.print(s_pacmanY);
  Serial.print(" strategicTarget=");
  Serial.print(targetX);
  Serial.print(',');
  Serial.println(targetY);

  const uint32_t now = millis();
  ghostInteractionApplySolverBlocks(now);
  if (s_returningHome && s_pacmanKnown)
    solveBlockCell(s_pacmanX, s_pacmanY);

  if (s_returningHome && s_pacmanKnown &&
      targetX == s_pacmanX && targetY == s_pacmanY)
  {
    Serial.println("[Ghost] home occupied by pacman");
    waitForPeer();
    return false;
  }

  if (ghostInteractionCellOccupied(targetX, targetY, now))
  {
    Serial.println("[Ghost] target occupied by peer");
    waitForPeer();
    return false;
  }

  SolveRoutePlan route = {};
  if (!solveCreateRoutePlan(targetX, targetY, route))
  {
    solveClearDynamicObstacles();
    if (!solveCreateRoutePlan(targetX, targetY, route))
    {
      Serial.println("[Ghost] no reachable path, waiting for new target/map");
      waitForPeer();
      return false;
    }

    Serial.println("[Ghost] path blocked by peer, waiting");
    waitForPeer();
    return false;
  }

  if (!routeMotionLoad(route))
  {
    Serial.println("[Ghost] route executor rejected plan");
    s_routeActive = false;
    return false;
  }

  int nextX;
  int nextY;
  if (!routeMotionNextCell(nextX, nextY))
  {
    s_routeActive = false;
    s_waitingForPeer = false;
    return true;
  }

  // Der Roboter steht beim Routenstart sicher in seiner aktuellen Zelle.
  // Jagd: vorausschauend die naechste Entscheidungszelle reservieren (Veto
  // laeuft sofort ueber die Korridor-Anfahrt). Heimfahrt: nur die unmittelbar
  // naechste Zelle (passt zur Schritt-Logik dort, siehe Parameter-Doku oben).
  int decX;
  int decY;
  if (reserveDecisionLookahead && routeMotionNextDecisionCell(decX, decY))
    reserveNextCell(decX, decY, reservationNeedsVeto(decX, decY));
  else
    reserveNextCell(nextX, nextY, reservationNeedsVeto(nextX, nextY));
  s_routeActive = true;
  s_waitingForPeer = false;
  return true;
}

static void stageRouteUpdateAtNextDecision(int targetX, int targetY)
{
  // Eine laufende Drehung gehoert bereits verbindlich zum aktuellen
  // Zellschritt. Erst danach darf ein Pacman-Update die Folgeroute ersetzen.
  if (navigationDriveCellIsTurning())
    return;

  int nextX;
  int nextY;
  if (!s_routeActive ||
      !routeMotionNextCell(nextX, nextY) ||
      !solveIsDecisionCell(nextX, nextY))
  {
    return;
  }

  if (nextX == s_stagedFromX && nextY == s_stagedFromY &&
      targetX == s_stagedTargetX && targetY == s_stagedTargetY)
  {
    return;
  }

  const uint32_t now = millis();
  ghostInteractionApplySolverBlocks(now);

  SolveRoutePlan route = {};
  if (!solveCreateRoutePlanFrom(nextX, nextY, targetX, targetY, route))
  {
    // Eine Route, die nur ohne Peer-Hindernisse moeglich waere, wird nicht
    // vorgemerkt. Die bestehende Route bleibt bis zur naechsten sicheren
    // Planungsmoeglichkeit aktiv.
    solveClearDynamicObstacles();
    return;
  }

  if (!routeMotionStageAtNextCell(route))
    return;

  s_activeTargetX = targetX;
  s_activeTargetY = targetY;
  s_stagedFromX = nextX;
  s_stagedFromY = nextY;
  s_stagedTargetX = targetX;
  s_stagedTargetY = targetY;

  Serial.print("[Ghost] staged replan at=");
  Serial.print(nextX);
  Serial.print(',');
  Serial.print(nextY);
  Serial.print(" target=");
  Serial.print(targetX);
  Serial.print(',');
  Serial.println(targetY);
}

static bool ghostCanReplan()
{
  return !routeMotionIsActive() && !routeMotionIsDrivingCell();
}

static void enterGhostError(bool wallHit, const char *message = nullptr)
{
  const char *detail = message ? message : solveLastError();
  Serial.print("[GhostError] detail=");
  Serial.print(detail);
  Serial.print(" pose=");
  Serial.print(solveCurrentX());
  Serial.print(',');
  Serial.print(solveCurrentY());
  Serial.print(" heading=");
  Serial.print(solveCurrentHeading());
  Serial.print(" returnState=");
  Serial.print((int)s_returnMotionState);
  Serial.print(" routeError=");
  Serial.print(routeMotionLastError());
  Serial.print(" mappingStatus=");
  Serial.print((int)mappingGetStatus());
  Serial.print(" mappingError=");
  Serial.println(mappingLastError());

  routeMotionAbort();
  releaseReservation();
  s_routeActive = false;
  s_errorWaiting = true;
  currentGameState = GAME_STATE::GS_ERROR;
  ledShowError();
  ledFlushToHardware();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawBox(0, 0, 128, 13);
  u8g2.setDrawColor(0);
  u8g2.drawStr(2, 10, wallHit ? "HINDERNIS!" : "SOLVER-FEHLER");
  u8g2.setDrawColor(1);
  u8g2.drawStr(2, 28, wallHit ? "Fahrt gestoppt" :
                                  detail);

  char position[24];
  snprintf(position, sizeof(position), "Position: %d,%d",
           solveCurrentX(), solveCurrentY());
  u8g2.drawStr(2, 43, position);
  u8g2.drawStr(2, 59, "Taste = Menue");
  oledSendBuffer();
}

bool ghostGameLoop()
{
  if (!s_active)
    return true;

  if (s_errorWaiting)
  {
    uartL3.update();
    if (gameGetPressedA() || gameGetPressedB() || gameGetPressedC())
      return true;
    return false;
  }

  if (gameGetPressedA() || gameGetPressedC())
    return true;

  uartL3.update();
  ghostGameBackgroundUpdate();
  sendGhostPosition(false);
  sendActiveReservation(false);
  drawGhostDriveScreen(false);

  if (!s_pacmanKnown)
    return false;

  int targetX;
  int targetY;
  ghostTargetsChooseTarget(s_role,
                           s_pacmanX, s_pacmanY,
                           solveCurrentX(), solveCurrentY(),
                           targetX, targetY);

  if (!s_routeActive && ghostCanReplan() &&
      (!s_waitingForPeer ||
       (int32_t)(millis() - s_nextPeerReplanMs) >= 0))
  {
    startGhostTarget(targetX, targetY);
  }

  if (s_routeActive)
  {
    stageRouteUpdateAtNextDecision(targetX, targetY);

    int nextX;
    int nextY;
    if (s_reservationActive)
    {
      const uint32_t now = millis();

      // Veto-/Commit-Fortschritt laeuft IMMER weiter – auch waehrend der
      // Korridor-Anfahrt zur vorausschauend reservierten Entscheidungszelle.
      // Geprueft wird die RESERVIERTE Zelle (s_reservation*), nicht die gerade
      // angefahrene Korridorzelle. So ist das Fenster beim Eintreffen meist
      // schon abgelaufen → fluessiges Durchfahren ohne Halt.
      if (s_reservationPending)
      {
        if (s_replanRequested ||
            ghostInteractionCellOccupied(s_reservationX, s_reservationY, now) ||
            ghostInteractionShouldYieldCell(s_reservationX, s_reservationY, now))
        {
          Serial.print("[Ghost] yielding reserved cell=");
          Serial.print(s_reservationX);
          Serial.print(',');
          Serial.println(s_reservationY);
          waitForPeer();
          return false;
        }
        if ((int32_t)(now - s_reservationReadyMs) >= 0)
          commitReservation(now);
      }

      // Ist die reservierte Entscheidungszelle committed und fahren wir gerade
      // die Korridorzelle DAVOR (reservierte Zelle = uebernaechste), den Halt
      // vor ihr aktiv loeschen → durchrollen statt anhalten. Default bleibt
      // "anhalten" (sicherer Fallback), nur bei committeter Freigabe geloescht.
      int folX;
      int folY;
      if (s_reservationCommitted &&
          routeMotionFollowingCell(folX, folY) &&
          folX == (int)s_reservationX && folY == (int)s_reservationY)
      {
        navigationDriveCellSetCenterStop(false);
      }

      // Nur ANHALTEN, wenn die Maus UNMITTELBAR in die reservierte Zelle
      // einfaehrt und diese noch nicht freigegeben ist (Veto laeuft / kurze
      // Ankuendigung). Waehrend der Korridor-Anfahrt (reservierte Zelle ist die
      // uebernaechste) wird NICHT gehalten → freies Weiterrollen.
      if (routeMotionNextCell(nextX, nextY) &&
          nextX == (int)s_reservationX && nextY == (int)s_reservationY)
      {
        if (s_reservationPending)
          return false;
        if (s_reservationCommitted &&
            (int32_t)(now - s_reservationReadyMs) < 0)
          return false;
      }
    }

    // Pro Fahrschritt: ohne aktiven Peer Kreuzungen ohne Halt durchfahren.
    updateDecisionStopGate();
    const RouteMotionStatus motionStatus = routeMotionStep();
    const bool cellReached =
      motionStatus == ROUTE_MOTION_CELL_REACHED ||
      motionStatus == ROUTE_MOTION_DONE;
    sendGhostPosition(cellReached);

    if (motionStatus == ROUTE_MOTION_ERROR)
    {
      enterGhostError(false, "route motion");
      return false;
    }

    if (cellReached && s_reservationActive)
    {
      // VORAUSSCHAU mit MAXIMALEM Vorlauf: die naechste Entscheidungszelle auf
      // der Restroute reservieren (kann mehrere Korridorzellen entfernt sein).
      // So laeuft ihr Veto-Fenster ueber den GESAMTEN Korridor-Abschnitt ab und
      // ist beim Eintreffen sicher committed → Durchfahren ohne Halt.
      int ddx;
      int ddy;
      const bool haveDecision = routeMotionNextDecisionCell(ddx, ddy);

      // Haelt der Slot bereits diese Entscheidungszelle, NICHT freigeben/neu
      // reservieren – sonst wuerde das laufende Veto-Fenster zurueckgesetzt und
      // es entstuende doch wieder ein Halt.
      const bool keepReservation =
        haveDecision && (int)s_reservationX == ddx && (int)s_reservationY == ddy;

      if (!keepReservation)
      {
        releaseReservation();
        if (haveDecision)
        {
          // Entscheidungszelle reservieren (Korridore bis dahin sind per
          // Positionen geschuetzt – heutige Korridor-Reservierungen bieten
          // ohnehin keinen gegenseitigen Ausschluss).
          reserveNextCell(ddx, ddy, reservationNeedsVeto(ddx, ddy));
        }
        else
        {
          // Keine Entscheidungszelle mehr voraus → die unmittelbar naechste
          // Zelle reservieren (Routenende / reiner Korridor).
          int n1x;
          int n1y;
          if (motionStatus != ROUTE_MOTION_DONE &&
              routeMotionNextCell(n1x, n1y))
            reserveNextCell(n1x, n1y, reservationNeedsVeto(n1x, n1y));
        }
      }
    }

    if (cellReached)
    {
      s_stagedFromX = -1;
      s_stagedFromY = -1;
      s_stagedTargetX = -1;
      s_stagedTargetY = -1;
    }

    if (motionStatus == ROUTE_MOTION_DONE)
    {
      releaseReservation();
      Serial.print("[Ghost] route reached pos=");
      Serial.print(solveCurrentX());
      Serial.print(',');
      Serial.print(solveCurrentY());
      Serial.print(" heading=");
      Serial.println(solveCurrentHeading());
      s_routeActive = false;
    }
  }

  return false;
}

void ghostGameLeave()
{
  routeMotionAbort();
  releaseReservation();
  s_active = false;
  s_pacmanKnown = false;
  s_lastPacmanSeenMs = 0;
  s_bumperConfirmationActive = false;
  s_bumperPressedSinceMs = 0;
  ledSetBumperIndicators(false, false);
  s_routeActive = false;
  s_errorWaiting = false;
  s_waitingForPeer = false;
  s_bumpersReady = false;
  s_returningHome = false;
  s_homeReached = false;
  s_captureMotionState = GHOST_CAPTURE_MOTION_IDLE;
  s_returnMotionState = GHOST_RETURN_MOTION_IDLE;
}

void ghostGameBeginCapture()
{
  showGhostRoleLeds();
  s_captureStartX = solveCurrentX();
  s_captureStartY = solveCurrentY();
  // Das Fang-Event setzt g_abortRequested, um die aktuelle Zellfahrt sofort
  // zu stoppen. Beide globalen Abbruchflags muessen vor dem Retreat-Step
  // geloescht sein, sonst endet die Rueckfahrt vor dem ersten Motorbefehl.
  g_abortRequested = false;
  g_btnAPressed = false;
  const NavigationRetreatBeginStatus retreatStatus =
    navigationCellRetreatBegin();
  routeMotionAbort();
  releaseReservation();
  stopMotors();
  s_routeActive = false;
  s_waitingForPeer = false;
  s_replanRequested = false;
  s_returningHome = false;
  s_homeReached = false;
  s_returnMotionState = GHOST_RETURN_MOTION_IDLE;
  if (retreatStatus == NAV_RETREAT_BEGIN_STARTED)
    s_captureMotionState = GHOST_CAPTURE_MOTION_RETREATING;
  else if (retreatStatus == NAV_RETREAT_BEGIN_STATIONARY)
  {
    solveSetPose(s_captureStartX, s_captureStartY, mappingGetHeading());
    s_captureMotionState = GHOST_CAPTURE_MOTION_DONE;
  }
  else
  {
    s_captureMotionState = GHOST_CAPTURE_MOTION_ERROR;
    s_captureMotionLastError = navigationCellRetreatLastError();
    enterGhostError(false, s_captureMotionLastError);
  }
}

bool ghostGameCaptureLoop()
{
  if (s_captureMotionState == GHOST_CAPTURE_MOTION_DONE ||
      s_captureMotionState == GHOST_CAPTURE_MOTION_IDLE)
  {
    return true;
  }

  if (s_captureMotionState == GHOST_CAPTURE_MOTION_ERROR)
    return false;

  const LineFollowRunStatus status = navigationCellRetreatStep();
  if (status == LINE_FOLLOW_RUN_RUNNING)
    return false;
  if (status != LINE_FOLLOW_RUN_DONE)
  {
    stopMotors();
    s_captureMotionState = GHOST_CAPTURE_MOTION_ERROR;
    const char *reason = navigationCellRetreatLastError();
    s_captureMotionLastError =
      reason[0] != '\0' ? reason : "capture retreat";
    enterGhostError(false, reason[0] != '\0' ? reason : "capture retreat");
    return false;
  }

  solveSetPose(s_captureStartX, s_captureStartY, mappingGetHeading());
  stopMotors();
  sendGhostPosition(true);
  s_captureMotionState = GHOST_CAPTURE_MOTION_DONE;
  return true;
}

bool ghostGameCaptureFailed()
{
  return s_captureMotionState == GHOST_CAPTURE_MOTION_ERROR;
}

const char *ghostGameCaptureLastError()
{
  return s_captureMotionLastError;
}

void ghostGamePrepareForHoming()
{
  routeMotionAbort();
  releaseReservation();
  stopMotors();
  s_routeActive = false;
  s_waitingForPeer = false;
  s_replanRequested = false;
  s_returningHome = false;
  s_homeReached = false;
  s_returnMotionState = GHOST_RETURN_MOTION_IDLE;
}

void ghostGameBeginReturnHome()
{
  ghostGamePrepareForHoming();
  showGhostRoleLeds();
  s_returningHome = true;
  s_nextPeerReplanMs = 0;
  s_returnMotionState = GHOST_RETURN_MOTION_PLANNING;
  currentGameState = GAME_STATE::GS_RETURNING_TO_HOME;
  Serial.print("[GhostHome] begin pose=");
  Serial.print(solveCurrentX());
  Serial.print(',');
  Serial.print(solveCurrentY());
  Serial.print(" heading=");
  Serial.println(solveCurrentHeading());
}

bool ghostGameReturnHomeLoop()
{
  if (!s_active || !s_returningHome)
    return false;
  if (s_homeReached)
    return true;

  uartL3.update();
  sendGhostPosition(false);
  sendActiveReservation(false);
  drawGhostDriveScreen(false);

  if (s_returnMotionState == GHOST_RETURN_MOTION_ERROR)
    return false;

  int homeX;
  int homeY;
  int homeHeading;
  ghostStartPose(s_role, homeX, homeY, homeHeading);

  if (s_returnMotionState == GHOST_RETURN_MOTION_ALIGNING)
  {
    const TurnRunStatus status = turnStep();
    if (status == TURN_RUN_RUNNING)
      return false;
    if (status != TURN_RUN_DONE)
    {
      s_returnMotionState = GHOST_RETURN_MOTION_ERROR;
      enterGhostError(false, "home align");
      return false;
    }

    solveSetPose(homeX, homeY, homeHeading);
    mappingSetPose(homeX, homeY, homeHeading);
    sendGhostPosition(true);
    s_homeReached = true;
    s_returnMotionState = GHOST_RETURN_MOTION_DONE;
    return true;
  }

  if (s_returnMotionState == GHOST_RETURN_MOTION_PLANNING)
  {
    if (solveCurrentX() == homeX && solveCurrentY() == homeY)
    {
      mappingSetPose(homeX, homeY, solveCurrentHeading());
      turnBegin(homeHeading);
      if (turnIsActive())
      {
        s_returnMotionState = GHOST_RETURN_MOTION_ALIGNING;
        return false;
      }

      solveSetPose(homeX, homeY, homeHeading);
      sendGhostPosition(true);
      s_homeReached = true;
      s_returnMotionState = GHOST_RETURN_MOTION_DONE;
      return true;
    }

    if (!ghostCanReplan() ||
        (s_waitingForPeer &&
         (int32_t)(millis() - s_nextPeerReplanMs) < 0))
    {
      return false;
    }

    // Heimfahrt reserviert nur die unmittelbar naechste Zelle (kein Look-ahead),
    // damit die Reservierung zur Schritt-Logik dieser Schleife passt.
    startGhostTarget(homeX, homeY, false);
    if (s_routeActive)
      s_returnMotionState = GHOST_RETURN_MOTION_DRIVING;
    return false;
  }

  int nextX;
  int nextY;
  if (s_reservationActive && routeMotionNextCell(nextX, nextY))
  {
    const uint32_t now = millis();
    if (s_reservationPending &&
        (s_replanRequested ||
         ghostInteractionCellOccupied(nextX, nextY, now) ||
         ghostInteractionShouldYieldCell(nextX, nextY, now)))
    {
      waitForPeer();
      s_returnMotionState = GHOST_RETURN_MOTION_PLANNING;
      return false;
    }

    if (s_reservationPending)
    {
      if ((int32_t)(now - s_reservationReadyMs) < 0)
        return false;
      commitReservation(now);
      return false;
    }

    if (s_reservationCommitted &&
        (int32_t)(now - s_reservationReadyMs) < 0)
    {
      return false;
    }
  }

  updateDecisionStopGate();
  const RouteMotionStatus motionStatus = routeMotionStep();
  const bool cellReached =
    motionStatus == ROUTE_MOTION_CELL_REACHED ||
    motionStatus == ROUTE_MOTION_DONE;
  sendGhostPosition(cellReached);

  if (motionStatus == ROUTE_MOTION_ERROR)
  {
    Serial.print("[GhostHome] route error reason=");
    Serial.print(routeMotionLastError());
    Serial.print(" pose=");
    Serial.print(solveCurrentX());
    Serial.print(',');
    Serial.print(solveCurrentY());
    Serial.print(" heading=");
    Serial.println(solveCurrentHeading());
    waitForPeer();
    s_returnMotionState = GHOST_RETURN_MOTION_PLANNING;
    return false;
  }

  if (cellReached && s_reservationActive)
  {
    releaseReservation();

    int followingX;
    int followingY;
    if (motionStatus != ROUTE_MOTION_DONE &&
        routeMotionNextCell(followingX, followingY))
    {
      reserveNextCell(followingX, followingY,
                      reservationNeedsVeto(followingX, followingY));
    }
  }

  if (motionStatus == ROUTE_MOTION_DONE)
  {
    releaseReservation();
    s_routeActive = false;
    mappingSetPose(homeX, homeY, solveCurrentHeading());
    turnBegin(homeHeading);
    if (turnIsActive())
    {
      s_returnMotionState = GHOST_RETURN_MOTION_ALIGNING;
      return false;
    }

    solveSetPose(homeX, homeY, homeHeading);
    sendGhostPosition(true);
    s_homeReached = true;
    s_returnMotionState = GHOST_RETURN_MOTION_DONE;
    return true;
  }

  return false;
}

void ghostGameStartNextRound()
{
  routeMotionAbort();
  releaseReservation();
  s_routeActive = false;
  s_waitingForPeer = false;
  s_replanRequested = false;
  s_returningHome = false;
  s_homeReached = false;
  s_captureMotionState = GHOST_CAPTURE_MOTION_IDLE;
  s_captureMotionLastError = "";
  s_returnMotionState = GHOST_RETURN_MOTION_IDLE;
  s_bumperConfirmationActive = false;
  s_bumperPressedSinceMs = 0;
  s_errorWaiting = false;
  ghostTargetsReset(s_role);
  ghostInteractionReset((uint8_t)robotId);
  currentGameState = GAME_STATE::GS_RUNNING;
  showGhostRoleLeds();
  sendGhostPosition(true);
  drawGhostDriveScreen(true);
}

void ghostGameResetAfterTimeout()
{
  routeMotionAbort();
  releaseReservation();
  navigationCellRetreatAbort();
  stopMotors();

  int homeX;
  int homeY;
  int homeHeading;
  ghostStartPose(s_role, homeX, homeY, homeHeading);
  solveSetPose(homeX, homeY, homeHeading);
  mappingSetPose(homeX, homeY, homeHeading);

  s_routeActive = false;
  s_waitingForPeer = false;
  s_replanRequested = false;
  s_returningHome = false;
  s_homeReached = true;
  s_captureMotionState = GHOST_CAPTURE_MOTION_IDLE;
  s_captureMotionLastError = "";
  s_returnMotionState = GHOST_RETURN_MOTION_IDLE;
  ghostTargetsReset(s_role);
  ghostInteractionReset((uint8_t)robotId);
  sendGhostPosition(true);
}

void ghostGameManualHomeReset()
{
  routeMotionAbort();
  releaseReservation();
  navigationDriveCellAbort();
  navigationCellRetreatAbort();
  stopMotors();

  int homeX;
  int homeY;
  int homeHeading;
  ghostStartPose(s_role, homeX, homeY, homeHeading);
  solveSetPose(homeX, homeY, homeHeading);
  mappingSetPose(homeX, homeY, homeHeading);

  s_pacmanKnown = false;
  s_lastPacmanSeenMs = 0;
  s_routeActive = false;
  s_errorWaiting = false;
  s_waitingForPeer = false;
  s_reservationActive = false;
  s_reservationPending = false;
  s_reservationCommitted = false;
  s_replanRequested = false;
  s_activeTargetX = -1;
  s_activeTargetY = -1;
  s_stagedFromX = -1;
  s_stagedFromY = -1;
  s_stagedTargetX = -1;
  s_stagedTargetY = -1;
  s_returningHome = false;
  s_homeReached = true;
  s_captureMotionState = GHOST_CAPTURE_MOTION_IDLE;
  s_captureMotionLastError = "";
  s_returnMotionState = GHOST_RETURN_MOTION_IDLE;
  s_bumperConfirmationActive = false;
  s_bumperPressedSinceMs = 0;
  ghostTargetsReset(s_role);
  ghostInteractionReset((uint8_t)robotId);
  ledSetBumperIndicators(false, false);
  ledSetAll(LED_WHITE);
  ledFlushToHardware();
  sendGhostPosition(true);
  drawGhostDriveScreen(true);
}

void ghostGameBackgroundUpdate()
{
  if (!s_active)
  {
    ledSetBumperIndicators(false, false);
    return;
  }

  sendGhostPosition(false);
  sendActiveReservation(false);

  const uint32_t now = millis();
  if (!s_bumpersReady || currentGameState != GAME_STATE::GS_RUNNING)
  {
    ledSetBumperIndicators(false, false);
    s_bumperConfirmationActive = false;
    return;
  }

  s_bumpers.read();
  const bool leftBumperPressed = s_bumpers.leftIsPressed();
  const bool rightBumperPressed = s_bumpers.rightIsPressed();
  const bool bumperPressed = leftBumperPressed || rightBumperPressed;
  ledSetBumperIndicators(leftBumperPressed, rightBumperPressed);

  const bool pacmanPositionFresh =
    s_pacmanKnown &&
    (uint32_t)(now - s_lastPacmanSeenMs) <= PACMAN_POSITION_TIMEOUT_MS;

  if (!pacmanPositionFresh)
  {
    s_bumperConfirmationActive = false;
    return;
  }

  int physicalX = solveCurrentX();
  int physicalY = solveCurrentY();
  int nextX;
  int nextY;
  const bool drivingIntoNextCell =
    routeMotionIsDrivingCell() &&
    !navigationDriveCellIsTurning() &&
    routeMotionNextCell(nextX, nextY);

  const bool coordinatesMatch =
    drivingIntoNextCell
      ? (nextX == s_pacmanX && nextY == s_pacmanY)
      : (physicalX == s_pacmanX && physicalY == s_pacmanY);

  if (!coordinatesMatch)
  {
    s_bumperConfirmationActive = false;
    return;
  }

  if (!bumperPressed)
  {
    s_bumperConfirmationActive = false;
    return;
  }

  const uint8_t captureX =
    (uint8_t)(drivingIntoNextCell ? nextX : physicalX);
  const uint8_t captureY =
    (uint8_t)(drivingIntoNextCell ? nextY : physicalY);

  if (!s_bumperConfirmationActive ||
      captureX != s_bumperCandidateX ||
      captureY != s_bumperCandidateY)
  {
    s_bumperConfirmationActive = true;
    s_bumperPressedSinceMs = now;
    s_bumperCandidateX = captureX;
    s_bumperCandidateY = captureY;
    return;
  }

  if ((uint32_t)(now - s_bumperPressedSinceMs) < GAME_BUMPER_CONFIRM_MS)
    return;

  s_bumperConfirmationActive = false;
  Serial.print("[Capture] confirmed bumper+coordinates ghost=");
  Serial.print((uint8_t)robotId);
  Serial.print(" cell=");
  Serial.print(captureX);
  Serial.print(',');
  Serial.println(captureY);

  gameReportCapture((uint8_t)robotId, captureX, captureY);
}

void ghostGameBroadcastPosition()
{
  if (!s_active)
    return;

  sendGhostPosition(false);
}

void ghostGameObservePacman(uint8_t senderId, uint8_t x, uint8_t y, int heading)
{
  if (!s_active || !inBounds(x, y))
    return;

  if (getRoleFromId(senderId) != GAME_ROLES::ROLE_PACMAN)
    return;

  const bool changed = !s_pacmanKnown || x != s_pacmanX || y != s_pacmanY;
  s_pacmanX = x;
  s_pacmanY = y;
  s_pacmanKnown = true;
  s_lastPacmanSeenMs = millis();
  if (currentGameState != GAME_STATE::GS_RUNNING)
    ghostInteractionObservePosition(senderId, x, y, heading, s_lastPacmanSeenMs);
  if (changed)
  {
    Serial.print("[Ghost] pacman sender=");
    Serial.print(senderId);
    Serial.print(" pos=");
    Serial.print(x);
    Serial.print(',');
    Serial.println(y);
  }
  ghostTargetsObservePacman(s_role, x, y, heading);
}

void ghostGameObservePeer(uint8_t senderId, uint8_t x, uint8_t y, int heading)
{
  if (!s_active || getRoleFromId(senderId) == GAME_ROLES::ROLE_PACMAN)
    return;

  ghostInteractionObservePosition(senderId, x, y, heading, millis());
}

void ghostGameObserveReservation(uint8_t senderId,
                                 uint8_t x, uint8_t y,
                                 uint8_t sequence,
                                 bool committed)
{
  if (!s_active || getRoleFromId(senderId) == GAME_ROLES::ROLE_PACMAN)
    return;

  if (!ghostInteractionObserveReservation(
        senderId, x, y, sequence, committed, millis()))
  {
    return;
  }

  const bool peerWantsOccupiedCell =
    x == solveCurrentX() && y == solveCurrentY();
  const bool sameReservedCell =
    s_reservationActive &&
    x == s_reservationX && y == s_reservationY;

  if (peerWantsOccupiedCell)
  {
    sendReplanRequest(senderId, sequence);
    return;
  }

  if (!sameReservedCell)
    return;

  if (s_reservationCommitted)
  {
    // Waehrend der Commit-Ankuendigung stehen beide Ghosts noch sicher. Falls
    // beide gleichzeitig committen, entscheidet dort weiterhin die
    // Prioritaet. Nach Beginn der Zellfahrt ist die Einfahrt unverdrängbar.
    if (!routeMotionIsDrivingCell() &&
        committed &&
        !ghostInteractionOwnHasPriority(senderId))
    {
      s_replanRequested = true;
      s_reservationPending = true;
      return;
    }

    sendReplanRequest(senderId, sequence);
    return;
  }

  if (committed || !ghostInteractionOwnHasPriority(senderId))
  {
    s_replanRequested = true;
    return;
  }

  sendReplanRequest(senderId, sequence);
}

void ghostGameObserveReservationRelease(uint8_t senderId, uint8_t sequence)
{
  if (!s_active || getRoleFromId(senderId) == GAME_ROLES::ROLE_PACMAN)
    return;

  ghostInteractionObserveRelease(senderId, sequence, millis());
}

void ghostGameObserveReplan(uint8_t senderId,
                            uint8_t ownerId,
                            uint8_t sequence)
{
  if (!s_active ||
      getRoleFromId(senderId) == GAME_ROLES::ROLE_PACMAN ||
      ownerId != (uint8_t)robotId ||
      !s_reservationActive ||
      sequence != s_reservationSequence)
  {
    return;
  }

  // Ein priorisierter Peer darf waehrend der Commit-Ankuendigung noch einen
  // simultanen Commit-Konflikt aufloesen. Nach Motorstart bleibt die Einfahrt
  // dagegen unverdrängbar.
  if (s_reservationPending ||
      (s_reservationCommitted &&
       !routeMotionIsDrivingCell() &&
       !ghostInteractionOwnHasPriority(senderId)))
  {
    s_replanRequested = true;
    s_reservationPending = true;
  }
}
