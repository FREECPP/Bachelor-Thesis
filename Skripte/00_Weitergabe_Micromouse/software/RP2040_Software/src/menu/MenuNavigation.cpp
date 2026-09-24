#include "MenuInternal.h"

// ============================================================
// NAVIGATION
// ============================================================
static void goBack()
{
  if (currentMenu->parent)
  {
    currentMenu = currentMenu->parent;
    selected    = 0;
  }
}

static void enterItem()
{
  MenuItem& item = currentMenu->items[selected];
  switch (item.type)
  {
    case ITEM_SUBMENU:
      if (item.submenu)
      {
        currentMenu = item.submenu;
        selected = 0;
        if (currentMenu == &spielEinstMenu)
          syncGameIdFromCommunication();
      }
      break;

    case ITEM_ACTION:
      if (item.action)
      {
        // Blockierende Aktionen (Offset-Editor, ToF-Kalibrierung, BT-Connect,
        // Start-Bestaetigungen ...) haben eigene Tastenschleifen mit
        // buttonAdvanced.update()/uartL3.update(). Bliebe uiState dabei auf
        // UI_NAVIGATE, wuerden diese Updates erneut die Button-/Controller-
        // Callbacks und damit handleInput() ausloesen: das Hintergrundmenue
        // navigiert + zeichnet (Flackern) und der A-Tastendruck wird vor der
        // Aktion abgefangen (kein Abbrechen). Daher waehrend der Aktion auf
        // UI_TEST schalten – diesen Zustand ignorieren die Callbacks
        // (wie bei menuRunBlockingTest).
        const UIState prevState = uiState;
        uiState = UI_TEST;
        buttonAdvanced.clearEvents();
        item.action();
        buttonAdvanced.clearEvents();
        // Hat die Aktion den State selbst gesetzt (z.B. UI_MAP_VIEW,
        // UI_SOLVE_RUNNING), diesen behalten; sonst zurueck auf vorher.
        if (uiState == UI_TEST) uiState = prevState;
      }
      break;

    case ITEM_EDIT:
      activeItem = &currentMenu->items[selected];
      uiState    = UI_EDIT;
      break;

    case ITEM_CYCLE:
      if (item.optIndex && item.optCount > 0)
      {
        (*item.optIndex)++;
        if (*item.optIndex >= item.optCount) *item.optIndex = 0;
        if (item.optIndex == &mausModusIdx)
          saveCurrentSettingsAndUpdateGameIdentity();
        else
          saveCurrentSettings();
      }
      break;
  }
}

// ============================================================
// INPUT-VERARBEITUNG
// MENU-STATE-MACHINE-TRANSITIONEN
// ============================================================
void handleInput()
{
  bool a = menuGetPressedA();
  bool b = menuGetPressedB();
  bool c = menuGetPressedC();
  bool up = g_ctrlUp;
  g_ctrlUp = g_ctrlDown = g_ctrlSel = g_ctrlBack = false;


  Serial.print("handleInput called options: ");
  Serial.print(a);
  Serial.print(", ");
  Serial.print(b);
  Serial.print(", ");
  Serial.print(c);
  Serial.print(", ");
  Serial.print(up);
  Serial.print("  current state= ");
  Serial.print(uiState);

  switch (uiState)
  {
    case UI_NAVIGATE:
      if (a) goBack();
      if (up) { selected--; if (selected < 0) selected = currentMenu->size - 1; }
      if (b)  { selected++; if (selected >= currentMenu->size) selected = 0; }
      if (c) enterItem();
      break;

    case UI_EDIT:
      if (a)
      {
        if (activeItem && activeItem->var == &gameLocalId)
          saveCurrentSettingsAndUpdateGameIdentity();
        else if (activeItem && (activeItem->var == &goalX ||
                                activeItem->var == &goalY ||
                                activeItem->var == &startPosX ||
                                activeItem->var == &startPosY ||
                                activeItem->var == &turboDurationS ||
                                activeItem->var == &turboCooldownS ||
                                activeItem->var == &turboFactorPct))
          saveCurrentSettings();
        uiState    = UI_NAVIGATE;
        activeItem = nullptr;
      }
      if ((b || up) && activeItem && activeItem->var)
      {
        int delta = up ? 1 : -1;
        int next = (int)*activeItem->var + delta;
        if (next < activeItem->minVal) next = activeItem->maxVal;
        if (next > activeItem->maxVal) next = activeItem->minVal;
        *activeItem->var = (uint8_t)next;
      }
      if (c && activeItem && activeItem->var)
      {
        int next = (int)*activeItem->var + 1;
        if (next > activeItem->maxVal) next = activeItem->minVal;
        *activeItem->var = (uint8_t)next;
      }
      break;

    case UI_RUN:
      if (a || c) { uiState = UI_NAVIGATE; g_inGameMode = false; }
      break;

    case UI_MAP_VIEW:
      if (a) uiState = UI_NAVIGATE;
      break;

    // MAP_RUNNING und SOLVE_RUNNING werden direkt in menuAppLoop() behandelt
    case UI_TEST:
    case UI_MAP_RUNNING:
    case UI_SOLVE_RUNNING:
      break;
  }

  Serial.print("  new state= ");
  Serial.println(uiState);

  // Draw new menu if neccessarry
  // TODO: Problems with Info page and such stuff (not so important)
  if (uiState == UI_NAVIGATE || uiState == UI_EDIT) drawMenu();
}
