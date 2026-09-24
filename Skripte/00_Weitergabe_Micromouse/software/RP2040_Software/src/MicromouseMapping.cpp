#include "MicromouseMapping.h"
#include "MicromouseMappingState.h"
#include "MicromouseGraphics.h"
#include "MicromousePersistence.h"
#include "MicromouseMenu.h"
#include "MicromouseSolving.h"
#include "MicromouseLeds.h"
#include "Globals.h"
#include "MicromousePlatform.h"
#include <Wire.h>
#include <Pololu3piPlus2040.h>
#include <U8g2lib.h>
#include "MicromouseGraphics.h"
#include <OPT3101.h>
#include <string.h>
#include <Arduino.h>
#include <math.h>

using namespace Pololu3piPlus2040;

// ============================================================
// Hardware
// ============================================================
// Display: U8G2 (definiert in main.cpp, extern referenziert via MicromouseGraphics.h)
// Buttons: Pololu-Objekte leben ausschliesslich auf Core 1 (MicromouseCore1.cpp).
Motors motors;
LineSensors lineSensors;
OPT3101 tof;
IMU imu;
bool mazePrinted = false;
bool mazeSaved = false;
bool mazeSaveAttempted = false;
bool mapReady = false;
// ============================================================
// OLED Grafikpuffer
// ============================================================
// ============================================================
// Maze-Größe
// ============================================================


int heading = NORTH;

// ============================================================
// Compass / reale Ausrichtung
// ============================================================
bool compassAvailable = false;
bool startCompassValid = false;
int startWorldHeading = START_HEADING;
float startCompassDegrees = 0.0f;

const uint8_t COMPASS_SAMPLES = 20;
const int MAG_OFFSET_X_DEFAULT = -8300;
const int MAG_OFFSET_Y_DEFAULT =  1000;
int magOffsetX = MAG_OFFSET_X_DEFAULT;
int magOffsetY = MAG_OFFSET_Y_DEFAULT;
const int8_t MAG_SIGN_X = 1;
const int8_t MAG_SIGN_Y = 1;
const float MAG_DECLINATION_DEG = 0.0f;
const int COMPASS_QUADRANT_OFFSET = 1;
const float COMPASS_MAX_CENTER_ERROR_DEG = 25.0f;

// ============================================================
// Fahrparameter
// ============================================================
// PWM-Werte sind als 3pi+-Standard-Referenz angegeben. PLATFORM_PWM()
// skaliert sie auf der Hyper-Edition hoch genug, dass die HPCB-Motoren
// zuverlaessig anlaufen; geschwindigkeits-proportionale Zeiten werden
// mit PLATFORM_TIME_MS konsistent gekuerzt, damit Drehwinkel und
// Bahnabschnitte identisch zur Standard-Edition bleiben.
// Skalierungsfaktoren und Begruendung: siehe MicromousePlatform.h.
const int LINE_BASE_SPEED   = PLATFORM_DRIVE_PWM(50);
const int LINE_MAX_SPEED    = PLATFORM_DRIVE_PWM(75);
const int LINE_SEARCH_SPEED = PLATFORM_PWM(40);

const int TURN_SPEED          = PLATFORM_PWM(55);
const int TURN_SLOW_SPEED     = PLATFORM_PWM(35);
const int32_t TURN_90_COUNTS  = PLATFORM_TURN_90_COUNTS;
const int32_t TURN_SLOW_COUNTS = PLATFORM_TURN_90_COUNTS / 4;
const int32_t TURN_TOLERANCE_COUNTS = 3;
const float TURN_CENTER_HOLD_KP = 1.5f;
const int TURN_CENTER_HOLD_MAX_PWM = PLATFORM_PWM(18);
const uint32_t TURN_REST_MS   = 120;
const uint32_t TURN_STALL_MS  = 350;
const uint32_t TURN_TIMEOUT_90_MS  = 2500;
const uint32_t TURN_TIMEOUT_180_MS = 4500;

const int CELL_RETREAT_SPEED = PLATFORM_DRIVE_PWM(45);
const int CELL_RETREAT_SLOW_SPEED = PLATFORM_DRIVE_PWM(27);
const int CELL_RETREAT_SYNC_MAX_SPEED = PLATFORM_DRIVE_PWM(50);
const int CELL_RETREAT_SYNC_MAX_CORRECTION = PLATFORM_DRIVE_PWM(12);
const int32_t CELL_RETREAT_SLOW_COUNTS = 80;
const int32_t CELL_RETREAT_TOLERANCE_COUNTS = 5;
const uint32_t CELL_RETREAT_STALL_MS = 1000;
const uint32_t CELL_RETREAT_TIMEOUT_MS = 12000;
const uint32_t CELL_RETREAT_POLARITY_CHECK_MS = 300;
const uint32_t CELL_TRAJECTORY_INTERVAL_MS = 50;
const uint8_t CELL_TRAJECTORY_MAX_SAMPLES = 224;

const int CENTERING_SPEED = PLATFORM_PWM(35);
const int32_t INTERSECTION_CENTER_COUNTS =
  PLATFORM_INTERSECTION_CENTER_COUNTS;
const uint32_t CENTERING_STALL_MS = 350;
const uint32_t CENTERING_TIMEOUT_MS = 1500;

// EINRAD-PIVOT (Klothoide verworfen).
//
// Sobald die Maus auf der Kreuzung ist, faehrt NUR das kurvenAEUSSERE Rad
// vorwaerts, das innere steht still → die Maus pivotet um das innere Rad:
//   Rechtsdrehung → linkes Rad aussen (faehrt), rechtes steht.
//   Linksdrehung  → rechtes Rad aussen (faehrt), linkes steht.
// Gedreht wird, bis 90° erreicht sind.
//
// 90°-Erkennung ENCODER-geschlossen: der Drehwinkel haengt nur von der
// Encoder-DIFFERENZ der Raeder ab. 90° ↔ |aussenDelta − innenDelta| =
// 2*TURN_90_COUNTS. Beim Pivot steht das innere Rad → das aeussere Rad
// faehrt diese 2*TURN_90_COUNTS. Robust gegen Batterie/Reibung.
// (Alternative waere reine Zeitsteuerung; der Encoder ist praeziser.)
const int      CURVE_OUTER_SPEED    = PLATFORM_PWM(70);  // Pivot-Tempo (Aussenrad)
const uint32_t CURVE_TIMEOUT_MS     = 2000;  // Gesamt-Watchdog (Encoder-Ziel verfehlt)
const uint32_t CURVE_STALL_MS       = 300;   // keine Rotationszunahme → Abbruch

// Das kurvenINNERE Rad dreht waehrend des Pivots GANZ MINIMAL rueckwaerts –
// invertiert zum aeusseren Rad (wie bei der Punktdrehung, nur sehr schwach).
// Dadurch pivotet die Maus etwas enger/zentrierter: der Drehpunkt wandert vom
// stehenden inneren Rad ein kleines Stueck nach innen (Richtung Mittelpunkt).
//   CURVE_INNER_SPEED << CURVE_OUTER_SPEED halten ("minimal").
//   Zu hoch  → es wird zur Punktdrehung.
//   Zu niedrig → das Rad ueberwindet Reibung/Schlepp nicht und steht praktisch.
//   Untergrenze fuer zuverlaessigen Anlauf ~PLATFORM_PWM(35) (= CENTERING_SPEED).
const int      CURVE_INNER_SPEED    = PLATFORM_PWM(35);

// Aktive Bremse (Reverse-Puls, encoder-ueberwacht) – killt den Vorwaerts-
// schwung vor dem Pivot, damit nichts nachlaeuft.
//   BRAKE_PWM        : Staerke des Gegen-Pulses (zu hoch → kippt ins Rueckwaerts).
//   BRAKE_SAMPLE_MS  : Mess-Intervall fuer die Stillstands-Erkennung.
//   BRAKE_STOP_COUNTS: Bewegung (|ΔL|+|ΔR|) pro Intervall, ab der "steht" gilt.
//   BRAKE_TIMEOUT_MS : Sicherheits-Obergrenze.
const int      BRAKE_PWM         = PLATFORM_PWM(45);
const uint32_t BRAKE_SAMPLE_MS   = 5;
const int32_t  BRAKE_STOP_COUNTS = 4;
const uint32_t BRAKE_TIMEOUT_MS  = 400;

// Anlauf-Verzoegerung VOR einer Abbiegung: die Linienfolge senkt das Tempo
// schon im letzten Zellabschnitt, damit die Maus langsam an der Kreuzung
// ankommt (weniger Schwung → sanfterer Pivot, kaum noch Bremsbedarf).
//   CURVE_DECEL_SPEED  : reduziertes Basistempo in der Verzoegerungszone.
//   CURVE_DECEL_COUNTS : ab wieviel Encoder-Counts VOR der Kreuzung
//                        (= CELL_TRAVEL_COUNTS) verzoegert wird.
const int      CURVE_DECEL_SPEED  = PLATFORM_PWM(35);
const int32_t  CURVE_DECEL_COUNTS = 250;

// CELL_IGNORE_TIME ist eine zusaetzliche zeitliche Sperre nach einer
// Kreuzung. Die eigentliche Mindeststrecke wird ueber die Encoder
// abgesichert.
// MAX_CELL_DRIVE_TIME ist ein Watchdog – kann unverändert bleiben.
const uint32_t CELL_IGNORE_TIME    = PLATFORM_TIME_MS(350);
const uint32_t MAX_CELL_DRIVE_TIME = 5000;

// Standard Edition 30:1: 29,86 * 12 ~= 358 Encoder-Counts pro
// Radumdrehung. Bei etwa 180 mm Zellabstand entspricht eine Zelle
// grob 640 Counts. Erst nach mindestens einer Radumdrehung wird die
// naechste Kreuzung akzeptiert.
const int32_t MIN_INTERSECTION_TRAVEL_COUNTS = 358;
const float CELL_TRAVEL_COUNTS = 640.0f;

// ============================================================
// Line Sensor Parameter
// ============================================================
const uint16_t LINE_CENTER = 2000;
const uint16_t LINE_THRESHOLD = 250;
const int LINE_KP = 14;
const int LINE_KD = 35;
const uint8_t LINE_LOST_SAMPLES_BEFORE_SEARCH = 3;

// Dynamische Tempo-Anpassung in Kurven (fest im Linienverfolger):
// Liegt die Linie weiter aus der Mitte (|error| > LINE_SLOWDOWN_START), wird
// das VORWAERTS-Tempo linear bis LINE_MIN_SPEED gesenkt – bei maximalem Versatz
// (|error| ~ LINE_CENTER) ist es ganz unten. Geradeaus (kleiner Fehler) bleibt
// das Tempo unveraendert, die bisherige Geradeaus-Fahrt ist also unberuehrt.
// Effekt: schnelle Geraden + kontrollierte Kurven (kein Ueberschiessen / kein
// Linienverlust), und LINE_BASE_SPEED kann gefahrlos hoeher gesetzt werden.
//   LINE_SLOWDOWN_START : Fehler-Schwelle, ab der verzoegert wird (0..2000).
//   LINE_MIN_SPEED      : Vorwaerts-Mindesttempo in der engsten Kurve.
const int16_t  LINE_SLOWDOWN_START = 600;
const int      LINE_MIN_SPEED      = PLATFORM_DRIVE_PWM(30);
const uint8_t INTERSECTION_CLEAR_SAMPLES = 3;
// Entprellung der Kreuzungserkennung. HIT 3→2 senkt die Latenz um ein
// Sample. Zusaetzlich gilt: ist die Maus laut Encoder schon NAHE der
// erwarteten Kreuzung (Zelllaenge − PREDICT_MARGIN), genuegt 1 Sample
// (encoder-gestuetzte Vorhersage → minimale Latenz, ohne mid-cell-
// Fehlauslosung, weil der Sensor die Linie trotzdem sehen muss).
const uint8_t INTERSECTION_HIT_SAMPLES = 2;
const int32_t INTERSECTION_PREDICT_MARGIN = 120;

// ============================================================
// TOF Parameter
// ============================================================
uint16_t tofWallThresholdMm = 100;

// Laufzeit-Multiplikator fuer die Geradeaus-Linienfahrt. 1.0 = normal.
// Wird vom Turbo-Modul (Spiel-Layer) gesetzt; das Mapping kennt den Turbo
// selbst nicht. Wirkt nur auf die PID-Linienfahrt, nicht auf Drehungen
// oder das Zentrieren an Kreuzungen.
float g_lineSpeedFactor = 1.0f;

// Einmal-Blink der LEDs bei jeder erkannten Kreuzung – nur im Solver aktiviert
// (setBlinkOnIntersection). Nicht-blockierend ueber flashColor(LED_OFF, ...);
// ledUpdate() in advanceRobotToIntersectionCenter stellt die Grundfarbe (gruen)
// danach wieder her. In calibrateLineSensors() bei jedem Modusstart auf false.
static bool s_blinkOnIntersection = false;
const unsigned long BLINK_INTERSECTION_MS = 80;

void setBlinkOnIntersection(bool enable)
{
  s_blinkOnIntersection = enable;
}
const uint16_t MIN_AMPLITUDE = 35;
const uint8_t TOF_MIN_SAMPLES = 2;
const uint8_t TOF_MAX_SAMPLES = 4;
const uint16_t TOF_STABLE_SPREAD_MM = 18;
const uint16_t TOF_UNKNOWN_MM = 9999;
const uint32_t SCAN_SETTLE_TIME_MS = 60;
const uint32_t SCAN_DONE_TIME_MS = 90;
const uint32_t KNOWN_CELL_TIME_MS = 60;
const uint32_t CELL_DONE_TIME_MS = 90;
const uint16_t LOOKAHEAD_CELL_MM = 180;
const uint16_t LOOKAHEAD_MARGIN_MM = 70;
const bool LOOKAHEAD_LOGGING = true;
const bool LOOKAHEAD_MARKING = true;
const bool LOOKAHEAD_SKIP_PREDICTED_FRONT = false;
const uint8_t LOOKAHEAD_MAX_PREDICTED_EDGES = 3;

