#include "MenuInternal.h"
#include "../MicromouseSplash.h"

// ============================================================
// APP-SETUP
// Wird von main.cpp setup() aufgerufen.
// ============================================================
void menuAppSetup()
{
  // Serial.begin(115200); // Nix da, sowas kommt in die Main!!!!!!!!!!!!!!!!!!!!
  delay(300);

  u8g2.begin();
  // u8g2.begin() → SPI.begin() setzt GP16/18/19 auf GPIO_FUNC_SPI
  // (arduino-mbed board=pico: SPI_MISO=GP16, SPI_SCK=GP18, SPI_MOSI=GP19).
  // Das überschreibt GPIO_FUNC_PIO0, das QTRSensors (g_singletonQTR)
  // beim Global-Init für GP16–GP22 gesetzt hat.
  // Zwingend GPIO_FUNC_PIO0 wiederherstellen – NICHT SIO! PIO-State-Machines
  // kontrollieren Pins nur, wenn deren GPIO-Funktion auf PIO0 gesetzt ist.
  gpio_set_function(16, GPIO_FUNC_PIO0);
  gpio_set_function(17, GPIO_FUNC_PIO0);
  gpio_set_function(18, GPIO_FUNC_PIO0);
  gpio_set_function(19, GPIO_FUNC_PIO0);

  // SPI.begin() kann (je nach Pin-Mapping) auch SPI1 berühren – dessen
  // alternative Funktion liegt auf GP10/11/14/15. Genau dort sitzen die
  // Motoren des 3pi+ 2040:
  //   GP14 = rightPWM (PWM7A),  GP15 = leftPWM (PWM7B)
  //   GP10 = rightDirection (SIO), GP11 = leftDirection (SIO)
  // mbed::PwmOut konfiguriert die Pin-Funktion nur einmal beim globalen
  // Konstruktor; wird sie hinterher von SPI überschrieben, drehen die
  // Motoren nicht mehr (auch Test-Menü → Motoren bleibt still, obwohl
  // der Display-Wert „L:80 R:0" korrekt steht).
  // Hier zwingend zurück auf die jeweils korrekte Funktion.
  gpio_set_function(14, GPIO_FUNC_PWM);   // rightPWM
  gpio_set_function(15, GPIO_FUNC_PWM);   // leftPWM
  gpio_set_function(10, GPIO_FUNC_SIO);   // rightDirectionPin
  gpio_set_function(11, GPIO_FUNC_SIO);   // leftDirectionPin

  splashScreen();   // MIKROMAUS-JAGD-Logo, 2 s

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.clearBuffer();
  u8g2.drawUTF8(2, 20, "Maus startet...");
  oledSendBuffer();

  linkMenus();
  menuAppActive();    // Set callbacks

  // Background-Callback: comm()->update() waehrend blockierender Dreh-/Fahroperationen,
  // damit BT-Controller-Switches (Pacman) und Pos-Daten waehrend turnTo() /
  // followLineToNextIntersectionFast() nicht verfallen.
  mappingRegisterBackgroundUpdate(menuBackgroundUpdate);

  mappingSetup();  // TOF, Compass, Liniensensoren, Flash-Autoload

  {
    SavedSettings s = {};
    if (loadSettings(s))
    {
      if (s.mausModusIdx >= 0 && s.mausModusIdx < MODUS_COUNT)
        mausModusIdx = s.mausModusIdx;
      if (s.solveKurvenModusIdx >= 0 && s.solveKurvenModusIdx < 2)
        solveKurvenModusIdx = s.solveKurvenModusIdx;
      if (s.goalX >= 0 && s.goalX < MAZE_W)
        goalX = s.goalX;
      if (s.goalY >= 0 && s.goalY < MAZE_H)
        goalY = s.goalY;
      magOffsetX           = s.magOffsetX;
      magOffsetY           = s.magOffsetY;
      selftestEnabled      = s.selftestEnabled;
      selftestBeeperEnabled = s.selftestBeeperEnabled;
      if (s.gameLocalId >= 0 && s.gameLocalId <= 31)
        gameLocalId = s.gameLocalId;
      else
        gameLocalId = s.gameLocalId & 0x1F;
      setOwnIdRole(modusEnumFromIdx[mausModusIdx], (uint8_t)gameLocalId);
      if (s.startPosX >= 0 && s.startPosX < MAZE_W)
        startPosX = s.startPosX;
      if (s.startPosY >= 0 && s.startPosY < MAZE_H)
        startPosY = s.startPosY;
      if (s.tofWallThresholdMm >= 40 && s.tofWallThresholdMm <= 200)
        tofWallThresholdMm = (uint16_t)s.tofWallThresholdMm;
      if (s.turboDurationS >= TURBO_DURATION_MIN && s.turboDurationS <= TURBO_DURATION_MAX)
        turboDurationS = (uint8_t)s.turboDurationS;
      if (s.turboCooldownS >= TURBO_COOLDOWN_MIN && s.turboCooldownS <= TURBO_COOLDOWN_MAX)
        turboCooldownS = (uint8_t)s.turboCooldownS;
      if (s.turboFactorPct >= TURBO_FACTOR_MIN && s.turboFactorPct <= TURBO_FACTOR_MAX)
        turboFactorPct = (uint8_t)s.turboFactorPct;
    }
  }
  pushGameIdToEsp();   // ID vom RP zum ESP pushen (ESP-Persistenz unzuverlaessig)

  if (selftestEnabled)
    menuRunBlockingTest(selftest);
}

