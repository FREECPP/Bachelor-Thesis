#ifndef MENU_INTERNAL_H
#define MENU_INTERNAL_H

#include "../MicromouseMenu.h"
#include "../MicromouseMappingState.h"
#include "../MicromouseMapping.h"
#include "../MicromouseSolving.h"
#include "../MicromouseGraphics.h"
#include "../MicromouseMazeFrame.h"
#include "../MicromousePersistence.h"
#include "../Globals.h"
#include "../MicromouseLeds.h"
#include "../MicromouseHardwareTest.h"
#include "../game/PacmanGame.h"
#include "../game/GhostTargets.h"
#include "../game/GameState.h"
#include "../game/Turbo.h"
#include "../ButtonAdvanced.h"

#include <Arduino.h>
#include <U8g2lib.h>
#include <UART_L3.h>
#include <hardware/gpio.h>
#include <string.h>

using namespace Pololu3piPlus2040;

extern UART_L3 uartL3;
extern ButtonAdvanced buttonAdvanced;
extern bool followLineToNextIntersectionFast(bool centerAndStop);
extern void turnTo(int target);

extern U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI u8g2;

enum UIState
{
  UI_NAVIGATE,
  UI_EDIT,
  UI_RUN,
  UI_TEST,
  UI_MAP_RUNNING,
  UI_SOLVE_RUNNING,
  UI_MAP_VIEW,
};

struct MausModusInfo
{
  const char* name;
  LedColor dimColor;
};

extern bool g_ctrlUp;
extern bool g_ctrlDown;
extern bool g_ctrlSel;
extern bool g_ctrlBack;
extern uint16_t g_ctrlSw;
extern int8_t g_ctrlLX;
extern int8_t g_ctrlLY;

extern UIState uiState;
extern bool g_lowBattery;
extern uint32_t g_lastBattCheck;
extern const uint16_t BATT_WARN_MV;
extern const LedColor BATT_LED_WARN;

extern uint32_t g_commRxSignalUntil;
extern LedColor g_commRxSignalColor;
extern bool g_btControllerConnected;

extern const MausModusInfo mausModi[];
extern const int MODUS_COUNT;
extern int mausModusIdx;
extern GAME_ROLES modusEnumFromIdx[];
extern uint8_t gameLocalId;
extern const char* optModus[];
extern uint8_t goalX;
extern uint8_t goalY;
extern const char* optKurvenModus[];
extern int solveKurvenModusIdx;
extern bool g_inGameMode;
extern int selftestEnabled;
extern int selftestBeeperEnabled;
extern const char* optAusAn[];

extern Menu spielEinstMenu;
extern Menu mainMenu;
extern Menu* currentMenu;
extern int selected;
extern MenuItem* activeItem;

void applyCommRxLedSignal();
void drawReceivedMaze();
void onMazeDataReceived(uint8_t length, uint8_t *data);
void onGameControlReceived(uint8_t senderId, uint8_t length, uint8_t *data);
void onBtControllerStateChanged(uint8_t stateId);
void onSubscribedReceived(uint8_t length, uint8_t *data);
void menuCbButtonPressed(uint8_t buttonId);
void menuCbButtonReleased(uint8_t buttonId);
void menuBackgroundUpdate();

void pushGameIdToEsp();
void currentGhostStartPose(int &x, int &y, int &heading);
void syncGameIdFromCommunication();
void linkMenus();
void saveCurrentSettings();
void saveCurrentSettingsAndUpdateGameIdentity();

#endif