struct MappingCellState
{
  bool visited;
  bool scanned;
  bool lookahead[4];
  uint16_t tofDistance[4];
};

static MappingCellState mappingState[MAZE_W][MAZE_H];

int mouseX = START_X;
int mouseY = START_Y;

// ============================================================
// DFS Stack
// ============================================================
int stackX[256];
int stackY[256];
int stackTop = -1;

// ============================================================
// Mapping-Run-Status (fuer die neue API)
// ============================================================
static MappingRunStatus mappingStatus = MAPPING_RUN_IDLE;
static const char *mappingErrorMessage = "";

MappingRunStatus mappingGetStatus() { return mappingStatus; }
const char *mappingLastError()      { return mappingErrorMessage; }

// ============================================================
// Zustand
// ============================================================
enum RobotState
{
  MAIN_MENU,
  MAPPING_MENU,
  MAP_RUNNING,
  MAP_DONE,
  TEST_RUNNING,
  ERROR_STATE
};

RobotState state = MAIN_MENU;

// ============================================================
// Laufvariablen
// ============================================================
int16_t lastLineError = 0;
uint16_t linePosition = 2000;

// Aktuelles Basistempo der Linienfolge. Default = LINE_BASE_SPEED; wird in
// der Anlauf-Verzoegerung vor einer Abbiegung auf CURVE_DECEL_SPEED gesenkt.
static int s_followBaseSpeed = LINE_BASE_SPEED;

uint16_t lastTofDistance = 9999;
bool lastWallDetected = false;
int lastScanDir = NORTH;

// Suchrichtung, wenn die Linie KOMPLETT verloren ist (kein Sensor sieht
// sie). Wird nach einer Bogenfahrt vom Solver gesetzt, damit die Maus
// gezielt zur richtigen Seite zurueck auf die neue Linie dreht:
//   +1 = nach rechts drehen (Maus bog rechts ab → Linie liegt rechts)
//   -1 = nach links drehen
//    0 = automatisch aus der readLineBlack-Memory ableiten (Default).
// readLineBlack() merkt sich anhand der AEUSSEREN Sensoren, auf welcher
// Seite die Linie zuletzt war (0 = links, 4000 = rechts) – das nutzen
// wir im 0-Fall als Fallback.
static int s_lineSearchDir = 0;
static uint8_t s_lineLostSampleCount = 0;

void setLineSearchDir(int dir) { s_lineSearchDir = dir; }

// ============================================================
// Grundfunktionen
// ============================================================
void stopMotors()
{
  motors.setSpeeds(0, 0);
}

// Aktive Bremse: kurz GEGEN die Fahrtrichtung ansteuern, bis die Maus steht –
// statt nur auszurollen (setSpeeds(0) = Coast).
//
// Es wird die SIGNIERTE Vorwaertsbewegung pro Intervall verfolgt (Summe der
// Raddeltas; forward = +/- Counts ist hardwareabhaengig, beide Raeder teilen
// die Konvention – siehe lineFollowForwardTravelCounts). Gestoppt wird, sobald
// die Vorwaertsbewegung im Anfangs-Vorzeichen unter BRAKE_STOP_COUNTS faellt –
// das erfasst auch den Moment, in dem der Reverse-Puls die Richtung kippt, und
// verhindert ein Rueckwaerts-Wegbeschleunigen.
// Annahme: positive setSpeeds = vorwaerts (Motor-Flip pro Edition erledigt das).
void motorsActiveBrake()
{
  // Anfangs-Vorwaertsbewegung messen (Coast, ohne Gegenpuls).
  const int32_t l0 = Encoders::getCountsLeft();
  const int32_t r0 = Encoders::getCountsRight();
  delay(BRAKE_SAMPLE_MS);
  int32_t prevL = Encoders::getCountsLeft();
  int32_t prevR = Encoders::getCountsRight();
  const int32_t fwd0 = (prevL - l0) + (prevR - r0);  // signiert
  if (fwd0 <= BRAKE_STOP_COUNTS && fwd0 >= -BRAKE_STOP_COUNTS)
  {
    motors.setSpeeds(0, 0);   // steht schon → kein Bremspuls
    return;
  }
  const int dir = (fwd0 > 0) ? 1 : -1;  // Vorwaerts-Vorzeichen

  // Gegenbremsen.
  motors.setSpeeds(-BRAKE_PWM, -BRAKE_PWM);
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < BRAKE_TIMEOUT_MS)
  {
    delay(BRAKE_SAMPLE_MS);
    const int32_t l = Encoders::getCountsLeft();
    const int32_t r = Encoders::getCountsRight();
    const int32_t fwd = (l - prevL) + (r - prevR);  // signiert
    prevL = l;
    prevR = r;
    // fwd*dir = Vorwaertsgeschwindigkeit im positiven Frame. Faellt sie unter
    // die Schwelle (inkl. negativ = Richtung gekippt) → fertig.
    if (fwd * dir <= BRAKE_STOP_COUNTS)
      break;
  }
  motors.setSpeeds(0, 0);
}

void setMappingError(const char *msg)
{
  mappingErrorMessage = msg;
  mappingStatus = MAPPING_RUN_ERROR;
}

const char* dirName(int d)
{
  if (d == NORTH) return "N";
  if (d == EAST)  return "E";
  if (d == SOUTH) return "S";
  return "W";
}

int worldDxForDir(int d)
{
  if (d == EAST) return 1;
  if (d == WEST) return -1;
  return 0;
}

int worldDyForDir(int d)
{
  if (d == NORTH) return 1;
  if (d == SOUTH) return -1;
  return 0;
}

int dirFromCompassAngle(float angle)
{
  while (angle < 0.0f) angle += 360.0f;
  while (angle >= 360.0f) angle -= 360.0f;

  if (angle >= 315.0f || angle < 45.0f) return EAST;
  if (angle < 135.0f) return NORTH;
  if (angle < 225.0f) return WEST;
  return SOUTH;
}

float compassSectorCenter(int d)
{
  if (d == EAST) return 0.0f;
  if (d == NORTH) return 90.0f;
  if (d == WEST) return 180.0f;
  return 270.0f;
}

float angularDistanceDeg(float a, float b)
{
  float diff = fabs(a - b);
  while (diff >= 360.0f) diff -= 360.0f;
  if (diff > 180.0f) diff = 360.0f - diff;
  return diff;
}

int worldDirFromInternal(int internalDir)
{
  int baseWorldHeading = startCompassValid ? startWorldHeading : START_HEADING;
  return (internalDir - START_HEADING + baseWorldHeading + 4) % 4;
}

int internalDirFromWorld(int worldDir)
{
  for (int d = 0; d < 4; d++)
  {
    if (worldDirFromInternal(d) == worldDir)
      return d;
  }

  return worldDir;
}

void internalToWorld(int x, int y, int &worldX, int &worldY)
{
  int relX = x - START_X;
  int relY = y - START_Y;

  int worldEast = worldDirFromInternal(EAST);
  int worldNorth = worldDirFromInternal(NORTH);

  worldX = relX * worldDxForDir(worldEast) + relY * worldDxForDir(worldNorth);
  worldY = relX * worldDyForDir(worldEast) + relY * worldDyForDir(worldNorth);
}

bool initCompass()
{
  if (!imu.init())
    return false;

  imu.enableDefault();
  imu.configureForCompassHeading();
  delay(80);
  return imu.getLastError() == 0;
}

bool captureStartCompassHeading()
{
  if (!compassAvailable)
  {
    startCompassValid = false;
    startWorldHeading = START_HEADING;
    return false;
  }

  int32_t sumX = 0;
  int32_t sumY = 0;
  uint8_t count = 0;

  for (uint8_t i = 0; i < COMPASS_SAMPLES; i++)
  {
    imu.readMag();

    if (imu.getLastError() == 0)
    {
      sumX += imu.m.x;
      sumY += imu.m.y;
      count++;
    }

    delay(15);
  }

  if (count == 0)
  {
    startCompassValid = false;
    startWorldHeading = START_HEADING;
    return false;
  }

  float mx = ((float)sumX / count - magOffsetX) * MAG_SIGN_X;
  float my = ((float)sumY / count - magOffsetY) * MAG_SIGN_Y;

  float angle = atan2(my, mx) * 180.0f / PI + MAG_DECLINATION_DEG;
  while (angle < 0.0f) angle += 360.0f;
  while (angle >= 360.0f) angle -= 360.0f;

  startCompassDegrees = angle;
  int rawCompassDir = dirFromCompassAngle(angle);
  float centerError = angularDistanceDeg(angle, compassSectorCenter(rawCompassDir));

  if (centerError > COMPASS_MAX_CENTER_ERROR_DEG)
  {
    startCompassValid = false;
    startWorldHeading = START_HEADING;

    Serial.print("COMPASS AMBIGUOUS angle=");
    Serial.print(angle, 1);
    Serial.print(" centerError=");
    Serial.println(centerError, 1);

    return false;
  }

  startWorldHeading = (rawCompassDir + COMPASS_QUADRANT_OFFSET + 4) % 4;
  startCompassValid = true;
  return true;
}

void setKnownWallLocal(int x, int y, int d, bool isWall)
{
  mazeSetWallLocal(x, y, d, isWall);
  if (inBounds(x, y))
    mappingState[x][y].lookahead[d] = false;
}

void pushStack(int x, int y)
{
  if (stackTop < 255)
  {
    stackTop++;
    stackX[stackTop] = x;
    stackY[stackTop] = y;
  }
}

bool stackIsEmpty()
{
  return stackTop < 0;
}

void popStackTo(int &x, int &y)
{
  x = stackX[stackTop];
  y = stackY[stackTop];
  stackTop--;
}

// ============================================================
// Live Mini-Map
// ============================================================
void drawLiveMap(const char *msg)
{
  (void)msg;
  clearGraphics();

  const uint8_t CELL = 12;
  const uint8_t MOUSE_SIZE = 5;

  // Map-Größe berechnen
  const uint8_t mapW = MAZE_W * CELL + 1;
  const uint8_t mapH = MAZE_H * CELL + 1;

  // Karte mittig auf 128x64 setzen
  const uint8_t OX = (128 - mapW) / 2;
  int8_t OY = (64 - mapH) / 2;
  if (OY < 0) OY = 0;

  for (int x = 0; x < MAZE_W; x++)
  {
    for (int y = 0; y < MAZE_H; y++)
    {
      uint8_t px = OX + x * CELL;
      uint8_t py = OY + (MAZE_H - 1 - y) * CELL;

      if (mappingState[x][y].visited)
      {
        uint8_t dotX = px + CELL / 2;
        uint8_t dotY = py + CELL / 2;
        drawHLine(dotX - 1, dotX + 1, dotY);
        drawVLine(dotX, dotY - 1, dotY + 1);
      }

      if (maze[x][y].known[NORTH] && maze[x][y].wall[NORTH])
        drawHLine(px, px + CELL, py);

      if (maze[x][y].known[EAST] && maze[x][y].wall[EAST])
        drawVLine(px + CELL, py, py + CELL);

      if (maze[x][y].known[SOUTH] && maze[x][y].wall[SOUTH])
        drawHLine(px, px + CELL, py + CELL);

      if (maze[x][y].known[WEST] && maze[x][y].wall[WEST])
        drawVLine(px, py, py + CELL);
    }
  }

  // Mausposition als ausgefülltes Kästchen
  uint8_t mx = OX + mouseX * CELL + (CELL - MOUSE_SIZE) / 2;
  uint8_t my = OY + (MAZE_H - 1 - mouseY) * CELL + (CELL - MOUSE_SIZE) / 2;
  drawSolidRectangle(mx, my, MOUSE_SIZE, MOUSE_SIZE);

  flushGraphicsToU8G2();
}
// ============================================================
// Initialisierung
// ============================================================
void initMaze()
{
  mazePrinted = false; 
  mazeSaved = false;
  mazeSaveAttempted = false;
  mapReady = false;
  for (int x = 0; x < MAZE_W; x++)
  {
    for (int y = 0; y < MAZE_H; y++)
    {
      mappingState[x][y].visited = false;
      mappingState[x][y].scanned = false;
      for (int d = 0; d < 4; d++)
      {
        mappingState[x][y].lookahead[d] = false;
        mappingState[x][y].tofDistance[d] = TOF_UNKNOWN_MM;
      }
    }
  }

  mazeClear();

  mouseX = START_X;
  mouseY = START_Y;
  heading = START_HEADING;
  stackTop = -1;

  lastTofDistance = 9999;
  lastWallDetected = false;
  lastScanDir = NORTH;

  startCompassValid = false;
  startWorldHeading = START_HEADING;
  startCompassDegrees = 0.0f;
}

static void callBgUpdate();

void calibrateLineSensors()
{
  // Jeder Modusstart ruft calibrateLineSensors() → Kreuzungs-Blink standardmaessig
  // aus; nur der Solver (doSolving*) schaltet ihn danach wieder ein.
  s_blinkOnIntersection = false;

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawUTF8(2, 10, "Liniensensoren");
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(2, 28, "Auf Linie stellen");
  oledSendBuffer();

  const uint32_t settleStartMs = millis();
  while ((uint32_t)(millis() - settleStartMs) < 700)
  {
    callBgUpdate();
    delay(5);
  }

  // Kurze Kalibrierbewegung aus der frueheren Revision:
  // ca. 90 Grad rechts, 180 Grad links, 90 Grad rechts. Damit werden beide
  // Seiten der Linie erfasst und der Roboter endet wieder in Startrichtung.
  const int calSpeed = PLATFORM_PWM(40);
  for (uint16_t sample = 0; sample < 80; sample++)
  {
    if (sample > 20 && sample <= 60)
      motors.setSpeeds(-calSpeed, calSpeed);
    else
      motors.setSpeeds(calSpeed, -calSpeed);

    lineSensors.calibrate();
    callBgUpdate();
    delay(20);
  }

  stopMotors();
  const uint32_t stopStartMs = millis();
  while ((uint32_t)(millis() - stopStartMs) < 300)
  {
    callBgUpdate();
    delay(5);
  }
}

