#include "MicromouseSolving.h"
#include "MicromousePlatform.h"
#include "MicromouseMappingState.h"
#include "MicromousePersistence.h"
#include <Arduino.h>
#include <string.h>
#include "Globals.h"

// =============================================================
// HARDWARE-ABSTRAKTION
// Diese extern-Funktionen werden in MicromouseMapping.cpp
// implementiert und kapseln die Pololu-Hardware (Display,
// Buttons, Motoren). Solving greift nur ueber diese Wrapper
// auf die Hardware zu – keine direkte Pololu-Abhaengigkeit.
// =============================================================
extern void solveDisplayClearGraphics();
extern void solveDisplayClear();
extern void solveDisplayNoAuto();
extern void solveDisplayUpdate();
extern void solveDisplayGotoXY(uint8_t x, uint8_t y);
extern void solveDisplayPrint(const char* s);
extern void solveDisplayPrintInt(int v);
extern bool solveButtonAPressed();
extern bool solveButtonCPressed();
extern void solveSetMotors(int left, int right);

// =============================================================
// LINIEN-PRIMITIVE
// followLineToNextIntersectionFast() folgt der schwarzen Linie
// bis zur naechsten Kreuzung. Die Funktion ist in
// MicromouseMapping.cpp implementiert und schreibt dort in
// Mapping-Variablen – das ist unkritisch, weil Mapping und
// Solving nie gleichzeitig aktiv sind.
// Parameter centerAndStop: true = am Kreuzungsmittelpunkt
// anhalten, false = durchfahren (fuer fliessende Bogendrehungen).
// =============================================================
extern bool followLineToNextIntersectionFast(bool centerAndStop);

// Suchrichtung fuer den Linien-Wiedereinfang nach einer Bogenfahrt
// (in MicromouseMapping.cpp). +1 = rechts, -1 = links, 0 = automatisch.
extern void setLineSearchDir(int dir);

// =============================================================
// SOLVING-EIGENER STATE
// Pose und Pfad sind lokal; die Maze-Topologie ist global geteilt.
// =============================================================

static bool solveMapLoaded = false;  // true nach erfolgreichem Flash-Load

// --- Graph-Repraesentation ---
// Das Labyrinth wird als ungerichteter Graph modelliert:
// Jede begehbare Verbindung zwischen zwei Nachbarzellen ist eine Kante.
// Der Graph wird aus maze[] aufgebaut (solveBuildGraph).
static const int MAX_NODES       = MAZE_W * MAZE_H;
static const int MAX_SOLVE_EDGES = MAZE_W * MAZE_H * 2;  // max 2 Kanten pro Zelle (EAST + NORTH)
static const int DIST_INF        = 30000;

struct SolveEdge { int fromX, fromY, toX, toY; };
static SolveEdge solveEdges[MAX_SOLVE_EDGES];
static int solveEdgeCount = 0;

// --- Dynamische Hindernisse (bewegliche Roboter als temporaere Waende) ---
// Zellen, die Dijkstra als blockiert behandelt (ausser Start/Ziel). Wird vom
// Spiel vor einer Heim-/Jagdplanung gesetzt, damit der Pfad nicht durch einen
// anderen Roboter (Pacman/Geist) fuehrt. Standard: leer.
static bool s_dynBlocked[MAZE_W][MAZE_H] = {{false}};

// --- Roboter-Pose (Solving-intern) ---
// Wird bei jedem Pfadschritt aktualisiert. Startposition ist
// die untere linke Ecke des Labyrinths, Blickrichtung EAST –
// muss der physischen Aufstellung des Roboters entsprechen.
static const int SOLVE_START_X       = 0;
static const int SOLVE_START_Y       = 0;
static const int SOLVE_START_HEADING = EAST;

static int solveX       = SOLVE_START_X;
static int solveY       = SOLVE_START_Y;

static void setSolvePose(int x, int y, int heading)
{
  solveX = x;
  solveY = y;
  mappingSetPose(x, y, heading);
}

// --- Pfad-Daten ---
// pathX/pathY: vom Dijkstra berechnete Wegpunkte (Start -> Ziel)
// pathStep: aktueller Fortschritt im Pfad
static int pathX[MAX_NODES];
static int pathY[MAX_NODES];
static int pathLen  = 0;   // Gesamtlaenge des Pfads (Anzahl Wegpunkte)
static int pathStep = 0;   // Index des zuletzt erreichten Wegpunkts

// Pro Pfadschritt vorab berechnete Aktion. pathActions[i] sagt,
// was zwischen Wegpunkt i und i+1 passiert (Drehung relativ zur
// aktuellen Roboter-Ausrichtung an Wegpunkt i).
enum SolveAction
{
  ACTION_STRAIGHT,    // geradeaus, keine Drehung
  ACTION_TURN_LEFT,   // 90° links abbiegen
  ACTION_TURN_RIGHT,  // 90° rechts abbiegen
  ACTION_TURN_AROUND  // 180° umdrehen
};
static SolveAction pathActions[MAX_NODES];

static const char *solveActionName(SolveAction action)
{
  switch (action)
  {
    case ACTION_STRAIGHT:    return "straight";
    case ACTION_TURN_LEFT:   return "left";
    case ACTION_TURN_RIGHT:  return "right";
    case ACTION_TURN_AROUND: return "around";
    default:                 return "?";
  }
}

