#include "MenuInternal.h"

// ============================================================
// AKTIONEN – Implementierungen
// ============================================================
void doKartierung()
{
  if (!mapReady)
  {
    mappingStartRun();
    ledShowMapping();
    uiState = UI_MAP_RUNNING;
  }
  else
  {
    // Karte vorhanden: Bestaetigung anfordern (A = neu, C = abbrechen)
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawBox(0, 0, 128, 12);
    u8g2.setDrawColor(0);
    u8g2.drawUTF8(2, 10, "Neue Karte?");
    u8g2.setDrawColor(1);
    u8g2.drawUTF8(2, 28, "C = Ja, neu starten");
    u8g2.drawUTF8(2, 40, "A = Abbrechen");
    oledSendBuffer();

    while (true)
    {
      //pollButtonFifo(); replaced with:
      buttonAdvanced.update();
      if (buttonAdvanced.isPressed(2))
      {
        mappingStartRun();
        ledShowMapping();
        uiState = UI_MAP_RUNNING;
        return;
      }
      if (buttonAdvanced.isPressed(0)) return;
      delay(20);
    }
  }
}

void doKarteAnzeigen()
{
  drawReceivedMaze();
  uiState = UI_MAP_VIEW;
}

static bool buildAndShowMazeFrame(MazeCommFrame &frame)
{
  if (!mapReady)
  {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawUTF8(2, 20, "Keine Karte!");
    u8g2.drawUTF8(2, 34, "Zuerst kartieren.");
    oledSendBuffer();
    delay(1000);
    return false;
  }

  if (!mazeBuildCommFrame(frame))
  {
    ledShowError();
    ledFlushToHardware();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawUTF8(2, 20, "Frame Fehler");
    u8g2.drawUTF8(2, 34, "Format pruefen");
    oledSendBuffer();
    delay(1000);
    ledShowIdle();
    ledFlushToHardware();
    return false;
  }

  mazePrintCommFrameToSerial(frame);
  return true;
}

void doKarteSendenSerial()
{
  MazeCommFrame frame;
  if (!buildAndShowMazeFrame(frame)) return;

  buildGraphFromMaze();
  exportMazeToSerial();
  exportGraphToSerial();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawUTF8(2, 10, "FRAME SERIAL");
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(2, 28, "Format OK");
  u8g2.drawUTF8(2, 40, "Serial Log");
  oledSendBuffer();

  ledShowDone();
  ledFlushToHardware();
  delay(800);
  ledShowIdle();
  ledFlushToHardware();
}

void doKarteSendenWireless()
{
  MazeCommFrame frame;
  if (!buildAndShowMazeFrame(frame)) return;

  uint8_t txBuffer[MAZE_FRAME_MAX_BYTES];
  size_t txLength = mazeCopyCommFrameBytes(frame, txBuffer, sizeof(txBuffer));
  if (txLength == 0 || txLength > 254)
  {
    ledSetAll(LED_RED);
    ledFlushToHardware();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawBox(0, 0, 128, 12);
    u8g2.setDrawColor(0);
    u8g2.drawUTF8(2, 10, "WIRELESS SEND");
    u8g2.setDrawColor(1);
    u8g2.drawUTF8(2, 28, "Buffer Fehler");
    oledSendBuffer();
    delay(1000);
    ledShowIdle();
    ledFlushToHardware();
    return;
  }



  uartL3.sendMazeData(255, static_cast<uint8_t>(txLength), txBuffer);
  for (uint8_t i = 0; i < 20; i++)
  {
    uartL3.update();
    delay(10);
  }

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawUTF8(2, 10, "WIRELESS SEND");
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(2, 28, "Frame gesendet");
  u8g2.drawUTF8(2, 40, "an ESP32");
  oledSendBuffer();

  ledSetAll(LED_GREEN);
  ledFlushToHardware();
  delay(1000);
  ledShowIdle();
  ledFlushToHardware();
}