// ============================================================
// Linie / Kreuzung
// ============================================================
bool lineSeen()
{
  for (uint8_t i = 0; i < LINE_SENSOR_COUNT; i++)
  {
    if (lineSensors.calibratedSensorValues[i] > LINE_THRESHOLD)
      return true;
  }
  return false;
}

// Kreuzungserkennung ueber Mitte und beide aeusseren Sensoren (Index 0/2/4).
//
// Geometrie 3pi+ 2040: die Sensorleiste folgt der runden Platinenkante –
// die aeusseren Sensoren (Index 0 und 4) sitzen dadurch weiter HINTEN als
// die mittleren drei. Das fruehere Kriterium "≥4 von 5" wartete deshalb,
// bis die Querlinie auch die zurueckversetzten Aussensensoren erreicht
// hatte → systematisch spaete Erkennung.
//
// Mitte und beide aeusseren Sensoren muessen die Querlinie innerhalb eines
// kurzen Zeitfensters gesehen haben. Das toleriert schraege Anfahrt, ohne die
// gewuenschte 1/3/5-Bedingung aufzugeben. Fehlauslosungen fangen zusaetzlich
// Arming, Zeit-/Encoder-Gate und die Entprellung in lineFollowFastStep ab.
// Detektions-Modus, automatisch vom Kurven-Schalter gesetzt
// (setSolveSmoothTurns / pacmanSetUseCurves / routeMotionSetAllowCurves):
//   true  = vordere 3 Sensoren (Index 1/2/3) – frueh erkannt.
//   false = mind. 4 von 5 Sensoren dunkel (Punktdrehung: erkennt etwas spaeter
//           und zentrierter, dafuer robuster – die Maus haelt ohnehin an).
static bool s_intersectionUseFront3 = true;

void setIntersectionUseFront3(bool useFront3)
{
  s_intersectionUseFront3 = useFront3;
}

// centerAndStop = true → die Maus haelt an dieser Kreuzung an (180°-Wende,
// Punktdrehung, letzte Zelle). Dann wird wie bei der Punktdrehung ueber 4 von 5
// erkannt: etwas spaeter, dafuer steht die Maus mittiger auf der Kreuzung.
// Nur beim ROLLENDEN Durchfahren in einen 90°-Bogen werden die vorderen 3
// genutzt (frueh → rechtzeitiger Pivot).
bool intersectionSeen(bool centerAndStop)
{
  if (s_intersectionUseFront3 && !centerAndStop)
  {
    // Kurvenfahrt (rollend in 90°-Bogen): die VORDEREN drei Sensoren (Index
    // 1/2/3) muessen dunkel sein. Sie laufen der Radachse voraus → fruehe
    // Erkennung. Die aeusseren Sensoren (0/4) sitzen geometrisch weiter hinten
    // und werden bewusst NICHT gefordert, sonst loest die Kreuzung zu spaet aus.
    return lineSensors.calibratedSensorValues[1] > LINE_THRESHOLD &&
           lineSensors.calibratedSensorValues[2] > LINE_THRESHOLD &&
           lineSensors.calibratedSensorValues[3] > LINE_THRESHOLD;
  }

  // Punktdrehung / 180°-Wende / Stopp: mindestens 4 von 5 Sensoren dunkel.
  int black = 0;
  for (uint8_t i = 0; i < LINE_SENSOR_COUNT; i++)
  {
    if (lineSensors.calibratedSensorValues[i] > LINE_THRESHOLD)
      black++;
  }
  return black >= 4;
}

// True, wenn die MITTLEREN Liniensensoren die Linie sehen.
// Sensor-Anordnung 3pi+ 2040: 5 Sensoren, Index 0..4 von links nach
// rechts, gleichmaessig entlang der Vorderkante. Index 2 = zentraler
// Sensor (DN3); readLineBlack() liefert 2000 wenn die Linie dort mittig
// liegt. Wir pruefen die mittleren drei (Index 1/2/3), damit die
// Erfassung robust ist, auch wenn die Maus leicht ausser Mitte aus dem
// Bogen kommt. Genutzt vom Solver, um die Bogenfahrt vorzeitig zu
// beenden, sobald die neue Linie wieder mittig erfasst wird.
bool lineSeenByCenterSensors()
{
  lineSensors.readLineBlack();  // aktualisiert calibratedSensorValues[]
  const int mid = LINE_SENSOR_COUNT / 2;  // = 2 bei 5 Sensoren
  for (int i = mid - 1; i <= mid + 1; i++)
  {
    if (i >= 0 && i < (int)LINE_SENSOR_COUNT &&
        lineSensors.calibratedSensorValues[i] > LINE_THRESHOLD)
      return true;
  }
  return false;
}

void followLineStep()
{
  linePosition = lineSensors.readLineBlack();

  if (!lineSeen())
  {
    if (s_lineLostSampleCount < LINE_LOST_SAMPLES_BEFORE_SEARCH)
      s_lineLostSampleCount++;
    if (s_lineLostSampleCount < LINE_LOST_SAMPLES_BEFORE_SEARCH)
      return;

    // Linie komplett verloren – typisch direkt nach einer Bogenfahrt,
    // wenn die Maus noch nicht ganz auf der neuen (quer verlaufenden)
    // Linie sitzt. Suchrichtung bestimmen:
    //   s_lineSearchDir != 0 : expliziter Hinweis aus der letzten Kurve.
    //   s_lineSearchDir == 0 : aus der readLineBlack-Memory ableiten –
    //     die Funktion gibt anhand der aeusseren Sensoren 0 (links
    //     verloren) bzw. 4000 (rechts verloren) zurueck.
    int dir = s_lineSearchDir;
    if (dir == 0)
      dir = (linePosition < LINE_CENTER) ? -1 : +1;
    motors.setSpeeds(dir * LINE_SEARCH_SPEED, -dir * LINE_SEARCH_SPEED);
    return;
  }

  // Linie wieder erfasst → expliziten Kurven-Hinweis loeschen. Ab jetzt
  // regelt der PID; die readLineBlack-Memory uebernimmt die Seitenlogik.
  s_lineLostSampleCount = 0;
  s_lineSearchDir = 0;

  int16_t error = (int16_t)linePosition - (int16_t)LINE_CENTER;
  int16_t derivative = error - lastLineError;
  lastLineError = error;

  int turn = (error / LINE_KP) + (derivative / LINE_KD);

  // Basistempo kombiniert beide Mechanismen:
  //  - s_followBaseSpeed: Anlauf-Verzoegerung vor einer Abbiegung
  //    (normal LINE_BASE_SPEED, vor Kurven auf CURVE_DECEL_SPEED gesenkt).
  //  - g_lineSpeedFactor: Turbo skaliert die Geradeaus-Fahrgeschwindigkeit.
  const int baseSpeed = (int)(s_followBaseSpeed * g_lineSpeedFactor);
  const int maxSpeed  = (int)(LINE_MAX_SPEED  * g_lineSpeedFactor);

  // Dynamische Tempo-Anpassung: je weiter die Linie aus der Mitte (|error|),
  // desto langsamer das Vorwaerts-Tempo → in Kurven vom Gas, kein Ueber-
  // schiessen. Nur senken, wenn das Basistempo ueberhaupt ueber dem Mindest-
  // tempo liegt (sonst – z.B. Anlauf-Decel vor Pivot – nicht weiter bremsen).
  int forward = baseSpeed;
  const int absErr = error < 0 ? -error : error;
  if (baseSpeed > LINE_MIN_SPEED && absErr > LINE_SLOWDOWN_START)
  {
    const int over   = absErr - LINE_SLOWDOWN_START;            // 0 .. span
    const int span   = (int)LINE_CENTER - LINE_SLOWDOWN_START;  // bis max Versatz
    const int reduce = ((baseSpeed - LINE_MIN_SPEED) * over) / span;
    forward = baseSpeed - reduce;
    if (forward < LINE_MIN_SPEED) forward = LINE_MIN_SPEED;
  }

  int left  = forward + turn;
  int right = forward - turn;

  if (left > maxSpeed) left = maxSpeed;
  if (left < -maxSpeed) left = -maxSpeed;

  if (right > maxSpeed) right = maxSpeed;
  if (right < -maxSpeed) right = -maxSpeed;

  motors.setSpeeds(left, right);
}

static void advanceRobotToIntersectionCenter(bool stopAfterCentering)
{
  const int previousBaseSpeed = s_followBaseSpeed;
  const int32_t startLeft = Encoders::getCountsLeft();
  const int32_t startRight = Encoders::getCountsRight();
  const uint32_t startMs = millis();
  uint32_t lastProgressMs = startMs;
  int32_t lastProgress = 0;

  s_followBaseSpeed = stopAfterCentering ? CENTERING_SPEED : LINE_BASE_SPEED;

  while (true)
  {
    const int32_t deltaLeft =
      abs(Encoders::getCountsLeft() - startLeft);
    const int32_t deltaRight =
      abs(Encoders::getCountsRight() - startRight);
    const int32_t progress = (deltaLeft + deltaRight) / 2;
    const uint32_t now = millis();

    if (progress >= INTERSECTION_CENTER_COUNTS)
      break;

    if (progress > lastProgress)
    {
      lastProgress = progress;
      lastProgressMs = now;
    }
    else if ((uint32_t)(now - lastProgressMs) >= CENTERING_STALL_MS)
    {
      Serial.println("ERROR: intersection centering stalled");
      break;
    }

    if ((uint32_t)(now - startMs) >= CENTERING_TIMEOUT_MS)
    {
      Serial.println("ERROR: intersection centering timeout");
      break;
    }

    // Auch beim Anhalten weiter auf der Linie regeln, damit die Maus vor
    // einer Scan-Drehung moeglichst mittig auf der Linie bleibt.
    followLineStep();
    callBgUpdate();
    ledUpdate();
    delay(5);
  }

  s_followBaseSpeed = previousBaseSpeed;
  if (stopAfterCentering)
  {
    stopMotors();
    delay(120);
  }
}

void centerRobotAfterIntersection()
{
  advanceRobotToIntersectionCenter(true);
}

bool followLineToNextIntersection()
{
  lineFollowFastBegin(true);
  while (true)
  {
    const LineFollowRunStatus status = lineFollowFastStep();
    if (status == LINE_FOLLOW_RUN_DONE) return true;
    if (status != LINE_FOLLOW_RUN_RUNNING) return false;
    delay(5);
  }
}

// ============================================================
// Background-Update-Hook
//
// Wird waehrend blockierender Dreh-/Fahroperationen ca. alle
// BG_INTERVAL_MS gerufen. Zweck: comm()->update() weiter pollen,
// damit BT-Controller-Switches und Pacman-Position waehrend
// turnTo() / followLine...() nicht verfallen.
// ============================================================
static void (*s_bgUpdate)()    = nullptr;
static uint32_t s_bgLastMs     = 0;
static const uint32_t BG_INTERVAL_MS = 50;

static void callBgUpdate()
{
  if (!s_bgUpdate) return;
  uint32_t now = millis();
  if (now - s_bgLastMs >= BG_INTERVAL_MS)
  {
    s_bgLastMs = now;
    s_bgUpdate();
  }
}

void mappingRegisterBackgroundUpdate(void (*fn)())
{
  s_bgUpdate = fn;
  s_bgLastMs = millis();
}

bool followLineToNextIntersectionFast(bool centerAndStop)
{
  lineFollowFastBegin(centerAndStop);
  while (true)
  {
    const LineFollowRunStatus status = lineFollowFastStep();
    if (status == LINE_FOLLOW_RUN_DONE) return true;
    if (status != LINE_FOLLOW_RUN_RUNNING) return false;
    delay(5);
  }
}

static bool s_lineFollowFastActive = false;
static bool s_lineFollowFastCenterAndStop = false;
static uint32_t s_lineFollowFastStartMs = 0;
static uint32_t s_lineFollowFastIgnoreUntilMs = 0;
static bool s_intersectionArmed = false;
static uint8_t s_intersectionClearCount = 0;
static uint8_t s_intersectionHitCount = 0;
static int32_t s_lineFollowStartEncoderLeft = 0;
static int32_t s_lineFollowStartEncoderRight = 0;
// true, wenn diese Zellfahrt in einer Abbiegung endet → im letzten
// Zellabschnitt auf CURVE_DECEL_SPEED verzoegern. Vom Aufrufer vor der
// Fahrt gesetzt; wird beim Ende der Fahrt automatisch zurueckgesetzt.
static bool s_lineFollowDecelBeforeTurn = false;

void lineFollowFastSetDecelBeforeTurn(bool enable)
{
  s_lineFollowDecelBeforeTurn = enable;
}

static int32_t lineFollowForwardTravelCounts()
{
  const int32_t deltaLeft =
    Encoders::getCountsLeft() - s_lineFollowStartEncoderLeft;
  const int32_t deltaRight =
    Encoders::getCountsRight() - s_lineFollowStartEncoderRight;

  int32_t forwardCounts = (deltaLeft + deltaRight) / 2;
  if (forwardCounts < 0)
    forwardCounts = -forwardCounts;
  return forwardCounts;
}

static void printLineFollowDebug(const char *event)
{
  Serial.print("[Line] ");
  Serial.print(event);
  Serial.print(" ms=");
  Serial.print(millis() - s_lineFollowFastStartMs);
  Serial.print(" enc=");
  Serial.print(lineFollowForwardTravelCounts());
  Serial.print(" pos=");
  Serial.print(linePosition);
  Serial.print(" sensors=");
  for (uint8_t i = 0; i < LINE_SENSOR_COUNT; i++)
  {
    if (i > 0) Serial.print(',');
    Serial.print(lineSensors.calibratedSensorValues[i]);
  }
  Serial.print(" armed=");
  Serial.print(s_intersectionArmed ? 1 : 0);
  Serial.print(" clear=");
  Serial.print(s_intersectionClearCount);
  Serial.print(" hit=");
  Serial.print(s_intersectionHitCount);
  Serial.print(" centerStop=");
  Serial.println(s_lineFollowFastCenterAndStop ? 1 : 0);
}

