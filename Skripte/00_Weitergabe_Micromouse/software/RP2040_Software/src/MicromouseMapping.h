#ifndef MICROMOUSE_MAPPING_H
#define MICROMOUSE_MAPPING_H

#include <Arduino.h>

const int MAZE_W = 10;
const int MAZE_H = 5;

struct Edge
{
  int fromX;
  int fromY;
  int toX;
  int toY;
};

const int MAX_EDGES = MAZE_W * MAZE_H * 2;

struct Graph
{
  Edge edges[MAX_EDGES];
  int edgeCount;
};

enum MappingRunStatus
{
  MAPPING_RUN_IDLE,
  MAPPING_RUN_RUNNING,
  MAPPING_RUN_DONE,
  MAPPING_RUN_ABORTED,
  MAPPING_RUN_ERROR
};

enum LineFollowRunStatus
{
  LINE_FOLLOW_RUN_IDLE,
  LINE_FOLLOW_RUN_RUNNING,
  LINE_FOLLOW_RUN_DONE,
  LINE_FOLLOW_RUN_ABORTED,
  LINE_FOLLOW_RUN_ERROR
};

enum TurnRunStatus
{
  TURN_RUN_IDLE,
  TURN_RUN_RUNNING,
  TURN_RUN_DONE,
  TURN_RUN_ABORTED,
  TURN_RUN_ERROR
};

enum CurveRunStatus
{
  CURVE_RUN_IDLE,
  CURVE_RUN_RUNNING,
  CURVE_RUN_DONE,
  CURVE_RUN_ABORTED
};

extern int magOffsetX;
extern int magOffsetY;
extern const int MAG_OFFSET_X_DEFAULT;
extern const int MAG_OFFSET_Y_DEFAULT;
extern uint16_t tofWallThresholdMm;

// Laufzeit-Multiplikator fuer die Geradeaus-Linienfahrt (1.0 = normal).
// Wird vom Turbo-Modul gesetzt; siehe MicromouseMapping.cpp.
extern float g_lineSpeedFactor;

void mappingSetup();
void mappingStartRun();
MappingRunStatus mappingLoopStep();
MappingRunStatus mappingGetStatus();
const char *mappingLastError();
void mappingAbortRun();

// Aktuelle interne Mauspose – nach einer Kollision verwendet das
// UI diese Werte zur Anzeige bzw. als Ausgangspunkt zum Editieren.
int  mappingGetMouseX();
int  mappingGetMouseY();
int  mappingGetHeading();
void mappingSetPose(int x, int y, int heading);

void buildGraphFromMaze();

void exportMazeToSerial();
void exportGraphToSerial();

void initMaze();
bool captureStartCompassHeading();
void drawLiveMap(const char *msg);
void stopMotors();
// Aktive Bremse: gegen die Fahrtrichtung ansteuern bis Stillstand
// (encoder-ueberwacht), statt nur auszurollen. Vor Pivot/Drehung nutzen,
// damit nichts nachlaeuft.
void motorsActiveBrake();
void calibrateLineSensors();
uint16_t mappingReadBatteryMv();
uint16_t mappingReadFrontDistance();

// Setzt die Suchrichtung fuer den Fall, dass die Linie waehrend der
// Verfolgung komplett verloren geht (z.B. direkt nach einer Bogenfahrt).
//   +1 = nach rechts drehen, -1 = nach links, 0 = automatisch (Default).
// Wird vom Solver nach jeder Bogendrehung gesetzt, damit die Maus zur
// richtigen Seite zurueck auf die neue Linie findet. followLineStep()
// setzt den Wert automatisch auf 0 zurueck, sobald die Linie wieder
// erfasst ist.
void setLineSearchDir(int dir);

// True, wenn die mittleren Liniensensoren (Index 1/2/3) die Linie sehen.
// Der Solver nutzt das, um eine Bogenfahrt vorzeitig zu beenden, sobald
// die neue Linie wieder mittig erfasst wird.
bool lineSeenByCenterSensors();

// Kreuzungserkennung umschalten: true = vordere 3 Sensoren (Kurvenfahrt),
// false = mind. 4 von 5 Sensoren dunkel (Punktdrehung). Wird automatisch vom
// Kurven-Modus-Schalter gesetzt (setSolveSmoothTurns / pacmanSetUseCurves /
// routeMotionSetAllowCurves). Siehe intersectionSeen().
void setIntersectionUseFront3(bool useFront3);

