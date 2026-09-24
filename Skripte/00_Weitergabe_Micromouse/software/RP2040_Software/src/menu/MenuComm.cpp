#include "MenuInternal.h"

// ============================================================
// KOMMUNIKATION RP2040 <-> ESP32
// UART_L3 verpackt die fachlichen Nachrichten, z.B. OPC_MAZE.
// TODO: Das ganze COM-Gedönse in zukunft auslagern
// ============================================================

// Maze Empfangen
uint32_t g_commRxSignalUntil = 0;
LedColor g_commRxSignalColor = LED_GREEN;
bool g_btControllerConnected = false;


void applyCommRxLedSignal()    // Was ist dein zweck=
{
  if (millis() < g_commRxSignalUntil)
    ledSetAll(g_commRxSignalColor);
}

void drawReceivedMaze()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  if (!mapReady)
  {
    u8g2.drawUTF8(2, 32, "Keine Karte");
    oledSendBuffer();
    return;
  }

  uint8_t cellX = 127 / MAZE_W;
  uint8_t cellY = 63 / MAZE_H;
  uint8_t cell = cellX < cellY ? cellX : cellY;
  if (cell < 2) cell = 2;

  uint8_t mapW = MAZE_W * cell + 1;
  uint8_t mapH = MAZE_H * cell + 1;
  uint8_t ox = (128 - mapW) / 2;
  uint8_t oy = (64 - mapH) / 2;

  clearGraphics();

  for (uint8_t x = 0; x < MAZE_W; x++)
  {
    for (uint8_t y = 0; y < MAZE_H; y++)
    {
      const uint8_t px = ox + x * cell;
      const uint8_t py = oy + (MAZE_H - 1 - y) * cell;
      if (mazeHasWall(x, y, NORTH)) drawHLine(px, px + cell, py);
      if (mazeHasWall(x, y, EAST))  drawVLine(px + cell, py, py + cell);
      if (mazeHasWall(x, y, SOUTH)) drawHLine(px, px + cell, py + cell);
      if (mazeHasWall(x, y, WEST))  drawVLine(px, py, py + cell);
    }
  }

  flushGraphicsToU8G2();
}

void onMazeDataReceived(uint8_t length, uint8_t *data)
{
  Serial.println();
  Serial.println("===== MAZE WIRELESS RX START =====");
  Serial.print("maze_payload_bytes=");
  Serial.println(length);
  Serial.print("bytes=");
  for (uint8_t i = 0; i < length; i++)
  {
    Serial.print(" 0x");
    if (data[i] < 16) Serial.print("0");
    Serial.print(data[i], HEX);
  }
  Serial.println();
  Serial.println("===== MAZE WIRELESS RX END =====");

  const bool decoded = mazeLoadCommFrameBytes(data, length);
  bool rxSaved = false;
  if (decoded)
  {
    mapReady = true;
    rxSaved = saveMazeToFlash();
  }

  g_commRxSignalColor = decoded && rxSaved ? LED_GREEN : LED_RED;
  g_commRxSignalUntil = millis() + 1500;
}

void onGameControlReceived(uint8_t senderId, uint8_t length, uint8_t *data)
{
  if (length < 1 || data == nullptr || data[0] != GC_START_GAME)
    return;

  if (rpState != RP_STATE_MENU ||
      getRoleFromId(senderId) != GAME_ROLES::ROLE_PACMAN ||
      getRoleFromId((uint8_t)robotId) == GAME_ROLES::ROLE_PACMAN)
  {
    return;
  }

  Serial.print("Game start received from Pacman ID ");
  Serial.println(senderId);
  rpState = RP_STATE_GAME;
}

// ============================================================
// JAGDMODUS (Hunt) – Verfolgermaus jagt Pacman ueber UART-Position
// TODO: REALLOKIREN! Was soll die Jagt-logik hier? -> Geister logik
// ============================================================
int solveKurvenModusIdx = 0;  // 0 = Kurve, 1 = Punkt
bool g_inGameMode = false;



void menuBackgroundUpdate()
{
  uartL3.update();
}

// TODO: Abändern/Hinzufügen: eigene (callback)funktion für Menu und spiel
void onBtControllerStateChanged(uint8_t stateId)
{
  g_btControllerConnected = (stateId == CONN_SUCCESS);
  if (g_btControllerConnected)
  {
    uartL3.subscribeBtController(CONTROLLER_SWITCHES, 0, true);
    uartL3.subscribeBtController(CONTROLLER_STICK_L, 8, true);
  }
}

void onSubscribedReceived(uint8_t length, uint8_t *data)   // Controller Data received
{
  if (length < 3) return;

  if (data[0] == CONTROLLER_STICK_L)
  {
    g_ctrlLX = (int8_t)data[1];
    g_ctrlLY = (int8_t)data[2];
    return;
  }

  if (data[0] != CONTROLLER_SWITCHES) return;
  uint16_t sw = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
  static uint16_t prevSw = 0;
  uint16_t pressed = sw & ~prevSw;
  prevSw = sw;
  g_ctrlSw = sw;

  if (pressed & (CSW_D_PAD_UP))                       g_ctrlUp   = true;
  if (pressed & (CSW_D_PAD_DOWN))                     g_ctrlDown = true;
  if (pressed & (CSW_BUTTON_A | CSW_D_PAD_RIGHT))     g_ctrlSel  = true;
  if (pressed & (CSW_BUTTON_B | CSW_D_PAD_LEFT))      g_ctrlBack = true;

  if (uiState == UI_NAVIGATE ||
      uiState == UI_EDIT ||
      uiState == UI_RUN ||
      uiState == UI_MAP_VIEW)
    handleInput();
}

// Neues Button handling
void menuCbButtonPressed(uint8_t buttonId) {
  Serial.print("menuCbButtonPressed: buttonId=");
  Serial.println(buttonId);
  switch (buttonId) {
    case 0:
      g_btnAPressed = true;
      break;
    case 1:
      g_btnBPressed = true;
      break;
    case 2:
      g_btnCPressed = true;
      break;
    default:
      break;
  }

  if (uiState == UI_NAVIGATE ||
      uiState == UI_EDIT ||
      uiState == UI_RUN ||
      uiState == UI_MAP_VIEW)
    handleInput();
}

void menuCbButtonReleased(uint8_t buttonId) {
  (void)buttonId;
}
