#include "MenuInternal.h"

bool     g_ctrlUp    = false;
bool     g_ctrlDown  = false;
bool     g_ctrlSel   = false;
bool     g_ctrlBack  = false;
uint16_t g_ctrlSw    = 0;
int8_t   g_ctrlLX    = 0;
int8_t   g_ctrlLY    = 0;

bool menuGetPressedA()     { bool v = buttonAdvanced.wasPressed(0) || g_ctrlBack; g_ctrlBack = false; return v; }
bool menuGetPressedB()     { bool v = buttonAdvanced.wasPressed(1) || g_ctrlDown; g_ctrlDown = false; return v; }
bool menuGetPressedC()     { bool v = buttonAdvanced.wasPressed(2) || g_ctrlSel;  g_ctrlSel  = false; return v; }
uint16_t menuGetCtrlSwitches()                      { uartL3.update(); return g_ctrlSw; }
void     menuGetCtrlStickL(int8_t &x, int8_t &y)   { x = g_ctrlLX; y = g_ctrlLY; }

void menuPollInputs()
{
  uartL3.update();
  buttonAdvanced.update();
}


// ============================================================
// MENU – State Machine
// ============================================================
UIState uiState = UI_NAVIGATE;

void menuRunBlockingTest(void (*testFn)())
{
  if (testFn == nullptr) return;

  const UIState previousState = uiState;
  uiState = UI_TEST;
  g_ctrlUp = g_ctrlDown = g_ctrlSel = g_ctrlBack = false;
  buttonAdvanced.clearEvents();

  testFn();

  g_ctrlUp = g_ctrlDown = g_ctrlSel = g_ctrlBack = false;
  buttonAdvanced.clearEvents();
  uiState = previousState;

  if (uiState == UI_NAVIGATE || uiState == UI_EDIT) drawMenu();
}

// ============================================================
// AKKU-WARNUNG
// ============================================================
bool     g_lowBattery    = false;
uint32_t g_lastBattCheck = 0;
extern const uint16_t BATT_WARN_MV = 4400;
extern const LedColor BATT_LED_WARN  = {51, 0, 0};