// Zeigt eine zweizeilige Lösen-Fehlermeldung sichtbar an (mit Pause),
// damit sie nicht sofort vom Menü-Redraw übermalt wird.
static void showSolveStartError(const char *line1, const char *line2)
{
  ledShowError();
  ledFlushToHardware();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawUTF8(2, 10, "LOESEN");
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(2, 30, line1);
  if (line2 && line2[0]) u8g2.drawUTF8(2, 44, line2);
  oledSendBuffer();
  delay(1500);
  ledShowIdle();
  ledFlushToHardware();
}

void doSolvingZiel()
{
  if (!mapReady)
  {
    showSolveStartError("Keine Karte!", "Zuerst kartieren.");
    return;
  }

  // Trivialfall: Ziel == Start (0,0). Dijkstra liefert dann einen Pfad
  // der Laenge 1 → die Solving-Schleife meldet sofort "fertig", die Maus
  // faehrt nie los. Genau das Symptom "C startet das Loesen nicht".
  if (goalX == 0 && goalY == 0)
  {
    showSolveStartError("Ziel = Start (0,0)", "Ziel X/Y setzen!");
    return;
  }

  setSolveSmoothTurns(solveKurvenModusIdx == 0);
  calibrateLineSensors();
  buildGraphFromMaze();

  if (!solvingResetToStart(0, 0, 0 /*NORTH*/))
  {
    showSolveStartError("Keine Flash-Karte", "Neu kartieren!");
    return;
  }
  if (startSolving(goalX, goalY))
  {
    ledShowSolving();
    setBlinkOnIntersection(true);   // LEDs blinken einmal pro Kreuzung
    uiState = UI_SOLVE_RUNNING;
  }
  else
  {
    showSolveStartError("Kein Pfad zum Ziel", "Anderes Ziel?");
  }
}

void doSolvingZufaellig()
{
  if (!mapReady)
  {
    showSolveStartError("Keine Karte!", "Zuerst kartieren.");
    return;
  }

  setSolveSmoothTurns(solveKurvenModusIdx == 0);
  calibrateLineSensors();
  buildGraphFromMaze();
  randomSeed(millis());

  if (!solvingResetToStart(0, 0, 0 /*NORTH*/))
  {
    showSolveStartError("Keine Flash-Karte", "Neu kartieren!");
    return;
  }
  if (startSolvingRandom())
  {
    ledShowSolving();
    setBlinkOnIntersection(true);   // LEDs blinken einmal pro Kreuzung
    uiState = UI_SOLVE_RUNNING;
  }
  else
  {
    showSolveStartError("Kein Zufallsziel", "erreichbar.");
  }
}

void doSpielDemo()
{
  //rpState = RP_STATE::RP_STATE_GAME;
  Serial.println("SpielDemo not implemented yet");
}