// true, wenn die Maus aktuell faehrt (vom vorigen Pfadschritt
// per centerAndStop=false durchgefahren). Steuert, ob am Anfang
// des naechsten Schritts eine Punktdrehung (Maus steht) oder
// eine Bogenfahrt (Maus rollt schon) gemacht werden muss.
static bool solveIsMoving = false;
static bool solveDrivingCell = false;

static int goalX = 0;
static int goalY = 0;

// true im Zufallsmodus (startSolvingRandom). main.cpp prueft das nach
// Zielerreichung und startet sofort einen neuen Zufallslauf, statt
// ins Menue zurueckzukehren.
static bool randomMode = false;

// --- Zustandsmaschine ---
// SOLVE_IDLE:     Inaktiv (noch nie gestartet)
// SOLVE_MENU:     Wartet auf Benutzereingabe
// SOLVE_RUNNING:  Roboter faehrt Pfad ab
// SOLVE_DONE:     Ziel erreicht
// SOLVE_ERROR:    Fehler (kein Pfad, Linie verloren, etc.)
enum SolveState
{
  SOLVE_IDLE,
  SOLVE_MENU,
  SOLVE_RUNNING,
  SOLVE_DONE,
  SOLVE_ERROR
};
static SolveState solveState = SOLVE_MENU;
static const char* s_lastSolveError = "";

// =============================================================
// Bogenfahrt – 90°-Kurven beim Durchfahren von Kreuzungen
// =============================================================
// Die Maus durchfaehrt Kreuzungen ohne anzuhalten:
//   - Geradeaus durch eine Kreuzung: centerAndStop=false, der
//     Liniensensor-PID arbeitet einfach weiter.
//   - 90°-Abbiegung: encoder-geschlossener Bogen (curveTurn*-Stepper aus
//     dem Mapping-Layer). Der Drehwinkel wird ueber die Encoder-Differenz
//     der Raeder geregelt → robust gegen Batterie/Reibung. Danach faengt
//     followLineToNextIntersectionFast() die neue Linie wieder ein.
//   - 180°-Drehung: weiterhin Punktdrehung im Stand (Encoder-Stepper).
// =============================================================
// Schaltet 90°-Abbiegungen zwischen fliessendem Bogen (true) und
// Punktdrehung (false) um. Der Bogen selbst wird inzwischen vom
// encoder-geschlossenen curveTurn*-Stepper aus dem Mapping-Layer gefahren
// (robuster 90°-Winkel ueber die Encoder-Differenz). Die fruehere
// zeit-/sensorbasierte Bogenlogik samt SOLVE_CURVE_*-Konstanten ist
// entfallen – Bahnform/Winkel werden ueber die CURVE_*-Konstanten in
// MicromouseMapping.cpp eingestellt (gemeinsam mit dem Spielmodus).
static bool SOLVE_USE_SMOOTH_TURNS = true;

// =============================================================
// HILFSFUNKTIONEN
// =============================================================

// Prueft ob Koordinaten innerhalb des Labyrinths liegen
static inline bool solveInBounds(int x, int y)
{
  return x >= 0 && x < MAZE_W && y >= 0 && y < MAZE_H;
}

// Wandelt (x,y)-Koordinaten in einen linearen Index fuer Dijkstra-Arrays um
static inline int nodeIdx(int x, int y)
{
  return y * MAZE_W + x;
}

static inline void solveStopMotors() { solveSetMotors(0, 0); }

// Blockierendes Warten bis Taste losgelassen – verhindert Mehrfach-Trigger
static void solveWaitReleaseA() { while (solveButtonAPressed()) delay(10); }
static void solveWaitReleaseC() { while (solveButtonCPressed()) delay(10); }

// =============================================================
// GRAPH-AUFBAU
// Erzeugt aus der globalen Maze-Topologie einen ungerichteten Graphen.
// Fuer jede Zelle werden nur EAST- und NORTH-Kanten betrachtet,
// um Duplikate zu vermeiden (die Gegenrichtung erfasst die
// jeweils andere Zelle). Dijkstra behandelt jede Kante
// bidirektional.
// =============================================================
static void solveBuildGraph()
{
  solveEdgeCount = 0;

  for (int y = 0; y < MAZE_H; y++)
  {
    for (int x = 0; x < MAZE_W; x++)
    {
      // EAST-Kante: Verbindung zu (x+1, y) wenn keine Wand
      if (maze[x][y].known[EAST] && !maze[x][y].wall[EAST])
      {
        int nx = x + 1;
        if (solveInBounds(nx, y) &&
            solveEdgeCount < MAX_SOLVE_EDGES)
        {
          solveEdges[solveEdgeCount++] = { x, y, nx, y };
        }
      }

      // NORTH-Kante: Verbindung zu (x, y+1) wenn keine Wand
      if (maze[x][y].known[NORTH] && !maze[x][y].wall[NORTH])
      {
        int ny = y + 1;
        if (solveInBounds(x, ny) &&
            solveEdgeCount < MAX_SOLVE_EDGES)
        {
          solveEdges[solveEdgeCount++] = { x, y, x, ny };
        }
      }
    }
  }
}

