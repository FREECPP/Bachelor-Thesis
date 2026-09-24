#pragma once

// =============================================================
// MicromouseSolving.h – Autonomer Pfadverfolgungs-Modus
// =============================================================
//
// Dieses Modul steuert den Roboter entlang vorberechneter Pfade
// durch ein zuvor gemapptes Labyrinth. Es arbeitet vollstaendig
// mit einer gemeinsamen Maze-Topologie:
//
//   - Globale Maze-Topologie fuer Mapping, Solving und Spiel
//   - Eigener Graph und Dijkstra-Pfadberechnung
//   - Eigene Positions-/Heading-Verfolgung (solveX, solveY)
//
// Von Mapping werden nur noch verwendet:
//   - Hardware-Wrapper (Display, Buttons, Motoren)
//   - followLineToNextIntersectionFast() als Linien-Primitive
//   - MAZE_W / MAZE_H als gemeinsame Labyrinth-Geometrie
//
// Architektur-Ueberblick:
//
//   1. solvingResetToStart()  – Flash laden, Pose auf (0,0,EAST)
//   2. startSolving(gx, gy)  – Dijkstra von aktueller Pose zum Ziel
//   3. solvingLoop()          – Zustandsmaschine: Drehen, Fahren,
//                                Schritt fuer Schritt den Pfad ablaufen
//
// Kaskadierung: Da startSolving() die Pose NICHT zuruecksetzt,
// koennen mehrere Ziele nacheinander angefahren werden, ohne
// dass der Roboter physisch zum Start zurueckkehren muss.
// =============================================================

#include "MicromouseMapping.h"  // MAZE_W, MAZE_H

constexpr int SOLVE_ROUTE_MAX_CELLS = MAZE_W * MAZE_H;

struct SolveRoutePlan
{
  uint8_t x[SOLVE_ROUTE_MAX_CELLS];
  uint8_t y[SOLVE_ROUTE_MAX_CELLS];
  uint8_t length;
};

// -------------------------------------------------------
// Zustandsmaschine – wird von menuAppLoop() zyklisch aufgerufen.
// Gibt true zurueck, sobald das Ziel erreicht wurde.
// -------------------------------------------------------
bool solvingLoop();

// -------------------------------------------------------
// Initialisierung / Reset
// -------------------------------------------------------

// Verwendet die gemeinsame Labyrinth-Karte (mit Flash-Fallback) und
// setzt die Roboter-Pose auf (startX, startY, startHeading).
//
// startHeading: 0=NORTH, 1=EAST, 2=SOUTH, 3=WEST
//
// Muss aufgerufen werden, bevor der Roboter physisch am
// Startkreuz steht. Gibt false zurueck, wenn kein gueltiges
// Karte vorhanden ist oder die Startkoordinate ausserhalb
// des Labyrinths liegt.
//
// Fuer den Standard-Start (0,0, EAST) einfach aufrufen mit:
//   solvingResetToStart(0, 0, 1);
bool solvingResetToStart(int startX, int startY, int startHeading);

// -------------------------------------------------------
// Zielnavigation
// -------------------------------------------------------

// Startet einen Solve-Lauf zum Ziel (gx, gy) AUSGEHEND VON
// DER AKTUELLEN ROBOTER-POSITION. Kein Flash-Reload, kein
// Pose-Reset – ermoeglicht kaskadierte Zielansteuerung:
//
//   solvingResetToStart();          // Pose = (0,0,EAST)
//   startSolving(3, 4);            // faehrt (0,0) -> (3,4)
//   // ... solvingLoop() bis true ...
//   startSolving(7, 2);            // faehrt (3,4) -> (7,2)
//   startSolving(0, 0);            // zurueck zum Start
//
// Voraussetzung: Karte muss durch solvingResetToStart()
// geladen sein. Gibt false bei fehlender Karte, ungueltigem
// Ziel oder fehlendem Pfad.
bool startSolving(int gx, int gy);

// Plant in Richtung des strategischen Ziels, beendet den Lauf aber am
// ersten Knoten mit echter Wahlmoeglichkeit (mindestens drei offene
// Kanten). Gibt es vorher keine Kreuzung, bleibt das strategische Ziel
// der Endpunkt. Fuer Ghost-Replanning an jeder Kreuzung.
bool startSolvingToNextDecision(int gx, int gy);
bool tryStartSolvingToNextDecision(int gx, int gy);

// Plant eine vollständige Route, ohne die Solver-Ausführung zu starten.
// Die Route beginnt immer mit der aktuellen Solver-Pose.
bool solveCreateRoutePlan(int gx, int gy, SolveRoutePlan &plan);
bool solveCreateRoutePlanFrom(int startX, int startY,
                              int gx, int gy,
                              SolveRoutePlan &plan);
bool solveIsDecisionCell(int x, int y);

// Waehlt ein zufaelliges erreichbares Ziel (verschieden von
// der aktuellen Position) und startet den Solve-Lauf dorthin.
// Fuer den Random-Modus: nach Zielerreichung erneut aufrufen.
bool startSolvingRandom();

// -------------------------------------------------------
// Positions-Abfrage
// -------------------------------------------------------

// Aktuelle Solver-Pose. Wird bei jedem Pfadschritt in
// solvingLoop() aktualisiert. Nach erfolgreichem Lauf gilt:
//   solveCurrentX() == goalX, solveCurrentY() == goalY
int solveCurrentX();
int solveCurrentY();
int solveCurrentHeading();
bool solvePlannedNextCell(int &x, int &y);
void solvePauseMotion();
bool solveIsDrivingCell();
bool solveIsRolling();

// true, wenn der aktuelle Lauf via startSolvingRandom() gestartet
// wurde. main.cpp triggert nach Zielerreichung dann automatisch
// einen weiteren Zufallslauf, statt ins Menue zurueckzukehren.
// Wird durch startSolving() oder solvingResetToStart() zurueckgesetzt.
bool solveIsRandomMode();

// -------------------------------------------------------
// Kollisions-Recovery (vom Menu nach collisionRecoveryUI aufgerufen)
// -------------------------------------------------------

bool solveIsAtError();
const char* solveLastError();

// Setzt die Solver-Pose direkt – wird vom Menu nach Edit/Reset in
// der Kollisions-Recovery-UI verwendet, bevor solveResumeFromCurrentPose()
// die Re-Planung triggert. Heading: 0=NORTH 1=EAST 2=SOUTH 3=WEST.
void solveSetPose(int x, int y, int heading);

// Plant einen neuen Pfad von der aktuellen Pose zum letzten Ziel und
// wechselt zurueck nach SOLVE_RUNNING. False wenn kein Pfad gefunden.
bool solveResumeFromCurrentPose();

// Bricht den Solving-Lauf hart ab (State -> SOLVE_MENU). Wird vom
// Menu aufgerufen, wenn die Recovery-UI Abbruch waehlt.
void solveAbortRun();

// Schaltet zwischen fliessendem Bogen (true) und Punktdrehung (false) um.
void setSolveSmoothTurns(bool enable);

// -------------------------------------------------------
// Dynamische Hindernisse (bewegliche Roboter als temporaere Waende)
// -------------------------------------------------------
// Vor startSolving() aufrufen, damit Dijkstra die markierten Zellen meidet
// (ausser sie sind selbst das Ziel). Anwendung: Heim-/Jagdplanung, die nicht
// durch einen anderen Roboter (Pacman/Geist) fuehren soll. solveBlockCell()
// markiert eine Zelle, solveClearDynamicObstacles() loescht alle.
void solveClearDynamicObstacles();
void solveBlockCell(int x, int y);
