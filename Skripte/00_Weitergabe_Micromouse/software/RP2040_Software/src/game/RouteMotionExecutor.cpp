#include "RouteMotionExecutor.h"

#include "../MicromouseMapping.h"
#include "../MicromouseMappingState.h"

static SolveRoutePlan s_plan = {};
static SolveRoutePlan s_pendingPlan = {};
static uint8_t s_step = 0;
static bool s_active = false;
static bool s_driving = false;
static bool s_pendingValid = false;
static int s_activeLegDirection = -1;
static int s_pendingIncomingDirection = -1;
static bool s_allowCurves = false;
// PHASE B – Kopplung an die Reservierungslogik: Wenn false, erzwingt eine
// Entscheidungszelle keinen Halt mehr (der Aufrufer/GhostGame stellt dann
// sicher, dass die Einfahrt anderweitig freigegeben ist – z.B. kein aktiver
// Peer, oder spaeter: vorausschauend committe Reservierung). Default true =
// bisheriges sicheres Verhalten (Halt + Veto vor jeder Kreuzung).
static bool s_stopBeforeDecisionCells = true;
static const char *s_lastError = "";

const char *routeMotionLastError()
{
  return s_lastError;
}

static int directionBetween(uint8_t fromX, uint8_t fromY,
                            uint8_t toX, uint8_t toY)
{
  const int dx = (int)toX - (int)fromX;
  const int dy = (int)toY - (int)fromY;
  if (dx == 1 && dy == 0) return EAST;
  if (dx == -1 && dy == 0) return WEST;
  if (dx == 0 && dy == 1) return NORTH;
  if (dx == 0 && dy == -1) return SOUTH;
  return -1;
}

void routeMotionSetAllowCurves(bool enable)
{
  s_allowCurves = enable;
  // Kurve → vordere 3 Sensoren, Punkt → 4 von 5 Sensoren.
  setIntersectionUseFront3(enable);
}

void routeMotionSetStopBeforeDecisionCells(bool enable)
{
  s_stopBeforeDecisionCells = enable;
}

// Geradeaus (alte Richtung) frei? Spiegelt PacmanGame::canMove: erlaubt, wenn
// die Wand unbekannt oder bekannt offen ist. Nur dann hat der vorwaerts
// schwingende Bogen Platz; an L-Ecken (Wand voraus) bleibt es bei der
// Punktdrehung.
static bool cellForwardClear(int x, int y, int dir)
{
  const int nx = x + dxDir(dir);
  const int ny = y + dyDir(dir);
  return inBounds(nx, ny) &&
         !(maze[x][y].known[dir] && maze[x][y].wall[dir]);
}

// PHASE A – Fliessende Abbiegungen: Eine Abbiegung an (turnCellX/Y) zwingt nur
// dann zum Halt, wenn sie NICHT als rollender 90°-Bogen (Pivot) gefahren wird.
// Bogen-Bedingung: Boege aktiv, 90°, und an der Abbiegezelle geradeaus (alte
// Richtung) frei (sonst feuert der Pivot nicht → Punktdrehung → Halt). 180°-
// Wenden und unbekannte Folgerichtung halten weiterhin an.
static bool turnRequiresStop(int turnCellX, int turnCellY,
                             int incomingDir, int outgoingDir)
{
  if (outgoingDir == incomingDir)
    return false;            // keine Abbiegung → kein Halt noetig
  if (outgoingDir < 0)
    return true;             // unbekannte Folge → sicher anhalten
  const int diff = ((outgoingDir - incomingDir) % 4 + 4) % 4;
  const bool is90 = (diff == 1 || diff == 3);
  const bool rollThrough =
    is90 && s_allowCurves &&
    cellForwardClear(turnCellX, turnCellY, incomingDir);
  return !rollThrough;
}

static bool pendingPlanRequiresStop(int incomingDirection)
{
  const int outgoingDirection =
    s_pendingPlan.length > 1
      ? directionBetween(s_pendingPlan.x[0], s_pendingPlan.y[0],
                         s_pendingPlan.x[1], s_pendingPlan.y[1])
      : -1;

  // Abbiegung an der naechsten Zelle: nur halten, wenn kein rollender Bogen.
  if (turnRequiresStop(s_pendingPlan.x[0], s_pendingPlan.y[0],
                       incomingDirection, outgoingDirection))
    return true;

  // Vor der folgenden Entscheidungszelle muss GhostGame im Stillstand eine
  // prioritaetsbasierte Reservierungsfreigabe abwarten koennen.
  return s_stopBeforeDecisionCells && s_pendingPlan.length > 1 &&
         solveIsDecisionCell(s_pendingPlan.x[1], s_pendingPlan.y[1]);
}

