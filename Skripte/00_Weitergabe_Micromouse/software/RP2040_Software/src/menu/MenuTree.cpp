#include "MenuInternal.h"

// ============================================================
// MENU-DEFINITIONEN
// ============================================================

// --- Karte senden ---
static MenuItem karteSendenItems[] = {
  {"Wireless", ITEM_ACTION, nullptr, doKarteSendenWireless,nullptr,0,0,nullptr,0,nullptr},
  {"Serial",   ITEM_ACTION, nullptr, doKarteSendenSerial,  nullptr,0,0,nullptr,0,nullptr},
};
static Menu karteSendenMenu = {"Senden", karteSendenItems, 2, nullptr};

// --- Karte ---
static MenuItem karteItems[] = {
  {"Kartierung", ITEM_ACTION, nullptr, doKartierung,   nullptr,0,0,nullptr,0,nullptr},
  {"Anzeigen",   ITEM_ACTION, nullptr, doKarteAnzeigen,nullptr,0,0,nullptr,0,nullptr},
  {"Senden",     ITEM_SUBMENU,&karteSendenMenu,nullptr,nullptr,0,0,nullptr,0,nullptr},
};
static Menu karteMenu = {"Karte", karteItems, 3, nullptr};

// --- Loesen: Ziel-X / Ziel-Y / Start ---
static MenuItem loesenItems[] = {
  {"Ziel X",   ITEM_EDIT,   nullptr, nullptr,           &goalX, 0, MAZE_W-1, nullptr,0,nullptr},
  {"Ziel Y",   ITEM_EDIT,   nullptr, nullptr,           &goalY, 0, MAZE_H-1, nullptr,0,nullptr},
  {"Starten",  ITEM_ACTION, nullptr, doSolvingZiel,     nullptr,0,0,nullptr,0,nullptr},
  {"Zufällig",ITEM_ACTION, nullptr, doSolvingZufaellig,nullptr,0,0,nullptr,0,nullptr},
};
static Menu loesenMenu = {"Lösen", loesenItems, 4, nullptr};

// --- Spiel ---
static MenuItem spielItems[] = {
  {"Demo",       ITEM_ACTION, nullptr, doSpielDemo, nullptr,0,0,nullptr,0,nullptr},
  {"Play Pacman",ITEM_ACTION, nullptr, doSpielPlay, nullptr,0,0,nullptr,0,nullptr},
};
static Menu spielMenu = {"Spiel", spielItems, 2, nullptr};

// --- Spieleinstellungen ---
static MenuItem spielEinstItems[] = {
  {"Modus", ITEM_CYCLE, nullptr,nullptr,nullptr,0,0,optModus,MODUS_COUNT,&mausModusIdx},
  {"ID",    ITEM_EDIT,  nullptr,nullptr,&gameLocalId,0,31,nullptr,0,nullptr},
  {"Start X", ITEM_EDIT, nullptr,nullptr,&startPosX,0,MAZE_W-1,nullptr,0,nullptr},
  {"Start Y", ITEM_EDIT, nullptr,nullptr,&startPosY,0,MAZE_H-1,nullptr,0,nullptr},
};
Menu spielEinstMenu = {"Spieleinstell.", spielEinstItems, 4, nullptr};

static MenuItem solveEinstItems[] = {
  {"Kurvenfahrt", ITEM_CYCLE, nullptr,nullptr,nullptr,0,0,optKurvenModus,2,&solveKurvenModusIdx},
};
static Menu solveEinstMenu = {"Solving-Einst.", solveEinstItems, 1, nullptr};

// --- Turbo-Einstellungen (Pacman X-Taste) ---
static MenuItem turboItems[] = {
  {"Zeit (s)",  ITEM_EDIT, nullptr,nullptr,&turboDurationS,TURBO_DURATION_MIN,TURBO_DURATION_MAX,nullptr,0,nullptr},
  {"Pause (s)", ITEM_EDIT, nullptr,nullptr,&turboCooldownS,TURBO_COOLDOWN_MIN,TURBO_COOLDOWN_MAX,nullptr,0,nullptr},
  {"Faktor %",  ITEM_EDIT, nullptr,nullptr,&turboFactorPct,TURBO_FACTOR_MIN,  TURBO_FACTOR_MAX,  nullptr,0,nullptr},
};
static Menu turboMenu = {"Turbo", turboItems, 3, nullptr};

