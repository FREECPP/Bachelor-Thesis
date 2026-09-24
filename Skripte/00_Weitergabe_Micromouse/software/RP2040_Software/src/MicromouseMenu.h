#ifndef MICROMOUSE_MENU_H
#define MICROMOUSE_MENU_H

#include <Arduino.h>

// ============================================================
// MicromouseMenu.h – Schnittstelle zum Menue-Framework
//
// Die komplette Menue-UI (Display, Buttons, State-Machine,
// Aktionen, Setup/Loop) lebt in MicromouseMenu.cpp. Dieser
// Header exportiert:
//   - Statusanzeigen, die MicromouseMapping.cpp aufruft
//   - menuAppSetup() / menuAppLoop() fuer main.cpp
// ============================================================


// Zeigt kurz eine zweizeilige Nachricht (blockierend, ~700 ms).
// Wird von mappingSetup() für "MAP AUTOLOAD" verwendet.
void menuShowTemporaryMessage(const char *line1, const char *line2 = "");

// Aufforderung, den Roboter auf das Startkreuz zu stellen.
void menuShowMapStartPrompt();

// Zeigt die erfasste Weltrichtung beim Mapping-Start.
void menuShowMapStartHeading(bool compassCaptured, const char *headingName);

// Button-Zugriff fuer Hardware-Tests (dieselben Instanzen wie das Menue).
// void     menuPollButtons();    // Wieso ist da mehr oder weniger 2 mal das gleiche implementiert?
bool     menuGetPressedA();
bool     menuGetPressedB();
bool     menuGetPressedC();
void     menuPollInputs();
void     menuRunBlockingTest(void (*testFn)());

// Aktueller Xbox-Controller Switch-Zustand (polling + UART-Update intern).
// Bits: CSW_D_PAD_UP/DOWN/LEFT/RIGHT, CSW_BUTTON_A/B/X/Y, ...
uint16_t menuGetCtrlSwitches();
// Linker Analog-Stick: x/y je -128..127, Mitte=0. Y negativ = Stick nach oben.
void     menuGetCtrlStickL(int8_t &x, int8_t &y);


// ===========================================================
// Menu Elemente
// ===========================================================

// Menu - Typen
struct Menu;  // Forward Declaration for MenuItem
enum ItemType
{
  ITEM_SUBMENU,
  ITEM_ACTION,
  ITEM_EDIT,
  ITEM_CYCLE,
};

struct MenuItem
{
  const char*  name;
  ItemType     type;
  Menu*        submenu;
  void         (*action)();
  volatile uint8_t* var;
  uint8_t      minVal;
  uint8_t      maxVal;
  const char** options;
  int          optCount;
  int*         optIndex;
};

struct Menu
{
  const char* title;
  MenuItem*   items;
  int         size;
  Menu*       parent;
};


void doKartierung();
void doKarteAnzeigen();
void doBtConnect();
void doKarteSendenWireless();
void doKarteSendenSerial();
void doSolvingZufaellig();
void doSolvingZiel();
void doSpielDemo();
void doSpielPlay();
void doEditMagOffsetX();
void doEditMagOffsetY();
void doResetMagOffsets();
void doTofCalibration();

void handleInput();
void drawMenu();

// Selbsttest-Einstellungen (abgerufen von MicromouseHardwareTest.cpp).
bool menuGetSelftestBeeperEnabled();

// Fahrmodus-Einstellung "Kurvenfahrt" (Menue): true = Kurve (fliessender
// Bogen), false = Punkt (Drehung im Stand). Wird vom Spielmodus gelesen,
// damit dieselbe Menue-Einstellung Menue-Loesen UND Spiel steuert.
bool menuGetUseCurves();

// ============================================================
// Kollisions-Recovery-UI
// ============================================================
// Blockierende UI, die nach einer Wand-Kollision (Mapping oder
// Solving) angezeigt wird. Zeigt die aktuelle Pose und bietet:
//   A = Weiterfahren mit (evtl. editierter) Pose      → CONTINUE
//   B = Pose editieren (Sub-Mode, dann zurueck)
//   C = Hardcoded-Reset auf (0,0,EAST), dann Weiter   → RESET
// Lang-A oder Mehrfachdruck-Sequenzen werden bewusst nicht
// genutzt – 3 Tasten reichen mit klarer Belegung.
enum CollisionAction
{
  COLLISION_CONTINUE,  // x/y/heading wurden ggf. editiert
  COLLISION_RESET,     // Pose auf (0,0,EAST) zurueckgesetzt
  COLLISION_ABORT      // (aktuell nicht angeboten, fuer spaeter)
};

CollisionAction collisionRecoveryUI(int &x, int &y, int &heading);

// Einfache Anzeige fuer Mapping-Kollisions-Abbruch (nicht editierbar).
// Zeigt Pose + Wartet auf C-Taste, dann zurueck ins Menue.
void mappingShowCollisionStop(int x, int y, int heading);

// Einstiegspunkte fuer den App-Loop (von main.cpp aufgerufen).
void menuAppSetup();
void menuAppActive();     // Called once when Menu is active (Sets callbacks...)
void menuAppLoop();

#endif