// =============================================================
// DIJKSTRA – Kuerzester-Pfad-Algorithmus
// =============================================================
// Alle Kanten haben Gewicht 1 (eine Zelle Abstand).
// Berechnet den kuerzesten Pfad von (sx,sy) nach (gx,gy)
// und schreibt das Ergebnis in pathX[]/pathY[]/pathLen.
//
// Implementierung: einfacher O(V²)-Dijkstra ohne Priority-Queue,
// ausreichend fuer die kleine Labyrinth-Groesse (max 50 Knoten).
static bool dijkstra(int sx, int sy, int gx, int gy)
{
  int dist[MAX_NODES];     // Kuerzeste bekannte Distanz zum Start
  int prev[MAX_NODES];     // Vorgaenger-Knoten fuer Pfad-Rekonstruktion
  bool settled[MAX_NODES]; // Knoten endgueltig abgearbeitet

  for (int i = 0; i < MAX_NODES; i++)
  {
    dist[i]    = DIST_INF;
    prev[i]    = -1;
    settled[i] = false;
  }

  int startNode = nodeIdx(sx, sy);
  int goalNode  = nodeIdx(gx, gy);
  dist[startNode] = 0;

  // Hauptschleife: in jeder Iteration den Knoten mit
  // kleinster Distanz auswaehlen und seine Nachbarn relaxieren
  for (int iter = 0; iter < MAX_NODES; iter++)
  {
    // Finde den noch nicht abgearbeiteten Knoten mit min. Distanz
    int u = -1;
    for (int i = 0; i < MAX_NODES; i++)
    {
      if (!settled[i] && dist[i] < DIST_INF)
      {
        if (u == -1 || dist[i] < dist[u]) u = i;
      }
    }
    if (u == -1) break;       // Keine erreichbaren Knoten mehr
    if (u == goalNode) break;  // Ziel gefunden – Abbruch (optimal)
    settled[u] = true;

    int ux = u % MAZE_W;
    int uy = u / MAZE_W;

    // Alle Kanten pruefen, ob sie an Knoten u angrenzen
    // (bidirektional: from->to und to->from)
    for (int e = 0; e < solveEdgeCount; e++)
    {
      const SolveEdge& edge = solveEdges[e];
      int v = -1, vx = -1, vy = -1;

      if (edge.fromX == ux && edge.fromY == uy)
      {
        v = nodeIdx(edge.toX, edge.toY);   vx = edge.toX;   vy = edge.toY;
      }
      else if (edge.toX == ux && edge.toY == uy)
      {
        v = nodeIdx(edge.fromX, edge.fromY); vx = edge.fromX; vy = edge.fromY;
      }

      if (v == -1 || settled[v]) continue;

      // Dynamisches Hindernis (anderer Roboter): Knoten meiden – ausser er ist
      // selbst das Ziel (dann fahren wir bewusst hin, z.B. Jagd auf Pacman).
      if (s_dynBlocked[vx][vy] && v != goalNode) continue;

      int newDist = dist[u] + 1;  // Kantengewicht = 1
      if (newDist < dist[v])
      {
        dist[v] = newDist;
        prev[v] = u;
      }
    }
  }

  if (dist[goalNode] == DIST_INF) return false;  // Kein Pfad gefunden

  // Pfad-Rekonstruktion: von Ziel ueber prev[] zurueck zum Start
  int tempX[MAX_NODES];
  int tempY[MAX_NODES];
  int len = 0;

  int cur = goalNode;
  while (cur != -1)
  {
    tempX[len] = cur % MAZE_W;
    tempY[len] = cur / MAZE_W;
    len++;
    cur = prev[cur];
  }

  // Pfad umkehren (war rueckwaerts: Ziel -> Start)
  pathLen = len;
  for (int i = 0; i < len; i++)
  {
    pathX[i] = tempX[len - 1 - i];
    pathY[i] = tempY[len - 1 - i];
  }
  return true;
}

// =============================================================
// BEWEGUNG
// =============================================================

// Bestimmt die Himmelsrichtung von einer Zelle zur Nachbarzelle
static int directionTo(int fromX, int fromY, int toX, int toY)
{
  int dx = toX - fromX;
  int dy = toY - fromY;
  if (dx ==  1 && dy ==  0) return EAST;
  if (dx == -1 && dy ==  0) return WEST;
  if (dx ==  0 && dy ==  1) return NORTH;
  if (dx ==  0 && dy == -1) return SOUTH;
  return NORTH;  // Fallback (sollte nicht vorkommen)
}

static int solveNodeDegree(int x, int y)
{
  int degree = 0;
  for (int e = 0; e < solveEdgeCount; e++)
  {
    const SolveEdge& edge = solveEdges[e];
    if ((edge.fromX == x && edge.fromY == y) ||
        (edge.toX == x && edge.toY == y))
    {
      degree++;
    }
  }
  return degree;
}

static void truncatePathAtNextDecision()
{
  for (int i = 1; i < pathLen - 1; i++)
  {
    if (solveNodeDegree(pathX[i], pathY[i]) >= 3)
    {
      pathLen = i + 1;
      return;
    }
  }
}

// --- Bogenfahrt (encoder-geschlossen, via Mapping-Layer) ---
// Wird aufgerufen, wenn die Maus durch eine Kreuzung fliesst und dort 90°
// abbiegen muss. Delegiert an curveTurnBegin/Step aus MicromouseMapping
// (Drehwinkel encoder-geregelt). Direkt nach dem Bogen faengt der Aufrufer
// die neue Linie wieder ein (followLineToNextIntersectionFast).
// turnRight = true → 90° rechts, false → 90° links.