// ----------------------------------------------------------------
// Mini-Karte mit markiertem Startnode fuer Bestaetigungsscreen.
// Zeichnet das Labyrinth (falls mapReady) und einen Punkt auf
// dem Ziel-Node. Koordinaten + Blickrichtung werden als Text
// angezeigt. Footer: A=Abbr, C=OK.
// ----------------------------------------------------------------
static void drawStartNodeMapScreen(const char* roleName,
                                   int targetX, int targetY, int targetHeading)
{
  const int CELL  = 6;
  const int MAP_W = MAZE_W * CELL + 1;   // 61 px
  const int MAP_H = MAZE_H * CELL + 1;   // 31 px
  const int MAP_X = (128 - MAP_W) / 2;   // 33 px links
  const int MAP_Y = 21;

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tf);

  // Header
  u8g2.drawBox(0, 0, 128, 10);
  u8g2.setDrawColor(0);
  u8g2.drawStr(2, 8, roleName);
  u8g2.setDrawColor(1);

  // Koordinaten-Zeile (klein)
  char buf[24];
  snprintf(buf, sizeof(buf), "x=%d y=%d %s",
           targetX, targetY, dirName(targetHeading));
  u8g2.drawStr(2, 19, buf);

  // Karten-Rahmen
  u8g2.drawFrame(MAP_X, MAP_Y, MAP_W, MAP_H);

  if (mapReady)
  {
    for (int x = 0; x < MAZE_W; x++)
    {
      for (int y = 0; y < MAZE_H; y++)
      {
        int px = MAP_X + x * CELL;
        int py = MAP_Y + y * CELL;
        if (maze[x][y].known[NORTH] && maze[x][y].wall[NORTH])
          u8g2.drawHLine(px, py, CELL + 1);
        if (maze[x][y].known[EAST] && maze[x][y].wall[EAST])
          u8g2.drawVLine(px + CELL, py, CELL + 1);
        if (maze[x][y].known[SOUTH] && maze[x][y].wall[SOUTH])
          u8g2.drawHLine(px, py + CELL, CELL + 1);
        if (maze[x][y].known[WEST] && maze[x][y].wall[WEST])
          u8g2.drawVLine(px, py, CELL + 1);
      }
    }
  }

  // Ziel-Node: gefuellter Kreis (r=2)
  int tx = MAP_X + targetX * CELL + CELL / 2;
  int ty = MAP_Y + targetY * CELL + CELL / 2;
  u8g2.drawDisc(tx, ty, 2);

  // Footer
  u8g2.drawLine(0, 54, 128, 54);
  u8g2.drawStr(2, 63, "A=Abbr.");
  u8g2.drawStr(128 - (int)strlen("C=OK") * 5 - 2, 63, "C=OK");
  oledSendBuffer();
}

void doSpielPlay()
{
  // Die Geister werden NICHT mehr hier (bei Menue-Auswahl) gestartet, sondern
  // erst mit dem Xbox-Tastendruck in der Pacman-Lobby (startPacmanRound
  // broadcastet dann GC_START_GAME). So beginnen Geister und Pacman synchron –
  // und ein zufaelliges Bewegen der Geister waehrend der Lobby entfaellt.
  rpState = RP_STATE::RP_STATE_GAME;
}