// --- Hardware ---
static MenuItem hardwareItems[] = {
  {"ToF Kalibrieren", ITEM_ACTION, nullptr, doTofCalibration, nullptr,0,0,nullptr,0,nullptr},
  {"Kompass Ofs X", ITEM_ACTION, nullptr, doEditMagOffsetX, nullptr,0,0,nullptr,0,nullptr},
  {"Kompass Ofs Y", ITEM_ACTION, nullptr, doEditMagOffsetY, nullptr,0,0,nullptr,0,nullptr},
  {"Reset Offsets", ITEM_ACTION, nullptr, doResetMagOffsets,nullptr,0,0,nullptr,0,nullptr},
};
static Menu hardwareMenu = {"Hardware", hardwareItems, 4, nullptr};

// --- Selbsttest-Einstellungen ---
static MenuItem selftestEinstItems[] = {
  {"Selbsttest", ITEM_CYCLE, nullptr,nullptr,nullptr,0,0,optAusAn,2,&selftestEnabled},
  {"Beeper",     ITEM_CYCLE, nullptr,nullptr,nullptr,0,0,optAusAn,2,&selftestBeeperEnabled},
};
static Menu selftestEinstMenu = {"Selbsttest-Einst.", selftestEinstItems, 2, nullptr};

// --- Einstellung ---
static MenuItem einstellungItems[] = {
  {"Spieleinst.", ITEM_SUBMENU, &spielEinstMenu,    nullptr,nullptr,0,0,nullptr,0,nullptr},
  {"Turbo",       ITEM_SUBMENU, &turboMenu,         nullptr,nullptr,0,0,nullptr,0,nullptr},
  {"Solving",     ITEM_SUBMENU, &solveEinstMenu,    nullptr,nullptr,0,0,nullptr,0,nullptr},
  {"Hardware",    ITEM_SUBMENU, &hardwareMenu,      nullptr,nullptr,0,0,nullptr,0,nullptr},
  {"Selbsttest",  ITEM_SUBMENU, &selftestEinstMenu, nullptr,nullptr,0,0,nullptr,0,nullptr},
  {"BT Connect",  ITEM_ACTION,  nullptr, doBtConnect, nullptr,0,0,nullptr,0,nullptr},
};
static Menu einstellungMenu = {"Einstellung", einstellungItems, 6, nullptr};

// --- Test ---
static void doselftest()        { menuRunBlockingTest(selftest); }
static void doTestBattery()     { menuRunBlockingTest(testBattery); }
static void doTestBatteryLife() { menuRunBlockingTest(testBatteryLife); }
static void doTestCompass()     { menuRunBlockingTest(testCompass); }
static void doTestLines()      { menuRunBlockingTest(testLineSensors); }
static void doTestGyroscope()  { menuRunBlockingTest(testGyroscope); }
static void doTestBumper()     { menuRunBlockingTest(testBumper); }
static void doTestTOF()        { menuRunBlockingTest(testTOF); }
static void doTestMotors()     { menuRunBlockingTest(testMotors); }
static void doTestLEDs()       { menuRunBlockingTest(testLEDs); }
static void doTestInfo()       { menuRunBlockingTest(testInfo); }

