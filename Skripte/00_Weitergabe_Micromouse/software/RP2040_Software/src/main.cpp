// ============================================================
// main.cpp
//
// Einstiegspunkt fuer den Pololu 3pi+ 2040. Die komplette Menue-
// UI, Display- und Button-Hardware-Definitionen sowie der App-
// Loop leben in MicromouseMenu.cpp. Hier wird nur noch der Menu-
// Setup und die Menue-Schleife aufgerufen.
// ============================================================

#include "MicromouseMenu.h"
#include "Globals.h"
#include "MicromouseLeds.h"
#include "UART_L3.h"
#include <Wire.h>
#include "ButtonAdvanced.h"
#include "game/GameState.h"


arduino::UART Serial2(28, 29, NC, NC);
UART_L3 uartL3;
ButtonAdvanced buttonAdvanced;

void setup()
{
  // Communication setup
  Serial.begin(115200);
  uartL3.begin();

  // I2C for ToF
  Wire.begin();

  // Menu and Display setup
  rpState = RP_STATE_MENU;
  menuAppSetup();
}

void loop()
{
  uartL3.update();
  buttonAdvanced.update();
  ledUpdate();

  // Top level State machine
  static RP_STATE lastState = rpState;
  if (lastState != rpState) {
    if (lastState == RP_STATE_GAME)
      gameLeave();

    if (rpState == RP_STATE_MENU)
      menuAppActive();    // Set the callbacks for Menu
    else if (rpState == RP_STATE_GAME)
      gameActive();

    lastState = rpState;
  }

  if (rpState == RP_STATE_MENU) {
    menuAppLoop();
  }
  if (rpState == RP_STATE_GAME) {
    gameLoop();
  }
}