void lineFollowFastBegin(bool centerAndStop)
{
  mappingStatus = MAPPING_RUN_RUNNING;
  s_lineFollowFastActive = true;
  s_lineFollowFastCenterAndStop = centerAndStop;
  s_lineFollowFastStartMs = millis();
  s_lineFollowFastIgnoreUntilMs = millis() + CELL_IGNORE_TIME;
  s_intersectionArmed = false;
  s_intersectionClearCount = 0;
  s_intersectionHitCount = 0;
  s_lineFollowStartEncoderLeft = Encoders::getCountsLeft();
  s_lineFollowStartEncoderRight = Encoders::getCountsRight();
  s_followBaseSpeed = LINE_BASE_SPEED;   // sauberes Tempo; Decel setzt der Step
  lastLineError = 0;
  s_lineLostSampleCount = 0;
  s_bgLastMs = millis();
  printLineFollowDebug("begin");
}

// Fortsetzung einer Zellfahrt direkt nach einem Bogen. Im Unterschied zu
// lineFollowFastBegin():
//  - Die Wegmessung startet NICHT bei 0, sondern bei (originLeft/Right) –
//    dem Encoder-Stand der Ausgangskreuzung. So zaehlt der Wegstrecken-
//    Schwellwert MIN_INTERSECTION_TRAVEL_COUNTS ab der Kreuzung; die vom
//    Bogen schon zurueckgelegte Strecke wird mitgezaehlt → die naechste
//    Kreuzung wird korrekt (nicht zu spaet) erkannt.
//  - Keine zusaetzliche Zeit-Sperre (CELL_IGNORE_TIME): der Bogen hat die
//    Ausgangskreuzung bereits verlassen, der Wegstrecken-Schutz genuegt.
static void lineFollowFastBeginContinuation(bool centerAndStop,
                                            int32_t originLeft,
                                            int32_t originRight)
{
  mappingStatus = MAPPING_RUN_RUNNING;
  s_lineFollowFastActive = true;
  s_lineFollowFastCenterAndStop = centerAndStop;
  s_lineFollowFastStartMs = millis();
  s_lineFollowFastIgnoreUntilMs = millis();   // keine zusaetzliche Zeit-Sperre
  s_intersectionArmed = false;
  s_intersectionClearCount = 0;
  s_intersectionHitCount = 0;
  s_lineFollowStartEncoderLeft = originLeft;
  s_lineFollowStartEncoderRight = originRight;
  s_followBaseSpeed = LINE_BASE_SPEED;   // sauberes Tempo; Decel setzt der Step
  lastLineError = 0;
  s_lineLostSampleCount = 0;
  s_bgLastMs = millis();
  printLineFollowDebug("begin-cont");
}

void lineFollowFastRequestCenterStop()
{
  if (s_lineFollowFastActive)
    s_lineFollowFastCenterAndStop = true;
}

LineFollowRunStatus lineFollowFastStep()
{
  if (!s_lineFollowFastActive)
    return LINE_FOLLOW_RUN_IDLE;

  if (g_abortRequested)
  {
    g_abortRequested = false;
    stopMotors();
    mappingStatus = MAPPING_RUN_ABORTED;
    s_lineFollowDecelBeforeTurn = false;
    s_followBaseSpeed = LINE_BASE_SPEED;
    s_lineFollowFastActive = false;
    return LINE_FOLLOW_RUN_ABORTED;
  }
  if (g_btnAPressed)
  {
    stopMotors();
    s_lineFollowDecelBeforeTurn = false;
    s_followBaseSpeed = LINE_BASE_SPEED;
    s_lineFollowFastActive = false;
    return LINE_FOLLOW_RUN_ABORTED;
  }

  // Anlauf-Verzoegerung vor Kurven ENTFERNT: die Maus faehrt mit vollem
  // LINE_BASE_SPEED an die Kreuzung. Die situative Drosselung uebernimmt jetzt
  // die dynamische Tempo-Anpassung (|error|) in followLineStep. Der Hebel
  // lineFollowFastSetDecelBeforeTurn bleibt erhalten, ist aber wirkungslos.
  s_followBaseSpeed = LINE_BASE_SPEED;

  callBgUpdate();
  followLineStep();   // liest die Sensoren bereits (readLineBlack)

  // HEBEL 1: Kein zweiter readLineBlack mehr. Das Verlassen der alten Kreuzung
  // und die Kreuzungserkennung nutzen die im followLineStep frisch gelesenen
  // calibratedSensorValues (selbe Iteration, Mikrosekunden alt).
  const bool intersection = intersectionSeen(s_lineFollowFastCenterAndStop);
  const int32_t travelNow = lineFollowForwardTravelCounts();

  if (!s_intersectionArmed)
  {
    if (!intersection)
    {
      if (s_intersectionClearCount < INTERSECTION_CLEAR_SAMPLES)
        s_intersectionClearCount++;
      if (s_intersectionClearCount >= INTERSECTION_CLEAR_SAMPLES)
        s_intersectionArmed = true;
    }
    else
    {
      s_intersectionClearCount = 0;
    }
  }

  const bool intersectionGateOpen =
    millis() > s_lineFollowFastIgnoreUntilMs &&
    travelNow >= MIN_INTERSECTION_TRAVEL_COUNTS;

  // HEBEL 3: nahe der erwarteten Kreuzung (Encoder ~ Zelllaenge) genuegt 1
  // Sample → minimale Latenz beim Pivot-Start. Sonst volle Entprellung.
  const uint8_t hitSamplesNeeded =
    (travelNow >= (int32_t)CELL_TRAVEL_COUNTS - INTERSECTION_PREDICT_MARGIN)
      ? 1 : INTERSECTION_HIT_SAMPLES;

  if (s_intersectionArmed && intersectionGateOpen)
  {
    if (intersection)
    {
      if (s_intersectionHitCount < hitSamplesNeeded)
        s_intersectionHitCount++;

      if (s_intersectionHitCount >= hitSamplesNeeded)
      {
        printLineFollowDebug("intersection");

        // Einmal-Blink bei Kreuzung (nur im Solver aktiviert): LEDs kurz aus →
        // wirkt gegen die durchgehende Grundfarbe (gruen) wie ein Blinzeln.
        // Nicht-blockierend; ledUpdate() (in advanceRobotToIntersectionCenter)
        // stellt die Grundfarbe nach BLINK_INTERSECTION_MS wieder her.
        if (s_blinkOnIntersection)
          flashColor(LED_OFF, BLINK_INTERSECTION_MS);

        // Die Sensorleiste erreicht die Kreuzung vor dem Drehpunkt der Maus.
        // Deshalb auch beim fliessenden Durchfahren erst bis zur Zellmitte
        // vorziehen. Nur der letzte Schritt stoppt dort; Geradeausfahrt und
        // Bogenfahrt uebernehmen die noch rollende Maus.
        advanceRobotToIntersectionCenter(s_lineFollowFastCenterAndStop);

        s_lineFollowDecelBeforeTurn = false;       // Decel-Zustand fuer naechste Fahrt loeschen
        s_followBaseSpeed = LINE_BASE_SPEED;
        s_lineFollowFastActive = false;
        return LINE_FOLLOW_RUN_DONE;
      }
    }
    else
    {
      s_intersectionHitCount = 0;
    }
  }
  else
  {
    s_intersectionHitCount = 0;
  }

  if (millis() - s_lineFollowFastStartMs > MAX_CELL_DRIVE_TIME)
  {
    lineSensors.readLineBlack();
    printLineFollowDebug("timeout");
    stopMotors();
    setMappingError("line timeout");
    s_lineFollowDecelBeforeTurn = false;
    s_followBaseSpeed = LINE_BASE_SPEED;
    s_lineFollowFastActive = false;
    return LINE_FOLLOW_RUN_ERROR;
  }

  return LINE_FOLLOW_RUN_RUNNING;
}

// ============================================================
// Drehungen
// ============================================================
struct EncoderTurnState
{
  bool active;
  bool settling;
  int targetHeading;
  int turnSign;
  int lineSearchDir;
  int32_t targetCounts;
  int32_t startLeft;
  int32_t startRight;
  int32_t lastProgress;
  uint32_t startedMs;
  uint32_t lastProgressMs;
  uint32_t settleUntilMs;
  uint32_t timeoutMs;
};

static EncoderTurnState s_turn = {};
const int32_t TURN_DIRECTION_CHECK_COUNTS = 8;
const int32_t TURN_MIN_WHEEL_PROGRESS_DIVISOR = 3;

static int turnSpeedForRemaining(int32_t remaining)
{
  if (remaining <= 0)
    return 0;
  if (remaining >= TURN_SLOW_COUNTS)
    return TURN_SPEED;

  return TURN_SLOW_SPEED +
         (int)((int32_t)(TURN_SPEED - TURN_SLOW_SPEED) * remaining /
               TURN_SLOW_COUNTS);
}

void turnBegin(int targetHeading)
{
  turnAbort();

  targetHeading = ((targetHeading % 4) + 4) % 4;
  const int diff = (targetHeading - heading + 4) % 4;
  if (diff == 0)
    return;

  s_turn.active = true;
  s_turn.settling = false;
  s_turn.targetHeading = targetHeading;
  s_turn.turnSign = (diff == 3) ? -1 : +1;
  s_turn.lineSearchDir = (diff == 2) ? 0 : s_turn.turnSign;
  s_turn.targetCounts = (diff == 2) ? 2 * TURN_90_COUNTS : TURN_90_COUNTS;
  s_turn.startLeft = Encoders::getCountsLeft();
  s_turn.startRight = Encoders::getCountsRight();
  s_turn.lastProgress = 0;
  s_turn.startedMs = millis();
  s_turn.lastProgressMs = s_turn.startedMs;
  s_turn.timeoutMs =
    (diff == 2) ? TURN_TIMEOUT_180_MS : TURN_TIMEOUT_90_MS;
  s_bgLastMs = s_turn.startedMs;

  Serial.print("[Turn] begin from=");
  Serial.print(heading);
  Serial.print(" target=");
  Serial.print(targetHeading);
  Serial.print(" counts=");
  Serial.println(s_turn.targetCounts);
}

TurnRunStatus turnStep()
{
  if (!s_turn.active)
    return TURN_RUN_IDLE;

  callBgUpdate();

  if (g_abortRequested || g_btnAPressed)
  {
    stopMotors();
    s_turn = {};
    return TURN_RUN_ABORTED;
  }

  const uint32_t now = millis();
  if (s_turn.settling)
  {
    if ((int32_t)(now - s_turn.settleUntilMs) < 0)
      return TURN_RUN_RUNNING;

    heading = s_turn.targetHeading;
    setLineSearchDir(s_turn.lineSearchDir);
    Serial.print("[Turn] done heading=");
    Serial.println(heading);
    s_turn = {};
    return TURN_RUN_DONE;
  }

  const int32_t leftDelta =
    Encoders::getCountsLeft() - s_turn.startLeft;
  const int32_t rightDelta =
    Encoders::getCountsRight() - s_turn.startRight;
  const int32_t leftProgress =
    leftDelta < 0 ? -leftDelta : leftDelta;
  const int32_t rightProgress =
    rightDelta < 0 ? -rightDelta : rightDelta;
  const int32_t rotationDelta = leftDelta - rightDelta;
  const int32_t rotationProgress =
    (rotationDelta < 0 ? -rotationDelta : rotationDelta) / 2;

  if (leftProgress >= TURN_DIRECTION_CHECK_COUNTS &&
      rightProgress >= TURN_DIRECTION_CHECK_COUNTS &&
      ((leftDelta > 0) == (rightDelta > 0)))
  {
    Serial.print("[Turn] encoder direction mismatch leftDelta=");
    Serial.print(leftDelta);
    Serial.print(" rightDelta=");
    Serial.println(rightDelta);
    stopMotors();
    s_turn = {};
    return TURN_RUN_ERROR;
  }

  if (rotationProgress > s_turn.lastProgress)
  {
    s_turn.lastProgress = rotationProgress;
    s_turn.lastProgressMs = now;
  }

  if ((uint32_t)(now - s_turn.startedMs) > s_turn.timeoutMs ||
      (uint32_t)(now - s_turn.lastProgressMs) > TURN_STALL_MS)
  {
    Serial.print("[Turn] error left=");
    Serial.print(leftProgress);
    Serial.print(" right=");
    Serial.println(rightProgress);
    stopMotors();
    s_turn = {};
    return TURN_RUN_ERROR;
  }

  const int32_t remaining = s_turn.targetCounts - rotationProgress;

  if (remaining <= TURN_TOLERANCE_COUNTS &&
      leftProgress >=
        s_turn.targetCounts / TURN_MIN_WHEEL_PROGRESS_DIVISOR &&
      rightProgress >=
        s_turn.targetCounts / TURN_MIN_WHEEL_PROGRESS_DIVISOR)
  {
    stopMotors();
    s_turn.settling = true;
    s_turn.settleUntilMs = now + TURN_REST_MS;
    return TURN_RUN_RUNNING;
  }

  const int turnMagnitude = turnSpeedForRemaining(remaining);

  // Bei einer idealen Punktdrehung sind die signierten Radwege gleich gross
  // und entgegengesetzt, ihre Summe bleibt also null. Ein von null
  // abweichender Wert bedeutet eine translatorische Vor-/Rueckwaertsbewegung.
  // Diese wird als gemeinsamer PWM-Anteil auf beiden Motoren ausgeregelt.
  const int32_t forwardDrift = leftDelta + rightDelta;
  int centerCorrection =
    (int)(-TURN_CENTER_HOLD_KP * (float)forwardDrift);
  if (centerCorrection > TURN_CENTER_HOLD_MAX_PWM)
    centerCorrection = TURN_CENTER_HOLD_MAX_PWM;
  if (centerCorrection < -TURN_CENTER_HOLD_MAX_PWM)
    centerCorrection = -TURN_CENTER_HOLD_MAX_PWM;

  motors.setSpeeds(s_turn.turnSign * turnMagnitude + centerCorrection,
                   -s_turn.turnSign * turnMagnitude + centerCorrection);
  return TURN_RUN_RUNNING;
}

void turnAbort()
{
  if (s_turn.active)
    stopMotors();
  s_turn = {};
}