static void solveSmoothTurn90(bool turnRight)
{
  // Delegiert an den encoder-geschlossenen Pivot aus dem Mapping-Layer:
  // der Drehwinkel wird ueber die Encoder-Differenz der Raeder geregelt
  // (robust gegen Batterie/Reibung) – identisch zum Spielmodus.
  //
  // Erst AKTIV bremsen (Reverse-Puls, encoder-ueberwacht), damit der Pivot
  // sauber aus dem Stand startet – sonst gleitet die rollende Maus vor dem
  // Drehen vorwaerts (inneres Rad auf 0 = ausrollend).
  motorsActiveBrake();

  // Blockierend ausgefuehrt, damit die bestehende Solving-Loop-Struktur
  // (blockierende Drehung, anschliessend Linienfolge) unveraendert bleibt.
  // curveTurnStep() aktualisiert bei DONE das Mapping-heading und setzt die
  // Linien-Suchrichtung selbst.
  curveTurnBegin(turnRight);
  while (curveTurnIsActive())
  {
    if (curveTurnStep() != CURVE_RUN_RUNNING)
      break;  // DONE oder ABORTED → Schleife verlassen
    delay(5);
  }

  // Mapping-Position (x/y) mit der Solver-Pose synchron halten; das
  // heading hat curveTurnStep() bereits korrekt gesetzt (rechts/links 90°).
  mappingSetPose(solveX, solveY, mappingGetHeading());
}

// =============================================================
// DISPLAY-FUNKTIONEN
// Alle Anzeigen nutzen das 8x4-Zeichen-Layout des OLED-Displays.
// =============================================================

// Zeigt den Fortschritt waehrend der Fahrt an
static void showSolveStatus(const char* line1, int step, int total)
{
  solveDisplayClearGraphics();
  solveDisplayNoAuto();
  solveDisplayClear();

  solveDisplayGotoXY(0, 0); solveDisplayPrint(line1);
  solveDisplayGotoXY(0, 1); solveDisplayPrint("Step ");
  solveDisplayPrintInt(step); solveDisplayPrint("/");
  solveDisplayPrintInt(total - 1);
  solveDisplayGotoXY(0, 2); solveDisplayPrint("X:");
  solveDisplayPrintInt(solveX); solveDisplayPrint(" Y:");
  solveDisplayPrintInt(solveY);
  solveDisplayGotoXY(0, 3); solveDisplayPrint("Goal:");
  solveDisplayPrintInt(goalX); solveDisplayPrint(",");
  solveDisplayPrintInt(goalY);

  solveDisplayUpdate();
}

static void showSolveMenu()
{
  solveDisplayClearGraphics();
  solveDisplayNoAuto();
  solveDisplayClear();

  solveDisplayGotoXY(0, 0); solveDisplayPrint("SOLVE MODE");

  if (solveMapLoaded)
  {
    solveDisplayGotoXY(0, 1); solveDisplayPrint("C = Starten");
    solveDisplayGotoXY(0, 2); solveDisplayPrint("Goal:");
    solveDisplayPrintInt(goalX); solveDisplayPrint(",");
    solveDisplayPrintInt(goalY);
    solveDisplayGotoXY(0, 3); solveDisplayPrint("Map: RAM OK");
  }
  else
  {
    solveDisplayGotoXY(0, 1); solveDisplayPrint("NO MAP");
    solveDisplayGotoXY(0, 2); solveDisplayPrint("Run mapping");
    solveDisplayGotoXY(0, 3); solveDisplayPrint("first!");
  }

  solveDisplayUpdate();
}

static void showSolveError(const char* msg)
{
  s_lastSolveError = msg;
  solveDisplayClearGraphics();
  solveDisplayNoAuto();
  solveDisplayClear();

  solveDisplayGotoXY(0, 0); solveDisplayPrint("SOLVE ERR");
  solveDisplayGotoXY(0, 1); solveDisplayPrint(msg);
  solveDisplayGotoXY(0, 2); solveDisplayPrint("X:");
  solveDisplayPrintInt(solveX); solveDisplayPrint(" Y:");
  solveDisplayPrintInt(solveY);
  solveDisplayGotoXY(0, 3); solveDisplayPrint("A = Stop");

  solveDisplayUpdate();
}

static void showSolveDone()
{
  solveDisplayClearGraphics();
  solveDisplayNoAuto();
  solveDisplayClear();

  solveDisplayGotoXY(0, 0); solveDisplayPrint("GOAL REACHED!");
  solveDisplayGotoXY(0, 1); solveDisplayPrint("X:");
  solveDisplayPrintInt(solveX); solveDisplayPrint(" Y:");
  solveDisplayPrintInt(solveY);
  solveDisplayGotoXY(0, 2); solveDisplayPrint("Steps: ");
  solveDisplayPrintInt(pathLen - 1);
  solveDisplayGotoXY(0, 3); solveDisplayPrint("C=nochmal");

  solveDisplayUpdate();
}

// =============================================================
// INTERNES: Gemeinsame Karte verwenden und Graph aufbauen.
// Nur wenn noch keine RAM-Karte bereitsteht, wird aus Flash geladen.
// =============================================================
static bool reloadMapFromFlash()
{
  solveDisplayClearGraphics();
  solveDisplayNoAuto();
  solveDisplayClear();
  solveDisplayGotoXY(0, 0); solveDisplayPrint("Load Map...");
  solveDisplayUpdate();

  solveMapLoaded = mapReady || loadMazeFromFlash();

  if (solveMapLoaded)
  {
    solveBuildGraph();
    Serial.print("[Solve] Gemeinsame Map bereit. Edges=");
    Serial.println(solveEdgeCount);
  }
  else
  {
    Serial.println("[Solve] Keine gueltige Map gefunden.");
  }

  return solveMapLoaded;
}