// ============================================================
// HARDWARE-EINSTELLUNGEN – Offset-Editor
// B gedrückt halten = -100/Tick, C = +100/Tick, A = speichern
// ============================================================
static void editIntFast(const char* title, int* val)
{
  const int      STEP      = 100;
  const uint32_t REPEAT_MS = 120;
  uint32_t lastChange = 0;

  while (true)
  {
    // menuPollButtons(); replaced by
    buttonAdvanced.update();
    if (menuGetPressedA()) { saveCurrentSettings(); return; }

    uint32_t now = millis();
    if (now - lastChange >= REPEAT_MS)
    {
      bool changed = false;
      // Halten = wiederholen: gehaltenen Zustand lesen, NICHT die Press-Flanke
      // (menuGetPressedB/C liefert nur einen Einzel-Event pro Tastendruck).
      if (buttonAdvanced.isPressed(1)) { *val -= STEP; if (*val < -32000) *val = -32000; changed = true; }
      if (buttonAdvanced.isPressed(2)) { *val += STEP; if (*val >  32000) *val =  32000; changed = true; }
      if (changed) lastChange = now;
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawBox(0, 0, 128, 12);
    u8g2.setDrawColor(0);
    u8g2.drawUTF8(2, 10, title);
    u8g2.setDrawColor(1);

    char buf[12];
    snprintf(buf, sizeof(buf), "%d", *val);
    u8g2.setFont(u8g2_font_8x13_tf);
    int w = u8g2.getStrWidth(buf);
    u8g2.drawUTF8((128 - w) / 2, 35, buf);

    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawLine(0, 53, 128, 53);
    u8g2.drawUTF8(2, 63, "A:OK  B:-100  C:+100");
    oledSendBuffer();
    delay(15);
  }
}

void doEditMagOffsetX()  { editIntFast("Kompass Ofs X", &magOffsetX); }
void doEditMagOffsetY()  { editIntFast("Kompass Ofs Y", &magOffsetY); }
void doResetMagOffsets()
{
  magOffsetX = MAG_OFFSET_X_DEFAULT;
  magOffsetY = MAG_OFFSET_Y_DEFAULT;
  saveCurrentSettings();
}

void doTofCalibration()
{
  const uint16_t MIN_THRESHOLD_MM = 40;
  const uint16_t MAX_THRESHOLD_MM = 200;
  const uint16_t STEP_MM = 5;
  uint16_t distanceMm = mappingReadFrontDistance();
  uint32_t lastMeasurementMs = millis();

  buttonAdvanced.clearEvents();

  while (true)
  {
    menuPollInputs();
    if (menuGetPressedA())
    {
      saveCurrentSettings();
      return;
    }
    if (menuGetPressedB())
      tofWallThresholdMm =
        tofWallThresholdMm >= MIN_THRESHOLD_MM + STEP_MM
          ? tofWallThresholdMm - STEP_MM
          : MIN_THRESHOLD_MM;
    if (menuGetPressedC())
      tofWallThresholdMm =
        tofWallThresholdMm <= MAX_THRESHOLD_MM - STEP_MM
          ? tofWallThresholdMm + STEP_MM
          : MAX_THRESHOLD_MM;

    uint32_t now = millis();
    if (now - lastMeasurementMs >= 150)
    {
      distanceMm = mappingReadFrontDistance();
      lastMeasurementMs = now;
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawBox(0, 0, 128, 12);
    u8g2.setDrawColor(0);
    u8g2.drawUTF8(2, 10, "ToF Kalibrieren");
    u8g2.setDrawColor(1);

    char buf[24];
    if (distanceMm >= 9000)
      snprintf(buf, sizeof(buf), "Distanz: kein Signal");
    else
      snprintf(buf, sizeof(buf), "Distanz: %u mm", (unsigned)distanceMm);
    u8g2.drawUTF8(2, 25, buf);

    snprintf(buf, sizeof(buf), "Schwelle: %u mm",
             (unsigned)tofWallThresholdMm);
    u8g2.drawUTF8(2, 38, buf);

    if (distanceMm >= 9000)
      u8g2.drawUTF8(2, 50, "Status: unbekannt");
    else if (distanceMm < tofWallThresholdMm)
      u8g2.drawUTF8(2, 50, "Status: WAND");
    else
      u8g2.drawUTF8(2, 50, "Status: FREI");

    u8g2.drawLine(0, 53, 128, 53);
    u8g2.drawUTF8(2, 63, "A:OK B:-5 C:+5");
    oledSendBuffer();
    delay(15);
  }
}

void doBtConnect()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawUTF8(2, 10, "BT Controller");
  u8g2.setDrawColor(1);
  u8g2.drawUTF8(2, 30, "Verbinde...");
  u8g2.drawUTF8(2, 48, "A = Abbrechen");
  oledSendBuffer();

  g_btControllerConnected = false;
  uartL3.connectBtController();

  uint32_t deadline = millis() + 8000;
  while (millis() < deadline)
  {
    uartL3.update();
    // pollButtonFifo();
    buttonAdvanced.update();
    if (g_btControllerConnected || buttonAdvanced.isPressed(0)) break;
    delay(20);
  }

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawUTF8(2, 10, "BT Controller");
  u8g2.setDrawColor(1);
  if (g_btControllerConnected)
    u8g2.drawUTF8(2, 35, "Verbunden!");
  else if (buttonAdvanced.isPressed(0))
    u8g2.drawUTF8(2, 35, "Abgebrochen");
  else
    u8g2.drawUTF8(2, 35, "Timeout");
  oledSendBuffer();
  delay(1500);
}
