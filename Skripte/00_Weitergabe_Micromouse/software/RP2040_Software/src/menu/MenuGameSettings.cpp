#include "MenuInternal.h"

// ============================================================
// MAUS-MODI (Pacman-Figuren)
// ============================================================
extern const MausModusInfo mausModi[] = {
  {"Pacman",  {51, 40,  0}},   // Gelb
  {"Shadow",  {51,  0,  0}},   // Rot
  {"Speedy",  {51,  4, 29}},   // Pink
  {"Bashful", { 0, 20, 51}},   // Hellblau
  {"Pokey",   {51, 16,  0}},   // Orange
  {"Testdriver",  {51, 40,  0}},   // Gelb
};
extern const int MODUS_COUNT = 6;
int  mausModusIdx  = 0;
GAME_ROLES modusEnumFromIdx[] = {ROLE_PACMAN, ROLE_RED, ROLE_PINK, ROLE_CYAN, ROLE_BROWN, TESTDRIVE};
uint8_t gameLocalId = 0;

const char* optModus[] = {"Pacman","Shadow","Speedy","Bashful","Pokey","Testdriver"};


// Die Spiel-ID ist auf dem RP persistent (SavedSettings). Der ESP verliert
// seine ID (EEPROM unzuverlaessig), daher beim Boot aktiv an den ESP pushen –
// mit kurzer Verzoegerung, damit der ESP bereit ist, und mehrfach gegen
// Paketverlust. Der RP ist damit die Quelle der Wahrheit fuer die ID.
void pushGameIdToEsp()
{
  for (uint8_t i = 0; i < 3; i++)
  {
    delay(150);
    uartL3.setId(robotId);
    uartL3.update();
  }
}


// TODO: Wofür nötig? -> Startpositionen Persistent übers menu einstellbar. -> Unabhängigkeit von der rolle
void currentGhostStartPose(int &x, int &y, int &heading)
{
  x = startPosX;
  y = startPosY;

  if (getRoleFromId(robotId) == GAME_ROLES::ROLE_RED)
  {
    heading = SHADOW_START_HEADING;
    return;
  }

  if (getRoleFromId(robotId) == GAME_ROLES::ROLE_PINK)
  {
    heading = SPEEDY_START_HEADING;
    return;
  }

  heading = EAST;
}

void syncGameIdFromCommunication()
{
  gameLocalId = robotId & 0x1F;
}

// ============================================================
// EDITIERBARE VARIABLEN
// ============================================================
uint8_t goalX = 0;
uint8_t goalY = 0;

// --- Solving-Einstellungen ---
// solveKurvenModusIdx ist weiter oben forward-deklariert
const char* optKurvenModus[] = {"Kurve", "Punkt"};

int selftestEnabled      = 0;  // 0=Aus, 1=An
int selftestBeeperEnabled = 1; // 0=Aus, 1=An (Standard: an)
const char* optAusAn[] = {"Aus", "An"};