// =============================================================
// OEFFENTLICHE API + interne Helfer
// =============================================================

// Gemeinsamer Kern fuer startSolving() und startSolvingRandom():
// Berechnet den Dijkstra-Pfad von der aktuellen Pose (solveX/solveY)
// zum Ziel und wechselt in den Zustand SOLVE_RUNNING.
// Laedt die Karte NICHT neu – das ist Aufgabe von solvingResetToStart().
static bool startSolvingInternal(int gx, int gy,
                                 bool stopAtNextDecision = false,
                                 bool reportFailure = true)
{
  const int requestedGoalX = gx;
  const int requestedGoalY = gy;
  g_btnAPressed = false;
  g_abortRequested = false;
  setLineSearchDir(0);
  goalX    = gx;
  goalY    = gy;
  pathStep = 0;
  pathLen  = 0;
  if (!solveInBounds(goalX, goalY))
  {
    if (reportFailure)
    {
      showSolveError("goal oob");
      solveState = SOLVE_ERROR;
    }
    else
    {
      solveState = SOLVE_MENU;
    }
    return false;
  }

  solveDisplayClearGraphics();
  solveDisplayNoAuto();
  solveDisplayClear();
  solveDisplayGotoXY(0, 0); solveDisplayPrint("Dijkstra...");
  solveDisplayUpdate();

  if (!dijkstra(solveX, solveY, goalX, goalY))
  {
    if (reportFailure)
    {
      showSolveError("no path");
      solveState = SOLVE_ERROR;
    }
    else
    {
      solveState = SOLVE_MENU;
    }
    return false;
  }

  if (stopAtNextDecision)
  {
    truncatePathAtNextDecision();
    goalX = pathX[pathLen - 1];
    goalY = pathY[pathLen - 1];
  }

  // Pro Pfadschritt die Drehrichtung relativ zur jeweils aktuellen
  // Maus-Ausrichtung vorab bestimmen. Beim ersten Schritt geht das
  // Heading aus dem globalen Navigationszustand, bei spaeteren Schritten aus der
  // Bewegungsrichtung des vorherigen Schritts.
  {
    int heading = mappingGetHeading();
    for (int i = 0; i < pathLen - 1; i++)
    {
      int dir   = directionTo(pathX[i], pathY[i], pathX[i+1], pathY[i+1]);
      int delta = (dir - heading + 4) % 4;
      if      (delta == 0) pathActions[i] = ACTION_STRAIGHT;
      else if (delta == 1) pathActions[i] = ACTION_TURN_RIGHT;
      else if (delta == 2) pathActions[i] = ACTION_TURN_AROUND;
      else                 pathActions[i] = ACTION_TURN_LEFT;
      heading = dir;
    }
  }

  Serial.println();
  Serial.print("[Solve] plan from=");
  Serial.print(solveX);
  Serial.print(',');
  Serial.print(solveY);
  Serial.print(" heading=");
  Serial.print(mappingGetHeading());
  Serial.print(" requested=");
  Serial.print(requestedGoalX);
  Serial.print(',');
  Serial.print(requestedGoalY);
  Serial.print(" effective=");
  Serial.print(goalX);
  Serial.print(',');
  Serial.print(goalY);
  Serial.print(" nextDecision=");
  Serial.println(stopAtNextDecision ? 1 : 0);
  Serial.println("===== DIJKSTRA PATH (FLASH MAP) =====");
  for (int i = 0; i < pathLen; i++)
  {
    Serial.print("(");
    Serial.print(pathX[i]);
    Serial.print(",");
    Serial.print(pathY[i]);
    Serial.print(")");
    if (i < pathLen - 1) Serial.print(" -> ");
  }
  Serial.println();
  Serial.print("Total steps: "); Serial.println(pathLen - 1);
  Serial.println("===== END =====");

  // Maus steht am Pfadstart still – am Anfang ist nichts in Bewegung.
  solveIsMoving = false;

  solveState = SOLVE_RUNNING;
  showSolveStatus("SOLVING", 0, pathLen);
  return true;
}

// Random-Ziel-Auswahl: Sammelt alle besuchten Zellen (ausser
// der aktuellen Position) als Kandidaten und versucht bis zu
// 16 Mal, ein zufaelliges Ziel zu finden, fuer das Dijkstra
// einen Pfad liefert. Mehrere Versuche sind noetig, weil nicht
// jede besuchte Zelle vom aktuellen Standort erreichbar sein muss.
static bool pickRandomGoalAndStart()
{
  int candX[MAX_NODES];
  int candY[MAX_NODES];
  int candCount = 0;

  for (int y = 0; y < MAZE_H; y++)
  {
    for (int x = 0; x < MAZE_W; x++)
    {
      if (x == solveX && y == solveY) continue;
      candX[candCount] = x;
      candY[candCount] = y;
      candCount++;
    }
  }

  if (candCount == 0)
  {
    showSolveError("no targets");
    solveState = SOLVE_ERROR;
    return false;
  }

  for (int attempt = 0; attempt < 16; attempt++)
  {
    int idx = random(candCount);
    if (startSolvingInternal(candX[idx], candY[idx])) return true;
  }

  showSolveError("no path");
  solveState = SOLVE_ERROR;
  return false;
}