// LEDs bei jeder erkannten Kreuzung einmal kurz blinken lassen (LEDs kurz aus).
// Default aus; wird vom Solver beim Start aktiviert und in calibrateLineSensors()
// (jeder Modusstart) wieder zurueckgesetzt.
void setBlinkOnIntersection(bool enable);

// Encoderbasierte Punktdrehung. turnBegin() startet nur; turnStep() muss
// zyklisch aufgerufen werden, bis DONE/ABORTED/ERROR erreicht ist.
void turnBegin(int targetHeading);
TurnRunStatus turnStep();
void turnAbort();
bool turnIsActive();
void turnTo(int targetHeading);  // blockierender Kompatibilitaets-Wrapper

// Nicht-blockierender fliessender 90°-Bogen (Klothoide-Kreis-Klothoide).
// Pendant zur blockierenden solveSmoothTurn90 im Solver, aber fuer die
// ereignisgesteuerte Spiel-Schleife: curveTurnBegin() startet nur,
// curveTurnStep() wird zyklisch gerufen bis DONE/ABORTED. Die Maus rollt
// dabei weiter; der Bogen endet sensor-gefuehrt, sobald die mittleren
// Sensoren die neue Linie erfassen (Fallback: Zeit-Obergrenze). Bei DONE
// ist heading aktualisiert und die Linien-Suchrichtung gesetzt; der
// Aufrufer muss anschliessend per lineFollowFast die Restzelle fahren.
// turnRight = true → 90° rechts, false → 90° links. (Kein 180°.)
void          curveTurnBegin(bool turnRight);
CurveRunStatus curveTurnStep();
void          curveTurnAbort();
bool          curveTurnIsActive();
float         curveTurnProgress();  // grobe 0..1-Schaetzung fuer HUD

void lineFollowFastBegin(bool centerAndStop);
void lineFollowFastRequestCenterStop();
// Vor einer Zellfahrt setzen, die in einer Abbiegung endet: im letzten
// Zellabschnitt wird auf CURVE_DECEL_SPEED verzoegert (langsamer Anlauf an
// die Kreuzung). Wird beim Fahrt-Ende automatisch zurueckgesetzt.
void lineFollowFastSetDecelBeforeTurn(bool enable);
LineFollowRunStatus lineFollowFastStep();
bool followLineToNextIntersectionFast(bool centerAndStop);

// Einheitliche Fahrt um genau eine Maze-Zelle. direction verwendet
// NORTH/EAST/SOUTH/WEST. Die blockierende Variante wird von Mapping und
// Solver-Helfern genutzt, Begin/Step von ereignisgesteuerten Spielmodi.
bool navigationDriveCell(int direction, bool centerAndStop);
void navigationDriveCellBegin(int direction, bool centerAndStop);
// Aktiviert fliessende Boegen fuer 90°-Abbiegungen im naechsten Begin.
// Nur setzen, wenn die Maus rollt (sonst Punktdrehung lassen).
void navigationDriveCellSetAllowCurve(bool enable);
void navigationDriveCellRequestCenterStop();
void navigationDriveCellSetCenterStop(bool centerAndStop);
float navigationDriveCellProgress();
float navigationDriveCellTurnProgress();
bool navigationDriveCellIsTurning();
LineFollowRunStatus navigationDriveCellStep();
void navigationDriveCellAbort();

// Nicht-blockierender Rueckzug zum Beginn der aktuell unterbrochenen
// Zellbewegung. Der Encoderstand wird vor Drehung und Linienfahrt gespeichert,
// sodass auch eine teilweise ausgefuehrte Drehung rueckgaengig gemacht wird.
// Prepare muss beim Fangsignal noch vor dem globalen Fahr-Abort aufgerufen
// werden, damit Begin den gespeicherten Stand danach sicher uebernehmen kann.
enum NavigationRetreatBeginStatus
{
  NAV_RETREAT_BEGIN_ERROR,
  NAV_RETREAT_BEGIN_STATIONARY,
  NAV_RETREAT_BEGIN_STARTED
};

void navigationCellRetreatPrepare();
void navigationCellRetreatPrepareAllowStationary();
NavigationRetreatBeginStatus navigationCellRetreatBegin();
LineFollowRunStatus navigationCellRetreatStep();
const char *navigationCellRetreatLastError();
void navigationCellRetreatAbort();

// Periodischer Callback waehrend blockierender Dreh-/Fahroperationen (ca. alle 50 ms).
// Wird z.B. fuer comm()->update() genutzt, damit BT-Daten waehrend
// laufender turnTo/followLineToNextIntersectionFast nicht verfallen.
void mappingRegisterBackgroundUpdate(void (*fn)());

#endif