void routeMotionReset()
{
  routeMotionAbort();
  s_plan = {};
  s_pendingPlan = {};
  s_step = 0;
  s_pendingValid = false;
  s_activeLegDirection = -1;
  s_pendingIncomingDirection = -1;
  s_lastError = "";
}

static bool routePlanValid(const SolveRoutePlan &plan)
{
  if (plan.length < 1 || plan.length > SOLVE_ROUTE_MAX_CELLS)
    return false;

  for (uint8_t i = 1; i < plan.length; i++)
  {
    if (directionBetween(plan.x[i - 1], plan.y[i - 1],
                         plan.x[i], plan.y[i]) < 0)
    {
      return false;
    }
  }
  return true;
}

bool routeMotionLoad(const SolveRoutePlan &plan)
{
  routeMotionReset();
  if (!routePlanValid(plan))
  {
    s_lastError = "invalid route";
    return false;
  }
  if (plan.x[0] != solveCurrentX() || plan.y[0] != solveCurrentY())
  {
    s_lastError = "route pose mismatch";
    return false;
  }
  s_plan = plan;
  s_step = 0;
  s_active = plan.length > 1;
  return true;
}

bool routeMotionStageAtNextCell(const SolveRoutePlan &plan)
{
  // Waehrend einer Drehung zeigt der globale Heading noch in die alte
  // Richtung. Der Aufrufer versucht das Staging nach Drehende erneut.
  if (navigationDriveCellIsTurning())
    return false;

  int nextX;
  int nextY;
  if (!routeMotionNextCell(nextX, nextY) || !routePlanValid(plan))
    return false;
  if (plan.x[0] != nextX || plan.y[0] != nextY)
    return false;

  s_pendingPlan = plan;
  s_pendingValid = true;

  const int incomingDirection =
    s_activeLegDirection >= 0
      ? s_activeLegDirection
      : directionBetween(s_plan.x[s_step], s_plan.y[s_step],
                         s_plan.x[s_step + 1], s_plan.y[s_step + 1]);
  s_pendingIncomingDirection = incomingDirection;
  navigationDriveCellSetCenterStop(
    pendingPlanRequiresStop(incomingDirection));
  return true;
}

bool routeMotionNextCell(int &x, int &y)
{
  if (!s_active || s_step + 1 >= s_plan.length)
    return false;
  x = s_plan.x[s_step + 1];
  y = s_plan.y[s_step + 1];
  return true;
}

bool routeMotionFollowingCell(int &x, int &y)
{
  if (!s_active || s_step + 2 >= s_plan.length)
    return false;
  x = s_plan.x[s_step + 2];
  y = s_plan.y[s_step + 2];
  return true;
}

// Naechste Entscheidungszelle (Knotengrad >= 3) auf der Restroute, ab der
// unmittelbar naechsten Zelle. Liefert sie zurueck, sodass GhostGame sie
// vorausschauend reservieren kann – das Veto-Fenster laeuft dann ueber den
// GESAMTEN Korridor-Abschnitt davor ab (mehrere Zellen Vorlauf statt nur eine).
// Die Korridorzellen bis dahin sind per Definition keine Entscheidungszellen.
bool routeMotionNextDecisionCell(int &x, int &y)
{
  if (!s_active)
    return false;
  for (uint8_t i = s_step + 1; i < s_plan.length; i++)
  {
    if (solveIsDecisionCell(s_plan.x[i], s_plan.y[i]))
    {
      x = s_plan.x[i];
      y = s_plan.y[i];
      return true;
    }
  }
  return false;
}