bool solvingResetToStart(int startX, int startY, int startHeading)
{
  navigationDriveCellAbort();
  solveStopMotors();
  setLineSearchDir(0);
  g_abortRequested = false;
  solveState = SOLVE_MENU;
  s_lastSolveError = "";
  solveIsMoving = false;
  solveDrivingCell = false;
  pathLen = 0;
  pathStep = 0;
  goalX = startX;
  goalY = startY;
  solveClearDynamicObstacles();

  // Gemeinsame RAM-Karte verwenden; falls noetig einmal aus Flash laden.
  randomMode = false;
  if (!reloadMapFromFlash())
  {
    showSolveError("no map");
    solveState = SOLVE_ERROR;
    return false;
  }

  // Startpose gemaess Aufruf-Parameter setzen.
  // Grenzen pruefen damit keine ungueltige Zelle verwendet wird.
  if (startX < 0 || startX >= MAZE_W || startY < 0 || startY >= MAZE_H)
  {
    showSolveError("bad start pos");
    solveState = SOLVE_ERROR;
    return false;
  }

  setSolvePose(startX, startY, startHeading);
  return true;
}

bool startSolving(int gx, int gy)
{
  // Plant einen Pfad von der AKTUELLEN Roboter-Pose (solveX/solveY)
  // zum Ziel. Kein Flash-Reload, kein Pose-Reset – damit lassen
  // sich Ziele kaskadiert ansteuern.
  if (!solveMapLoaded)
  {
    showSolveError("no map");
    solveState = SOLVE_ERROR;
    return false;
  }

  randomMode = false;
  return startSolvingInternal(gx, gy);
}

bool startSolvingToNextDecision(int gx, int gy)
{
  if (!solveMapLoaded)
  {
    showSolveError("no map");
    solveState = SOLVE_ERROR;
    return false;
  }

  randomMode = false;
  return startSolvingInternal(gx, gy, true);
}

bool tryStartSolvingToNextDecision(int gx, int gy)
{
  if (!solveMapLoaded)
    return false;

  randomMode = false;
  return startSolvingInternal(gx, gy, true, false);
}

bool solveCreateRoutePlan(int gx, int gy, SolveRoutePlan &plan)
{
  return solveCreateRoutePlanFrom(solveX, solveY, gx, gy, plan);
}

bool solveCreateRoutePlanFrom(int startX, int startY,
                              int gx, int gy,
                              SolveRoutePlan &plan)
{
  plan.length = 0;
  if (!solveMapLoaded ||
      !solveInBounds(startX, startY) ||
      !solveInBounds(gx, gy))
    return false;
  if (!dijkstra(startX, startY, gx, gy))
    return false;
  if (pathLen < 1 || pathLen > SOLVE_ROUTE_MAX_CELLS)
    return false;

  plan.length = (uint8_t)pathLen;
  for (int i = 0; i < pathLen; i++)
  {
    plan.x[i] = (uint8_t)pathX[i];
    plan.y[i] = (uint8_t)pathY[i];
  }
  return true;
}

bool solveIsDecisionCell(int x, int y)
{
  return solveInBounds(x, y) && solveNodeDegree(x, y) >= 3;
}

bool startSolvingRandom()
{
  // Random-Ziel ausgehend von der aktuellen Pose. Kein Reload,
  // sodass der Modus nach jedem erreichten Ziel direkt erneut
  // aufgerufen werden kann.
  if (!solveMapLoaded)
  {
    showSolveError("no map");
    solveState = SOLVE_ERROR;
    return false;
  }

  randomMode = true;
  return pickRandomGoalAndStart();
}

int solveCurrentX()        { return solveX; }
int solveCurrentY()        { return solveY; }
int solveCurrentHeading()  { return mappingGetHeading(); }
bool solveIsRandomMode()   { return randomMode; }
void setSolveSmoothTurns(bool enable)
{
  SOLVE_USE_SMOOTH_TURNS = enable;
  // Kurve → vordere 3 Sensoren, Punkt → 4 von 5 Sensoren.
  setIntersectionUseFront3(enable);
}

// --- Dynamische Hindernisse ---
// Vor startSolving() setzen, um bewegliche Roboter als temporaere Waende zu
// behandeln. Gilt fuer alle folgenden Planungen, bis solveClearDynamicObstacles().
void solveClearDynamicObstacles()
{
  for (int x = 0; x < MAZE_W; x++)
    for (int y = 0; y < MAZE_H; y++)
      s_dynBlocked[x][y] = false;
}

void solveBlockCell(int x, int y)
{
  if (solveInBounds(x, y))
    s_dynBlocked[x][y] = true;
}

bool solvePlannedNextCell(int &x, int &y)
{
  if (pathStep >= pathLen - 1)
    return false;

  x = pathX[pathStep + 1];
  y = pathY[pathStep + 1];
  return true;
}

void solvePauseMotion()
{
  turnAbort();
  solveStopMotors();
  solveIsMoving = false;
  solveDrivingCell = false;
}

bool solveIsDrivingCell()
{
  return solveDrivingCell;
}

bool solveIsRolling()
{
  return solveIsMoving;
}

// =============================================================
// Kollisions-Recovery-API (wird vom Menu nach collisionRecoveryUI
// aufgerufen, um die Pose zu korrigieren und neu zu planen)
// =============================================================

bool solveIsAtError()
{
  return solveState == SOLVE_ERROR;
}

