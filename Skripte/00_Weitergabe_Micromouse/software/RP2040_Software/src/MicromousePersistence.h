#ifndef MICROMOUSE_PERSISTENCE_H
#define MICROMOUSE_PERSISTENCE_H

#include <Arduino.h>

bool saveMazeToFlash();
bool loadMazeFromFlash();

struct SavedSettings
{
  int mausModusIdx;
  int solveKurvenModusIdx;
  int goalX;
  int goalY;
  int  magOffsetX;
  int  magOffsetY;
  int selftestEnabled;      // 0=Aus, 1=An
  int selftestBeeperEnabled; // 0=Aus, 1=An
  int gameLocalId;          // 0..31, untere 5 Bit der Game-ID
  int startPosX;        // Game Starting Positions
  int startPosY;
  int tofWallThresholdMm;
  int turboDurationS;   // Turbo-Boost-Dauer in Sekunden
  int turboCooldownS;   // Turbo-Cooldown in Sekunden
  int turboFactorPct;   // Turbo-Geschwindigkeitsfaktor in Prozent
};

bool saveSettings(const SavedSettings &s);
bool loadSettings(SavedSettings &s);

// Ergebnis des Akku-Lebensdauer-Tests. Wird waehrend des Tests periodisch
// an Haltepunkten geschrieben, damit der letzte Stand einen Brownout
// ueberlebt und beim naechsten Aufruf angezeigt werden kann.
struct BatteryTestResult
{
  uint32_t elapsedSeconds;  // Laufzeit bis zum letzten Schreibzeitpunkt
  uint16_t startMv;         // Spannung bei Teststart
  uint16_t minMv;           // niedrigste (entprellte) Spannung
  uint16_t lastMv;          // zuletzt gemessene (entprellte) Spannung
  uint16_t runsCompleted;   // erreichte Zufallsziele
  uint16_t enteredSpin;     // 0/1: in den Dreh-Fallback gewechselt
};

bool saveBatteryTestResult(const BatteryTestResult &r);
bool loadBatteryTestResult(BatteryTestResult &r);

#endif