bool turnIsActive()
{
  return s_turn.active;
}

void turnTo(int targetHeading)
{
  turnBegin(targetHeading);
  while (turnIsActive())
  {
    const TurnRunStatus status = turnStep();
    if (status == TURN_RUN_ABORTED || status == TURN_RUN_ERROR)
      return;
    delay(5);
  }
}

// ============================================================
// Nicht-blockierender 90°-EINRAD-PIVOT (Klothoide verworfen)
// ============================================================
// Sobald die Maus auf der Kreuzung ist, faehrt nur das kurvenAEUSSERE Rad,
// das innere steht → Pivot um das innere Rad bis 90°.
//   Rechtsdrehung → linkes Rad aussen; Linksdrehung → rechtes Rad aussen.
// Ausstieg ENCODER-geschlossen: 90° ↔ |aussenDelta − innenDelta| =
// 2*TURN_90_COUNTS (innen steht → aeusseres Rad faehrt 2*TURN_90_COUNTS).
// Sicherheit: Gesamt-Timeout + Stall-Erkennung → ABORTED.
// Bei DONE ist heading aktualisiert und die Linien-Suchrichtung gesetzt;
// der Aufrufer faehrt per lineFollowFast(Continuation) die Restzelle.
struct CurveTurnState
{
  bool     active;
  bool     turnRight;
  int      targetHeading;
  uint32_t startMs;          // Pivot-Start (Watchdog)
  int32_t  lastRotProgress;  // bisher max. erreichte Rotation (Stall-Schutz)
  uint32_t lastProgressMs;
};
static CurveTurnState s_curve = {};

// Encoder-Stand beim Pivot-Start (= Ausgangskreuzung). Bleibt nach dem
// Pivot erhalten, damit die anschliessende Restzellen-Fahrt ihre Wegmessung
// KONTINUIERLICH von der Kreuzung fortsetzen kann statt bei 0 neu zu
// beginnen. So erkennt der Wegstrecken-Schwellwert die naechste Kreuzung
// korrekt (sonst zaehlt er erst ab Pivot-Ende → zu spaet).
static int32_t s_curveOriginLeft  = 0;
static int32_t s_curveOriginRight = 0;

// Pivot: das kurvenAEUSSERE Rad faehrt vorwaerts, das innere dreht GANZ MINIMAL
// rueckwaerts (invertiert, wie bei der Punktdrehung – nur schwach). Der gemein-
// same Drehpunkt wandert dadurch etwas vom inneren Rad nach innen → engerer,
// zentrierterer Pivot. Vorzeichen: +PWM = vorwaerts.
// Rechtsdrehung → linkes Rad aussen (vor), rechtes innen (minimal zurueck);
// Linksdrehung umgekehrt.
static void curveSetMotors()
{
  if (s_curve.turnRight)
    motors.setSpeeds(CURVE_OUTER_SPEED, -CURVE_INNER_SPEED);  // links vor, rechts minimal zurueck
  else
    motors.setSpeeds(-CURVE_INNER_SPEED, CURVE_OUTER_SPEED);  // rechts vor, links minimal zurueck
}

// Aktuelle Rotation des Bogens in Encoder-Counts (Raddifferenz, immer >= 0).
// 90° entsprechen 2*TURN_90_COUNTS.
//
// Rotation = Aussenrad-Delta − Innenrad-Delta; der gemeinsame Vorwaertsanteil
// hebt sich auf. BETRAG, weil die Encoder-Vorwaertskonvention (forward = +/-
// Counts) hardware-/editionsabhaengig ist – beide Raeder teilen sie aber
// (lineFollowForwardTravelCounts() bildet die Summe und funktioniert).
// Genau wie die Punktdrehung (turnStep) mit |delta| arbeitet, sind wir damit
// unabhaengig vom Vorzeichen.
static int32_t curveRotationCounts()
{
  const int32_t leftDelta  = Encoders::getCountsLeft()  - s_curveOriginLeft;
  const int32_t rightDelta = Encoders::getCountsRight() - s_curveOriginRight;
  const int32_t outer = s_curve.turnRight ? leftDelta : rightDelta;
  const int32_t inner = s_curve.turnRight ? rightDelta : leftDelta;
  const int32_t rot = outer - inner;
  return rot < 0 ? -rot : rot;
}

void curveTurnBegin(bool turnRight)
{
  s_curve = {};
  s_curve.active        = true;
  s_curve.turnRight     = turnRight;
  // NORTH=0,EAST=1,SOUTH=2,WEST=3 im Uhrzeigersinn → rechts = +1, links = +3.
  s_curve.targetHeading = turnRight ? (heading + 1) % 4 : (heading + 3) % 4;
  s_curve.startMs       = millis();
  s_curve.lastRotProgress = 0;
  s_curve.lastProgressMs  = s_curve.startMs;
  s_bgLastMs            = s_curve.startMs;
  // Encoder-Ausgangsposition (Kreuzung) merken – dient sowohl der
  // Rotationsmessung (Raddifferenz) als auch der kontinuierlichen
  // Wegmessung der Restzelle nach dem Pivot (Vorwaertsanteil).
  s_curveOriginLeft  = Encoders::getCountsLeft();
  s_curveOriginRight = Encoders::getCountsRight();
  curveSetMotors();
}

CurveRunStatus curveTurnStep()
{
  if (!s_curve.active)
    return CURVE_RUN_IDLE;

  if (g_abortRequested || g_btnAPressed)
  {
    stopMotors();
    s_curve = {};
    return CURVE_RUN_ABORTED;
  }

  callBgUpdate();

  const uint32_t now     = millis();
  const uint32_t elapsed = now - s_curve.startMs;
  const int32_t  rotTarget = 2 * TURN_90_COUNTS;
  const int32_t  rot       = curveRotationCounts();

  // 90° erreicht (Encoder) → Heading & Linien-Suchrichtung setzen, fertig.
  if (rot >= rotTarget)
  {
    heading = s_curve.targetHeading;
    setLineSearchDir(s_curve.turnRight ? +1 : -1);
    s_curve = {};
    return CURVE_RUN_DONE;
  }

  // Stall-/Timeout-Schutz (Rad blockiert / Encoder liefert nichts).
  if (rot > s_curve.lastRotProgress)
  {
    s_curve.lastRotProgress = rot;
    s_curve.lastProgressMs  = now;
  }
  if (elapsed > CURVE_TIMEOUT_MS ||
      (uint32_t)(now - s_curve.lastProgressMs) > CURVE_STALL_MS)
  {
    stopMotors();
    s_curve = {};
    return CURVE_RUN_ABORTED;
  }

  // Pivot weiterfahren: nur das aeussere Rad, konstantes Tempo.
  curveSetMotors();

  return CURVE_RUN_RUNNING;
}

void curveTurnAbort()
{
  if (s_curve.active)
    stopMotors();
  s_curve = {};
}

bool curveTurnIsActive()
{
  return s_curve.active;
}

// Fortschritt 0..1 fuer die HUD-Animation: gemessene Rotation / 90°-Ziel.
float curveTurnProgress()
{
  if (!s_curve.active)
    return 0.0f;

  const int32_t rotTarget = 2 * TURN_90_COUNTS;
  if (rotTarget <= 0)
    return 1.0f;

  float p = (float)curveRotationCounts() / (float)rotTarget;
  if (p < 0.0f) p = 0.0f;
  if (p > 1.0f) p = 1.0f;
  return p;
}

enum NavigationDrivePhase
{
  NAV_DRIVE_IDLE,
  NAV_DRIVE_TURNING,
  NAV_DRIVE_CURVING,
  NAV_DRIVE_FOLLOWING
};

static NavigationDrivePhase s_navigationDrivePhase = NAV_DRIVE_IDLE;
static bool s_navigationCenterAndStop = false;
static bool s_navigationCellStartValid = false;
static bool s_navigationCellRetreatPrepared = false;
static int32_t s_navigationCellStartLeft = 0;
static int32_t s_navigationCellStartRight = 0;
static int s_navigationCellStartHeading = NORTH;

struct CellTrajectorySample
{
  int32_t left;
  int32_t right;
  NavigationDrivePhase phase;
};

static CellTrajectorySample
  s_cellTrajectory[CELL_TRAJECTORY_MAX_SAMPLES] = {};
static uint8_t s_cellTrajectoryCount = 0;
static uint32_t s_cellTrajectoryLastSampleMs = 0;
static bool s_cellTrajectoryOverflow = false;

// Zuletzt waehrend einer Geradeausfahrt gelernte Encoder-Vorwaertspolaritaet
// pro Rad (Hardware-Eigenschaft, pro Roboter konstant). Dient als Rueckfall,
// wenn der Fang mitten in einer Punktdrehung passiert und die aktuelle Zelle
// keine translatorische Bewegung enthaelt, aus der sich die Polaritaet
// ableiten liesse. 0 = noch nicht gelernt.
static int8_t s_translationPolarityLeft = 0;
static int8_t s_translationPolarityRight = 0;

struct CellRetreatState
{
  bool active;
  bool done;
  int16_t sampleIndex;
  int32_t targetLeft;
  int32_t targetRight;
  int32_t segmentStartLeft;
  int32_t segmentStartRight;
  int32_t lastRemaining;
  int32_t lastObservedLeft;
  int32_t lastObservedRight;
  int32_t polarityReferenceLeft;
  int32_t polarityReferenceRight;
  int8_t leftPolarity;
  int8_t rightPolarity;
  bool leftPolarityCorrected;
  bool rightPolarityCorrected;
  bool leftPolarityVerified;
  bool rightPolarityVerified;
  uint32_t leftPolarityCheckMs;
  uint32_t rightPolarityCheckMs;
  int startHeading;
  uint32_t startedMs;
  uint32_t lastProgressMs;
};

static CellRetreatState s_cellRetreat = {};
static const char *s_cellRetreatLastError = "";

const char *navigationCellRetreatLastError()
{
  return s_cellRetreatLastError;
}

static bool trajectoryPhaseIsForwardTranslation(NavigationDrivePhase phase)
{
  return phase == NAV_DRIVE_FOLLOWING;
}

static bool appendCellTrajectorySample(bool force)
{
  if (!s_navigationCellStartValid)
    return false;

  const uint32_t now = millis();
  if (!force &&
      (uint32_t)(now - s_cellTrajectoryLastSampleMs) <
        CELL_TRAJECTORY_INTERVAL_MS)
  {
    return true;
  }

  const int32_t left = Encoders::getCountsLeft();
  const int32_t right = Encoders::getCountsRight();
  if (s_cellTrajectoryCount > 0)
  {
    const CellTrajectorySample &last =
      s_cellTrajectory[s_cellTrajectoryCount - 1];

    // Vorwaertspolaritaet pro Rad mitlernen, solange wir uns translatorisch
    // (also NICHT in einer Punktdrehung) bewegen. In einer Punktdrehung laufen
    // die Raeder gegenlaeufig; in einem Pivot/Kurvenbogen laeuft mindestens
    // ein Rad nicht wie bei normaler Vorwaertsfahrt. Beides darf die
    // Hardware-Polaritaet nicht neu lernen.
    if (trajectoryPhaseIsForwardTranslation(last.phase) &&
        trajectoryPhaseIsForwardTranslation(s_navigationDrivePhase))
    {
      const int32_t dl = left - last.left;
      const int32_t dr = right - last.right;
      if (dl > CELL_RETREAT_TOLERANCE_COUNTS ||
          dl < -CELL_RETREAT_TOLERANCE_COUNTS)
        s_translationPolarityLeft = dl > 0 ? 1 : -1;
      if (dr > CELL_RETREAT_TOLERANCE_COUNTS ||
          dr < -CELL_RETREAT_TOLERANCE_COUNTS)
        s_translationPolarityRight = dr > 0 ? 1 : -1;
    }

    if (last.left == left && last.right == right &&
        last.phase == s_navigationDrivePhase)
    {
      s_cellTrajectoryLastSampleMs = now;
      return true;
    }
  }

  if (s_cellTrajectoryCount >= CELL_TRAJECTORY_MAX_SAMPLES)
  {
    s_cellTrajectoryOverflow = true;
    if (force)
    {
      s_cellTrajectory[CELL_TRAJECTORY_MAX_SAMPLES - 1] = {
        left, right, s_navigationDrivePhase
      };
      s_cellTrajectoryLastSampleMs = now;
    }
    return false;
  }

  s_cellTrajectory[s_cellTrajectoryCount++] = {
    left, right, s_navigationDrivePhase
  };
  s_cellTrajectoryLastSampleMs = now;
  return true;
}

static int32_t signedAbs(int32_t value)
{
  return value < 0 ? -value : value;
}

static int8_t encoderPolarityFromTrajectory(bool left)
{
  // Nur echte Linienfolge-Segmente verwenden. Punktdrehungen und Pivots/
  // Kurvenboegen verfremden mindestens ein Encoder-Vorzeichen und duerfen
  // deshalb nicht als translatorische Vorwaertspolaritaet gelten.
  for (int i = (int)s_cellTrajectoryCount - 1; i > 0; i--)
  {
    if (!trajectoryPhaseIsForwardTranslation(s_cellTrajectory[i].phase) ||
        !trajectoryPhaseIsForwardTranslation(s_cellTrajectory[i - 1].phase))
    {
      continue;
    }
    const int32_t delta =
      (left ? s_cellTrajectory[i].left : s_cellTrajectory[i].right) -
      (left ? s_cellTrajectory[i - 1].left : s_cellTrajectory[i - 1].right);
    if (signedAbs(delta) > CELL_RETREAT_TOLERANCE_COUNTS)
      return delta > 0 ? 1 : -1;
  }

  // Keine translatorische Bewegung in dieser Zelle (z.B. Fang mitten in einer
  // Punktdrehung). Auf die zuletzt waehrend einer Geradeausfahrt gelernte
  // Hardware-Polaritaet zurueckgreifen - diese ist pro Roboter konstant.
  const int8_t cached =
    left ? s_translationPolarityLeft : s_translationPolarityRight;
  if (cached != 0)
    return cached;

  const int32_t start =
    left ? s_navigationCellStartLeft : s_navigationCellStartRight;
  const int32_t current =
    left ? Encoders::getCountsLeft() : Encoders::getCountsRight();
  const int32_t totalDelta = current - start;
  if (signedAbs(totalDelta) > CELL_RETREAT_TOLERANCE_COUNTS)
    return totalDelta > 0 ? 1 : -1;

  return 0;
}