const char* solveLastError()
{
  return s_lastSolveError;
}

void solveSetPose(int x, int y, int heading)
{
  if (!solveInBounds(x, y)) return;
  setSolvePose(x, y, ((heading % 4) + 4) % 4);
}

bool solveResumeFromCurrentPose()
{
  // Dijkstra neu rechnen von der jetzigen Pose. startSolvingInternal()
  // setzt solveState anschliessend wieder auf SOLVE_RUNNING.
  return startSolvingInternal(goalX, goalY);
}

void solveAbortRun()
{
  navigationDriveCellAbort();
  solveStopMotors();
  solveIsMoving = false;
  solveDrivingCell = false;
  solveState = SOLVE_MENU;
}

// =============================================================
// HAUPTSCHLEIFE – Zustandsmaschine
// =============================================================
// Wird zyklisch von menuAppLoop() aufgerufen. Pro Aufruf wird
// genau ein Pfadschritt ausgefuehrt (eine Zelle weiterfahren).
// Gibt true zurueck, wenn das Ziel erreicht wurde.
bool solvingLoop()
{
  // A-Taste bricht jederzeit ab und kehrt zum Menue zurueck
  if (solveButtonAPressed())
  {
    solveWaitReleaseA();
    turnAbort();
    solveStopMotors();
    solveIsMoving = false;
    solveState = SOLVE_MENU;
    showSolveMenu();
    return false;
  }

  switch (solveState)
  {
    // ---------------------------------------------------------
    // SOLVE_MENU: Wartet auf Benutzereingabe (A = Start)
    // ---------------------------------------------------------
    case SOLVE_MENU:
    {
      showSolveMenu();

      if (solveMapLoaded && solveButtonCPressed())
      {
        solveWaitReleaseC();
        startSolving(goalX, goalY);
      }

      delay(100);
      return false;
    }

    // ---------------------------------------------------------
    // SOLVE_RUNNING: Faehrt den Pfad Schritt fuer Schritt ab.
    //
    // Bewegungsablauf pro Zelle:
    //   1. Drehphase (vor der Fahrt zur naechsten Zelle):
    //      - STRAIGHT    → keine Drehung
    //      - 90°-Kurve   → falls Maus rollt: Klothoide-Kreis-
    //                       Klothoide-Bogen; sonst Punktdrehung.
    //      - 180°        → falls noetig: anhalten, dann Punktdrehung
    //   2. Fahrphase: followLineToNextIntersectionFast(centerAndStop)
    //      - centerAndStop=false: Kreuzung wird ohne Halt durchfahren
    //        (Geradeaus-Folge oder anschliessende Bogenfahrt).
    //      - centerAndStop=true: an der naechsten Kreuzung zentriert
    //        anhalten (Pfad-Ende oder anschliessende 180°-Drehung).
    // ---------------------------------------------------------
    case SOLVE_RUNNING:
    {
      // Alle Wegpunkte abgefahren -> Ziel erreicht
      if (pathStep >= pathLen - 1)
      {
        solveStopMotors();
        solveIsMoving = false;
        solveState = SOLVE_DONE;
        showSolveDone();
        return false;
      }

      const int  toX        = pathX[pathStep + 1];
      const int  toY        = pathY[pathStep + 1];
      const int desiredHeading =
        directionTo(pathX[pathStep], pathY[pathStep], toX, toY);
      const SolveAction action = pathActions[pathStep];
      const bool isLastMove = (pathStep + 1 == pathLen - 1);
      bool smoothTurnExecuted = false;

      // === DREH-PHASE ===
      // Maus rollt entweder schon (solveIsMoving=true, vom vorigen
      // Schritt per centerAndStop=false durchgefahren) oder steht
      // zentriert (z.B. erster Schritt, nach 180°, am Pfadanfang).
      if (action == ACTION_TURN_LEFT || action == ACTION_TURN_RIGHT)
      {
        const bool turnRight = (action == ACTION_TURN_RIGHT);
        if (solveIsMoving && SOLVE_USE_SMOOTH_TURNS)
        {
          // Maus rollt → fliessender Bogen ueber die Kreuzung.
          // Linien-PID waehrend des Bogens aus; nach dem Bogen
          // faengt followLineToNextIntersectionFast die neue
          // Linie wieder ein (CELL_IGNORE_TIME schuetzt vor
          // erneutem Trigger der gerade verlassenen Kreuzung).
          solveSmoothTurn90(turnRight);
          smoothTurnExecuted = true;
        }
        else
        {
          // Punktdrehungen laufen unten ueber den gemeinsamen
          // nicht-blockierenden Encoder-Stepper.
          if (solveIsMoving)
          {
            solveStopMotors();
            solveIsMoving = false;
          }
        }
      }
      else if (action == ACTION_TURN_AROUND)
      {
        if (solveIsMoving)
        {
          solveStopMotors();
          solveIsMoving = false;
        }
      }
      // ACTION_STRAIGHT: keine Drehung

      const bool pointTurnRequired =
        action == ACTION_TURN_AROUND ||
        ((action == ACTION_TURN_LEFT || action == ACTION_TURN_RIGHT) &&
         !smoothTurnExecuted);
      if (pointTurnRequired && mappingGetHeading() != desiredHeading)
      {
        if (!turnIsActive())
          turnBegin(desiredHeading);

        const TurnRunStatus turnStatus = turnStep();
        if (turnStatus == TURN_RUN_RUNNING)
          return false;
        if (turnStatus == TURN_RUN_ABORTED)
        {
          Serial.println("[Solve] turn aborted");
          solveStopMotors();
          solveIsMoving = false;
          solveState = SOLVE_MENU;
          return false;
        }
        if (turnStatus != TURN_RUN_DONE)
        {
          Serial.println("[Solve] turn encoder error");
          solveStopMotors();
          solveIsMoving = false;
          solveState = SOLVE_ERROR;
          showSolveError("turn encoder");
          return false;
        }
      }

      if (mappingGetHeading() != desiredHeading)
      {
        Serial.print("[Solve] heading mismatch actual=");
        Serial.print(mappingGetHeading());
        Serial.print(" desired=");
        Serial.println(desiredHeading);
        solveStopMotors();
        solveIsMoving = false;
        solveState = SOLVE_ERROR;
        showSolveError("heading mismatch");
        return false;
      }

      // === FAHR-PHASE ===
      // centerAndStop nur dann, wenn der naechste Schritt eine
      // 180°-Drehung erfordert (Maus muss anhalten) ODER es der
      // letzte Pfadschritt ist. Sonst durchfahren – Geradeaus-
      // Folge laeuft mit Linien-PID weiter, 90°-Drehungen werden
      // im naechsten solvingLoop-Eintritt als Bogen gefahren.
      bool centerAndStop = isLastMove;
      bool nextIsPivot = false;
      if (!isLastMove)
      {
        const SolveAction nextAction = pathActions[pathStep + 1];
        if (nextAction == ACTION_TURN_AROUND)
          centerAndStop = true;
        else if (!SOLVE_USE_SMOOTH_TURNS &&
                 (nextAction == ACTION_TURN_LEFT || nextAction == ACTION_TURN_RIGHT))
          centerAndStop = true;
        else
          centerAndStop = false;

        // Naechster Schritt ist ein fliessender 90°-Pivot → diese Fahrt
        // endet in einer Abbiegung: schon im Anlauf verzoegern.
        nextIsPivot = SOLVE_USE_SMOOTH_TURNS &&
                      (nextAction == ACTION_TURN_LEFT ||
                       nextAction == ACTION_TURN_RIGHT);
      }
      lineFollowFastSetDecelBeforeTurn(nextIsPivot);

      Serial.print("[Solve] step=");
      Serial.print(pathStep);
      Serial.print('/');
      Serial.print(pathLen - 1);
      Serial.print(" from=");
      Serial.print(solveX);
      Serial.print(',');
      Serial.print(solveY);
      Serial.print(" to=");
      Serial.print(toX);
      Serial.print(',');
      Serial.print(toY);
      Serial.print(" heading=");
      Serial.print(mappingGetHeading());
      Serial.print(" action=");
      Serial.print(solveActionName(action));
      Serial.print(" rolling=");
      Serial.print(solveIsMoving ? 1 : 0);
      Serial.print(" centerStop=");
      Serial.println(centerAndStop ? 1 : 0);

      solveDrivingCell = true;
      const bool lineOk = followLineToNextIntersectionFast(centerAndStop);
      solveDrivingCell = false;

      if (!lineOk)
      {
        if (mappingGetStatus() == MAPPING_RUN_ABORTED)
        {
          Serial.println("[Solve] movement aborted");
          solveStopMotors();
          solveIsMoving = false;
          solveState = SOLVE_MENU;
          return false;
        }

        Serial.print("[Solve] step-failed from=");
        Serial.print(solveX);
        Serial.print(',');
        Serial.print(solveY);
        Serial.print(" to=");
        Serial.print(toX);
        Serial.print(',');
        Serial.print(toY);
        Serial.print(" heading=");
        Serial.print(mappingGetHeading());
        Serial.print(" action=");
        Serial.println(solveActionName(action));
        solveStopMotors();
        solveIsMoving = false;
        solveState = SOLVE_ERROR;
        showSolveError("line lost");
        return false;
      }

      solveIsMoving = !centerAndStop;
      setSolvePose(toX, toY, desiredHeading);
      pathStep++;

      Serial.print("[Solve] reached=");
      Serial.print(solveX);
      Serial.print(',');
      Serial.print(solveY);
      Serial.print(" heading=");
      Serial.print(mappingGetHeading());
      Serial.print(" rolling=");
      Serial.println(solveIsMoving ? 1 : 0);

      showSolveStatus("RUNNING", pathStep, pathLen);
      return false;
    }

    // ---------------------------------------------------------
    // SOLVE_DONE: Ziel erreicht. C = erneut fahren (nach
    // physischem Zurueckstellen auf das Startkreuz).
    // ---------------------------------------------------------
    case SOLVE_DONE:
    {
      if (solveButtonCPressed())
      {
        solveWaitReleaseC();
        if (solvingResetToStart(solveX, solveY, mappingGetHeading()))
          startSolving(goalX, goalY);
      }

      delay(100);
      return true;  // Signalisiert dem Aufrufer: Ziel wurde erreicht
    }

    // ---------------------------------------------------------
    // SOLVE_ERROR: Fehler aufgetreten – Motoren stoppen, warten
    // ---------------------------------------------------------
    case SOLVE_ERROR:
    {
      solveStopMotors();
      delay(300);
      return false;
    }

    // ---------------------------------------------------------
    case SOLVE_IDLE:
    default:
      return false;
  }

  return false;
}
