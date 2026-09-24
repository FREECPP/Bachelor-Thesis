#pragma once
#include <Arduino.h>
#include "../MicromouseMapping.h"  // MAZE_W, MAZE_H

// ============================================================
// Pac-Man Spielmodus
//
// Pac-Man-Maus (mausModusIdx == 0) wird per Xbox-Controller
// gesteuert. Muenzen liegen auf allen Nodes ausser den
// Ghost-Spawn-Feldern. Leben: 3. Steuerung ueber D-Pad.
//
// Eingabe wird via gameGetCtrlSwitches() aus dem GameState-Layer gelesen.
// ============================================================

// Startposition Pac-Man
constexpr int PAC_START_HEADING = 0;  // NORTH

constexpr int PAC_LIVES = 3;

// Ghost-Spawn – keine Muenzen hier
constexpr int SHADOW_START_X       = 3;
constexpr int SHADOW_START_Y       = 3;
constexpr int SHADOW_START_HEADING = 0;  // North

constexpr int SPEEDY_START_HEADING = 0;  // NORTH

constexpr int GHOST_SPAWN_X = SHADOW_START_X, GHOST_SPAWN_Y = SHADOW_START_Y;

// Einmalig vor dem Spiel aufrufen (nach solvingResetToStart,
// calibrateLineSensors, buildGraphFromMaze).
void pacmanGameInit();

// Fahrmodus setzen: true = 90°-Abbiegungen als fliessender Bogen, false =
// Punktdrehung im Stand. Wird aus der Menue-Einstellung "Kurvenfahrt"
// gespeist (vor pacmanGameInit aufrufen).
void pacmanSetUseCurves(bool enable);

// Einmal pro gameLoop-Zyklus aufrufen.
// Gibt true zurueck, wenn das Spiel beendet ist (Quit oder Game Over).
bool pacmanGameLoop();

void pacmanGameObserveGhost(uint8_t senderId, uint8_t x, uint8_t y);
void pacmanGameBeginCapture(uint8_t ghostId);
void pacmanGameBeginTimeoutRetreat();
bool pacmanGameCaptureLoop();
bool pacmanGameCaptureFailed();
const char *pacmanGameCaptureLastError();
bool pacmanGameHasLives();
void pacmanGameBeginReturnHome();
bool pacmanGameReturnHomeLoop();
void pacmanGameStartNextRound();
void pacmanGameResetAfterTimeout();
void pacmanGameManualHomeReset();
void pacmanGameBroadcastPosition();

// Wird aus menuBackgroundUpdate() gerufen – prüft Pacmans eigene Bumper
// um Kollisionen zu erkennen wenn Pacman in einen Geist reinfaehrt.
void pacmanCheckBumpers();

// Aktualisiert das HUD waehrend einer Fahrzelle (Pfeil laeuft mit).
void pacmanUpdateHUDDuringDrive();