void menuAppActive() {
  // Wireless Data
  uartL3.registerCbReceiveMazeData(onMazeDataReceived);
  uartL3.registerCbReceiveBtControllerData(onSubscribedReceived);
  uartL3.registerCbConnectBtControllerState(onBtControllerStateChanged);
  uartL3.registerCbReceivePosData(nullptr);
  uartL3.registerCbConnectNowState(nullptr);
  uartL3.registerCbError(nullptr);
  uartL3.registerCbReceiveControlData(onGameControlReceived);

  // Buttons
  buttonAdvanced.setCallbackPressed(menuCbButtonPressed);
  buttonAdvanced.setCallbackReleased(menuCbButtonReleased);
  buttonAdvanced.setCallbackHold(nullptr);
  buttonAdvanced.clearEvents();
}

// ============================================================
// APP-LOOP
// Wird von main.cpp loop() aufgerufen.
// ============================================================
void menuAppLoop()
{
  // const bool editingGameId = (uiState == UI_EDIT && activeItem && activeItem->var == &gameLocalId);
  // if (!g_inGameMode && !editingGameId)
  //   syncGameIdFromCommunication();
  // Sinn?

  // Akku alle 5 s pruefen
  uint32_t now = millis();
  if (now - g_lastBattCheck >= 5000)
  {
    g_lastBattCheck = now;
    uint16_t mv = mappingReadBatteryMv();
    // Unter 500 mV = USB-betrieb, keine Warnung noetig
    g_lowBattery = (mv >= 500 && mv < BATT_WARN_MV);
  }

  // -------------------------------------------------------
  // Aktives Spiel: komplett von der globalen Spiel-State-Machine getrieben.
  // GameUI zeichnet die Phasen-Screens, GameState faehrt/kommuniziert.
  // -------------------------------------------------------
  // if (gameStateActive())
  // {
  //   if (g_pressedA)
  //     gameStateAbort();            // A bricht das Spiel ab
  //
  //   gameStateTick();
  //   gameUIRender();
  //
  //   if (gameStatePhase() == GS_DONE)
  //   {
  //     gameStateReset();
  //     gameUIResetView();
  //     uiState      = UI_NAVIGATE;
  //     g_inGameMode = false;
  //     ledShowIdle();
  //   }
  //   else
  //   {
  //     // Spielfarbe auf den LEDs (alle ausser Index 1 = Batterie-LED)
  //     LedColor gc = mausModi[mausModusIdx].dimColor;
  //     for (int i = 0; i < LED_COUNT; i++) { if (i != 1) ledSet(i, gc); }
  //     if (g_lowBattery && (millis() / 500) % 2 == 0) ledSet(1, BATT_LED_WARN);
  //     else ledSet(1, LED_OFF);
  //   }
  //
  //   applyCommRxLedSignal();
  //   ledFlushToHardware();
  //   return;
  // }

  switch (uiState)
  {
    // ----------------------------------------------------------
    case UI_MAP_RUNNING:
    {
      if (menuGetPressedA())
      {
        stopMotors();
        uiState = UI_NAVIGATE;
        ledShowIdle();
        break;
      }

      MappingRunStatus ms = mappingLoopStep();

      if (ms == MAPPING_RUN_DONE)
      {
        ledShowDone();
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawBox(0, 0, 128, 12);
        u8g2.setDrawColor(0);
        u8g2.drawUTF8(2, 10, "Kartierung fertig");
        u8g2.setDrawColor(1);
        u8g2.drawUTF8(2, 28, "Karte gespeichert");
        u8g2.drawUTF8(2, 40, "A = Menue");
        oledSendBuffer();
        applyCommRxLedSignal();
        ledFlushToHardware();

        while (true) { buttonAdvanced.update(); if (buttonAdvanced.isPressed(0)) break; delay(20); }
        uiState = UI_NAVIGATE;
        ledShowIdle();
      }
      else if (ms == MAPPING_RUN_ABORTED)
      {
        uiState = UI_NAVIGATE;
        ledShowIdle();
      }
      else if (ms == MAPPING_RUN_ERROR)
      {
        ledShowError();
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawBox(0, 0, 128, 12);
        u8g2.setDrawColor(0);
        u8g2.drawUTF8(2, 10, "FEHLER");
        u8g2.setDrawColor(1);
        u8g2.drawUTF8(2, 28, mappingLastError());
        u8g2.drawUTF8(2, 40, "A = Menue");
        oledSendBuffer();
        applyCommRxLedSignal();
        ledFlushToHardware();

        delay(300);
        while (true) { buttonAdvanced.update(); if (buttonAdvanced.isPressed(0)) break; delay(20); }
        uiState = UI_NAVIGATE;
        ledShowIdle();
      }
      // MAPPING_RUN_RUNNING → Loop laeuft weiter
      applyCommRxLedSignal();
      ledFlushToHardware();
      return;
    }

    // ----------------------------------------------------------
    case UI_SOLVE_RUNNING:
    {
      if (menuGetPressedA())
      {
        stopMotors();
        uiState = UI_NAVIGATE;
        g_inGameMode = false;
        ledShowIdle();
        break;
      }

      bool done = solvingLoop();

      if (done)
      {
        if (solveIsRandomMode())
        {
          delay(400);
          if (!startSolvingRandom())
          {
            stopMotors();
            delay(500);
            uiState = UI_NAVIGATE;
            g_inGameMode = false;
            ledShowIdle();
          }
          // sonst: startSolvingRandom() hat neues Ziel gesetzt → g_inGameMode bleibt
        }
        else
        {
          delay(500);
          uiState = UI_NAVIGATE;
          g_inGameMode = false;
          ledShowIdle();
        }
      }
      // LEDs: game-farbe wenn Demo, sonst gruen (ledShowSolving)
      if (g_inGameMode)
      {
        LedColor c = mausModi[mausModusIdx].dimColor;
        for (int i = 0; i < LED_COUNT; i++) { if (i != 1) ledSet(i, c); }
        if (g_lowBattery && (millis() / 500) % 2 == 0) ledSet(1, BATT_LED_WARN);
        else ledSet(1, LED_OFF);
      }
      applyCommRxLedSignal();
      ledFlushToHardware();
      return;
    }

    // ----------------------------------------------------------
    case UI_MAP_VIEW:
    {
      if (menuGetPressedA())
      {
        uiState = UI_NAVIGATE;
        ledShowIdle();
      }
      applyCommRxLedSignal();
      ledFlushToHardware();
      return;
    }

    // ----------------------------------------------------------
    default:
      break;
  }



  // Nach handleInput() kann sich der UI-State geaendert haben
  // (z.B. doKarteAnzeigen() -> UI_MAP_VIEW).
  // Menue nicht ueber die soeben gezeichnete Anzeige malen.
  static unsigned long lastAutoMenuDraw = 0;
  if ((uiState == UI_NAVIGATE || uiState == UI_EDIT )&& lastAutoMenuDraw + 100 < millis()) {
    lastAutoMenuDraw = millis();
    drawMenu();
  }
  // => Called at the end of handle input

  // TODO: MENU LED Interatkionen: Event driven
  // Game-Modus LEDs (alle außer Index 1 = Batterie-LED)
  bool inGameContext = g_inGameMode
                    || (uiState == UI_NAVIGATE && currentMenu == &spielEinstMenu);
  LedColor gameColor = mausModi[mausModusIdx].dimColor;
  for (int i = 0; i < LED_COUNT; i++)
  {
    if (i == 1) continue;
    ledSet(i, inGameContext ? gameColor : LED_OFF);
  }

  // Batterie-LED (Index 1): blinkt rot wenn schwach, sonst aus
  if (g_lowBattery && (millis() / 500) % 2 == 0)
    ledSet(1, BATT_LED_WARN);
  else
    ledSet(1, LED_OFF);

  applyCommRxLedSignal();

  ledFlushToHardware();
}