static MenuItem testItems[] = {
  {"Selbsttest", ITEM_ACTION, nullptr, doselftest,nullptr,0,0,nullptr,0,nullptr},
  {"Batterie",   ITEM_ACTION, nullptr, doTestBattery,   nullptr,0,0,nullptr,0,nullptr},
  {"Akku-Lebensdauer", ITEM_ACTION, nullptr, doTestBatteryLife, nullptr,0,0,nullptr,0,nullptr},
  {"Kompass",    ITEM_ACTION, nullptr, doTestCompass,   nullptr,0,0,nullptr,0,nullptr},
  {"Linien",     ITEM_ACTION, nullptr, doTestLines,     nullptr,0,0,nullptr,0,nullptr},
  {"Gyroskop / Accelerometer", ITEM_ACTION, nullptr, doTestGyroscope, nullptr,0,0,nullptr,0,nullptr},
  {"Bumper",     ITEM_ACTION, nullptr, doTestBumper,    nullptr,0,0,nullptr,0,nullptr},
  {"TOF",        ITEM_ACTION, nullptr, doTestTOF,       nullptr,0,0,nullptr,0,nullptr},
  {"Motoren",    ITEM_ACTION, nullptr, doTestMotors,    nullptr,0,0,nullptr,0,nullptr},
  {"LEDs",       ITEM_ACTION, nullptr, doTestLEDs,      nullptr,0,0,nullptr,0,nullptr},
 // {"Info",       ITEM_ACTION, nullptr, doTestInfo,      nullptr,0,0,nullptr,0,nullptr},
};
static Menu testMenu = {"Test", testItems, 10, nullptr};

// --- Hauptmenue ---
static MenuItem mainItems[] = {
  {"Karte",       ITEM_SUBMENU, &karteMenu,       nullptr,nullptr,0,0,nullptr,0,nullptr},
  {"Lösen",      ITEM_SUBMENU, &loesenMenu,      nullptr,nullptr,0,0,nullptr,0,nullptr},
  {"Spiel",       ITEM_SUBMENU, &spielMenu,       nullptr,nullptr,0,0,nullptr,0,nullptr},
  {"Einstellung", ITEM_SUBMENU, &einstellungMenu, nullptr,nullptr,0,0,nullptr,0,nullptr},
  {"Test",        ITEM_SUBMENU, &testMenu,        nullptr,nullptr,0,0,nullptr,0,nullptr},
  {"Info",       ITEM_ACTION, nullptr, doTestInfo,      nullptr,0,0,nullptr,0,nullptr},
};
Menu mainMenu = {"Maus", mainItems, 6, nullptr};

// ============================================================
// AKTUELLER MENUE-ZUSTAND
// ============================================================
Menu*     currentMenu = &mainMenu;
int       selected    = 0;
MenuItem* activeItem  = nullptr;

// ============================================================
// MENUE-VERKNUEPFUNG (Parent-Pointer)
// ============================================================
void linkMenus()
{
  karteMenu.parent       = &mainMenu;
  karteSendenMenu.parent = &karteMenu;
  loesenMenu.parent      = &mainMenu;
  spielMenu.parent       = &mainMenu;
  einstellungMenu.parent = &mainMenu;
  testMenu.parent        = &mainMenu;
  spielEinstMenu.parent  = &einstellungMenu;
  solveEinstMenu.parent  = &einstellungMenu;
  hardwareMenu.parent        = &einstellungMenu;
  selftestEinstMenu.parent   = &einstellungMenu;
  turboMenu.parent           = &einstellungMenu;
}


// ============================================================
// EINSTELLUNGEN – Hilfsfunktion zum Speichern
// ============================================================
void saveCurrentSettings()   // TODO: Warum ist Save und Load so weit von einander entfernt?
{
  SavedSettings s = {mausModusIdx, solveKurvenModusIdx, goalX, goalY,
                     magOffsetX, magOffsetY, selftestEnabled, selftestBeeperEnabled,
                     gameLocalId & 0x1F, startPosX, startPosY,
                     (int)tofWallThresholdMm,
                     (int)turboDurationS, (int)turboCooldownS, (int)turboFactorPct};
  saveSettings(s);
}

void saveCurrentSettingsAndUpdateGameIdentity()
{
  gameLocalId &= 0x1F;
  setOwnIdRole(modusEnumFromIdx[mausModusIdx], (uint8_t)gameLocalId);
  saveCurrentSettings();
  //updateGameIdentity();  moved to code here
  //gameIdentitySet(currentGameRole(), (uint8_t)gameLocalId);
  uartL3.setId(robotId);
  uartL3.update();
}