static int retreatSpeedForRemaining(int32_t remaining)
{
  if (remaining <= CELL_RETREAT_TOLERANCE_COUNTS)
    return 0;
  if (remaining >= CELL_RETREAT_SLOW_COUNTS)
    return CELL_RETREAT_SPEED;

  return CELL_RETREAT_SLOW_SPEED +
         (int)((int32_t)(CELL_RETREAT_SPEED - CELL_RETREAT_SLOW_SPEED) *
               remaining / CELL_RETREAT_SLOW_COUNTS);
}

static bool retreatTargetReached(int32_t segmentStart,
                                 int32_t current,
                                 int32_t target)
{
  if (signedAbs(target - current) <= CELL_RETREAT_TOLERANCE_COUNTS)
    return true;

  const int32_t segmentDelta = target - segmentStart;
  const int32_t remainingDelta = target - current;
  return segmentDelta != 0 &&
         ((segmentDelta > 0 && remainingDelta < 0) ||
          (segmentDelta < 0 && remainingDelta > 0));
}

static int32_t retreatSegmentProgress(int32_t segmentStart,
                                      int32_t current,
                                      int32_t target)
{
  const int32_t distance = signedAbs(target - segmentStart);
  if (distance <= CELL_RETREAT_TOLERANCE_COUNTS)
    return 1024;

  int32_t progress =
    signedAbs(current - segmentStart) * 1024 / distance;
  if (progress < 0) progress = 0;
  if (progress > 1024) progress = 1024;
  return progress;
}

static int clampRetreatMagnitude(int magnitude)
{
  if (magnitude < CELL_RETREAT_SLOW_SPEED)
    return CELL_RETREAT_SLOW_SPEED;
  if (magnitude > CELL_RETREAT_SYNC_MAX_SPEED)
    return CELL_RETREAT_SYNC_MAX_SPEED;
  return magnitude;
}

// Wenn true, faehrt navigationDriveCellBegin 90°-Abbiegungen als fliessenden
// Bogen statt als Punktdrehung. Der AUFRUFER ist dafuer verantwortlich, das
// nur zu setzen, wenn die Maus tatsaechlich rollt (sonst lurcht sie aus dem
// Stand in den Bogen). Default false → unveraendertes Punktdreh-Verhalten.
static bool s_navigationAllowCurve = false;

void navigationDriveCellSetAllowCurve(bool enable)
{
  s_navigationAllowCurve = enable;
}

void navigationDriveCellBegin(int direction, bool centerAndStop)
{
  navigationDriveCellAbort();
  navigationCellRetreatAbort();
  s_navigationCellStartLeft = Encoders::getCountsLeft();
  s_navigationCellStartRight = Encoders::getCountsRight();
  s_navigationCellStartHeading = heading;
  s_navigationCellStartValid = true;
  s_navigationCellRetreatPrepared = false;
  s_cellTrajectoryCount = 0;
  s_cellTrajectoryOverflow = false;
  s_cellTrajectoryLastSampleMs = millis();
  appendCellTrajectorySample(true);
  s_navigationCenterAndStop = centerAndStop;

  // Bogen-Flag ist EINMALIG: jeder Aufrufer muss es vor jedem Begin neu
  // setzen. So koennen Aufrufer ohne Bogen-Wunsch (z.B. Ghost/Mapping)
  // nie versehentlich ein stehengebliebenes true erben.
  const bool allowCurve = s_navigationAllowCurve;
  s_navigationAllowCurve = false;

  // Fliessender Bogen statt Punktdrehung – nur bei 90°-Abbiegungen
  // (diff 1 = rechts, 3 = links; 180° und Geradeaus ausgeschlossen).
  const int diff = ((direction - heading) % 4 + 4) % 4;
  if (allowCurve && (diff == 1 || diff == 3))
  {
    s_navigationDrivePhase = NAV_DRIVE_CURVING;
    appendCellTrajectorySample(true);
    curveTurnBegin(diff == 1);
    return;
  }

  turnBegin(direction);
  if (turnIsActive())
  {
    s_navigationDrivePhase = NAV_DRIVE_TURNING;
    appendCellTrajectorySample(true);
    return;
  }

  s_navigationDrivePhase = NAV_DRIVE_FOLLOWING;
  appendCellTrajectorySample(true);
  lineFollowFastBegin(centerAndStop);
}

void navigationDriveCellRequestCenterStop()
{
  navigationDriveCellSetCenterStop(true);
}

void navigationDriveCellSetCenterStop(bool centerAndStop)
{
  s_navigationCenterAndStop = centerAndStop;
  if (s_navigationDrivePhase == NAV_DRIVE_FOLLOWING)
    s_lineFollowFastCenterAndStop = centerAndStop;
}

float navigationDriveCellProgress()
{
  if (s_navigationDrivePhase != NAV_DRIVE_FOLLOWING)
    return 0.0f;

  float progress =
    (float)lineFollowForwardTravelCounts() / CELL_TRAVEL_COUNTS;
  if (progress < 0.0f) progress = 0.0f;
  if (progress > 1.0f) progress = 1.0f;
  return progress;
}

float navigationDriveCellTurnProgress()
{
  if (s_navigationDrivePhase == NAV_DRIVE_CURVING)
    return curveTurnProgress();

  if (s_navigationDrivePhase != NAV_DRIVE_TURNING || !s_turn.active)
    return 0.0f;
  if (s_turn.settling)
    return 1.0f;
  if (s_turn.targetCounts <= 0)
    return 1.0f;

  const int32_t rotationDelta =
    (Encoders::getCountsLeft() - s_turn.startLeft) -
    (Encoders::getCountsRight() - s_turn.startRight);
  float progress =
    (float)(rotationDelta < 0 ? -rotationDelta : rotationDelta) /
    (2.0f * (float)s_turn.targetCounts);
  if (progress < 0.0f) progress = 0.0f;
  if (progress > 1.0f) progress = 1.0f;
  return progress;
}

bool navigationDriveCellIsTurning()
{
  if (s_navigationDrivePhase == NAV_DRIVE_CURVING)
    return curveTurnIsActive();
  return s_navigationDrivePhase == NAV_DRIVE_TURNING && s_turn.active;
}

LineFollowRunStatus navigationDriveCellStep()
{
  if (s_navigationDrivePhase == NAV_DRIVE_IDLE)
    return LINE_FOLLOW_RUN_IDLE;

  appendCellTrajectorySample(false);

  if (s_navigationDrivePhase == NAV_DRIVE_CURVING)
  {
    const CurveRunStatus curveStatus = curveTurnStep();
    if (curveStatus == CURVE_RUN_RUNNING)
      return LINE_FOLLOW_RUN_RUNNING;
    if (curveStatus == CURVE_RUN_ABORTED)
    {
      s_navigationDrivePhase = NAV_DRIVE_IDLE;
      return LINE_FOLLOW_RUN_ABORTED;
    }
    appendCellTrajectorySample(true);
    // CURVE_RUN_DONE: Maus rollt bereits in der neuen Richtung. Restzelle
    // bis zur naechsten Kreuzung per Linienfolger fahren (faengt die neue
    // Linie ueber die vom Bogen gesetzte Suchrichtung wieder ein).
    // KONTINUIERLICHE Wegmessung ab der Ausgangskreuzung (s_curveOrigin*),
    // damit die schon im Bogen zurueckgelegte Strecke mitzaehlt und die
    // naechste Kreuzung nicht zu spaet erkannt wird.
    lineFollowFastBeginContinuation(s_navigationCenterAndStop,
                                    s_curveOriginLeft, s_curveOriginRight);
    s_navigationDrivePhase = NAV_DRIVE_FOLLOWING;
    appendCellTrajectorySample(true);
  }
  else if (s_navigationDrivePhase == NAV_DRIVE_TURNING)
  {
    const TurnRunStatus turnStatus = turnStep();
    if (turnStatus == TURN_RUN_RUNNING)
      return LINE_FOLLOW_RUN_RUNNING;
    if (turnStatus == TURN_RUN_ABORTED)
    {
      s_navigationDrivePhase = NAV_DRIVE_IDLE;
      return LINE_FOLLOW_RUN_ABORTED;
    }
    if (turnStatus != TURN_RUN_DONE)
    {
      s_navigationDrivePhase = NAV_DRIVE_IDLE;
      return LINE_FOLLOW_RUN_ERROR;
    }

    appendCellTrajectorySample(true);
    s_navigationDrivePhase = NAV_DRIVE_FOLLOWING;
    appendCellTrajectorySample(true);
    lineFollowFastBegin(s_navigationCenterAndStop);
  }

  const LineFollowRunStatus status = lineFollowFastStep();
  if (status != LINE_FOLLOW_RUN_RUNNING)
  {
    s_navigationDrivePhase = NAV_DRIVE_IDLE;
    if (status == LINE_FOLLOW_RUN_DONE)
    {
      s_navigationCellStartValid = false;
      s_navigationCellRetreatPrepared = false;
    }
  }
  return status;
}

void navigationDriveCellAbort()
{
  turnAbort();
  curveTurnAbort();
  if (s_lineFollowFastActive)
    stopMotors();
  s_lineFollowFastActive = false;
  s_navigationDrivePhase = NAV_DRIVE_IDLE;
}

void navigationCellRetreatPrepare()
{
  s_cellRetreatLastError = "";
  if (s_navigationCellStartValid &&
      s_navigationDrivePhase != NAV_DRIVE_IDLE)
  {
    appendCellTrajectorySample(true);
    s_navigationCellRetreatPrepared = true;
    Serial.print("[Retreat] prepared phase=");
    Serial.print((int)s_navigationDrivePhase);
    Serial.print(" samples=");
    Serial.print(s_cellTrajectoryCount);
    Serial.print(" startHeading=");
    Serial.println(s_navigationCellStartHeading);
    return;
  }

  s_cellRetreatLastError = "no active cell";
  Serial.print("[Retreat] prepare rejected valid=");
  Serial.print(s_navigationCellStartValid ? 1 : 0);
  Serial.print(" phase=");
  Serial.println((int)s_navigationDrivePhase);
}

void navigationCellRetreatPrepareAllowStationary()
{
  navigationCellRetreatPrepare();
  if (s_navigationCellRetreatPrepared ||
      s_navigationDrivePhase != NAV_DRIVE_IDLE)
  {
    return;
  }

  s_navigationCellStartLeft = Encoders::getCountsLeft();
  s_navigationCellStartRight = Encoders::getCountsRight();
  s_navigationCellStartHeading = heading;
  s_navigationCellStartValid = true;
  s_navigationCellRetreatPrepared = true;
  s_cellTrajectoryCount = 0;
  s_cellTrajectoryOverflow = false;
  s_cellTrajectoryLastSampleMs = millis();
  appendCellTrajectorySample(true);
  s_cellRetreatLastError = "";
  Serial.println("[Retreat] prepared stationary pose");
}

NavigationRetreatBeginStatus navigationCellRetreatBegin()
{
  if (!s_navigationCellStartValid || !s_navigationCellRetreatPrepared)
  {
    if (s_cellRetreatLastError[0] == '\0')
      s_cellRetreatLastError = "not prepared";
    Serial.println("[Retreat] no prepared cell movement");
    return NAV_RETREAT_BEGIN_ERROR;
  }

  navigationDriveCellAbort();
  stopMotors();

  appendCellTrajectorySample(true);
  const int32_t currentLeft = Encoders::getCountsLeft();
  const int32_t currentRight = Encoders::getCountsRight();
  const int32_t totalLeft =
    signedAbs(s_navigationCellStartLeft - currentLeft);
  const int32_t totalRight =
    signedAbs(s_navigationCellStartRight - currentRight);
  if (totalLeft <= CELL_RETREAT_TOLERANCE_COUNTS &&
      totalRight <= CELL_RETREAT_TOLERANCE_COUNTS)
  {
    heading = s_navigationCellStartHeading;
    s_navigationCellStartValid = false;
    s_navigationCellRetreatPrepared = false;
    s_cellRetreatLastError = "";
    Serial.println("[Retreat] stationary at cell origin");
    return NAV_RETREAT_BEGIN_STATIONARY;
  }

  // Auch reine Dreh-/Kurvenbewegungen werden rueckwaerts abgefahren.
  // Der alte stationaere Shortcut konnte die physische Ausrichtung um 90 Grad
  // stehen lassen, waehrend die Software bereits einen Heading gesetzt hatte.

  if (s_cellTrajectoryCount < 2)
  {
    s_cellRetreatLastError = "invalid trajectory";
    Serial.println("[Retreat] invalid trajectory");
    s_navigationCellStartValid = false;
    s_navigationCellRetreatPrepared = false;
    return NAV_RETREAT_BEGIN_ERROR;
  }

  const int16_t sampleIndex = (int16_t)s_cellTrajectoryCount - 2;
  const int32_t targetLeft = s_cellTrajectory[sampleIndex].left;
  const int32_t targetRight = s_cellTrajectory[sampleIndex].right;
  const int32_t remainingLeft = signedAbs(targetLeft - currentLeft);
  const int32_t remainingRight = signedAbs(targetRight - currentRight);
  const uint32_t now = millis();

  s_cellRetreat.active = true;
  s_cellRetreat.done = false;
  s_cellRetreat.sampleIndex = sampleIndex;
  s_cellRetreat.targetLeft = targetLeft;
  s_cellRetreat.targetRight = targetRight;
  s_cellRetreat.segmentStartLeft = currentLeft;
  s_cellRetreat.segmentStartRight = currentRight;
  s_cellRetreat.lastRemaining = remainingLeft + remainingRight;
  s_cellRetreat.lastObservedLeft = currentLeft;
  s_cellRetreat.lastObservedRight = currentRight;
  s_cellRetreat.polarityReferenceLeft = remainingLeft;
  s_cellRetreat.polarityReferenceRight = remainingRight;
  const int8_t inferredLeftPolarity = encoderPolarityFromTrajectory(true);
  const int8_t inferredRightPolarity = encoderPolarityFromTrajectory(false);
  s_cellRetreat.leftPolarity =
    inferredLeftPolarity != 0 ? inferredLeftPolarity : 1;
  s_cellRetreat.rightPolarity =
    inferredRightPolarity != 0 ? inferredRightPolarity : 1;
  s_cellRetreat.leftPolarityCorrected = false;
  s_cellRetreat.rightPolarityCorrected = false;
  s_cellRetreat.leftPolarityVerified = inferredLeftPolarity != 0;
  s_cellRetreat.rightPolarityVerified = inferredRightPolarity != 0;
  s_cellRetreat.leftPolarityCheckMs =
    now + CELL_RETREAT_POLARITY_CHECK_MS;
  s_cellRetreat.rightPolarityCheckMs =
    now + CELL_RETREAT_POLARITY_CHECK_MS;
  s_cellRetreat.startHeading = s_navigationCellStartHeading;
  s_cellRetreat.startedMs = now;
  s_cellRetreat.lastProgressMs = now;
  s_cellRetreatLastError = "";
  s_navigationCellStartValid = false;
  s_navigationCellRetreatPrepared = false;

  Serial.print("[Retreat] begin left=");
  Serial.print(currentLeft);
  Serial.print(" samples=");
  Serial.print(s_cellTrajectoryCount);
  Serial.print(" right=");
  Serial.print(currentRight);
  Serial.print(" polarity=");
  Serial.print((int)s_cellRetreat.leftPolarity);
  Serial.print(',');
  Serial.println((int)s_cellRetreat.rightPolarity);
  if (s_cellTrajectoryOverflow)
    Serial.println("[Retreat] trajectory truncated");
  return NAV_RETREAT_BEGIN_STARTED;
}