RouteMotionStatus routeMotionStep()
{
  if (!s_active)
    return ROUTE_MOTION_IDLE;

  if (!s_driving)
  {
    if (s_step + 1 >= s_plan.length)
    {
      s_active = false;
      return ROUTE_MOTION_DONE;
    }

    const int direction =
      directionBetween(s_plan.x[s_step], s_plan.y[s_step],
                       s_plan.x[s_step + 1], s_plan.y[s_step + 1]);
    if (direction < 0)
    {
      s_lastError = "invalid route step";
      Serial.print("[RouteMotion] invalid step index=");
      Serial.print(s_step);
      Serial.print(" from=");
      Serial.print(s_plan.x[s_step]);
      Serial.print(',');
      Serial.print(s_plan.y[s_step]);
      Serial.print(" to=");
      Serial.print(s_plan.x[s_step + 1]);
      Serial.print(',');
      Serial.println(s_plan.y[s_step + 1]);
      s_active = false;
      return ROUTE_MOTION_ERROR;
    }

    bool centerAndStop;
    if (s_pendingValid)
    {
      centerAndStop = pendingPlanRequiresStop(direction);
    }
    else
    {
      centerAndStop = s_step + 2 >= s_plan.length;
      if (!centerAndStop)
      {
        const int followingDirection =
          directionBetween(s_plan.x[s_step + 1], s_plan.y[s_step + 1],
                           s_plan.x[s_step + 2], s_plan.y[s_step + 2]);
        // PHASE A: Abbiegung an s_step+1 nur anhalten, wenn kein rollender
        // Bogen. PHASE B (offen) wird den Halt vor Entscheidungszellen ueber
        // vorausschauende Reservierung aufloesen.
        centerAndStop =
          turnRequiresStop(s_plan.x[s_step + 1], s_plan.y[s_step + 1],
                           direction, followingDirection) ||
          (s_stopBeforeDecisionCells &&
           solveIsDecisionCell(s_plan.x[s_step + 2],
                               s_plan.y[s_step + 2]));
      }
    }

    // Fliessender Bogen bei 90°-Abbiegung, sofern aktiviert und geradeaus
    // (alte Richtung = globaler heading) frei ist. Das Flag ist EINMALIG;
    // navigationDriveCellBegin prueft diff==1||3 selbst, daher wird bei
    // Geradeausfahrt/180° automatisch die Punktdrehung gefahren.
    navigationDriveCellSetAllowCurve(
      s_allowCurves &&
      cellForwardClear(s_plan.x[s_step], s_plan.y[s_step], heading));
    navigationDriveCellBegin(direction, centerAndStop);
    s_activeLegDirection = direction;
    s_driving = true;
  }

  const LineFollowRunStatus status = navigationDriveCellStep();
  if (status == LINE_FOLLOW_RUN_RUNNING)
    return ROUTE_MOTION_RUNNING;

  if (status != LINE_FOLLOW_RUN_DONE)
  {
    s_driving = false;
    s_activeLegDirection = -1;
    s_lastError =
      status == LINE_FOLLOW_RUN_ABORTED
        ? "navigation aborted"
        : (mappingGetStatus() == MAPPING_RUN_ERROR
            ? mappingLastError()
            : "navigation error");
    Serial.print("[RouteMotion] drive failed status=");
    Serial.print((int)status);
    Serial.print(" step=");
    Serial.print(s_step);
    Serial.print(" from=");
    Serial.print(s_plan.x[s_step]);
    Serial.print(',');
    Serial.print(s_plan.y[s_step]);
    Serial.print(" to=");
    Serial.print(s_plan.x[s_step + 1]);
    Serial.print(',');
    Serial.print(s_plan.y[s_step + 1]);
    Serial.print(" heading=");
    Serial.print(mappingGetHeading());
    Serial.print(" mappingStatus=");
    Serial.print((int)mappingGetStatus());
    Serial.print(" reason=");
    Serial.println(s_lastError);
    s_active = false;
    return ROUTE_MOTION_ERROR;
  }

  const int completedDirection = s_activeLegDirection;
  s_driving = false;
  s_activeLegDirection = -1;
  s_step++;
  if (completedDirection < 0)
  {
    s_lastError = "missing leg direction";
    s_active = false;
    return ROUTE_MOTION_ERROR;
  }
  solveSetPose(s_plan.x[s_step], s_plan.y[s_step], completedDirection);

  if (s_pendingValid)
  {
    const bool pendingMatchesArrival =
      s_pendingIncomingDirection == completedDirection &&
      s_pendingPlan.x[0] == s_plan.x[s_step] &&
      s_pendingPlan.y[0] == s_plan.y[s_step];
    if (pendingMatchesArrival)
    {
      Serial.print("[RouteMotion] apply staged route at=");
      Serial.print(s_plan.x[s_step]);
      Serial.print(',');
      Serial.print(s_plan.y[s_step]);
      Serial.print(" heading=");
      Serial.println(completedDirection);
      s_plan = s_pendingPlan;
      s_step = 0;
    }
    else
    {
      Serial.print("[RouteMotion] discard staged route arrivalHeading=");
      Serial.print(completedDirection);
      Serial.print(" expected=");
      Serial.println(s_pendingIncomingDirection);
    }
    s_pendingPlan = {};
    s_pendingValid = false;
    s_pendingIncomingDirection = -1;
  }

  if (s_step + 1 >= s_plan.length)
  {
    s_active = false;
    return ROUTE_MOTION_DONE;
  }
  return ROUTE_MOTION_CELL_REACHED;
}

void routeMotionAbort()
{
  navigationDriveCellAbort();
  s_pendingPlan = {};
  s_active = false;
  s_driving = false;
  s_pendingValid = false;
  s_activeLegDirection = -1;
  s_pendingIncomingDirection = -1;
}

bool routeMotionIsActive()
{
  return s_active;
}

bool routeMotionIsDrivingCell()
{
  return s_driving;
}
