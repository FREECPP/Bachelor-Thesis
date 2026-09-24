#ifndef GAME_ROUTE_MOTION_EXECUTOR_H
#define GAME_ROUTE_MOTION_EXECUTOR_H

#include "../MicromouseSolving.h"

enum RouteMotionStatus
{
  ROUTE_MOTION_IDLE,
  ROUTE_MOTION_RUNNING,
  ROUTE_MOTION_CELL_REACHED,
  ROUTE_MOTION_DONE,
  ROUTE_MOTION_ERROR
};

void routeMotionReset();
// Aktiviert fliessende 90°-Boegen (Pivot) statt Punktdrehung fuer die
// Jagd-Fahrt. Default false (Punktdrehung), damit Bestandsverhalten erhalten
// bleibt. Wird aus der Menue-Einstellung "Kurvenfahrt" gespeist.
void routeMotionSetAllowCurves(bool enable);
// PHASE B – Halt vor Entscheidungszellen ein-/ausschalten. Default true =
// bisheriges Verhalten (Halt + Veto). Auf false setzen, wenn der Aufrufer die
// Einfahrt anderweitig absichert (Einzel-Geist-Schnellpfad; spaeter:
// vorausschauend committe Reservierung). Pro Fahrschritt setzen.
void routeMotionSetStopBeforeDecisionCells(bool enable);
bool routeMotionLoad(const SolveRoutePlan &plan);
bool routeMotionStageAtNextCell(const SolveRoutePlan &plan);
RouteMotionStatus routeMotionStep();
const char *routeMotionLastError();
void routeMotionAbort();
bool routeMotionIsActive();
bool routeMotionIsDrivingCell();
bool routeMotionNextCell(int &x, int &y);
bool routeMotionFollowingCell(int &x, int &y);
// Naechste Entscheidungszelle (Knotengrad >= 3) auf der Restroute. Fuer die
// vorausschauende Reservierung: Veto-Fenster laeuft ueber den ganzen Korridor-
// Abschnitt davor ab, sodass die Maus die Kreuzung ohne Halt durchfaehrt.
bool routeMotionNextDecisionCell(int &x, int &y);

#endif