LineFollowRunStatus navigationCellRetreatStep()
{
  if (s_cellRetreat.done)
    return LINE_FOLLOW_RUN_DONE;
  if (!s_cellRetreat.active)
    return LINE_FOLLOW_RUN_IDLE;

  if (g_abortRequested || g_btnAPressed)
  {
    s_cellRetreatLastError =
      g_abortRequested ? "abort requested" : "button abort";
    Serial.print("[Retreat] aborted reason=");
    Serial.println(s_cellRetreatLastError);
    stopMotors();
    s_cellRetreat = {};
    return LINE_FOLLOW_RUN_ABORTED;
  }

  const int32_t currentLeft = Encoders::getCountsLeft();
  const int32_t currentRight = Encoders::getCountsRight();
  const int32_t errorLeft = s_cellRetreat.targetLeft - currentLeft;
  const int32_t errorRight = s_cellRetreat.targetRight - currentRight;
  const int32_t remainingLeft = signedAbs(errorLeft);
  const int32_t remainingRight = signedAbs(errorRight);
  const bool leftDone =
    retreatTargetReached(s_cellRetreat.segmentStartLeft,
                         currentLeft,
                         s_cellRetreat.targetLeft);
  const bool rightDone =
    retreatTargetReached(s_cellRetreat.segmentStartRight,
                         currentRight,
                         s_cellRetreat.targetRight);
  const uint32_t now = millis();

  if (!leftDone &&
      !s_cellRetreat.leftPolarityVerified &&
      (int32_t)(now - s_cellRetreat.leftPolarityCheckMs) >= 0)
  {
    if (remainingLeft >
        s_cellRetreat.polarityReferenceLeft + 4)
    {
      if (s_cellRetreat.leftPolarityCorrected)
      {
        s_cellRetreatLastError = "left encoder direction";
        stopMotors();
        s_cellRetreat = {};
        Serial.println("[Retreat] left encoder direction error");
        return LINE_FOLLOW_RUN_ERROR;
      }
      s_cellRetreat.leftPolarity = -s_cellRetreat.leftPolarity;
      s_cellRetreat.leftPolarityCorrected = true;
      s_cellRetreat.polarityReferenceLeft = remainingLeft;
      s_cellRetreat.leftPolarityCheckMs =
        now + CELL_RETREAT_POLARITY_CHECK_MS;
    }
    else
    {
      s_cellRetreat.leftPolarityVerified = true;
    }
  }

  if (!rightDone &&
      !s_cellRetreat.rightPolarityVerified &&
      (int32_t)(now - s_cellRetreat.rightPolarityCheckMs) >= 0)
  {
    if (remainingRight >
        s_cellRetreat.polarityReferenceRight + 4)
    {
      if (s_cellRetreat.rightPolarityCorrected)
      {
        s_cellRetreatLastError = "right encoder direction";
        stopMotors();
        s_cellRetreat = {};
        Serial.println("[Retreat] right encoder direction error");
        return LINE_FOLLOW_RUN_ERROR;
      }
      s_cellRetreat.rightPolarity = -s_cellRetreat.rightPolarity;
      s_cellRetreat.rightPolarityCorrected = true;
      s_cellRetreat.polarityReferenceRight = remainingRight;
      s_cellRetreat.rightPolarityCheckMs =
        now + CELL_RETREAT_POLARITY_CHECK_MS;
    }
    else
    {
      s_cellRetreat.rightPolarityVerified = true;
    }
  }

  if (leftDone && rightDone)
  {
    s_cellRetreat.sampleIndex--;
    if (s_cellRetreat.sampleIndex < 0)
    {
      stopMotors();
      heading = s_cellRetreat.startHeading;
      s_cellRetreat.active = false;
      s_cellRetreat.done = true;
      Serial.print("[Retreat] done left=");
      Serial.print(currentLeft);
      Serial.print(" right=");
      Serial.print(currentRight);
      Serial.print(" heading=");
      Serial.println(heading);
      return LINE_FOLLOW_RUN_DONE;
    }

    const CellTrajectorySample &next =
      s_cellTrajectory[s_cellRetreat.sampleIndex];
    s_cellRetreat.segmentStartLeft = currentLeft;
    s_cellRetreat.segmentStartRight = currentRight;
    s_cellRetreat.targetLeft = next.left;
    s_cellRetreat.targetRight = next.right;
    s_cellRetreat.lastRemaining =
      signedAbs(next.left - currentLeft) +
      signedAbs(next.right - currentRight);
    s_cellRetreat.polarityReferenceLeft =
      signedAbs(next.left - currentLeft);
    s_cellRetreat.polarityReferenceRight =
      signedAbs(next.right - currentRight);
    if (!s_cellRetreat.leftPolarityVerified)
      s_cellRetreat.leftPolarityCheckMs =
        now + CELL_RETREAT_POLARITY_CHECK_MS;
    if (!s_cellRetreat.rightPolarityVerified)
      s_cellRetreat.rightPolarityCheckMs =
        now + CELL_RETREAT_POLARITY_CHECK_MS;
    s_cellRetreat.lastProgressMs = now;
    return LINE_FOLLOW_RUN_RUNNING;
  }

  const int32_t remaining = remainingLeft + remainingRight;
  const bool encoderMoved =
    signedAbs(currentLeft - s_cellRetreat.lastObservedLeft) > 1 ||
    signedAbs(currentRight - s_cellRetreat.lastObservedRight) > 1;
  if (encoderMoved)
  {
    s_cellRetreat.lastObservedLeft = currentLeft;
    s_cellRetreat.lastObservedRight = currentRight;
    s_cellRetreat.lastProgressMs = now;
  }
  if (remaining < s_cellRetreat.lastRemaining)
    s_cellRetreat.lastRemaining = remaining;

  if ((uint32_t)(now - s_cellRetreat.startedMs) >
        CELL_RETREAT_TIMEOUT_MS ||
      (uint32_t)(now - s_cellRetreat.lastProgressMs) >
        CELL_RETREAT_STALL_MS)
  {
    s_cellRetreatLastError =
      (uint32_t)(now - s_cellRetreat.lastProgressMs) >
        CELL_RETREAT_STALL_MS
        ? "encoder stall"
        : "retreat timeout";
    stopMotors();
    s_cellRetreat = {};
    Serial.print("[Retreat] error leftRemaining=");
    Serial.print(remainingLeft);
    Serial.print(" rightRemaining=");
    Serial.print(remainingRight);
    Serial.print(" reason=");
    Serial.println(s_cellRetreatLastError);
    return LINE_FOLLOW_RUN_ERROR;
  }

  int leftMagnitude =
    leftDone ? 0 : retreatSpeedForRemaining(remainingLeft);
  int rightMagnitude =
    rightDone ? 0 : retreatSpeedForRemaining(remainingRight);

  const int32_t segmentLeft =
    signedAbs(s_cellRetreat.targetLeft -
              s_cellRetreat.segmentStartLeft);
  const int32_t segmentRight =
    signedAbs(s_cellRetreat.targetRight -
              s_cellRetreat.segmentStartRight);
  if (!leftDone && !rightDone &&
      segmentLeft > CELL_RETREAT_TOLERANCE_COUNTS &&
      segmentRight > CELL_RETREAT_TOLERANCE_COUNTS)
  {
    const int32_t progressLeft =
      retreatSegmentProgress(s_cellRetreat.segmentStartLeft,
                             currentLeft,
                             s_cellRetreat.targetLeft);
    const int32_t progressRight =
      retreatSegmentProgress(s_cellRetreat.segmentStartRight,
                             currentRight,
                             s_cellRetreat.targetRight);
    const int correction =
      (int)((progressLeft - progressRight) *
            CELL_RETREAT_SYNC_MAX_CORRECTION / 1024);

    // Positiv: linkes Rad ist voraus. Es wird gebremst und das rechte
    // beschleunigt. Bei negativem Wert gilt das entsprechend umgekehrt.
    leftMagnitude = clampRetreatMagnitude(leftMagnitude - correction);
    rightMagnitude = clampRetreatMagnitude(rightMagnitude + correction);
  }

  const int leftSpeed =
    (errorLeft < 0 ? -leftMagnitude : leftMagnitude) *
    s_cellRetreat.leftPolarity;
  const int rightSpeed =
    (errorRight < 0 ? -rightMagnitude : rightMagnitude) *
    s_cellRetreat.rightPolarity;
  motors.setSpeeds(leftSpeed, rightSpeed);
  return LINE_FOLLOW_RUN_RUNNING;
}

void navigationCellRetreatAbort()
{
  if (s_cellRetreat.active)
    stopMotors();
  s_cellRetreat = {};
  s_navigationCellRetreatPrepared = false;
}

bool navigationDriveCell(int direction, bool centerAndStop)
{
  navigationDriveCellBegin(direction, centerAndStop);

  while (true)
  {
    const LineFollowRunStatus status = navigationDriveCellStep();
    if (status == LINE_FOLLOW_RUN_DONE) return true;
    if (status != LINE_FOLLOW_RUN_RUNNING) return false;
    delay(5);
  }
}

// ============================================================
// TOF / Wände
// ============================================================
uint16_t readTofDistance()
{
  uint32_t sum = 0;
  uint8_t count = 0;
  uint16_t minDistance = TOF_UNKNOWN_MM;
  uint16_t maxDistance = 0;

  for (uint8_t i = 0; i < TOF_MAX_SAMPLES; i++)
  {
    tof.sample();

    if (tof.amplitude > MIN_AMPLITUDE && tof.distanceMillimeters > 0)
    {
      uint16_t distance = tof.distanceMillimeters;

      sum += distance;
      count++;

      if (distance < minDistance) minDistance = distance;
      if (distance > maxDistance) maxDistance = distance;

      if (count >= TOF_MIN_SAMPLES && maxDistance - minDistance <= TOF_STABLE_SPREAD_MM)
        break;
    }

    delay(15);
  }

  if (count == 0) return TOF_UNKNOWN_MM;
  return sum / count;
}

void setWall(int x, int y, int d, bool isWall)
{
  if (!inBounds(x, y)) return;

  mazeSetWall(x, y, d, isWall);
  mappingState[x][y].lookahead[d] = false;

  int nx = x + dxDir(d);
  int ny = y + dyDir(d);

  if (inBounds(nx, ny))
  {
    int od = oppositeOf(d);
    mappingState[nx][ny].lookahead[od] = false;
  }
}

void scanDirection(int d)
{
  turnTo(d);

  lastScanDir = d;
  drawLiveMap("SCAN");

  delay(SCAN_SETTLE_TIME_MS);

  lastTofDistance = readTofDistance();
  lastWallDetected = lastTofDistance < tofWallThresholdMm;
  mappingState[mouseX][mouseY].tofDistance[d] = lastTofDistance;

  setWall(mouseX, mouseY, d, lastWallDetected);

  Serial.print("SCAN x="); Serial.print(mouseX);
  Serial.print(" y="); Serial.print(mouseY);
  Serial.print(" dir="); Serial.print(dirName(d));
  Serial.print(" dist="); Serial.print(lastTofDistance);
  Serial.print(" amp="); Serial.print(tof.amplitude);
  Serial.println(lastWallDetected ? " WALL" : " open");

  drawLiveMap("SCANNED");

  delay(SCAN_DONE_TIME_MS);
}

bool markLookaheadOpenEdge(int x, int y, int d, uint16_t estimatedDistance)
{
  if (!LOOKAHEAD_MARKING) return false;
  if (!inBounds(x, y)) return false;

  int nx = x + dxDir(d);
  int ny = y + dyDir(d);
  if (!inBounds(nx, ny)) return false;

  if (maze[x][y].known[d] && maze[x][y].wall[d] && !mappingState[x][y].lookahead[d])
    return false;

  int od = oppositeOf(d);
  if (maze[nx][ny].known[od] && maze[nx][ny].wall[od] && !mappingState[nx][ny].lookahead[od])
    return false;

  bool wasNew = !maze[x][y].known[d] || maze[x][y].wall[d];

  mazeSetWall(x, y, d, false);
  mappingState[x][y].lookahead[d] = true;
  mappingState[x][y].tofDistance[d] = estimatedDistance;
  mappingState[nx][ny].lookahead[od] = true;

  return wasNew;
}

bool predictedOpenByLookahead(int x, int y, int d)
{
  if (!inBounds(x, y)) return false;
  return maze[x][y].known[d] && !maze[x][y].wall[d] && mappingState[x][y].lookahead[d];
}

void applyLookahead(int d)
{
  uint16_t distance = mappingState[mouseX][mouseY].tofDistance[d];
  if (distance == TOF_UNKNOWN_MM) return;

  uint8_t maybeFreeEdges = 0;
  if (distance > LOOKAHEAD_MARGIN_MM)
    maybeFreeEdges = (distance - LOOKAHEAD_MARGIN_MM) / LOOKAHEAD_CELL_MM;

  if (maybeFreeEdges <= 1) return;
  if (maybeFreeEdges > LOOKAHEAD_MAX_PREDICTED_EDGES)
    maybeFreeEdges = LOOKAHEAD_MAX_PREDICTED_EDGES;

  uint8_t predictedEdges = 0;

  int x = mouseX;
  int y = mouseY;

  for (uint8_t step = 0; step < maybeFreeEdges; step++)
  {
    int nx = x + dxDir(d);
    int ny = y + dyDir(d);
    if (!inBounds(nx, ny)) break;

    if (step > 0)
    {
      uint16_t estimatedDistance = distance > (uint32_t)LOOKAHEAD_CELL_MM * step
        ? distance - LOOKAHEAD_CELL_MM * step
        : TOF_UNKNOWN_MM;

      if (markLookaheadOpenEdge(x, y, d, estimatedDistance))
        predictedEdges++;
    }

    x = nx;
    y = ny;
  }

  if (LOOKAHEAD_LOGGING && predictedEdges > 0)
  {
    Serial.print("LOOKAHEAD x=");
    Serial.print(mouseX);
    Serial.print(" y=");
    Serial.print(mouseY);
    Serial.print(" dir=");
    Serial.print(dirName(d));
    Serial.print(" dist=");
    Serial.print(distance);
    Serial.print(" freeEdges=");
    Serial.print(maybeFreeEdges);
    Serial.print(" predictedEdges=");
    Serial.println(predictedEdges);
  }
}

bool reusePredictedFrontScan(int d)
{
  if (!LOOKAHEAD_SKIP_PREDICTED_FRONT) return false;
  if (!predictedOpenByLookahead(mouseX, mouseY, d)) return false;

  lastScanDir = d;
  lastTofDistance = mappingState[mouseX][mouseY].tofDistance[d];
  lastWallDetected = false;

  if (LOOKAHEAD_LOGGING)
  {
    Serial.print("LOOKAHEAD_SKIP x=");
    Serial.print(mouseX);
    Serial.print(" y=");
    Serial.print(mouseY);
    Serial.print(" dir=");
    Serial.print(dirName(d));
    Serial.print(" estDist=");
    Serial.println(lastTofDistance);
  }

  drawLiveMap("LKHD");
  delay(KNOWN_CELL_TIME_MS);
  return true;
}

void scanCurrentCell()
{
  mappingState[mouseX][mouseY].visited = true;

  if (mappingState[mouseX][mouseY].scanned)
  {
    drawLiveMap("KNOWN");
    delay(KNOWN_CELL_TIME_MS);
    return;
  }

  mappingState[mouseX][mouseY].scanned = true;

  int original = heading;

  if (!(mouseX == START_X && mouseY == START_Y && stackTop < 0))
  {
    int backDir = oppositeOf(original);
    setWall(mouseX, mouseY, backDir, false);
  }

  int leftDir = leftOf(original);
  int frontDir = original;
  int rightDir = rightOf(original);

  // Scan front first: robot is freshly stopped, most stable position.
  if (!reusePredictedFrontScan(frontDir))
    scanDirection(frontDir);
  applyLookahead(frontDir);
  scanDirection(leftDir);
  scanDirection(rightDir);

  turnTo(original);

  drawLiveMap("CELL OK");
  delay(CELL_DONE_TIME_MS);
}

// ============================================================
// DFS
// ============================================================

bool cellHasUnvisitedNeighbor(int x, int y)
{
  for (int d = 0; d < 4; d++)
  {
    int nx = x + dxDir(d);
    int ny = y + dyDir(d);

    if (!inBounds(nx, ny)) continue;
    if (!maze[x][y].known[d]) continue;
    if (maze[x][y].wall[d]) continue;

    if (!mappingState[nx][ny].visited)
    {
      return true;
    }
  }

  return false;
}

bool neighborUnvisited(int d)
{
  int nx = mouseX + dxDir(d);
  int ny = mouseY + dyDir(d);

  if (!inBounds(nx, ny)) return false;
  if (!maze[mouseX][mouseY].known[d]) return false;
  if (maze[mouseX][mouseY].wall[d]) return false;

  return !mappingState[nx][ny].visited;
}

uint16_t directionScore(int d)
{
  uint16_t distance = mappingState[mouseX][mouseY].tofDistance[d];

  if (distance == TOF_UNKNOWN_MM)
    return 0;

  return distance;
}

bool chooseUnvisitedNeighbor(int &target)
{
  if (neighborUnvisited(heading))
  {
    target = heading;
    return true;
  }

  int leftDir = leftOf(heading);
  int rightDir = rightOf(heading);
  bool canGoLeft = neighborUnvisited(leftDir);
  bool canGoRight = neighborUnvisited(rightDir);

  if (canGoLeft && canGoRight)
  {
    target = directionScore(rightDir) > directionScore(leftDir) ? rightDir : leftDir;
    return true;
  }

  if (canGoLeft)
  {
    target = leftDir;
    return true;
  }

  if (canGoRight)
  {
    target = rightDir;
    return true;
  }

  int backDir = oppositeOf(heading);
  if (neighborUnvisited(backDir))
  {
    target = backDir;
    return true;
  }

  return false;
}

void moveToNeighbor(int d)
{
  if (!maze[mouseX][mouseY].known[d] || maze[mouseX][mouseY].wall[d])
  {
    drawLiveMap("NO MOVE");
    delay(350);
    return;
  }

  int oldX = mouseX;
  int oldY = mouseY;

  pushStack(oldX, oldY);

  drawLiveMap("MOVE");
  delay(120);

  if (navigationDriveCell(d, true))
  {
    mouseX += dxDir(d);
    mouseY += dyDir(d);

    if (!inBounds(mouseX, mouseY))
    {
      setMappingError("out bounds");
      return;
    }

    heading = d;
    drawLiveMap("ARRIVED");
    delay(150);
  }
  else
  {
if (!stackIsEmpty())
{
  int dummyX;
  int dummyY;
  popStackTo(dummyX, dummyY);
}
  }
}

void backtrackFastUntilUsefulCell()
{
  while (!stackIsEmpty())
  {
    int prevX;
    int prevY;
    popStackTo(prevX, prevY);

    int diffX = prevX - mouseX;
    int diffY = prevY - mouseY;

    int backDir;

    if (diffX == 1 && diffY == 0)
      backDir = EAST;
    else if (diffX == -1 && diffY == 0)
      backDir = WEST;
    else if (diffX == 0 && diffY == 1)
      backDir = NORTH;
    else if (diffX == 0 && diffY == -1)
      backDir = SOUTH;
    else
    {
      setMappingError("bad stack");
      return;
    }

    bool usefulTarget = cellHasUnvisitedNeighbor(prevX, prevY);
    bool continueStraight = false;

    // Nur dann ohne Halt durch die Zielkreuzung fahren, wenn auch der
    // darauffolgende DFS-Rueckweg in derselben Richtung weitergeht.
    // Vor Richtungswechseln muss die Maus wie im normalen Game-Ablauf
    // zentriert stehen, bevor navigationDriveCell die Punktdrehung startet.
    if (!usefulTarget && !stackIsEmpty())
    {
      const int nextX = stackX[stackTop];
      const int nextY = stackY[stackTop];
      continueStraight =
        nextX - prevX == dxDir(backDir) &&
        nextY - prevY == dyDir(backDir);
    }

    drawLiveMap("FAST BACK");

    if (!navigationDriveCell(backDir, !continueStraight))
    {
      setMappingError("back fail");
      return;
    }

    mouseX = prevX;
    mouseY = prevY;
    heading = backDir;

    if (usefulTarget)
    {
      stopMotors();
      drawLiveMap("BACK TARGET");
      delay(100);
      return;
    }
  }

  mappingStatus = MAPPING_RUN_DONE;
  stopMotors();
}

void mappingStep()
{
  if (!inBounds(mouseX, mouseY))
  {
    setMappingError("bad pos");
    return;
  }

  scanCurrentCell();

  int nextDir;

  if (chooseUnvisitedNeighbor(nextDir))
    moveToNeighbor(nextDir);
  else
  backtrackFastUntilUsefulCell();
}

// ============================================================
// Setup
// ============================================================

void serialEvent(){

  exportMazeToSerial();
  while(Serial.available()) Serial.read();
}

void startNewMappingRun()
{
  initMaze();

  menuShowMapStartPrompt();

  delay(300);

  bool compassCaptured = captureStartCompassHeading();

  menuShowMapStartHeading(compassCaptured, dirName(startWorldHeading));
  delay(700);

  state = MAP_RUNNING;
  drawLiveMap("START");
}

void mappingSetup()
{

  // Plattform-spezifische Motor-Drehrichtung. Die Hyper-Edition hat
  // ein invertiertes Getriebe (Pinion dreht entgegen Output-Welle);
  // mit den flip-Flags der Pololu-Library wird das transparent
  // kompensiert, sodass setSpeeds(positiv, positiv) immer vorwaerts
  // bedeutet – egal welche Mausvariante kompiliert wurde.
#if MOUSE_MOTOR_FLIP_LEFT
  motors.flipLeftMotor(true);
#endif
#if MOUSE_MOTOR_FLIP_RIGHT
  motors.flipRightMotor(true);
#endif

  Serial.print("[Platform] ");
  Serial.println(MOUSE_PLATFORM_NAME);

  tof.init();
  tof.setBrightness(OPT3101Brightness::Adaptive);
  tof.setFrameTiming(32);
  tof.setChannel(1);

  compassAvailable = initCompass();

  initMaze();

  if (loadMazeFromFlash())
  {
    buildGraphFromMaze();
  }
}

// ============================================================
// Display-Wrapper fuer MicromouseSolving
// (damit Solving kein Pololu3piPlus2040.h includen muss)
// ============================================================
void solveDisplayClearGraphics()  { clearGraphics(); }
void solveDisplayClear()          { u8g2.clearBuffer(); }
void solveDisplayNoAuto()         { /* U8G2 has no autoDisplay mode */ }
void solveDisplayUpdate()         { oledSendBuffer(); }

uint16_t mappingReadBatteryMv()   { return readBatteryMillivolts(); }
uint16_t mappingReadFrontDistance() { return readTofDistance(); }
void solveDisplayGotoXY(uint8_t x, uint8_t y)
{
  // U8G2 uses pixel coordinates; convert from character grid (6px wide, 10px high)
  // Pololu gotoXY: x=column (chars), y=row (rows of 8px)
  // We map: pixelX = x*6+2, pixelY = y*10+9 (baseline)
  // Store for use by solveDisplayPrint
  static uint8_t cursorX = 0, cursorY = 0;
  cursorX = x * 6 + 2;
  cursorY = y * 10 + 9;
  u8g2.setCursor(cursorX, cursorY);
}
void solveDisplayPrint(const char* s)  { u8g2.setFont(u8g2_font_6x10_tf); u8g2.print(s); }
void solveDisplayPrintInt(int v)       { u8g2.setFont(u8g2_font_6x10_tf); u8g2.print(v); }

bool solveButtonAPressed()
{
  // Liest und loescht das Flag (edge-triggered). solveWaitReleaseA()
  // in MicromouseSolving gibt sofort zurueck, da das Flag bereits false ist.
  bool v = g_btnAPressed;
  g_btnAPressed = false;
  return v;
}

bool solveButtonCPressed()
{
  // Liest und loescht das Flag. solveWaitReleaseC() gibt sofort zurueck.
  bool v = g_abortRequested;
  g_abortRequested = false;
  return v;
}

// Motor-Wrapper fuer Arc Turns
void solveSetMotors(int left, int right) { motors.setSpeeds(left, right); }

// ============================================================
// Neue Mapping-API (aufgerufen von menuAppLoop)
// ============================================================
void mappingStartRun()
{
  g_btnAPressed = false;
  g_abortRequested = false;
  // Kartierung nutzt die vordere-3-Erkennung (unveraendert); explizit setzen,
  // damit eine vorherige Punktdrehungs-Partie (4 von 5) nicht haengen bleibt.
  setIntersectionUseFront3(true);
  calibrateLineSensors();
  mappingStatus = MAPPING_RUN_RUNNING;
  startNewMappingRun();
}

MappingRunStatus mappingLoopStep()
{
  if (mappingStatus != MAPPING_RUN_RUNNING) return mappingStatus;

  if (g_abortRequested)
  {
    g_abortRequested = false;
    stopMotors();
    mappingStatus = MAPPING_RUN_ABORTED;
    return mappingStatus;
  }

  mappingStep();
  delay(50);

  if (mappingStatus == MAPPING_RUN_DONE)
  {
    stopMotors();
    buildGraphFromMaze();
    mapReady = true;
    if (!mazeSaveAttempted)
    {
      mazeSaved = saveMazeToFlash();
      mazeSaveAttempted = true;
    }
  }

  return mappingStatus;
}

void mappingAbortRun()
{
  stopMotors();
  mappingStatus = MAPPING_RUN_ABORTED;
  state = MAPPING_MENU;
}

// ============================================================
// Pose-Zugriff (fuer Kollisions-Recovery im Menue)
// ============================================================
int  mappingGetMouseX()  { return mouseX; }
int  mappingGetMouseY()  { return mouseY; }
int  mappingGetHeading() { return heading; }

void mappingSetPose(int x, int y, int h)
{
  if (inBounds(x, y))
  {
    mouseX = x;
    mouseY = y;
  }
  heading = ((h % 4) + 4) % 4;
}
