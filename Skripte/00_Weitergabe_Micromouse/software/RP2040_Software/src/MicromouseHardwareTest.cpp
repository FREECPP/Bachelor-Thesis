#include "MicromouseHardwareTest.h"
#include "MicromouseGraphics.h"
#include <COMMUNICATION_CODES.h>
#include "MicromouseLeds.h"
#include "MicromouseMapping.h"
#include "MicromouseMappingState.h"
#include "MicromouseSolving.h"
#include "MicromousePersistence.h"
#include "MicromouseMenu.h"
#include "MicromousePlatform.h"
#include "UART_L3.h"
#include "game/GameBumperConfig.h"

#include <Pololu3piPlus2040Motors.h>
#include <Pololu3piPlus2040Encoders.h>
#include <Pololu3piPlus2040LineSensors.h>
#include <Pololu3piPlus2040IMU.h>
#include <Pololu3piPlus2040BumpSensors.h>
#include <Pololu3piPlus2040Buzzer.h>
#include <OPT3101.h>
#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include "ButtonAdvanced.h"

using namespace Pololu3piPlus2040;

extern Motors      motors;
extern LineSensors lineSensors;
extern OPT3101     tof;
extern IMU         imu;
extern ButtonAdvanced buttonAdvanced;
extern int solveKurvenModusIdx;   // 0 = Kurve, 1 = Punkt (Menu-Einstellung)
extern UART_L3 uartL3;
extern bool g_btControllerConnected;

// ============================================================
// Shared display helpers
// ============================================================

static void drawHeader(const char* title)
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawUTF8(2, 10, title);
  u8g2.setDrawColor(1);
}

static void drawFooter(const char* hint)
{
  u8g2.drawLine(0, 53, 128, 53);
  u8g2.drawUTF8(2, 63, hint);
}

// ============================================================
// 1. Battery
// ============================================================
void testBattery()
{
  uint32_t lastDraw = 0;

  while (true)
  {
    // // menuPollButtons(); replaced by
    menuPollInputs();
    if (menuGetPressedA()) return;

    uint32_t now = millis();
    if (now - lastDraw >= 200)
    {
      lastDraw = now;

      uint16_t mv = mappingReadBatteryMv();
      int pct = (int)((long)(mv - 4300) * 100 / 900);  // 4300 mV = leer, 5200 mV = voll
      if (pct < 0)   pct = 0;
      if (pct > 100) pct = 100;

      drawHeader("Batterie");
      char buf[24];
      snprintf(buf, sizeof(buf), "%u mV", mv);
      u8g2.drawUTF8(2, 26, buf);
      snprintf(buf, sizeof(buf), "%d%%", pct);
      u8g2.drawUTF8(96, 26, buf);
      u8g2.drawFrame(2, 32, 124, 12);
      if (pct > 0) u8g2.drawBox(2, 32, pct * 124 / 100, 12);
      drawFooter("A:zurück");
      oledSendBuffer();
    }
    delay(15);
  }
}

// ============================================================
// 1b. Batterie-Lebensdauer (Entladetest unter Last)
//
// Faehrt Random Solving als Last durch das (bereits kartierte) Labyrinth.
// Bei einem Solving-Fehler (Kollision/Linienverlust/kein Pfad) wechselt der
// Test dauerhaft in einen Dreh-Fallback (auf der Stelle), um die Motorlast
// bis zum Brownout zu halten. Die laufende Zeit wird periodisch an
// Haltepunkten in den Flash geschrieben, damit das Ergebnis den Brownout
// ueberlebt und beim naechsten Aufruf angezeigt werden kann.
// ============================================================
static void battLifeError(const char* l1, const char* l2)
{
  drawHeader("Akku-Lebensdauer");
  u8g2.drawUTF8(2, 28, l1);
  u8g2.drawUTF8(2, 42, l2);
  drawFooter("A:zurück");
  oledSendBuffer();
}

static void battLifeFormatTime(uint32_t sec, char* buf, size_t n)
{
  snprintf(buf, n, "%lu:%02lu",
           (unsigned long)(sec / 60), (unsigned long)(sec % 60));
}

static void battLifePersist(uint32_t startMs, uint16_t startMv, uint16_t minMv,
                            uint16_t lastMv, uint16_t runs, bool spinning)
{
  BatteryTestResult r;
  r.elapsedSeconds = (millis() - startMs) / 1000;
  r.startMv        = startMv;
  r.minMv          = minMv;
  r.lastMv         = lastMv;
  r.runsCompleted  = runs;
  r.enteredSpin    = spinning ? 1 : 0;
  saveBatteryTestResult(r);
}

void testBatteryLife()
{
  // --- Voraussetzung: Karte ---
  if (!mapReady)
  {
    while (true)
    {
      menuPollInputs();
      battLifeError("Keine Karte!", "Zuerst kartieren.");
      if (menuGetPressedA()) return;
      delay(15);
    }
  }

  // --- Bestaetigung + letztes Ergebnis ---
  BatteryTestResult last;
  bool haveLast = loadBatteryTestResult(last);
  while (true)
  {
    menuPollInputs();
    drawHeader("Akku-Lebensdauer");
    u8g2.drawUTF8(2, 23, "Entlaedt Akku bis");
    u8g2.drawUTF8(2, 33, "Brownout (Last).");
    char buf[28];
    if (haveLast)
    {
      char t[12];
      battLifeFormatTime(last.elapsedSeconds, t, sizeof(t));
      snprintf(buf, sizeof(buf), "Letzter: %s / %umV", t, last.minMv);
    }
    else
    {
      snprintf(buf, sizeof(buf), "Letzter: --");
    }
    u8g2.drawUTF8(2, 46, buf);
    drawFooter("C:Start  A:zurück");
    oledSendBuffer();
    if (menuGetPressedA()) return;
    if (menuGetPressedC()) break;
    delay(15);
  }

  // --- Start (analog doSolvingZufaellig) ---
  setSolveSmoothTurns(solveKurvenModusIdx == 0);   // respektiert Menu-Einstellung
  calibrateLineSensors();
  buildGraphFromMaze();
  randomSeed(millis());
  if (!solvingResetToStart(0, 0, 0 /*NORTH*/))
  {
    while (true)
    {
      menuPollInputs();
      battLifeError("Keine Flash-Karte", "Neu kartieren!");
      if (menuGetPressedA()) return;
      delay(15);
    }
  }

  bool spinning = !startSolvingRandom();   // kein Ziel erreichbar -> sofort drehen

  // Realistische Spiel-Last: zusaetzlich zum Fahren leuchten die LEDs hell
  // und der BT-Controller wird verbunden (ESP-Funk aktiv) – beides zieht
  // wie im echten Spiel Strom, damit die gemessene Laufzeit realistisch ist.
  ledSetAll(LED_YELLOW);
  ledFlushToHardware();
  g_btControllerConnected = false;
  uartL3.connectBtController();
  bool rumbledOnConnect = false;

  const uint32_t startMs   = millis();
  uint32_t lastFlashMs     = startMs;
  uint32_t lastDrawMs      = 0;
  const uint16_t startMv   = mappingReadBatteryMv();
  int      emaMv           = startMv;      // entprellte Spannung (EMA, alpha=1/8)
  uint16_t minMv           = startMv;
  uint16_t runs            = 0;
  const int SPIN_SPEED     = PLATFORM_PWM(60);
  const uint32_t FLASH_INTERVAL_MS = 15000;
  const uint32_t POS_TX_INTERVAL_MS = 400;   // wie im Spiel (sendPosData-Broadcast)
  uint32_t lastPosTxMs     = startMs;

  // Sofort einen ersten Stand sichern (auch ein sehr frueher Brownout zaehlt).
  battLifePersist(startMs, startMv, minMv, (uint16_t)emaMv, runs, spinning);

  while (true)
  {
    menuPollInputs();

    // BT-Verbindung am Leben halten / Verbindungsstatus verarbeiten.
    uartL3.update();
    if (g_btControllerConnected && !rumbledOnConnect)
    {
      uartL3.sendControllerRumble(80, 200, 200);  // "verbunden"-Feedback
      rumbledOnConnect = true;
    }

    // Spannungsabtastung + entprellte Min-/Anzeigespannung (gegen Strom-Spitzen).
    uint16_t mv = mappingReadBatteryMv();
    emaMv += ((int)mv - emaMv) / 8;
    if ((uint16_t)emaMv < minMv) minMv = (uint16_t)emaMv;

    const uint32_t now = millis();
    const bool flashDue = (now - lastFlashMs >= FLASH_INTERVAL_MS);

    // Positionsdaten broadcasten wie im Spiel (zusaetzliche Funk-Last).
    if (now - lastPosTxMs >= POS_TX_INTERVAL_MS)
    {
      lastPosTxMs = now;
      uartL3.sendPosData(255 /*Broadcast*/,
                         (uint8_t)solveCurrentX(), (uint8_t)solveCurrentY());
    }

    // --- Manueller Abbruch ---
    if (menuGetPressedA())
    {
      stopMotors();
      battLifePersist(startMs, startMv, minMv, (uint16_t)emaMv, runs, spinning);
      while (true)   // Ergebnis-Screen
      {
        menuPollInputs();
        char t[12], b[28];
        battLifeFormatTime((millis() - startMs) / 1000, t, sizeof(t));
        drawHeader("Akku-Lebensdauer");
        snprintf(b, sizeof(b), "Laufzeit: %s", t);          u8g2.drawUTF8(2, 24, b);
        snprintf(b, sizeof(b), "min %u mV", minMv);          u8g2.drawUTF8(2, 36, b);
        snprintf(b, sizeof(b), "Laeufe: %u%s", runs, spinning ? " +Dreh" : "");
        u8g2.drawUTF8(2, 48, b);
        drawFooter("A:zurück");
        oledSendBuffer();
        if (menuGetPressedA()) { ledShowIdle(); return; }
        delay(15);
      }
    }

    // --- Last erzeugen ---
    if (!spinning)
    {
      bool done = solvingLoop();
      if (solveIsAtError())
      {
        spinning = true;            // Fallback: ab jetzt auf der Stelle drehen
      }
      else if (done)
      {
        runs++;
        if (flashDue)               // Haltepunkt: faelligen Flash-Write ausfuehren
        {
          battLifePersist(startMs, startMv, minMv, (uint16_t)emaMv, runs, spinning);
          lastFlashMs = millis();
        }
        delay(300);
        if (!startSolvingRandom()) spinning = true;
      }
    }
    else
    {
      if (flashDue)                 // Dreh-Modus: kurz anhalten, schreiben, weiter
      {
        motors.setSpeeds(0, 0);
        delay(20);
        battLifePersist(startMs, startMv, minMv, (uint16_t)emaMv, runs, spinning);
        lastFlashMs = millis();
      }
      motors.setSpeeds(SPIN_SPEED, -SPIN_SPEED);
    }

    // --- Live-Anzeige (gedrosselt; nach solvingLoop -> gewinnt das Bild) ---
    if (now - lastDrawMs >= 300)
    {
      lastDrawMs = now;
      char t[12], b[28];
      battLifeFormatTime((now - startMs) / 1000, t, sizeof(t));
      drawHeader("Akku-Lebensdauer");
      snprintf(b, sizeof(b), "Zeit %s", t);                    u8g2.drawUTF8(2, 23, b);
      snprintf(b, sizeof(b), "%u mV (min %u)", (uint16_t)emaMv, minMv);
      u8g2.drawUTF8(2, 34, b);
      snprintf(b, sizeof(b), "%s L:%u BT:%s", spinning ? "Dreh" : "Solv", runs,
               g_btControllerConnected ? "ok" : "--");
      u8g2.drawUTF8(2, 45, b);
      drawFooter("A:Abbruch");
      oledSendBuffer();
    }
  }
}

// ============================================================
// 2. Compass (Magnetometer)
// C:Reset – Min/Max zurücksetzen fuer neue Kalibrierfahrt
// Nach 360deg-Drehung: Ofs-Werte als MAG_OFFSET_X/Y eintragen
// ============================================================
void testCompass()
{
  int16_t minX =  32767, maxX = -32768;
  int16_t minY =  32767, maxY = -32768;
  uint32_t lastDraw = 0;

  while (true)
  {
    // menuPollButtons(); replaced by
    menuPollInputs();
    if (menuGetPressedA()) return;
    if (menuGetPressedC())
    {
      minX =  32767; maxX = -32768;
      minY =  32767; maxY = -32768;
    }

    uint32_t now = millis();
    if (now - lastDraw >= 100)
    {
      lastDraw = now;

      imu.readMag();
      int16_t rx = imu.m.x;
      int16_t ry = imu.m.y;

      if (rx < minX) minX = rx;
      if (rx > maxX) maxX = rx;
      if (ry < minY) minY = ry;
      if (ry > maxY) maxY = ry;

      int16_t ofsX = (minX + maxX) / 2;
      int16_t ofsY = (minY + maxY) / 2;

      float cx = (float)(rx - ofsX);
      float cy = (float)(ry - ofsY);
      float angle = atan2f(cy, cx) * 180.0f / (float)M_PI;
      if (angle < 0.0f) angle += 360.0f;

      drawHeader("Kompass");
      char buf[32];

      snprintf(buf, sizeof(buf), "X:%6d  Y:%6d", rx, ry);
      u8g2.drawUTF8(2, 24, buf);

      snprintf(buf, sizeof(buf), "Ofs:%5d /%5d", ofsX, ofsY);
      u8g2.drawUTF8(2, 35, buf);

      const char* dir;
      if      (angle <  45.0f || angle >= 315.0f) dir = "N";
      else if (angle <  135.0f)                   dir = "O";
      else if (angle <  225.0f)                   dir = "S";
      else                                         dir = "W";

      snprintf(buf, sizeof(buf), "W: %.1f Grad  %s", angle, dir);
      u8g2.drawUTF8(2, 46, buf);

      drawFooter("A:zurück  C:Reset");
      oledSendBuffer();
    }
    delay(15);
  }
}

// ============================================================
// 3. Liniensensoren
// A: zurück  B: Roh/Kalibriert umschalten  C: neu kalibrieren
// ============================================================
void testLineSensors()
{
  bool showRaw = false;
  uint32_t lastDraw = 0;

  while (true)
  {
    // menuPollButtons(); replaced by
    menuPollInputs();
    if (menuGetPressedA()) return;
    if (menuGetPressedB()) showRaw = !showRaw;
    if (menuGetPressedC())
    {
      // Neu kalibrieren – Funktion zeigt eigenen Screen und dreht Motoren
      calibrateLineSensors();
    }

    uint32_t now = millis();
    if (now - lastDraw >= 50)
    {
      lastDraw = now;

      uint16_t vals[5];
      uint16_t maxVal;
      if (showRaw)
      {
        lineSensors.read();
        for (int i = 0; i < 5; i++) vals[i] = lineSensors.rawSensorValues[i];
        maxVal = 2500;
      }
      else
      {
        lineSensors.readCalibrated();
        for (int i = 0; i < 5; i++) vals[i] = lineSensors.calibratedSensorValues[i];
        maxVal = 1000;
      }

      drawHeader(showRaw ? "Linien (roh)" : "Linien (kal.)");

      for (int i = 0; i < 5; i++)
      {
        int barW = (int)((long)vals[i] * 94 / maxVal);
        if (barW > 94) barW = 94;
        int y = 14 + i * 8;
        char buf[6];
        snprintf(buf, sizeof(buf), "%4u", vals[i]);
        u8g2.drawUTF8(2, y + 7, buf);
        u8g2.drawFrame(30, y + 1, 96, 6);
        if (barW > 0) u8g2.drawBox(30, y + 1, barW, 6);
      }

      drawFooter("B:Roh/Kal C:Kalib.");
      oledSendBuffer();
    }
    delay(15);
  }
}

// ============================================================
// 4. Gyroskop + Beschleunigungssensor
// ============================================================
void testGyroscope()
{
  bool showGyro = true;
  uint32_t lastDraw = 0;

  // EMA-Filter: alpha = 0.15 → träge, stabile Anzeige
  const float ALPHA = 0.15f;
  float gx = 0, gy = 0, gz = 0;
  float ax = 0, ay = 0, az = 0;
  bool first = true;

  while (true)
  {
    // menuPollButtons(); replaced by
    menuPollInputs();
    if (menuGetPressedA()) return;
    if (menuGetPressedB()) showGyro = !showGyro;

    uint32_t now = millis();
    if (now - lastDraw >= 50)
    {
      lastDraw = now;

      imu.readGyro();
      imu.readAcc();

      if (first)
      {
        gx = imu.g.x; gy = imu.g.y; gz = imu.g.z;
        ax = imu.a.x; ay = imu.a.y; az = imu.a.z;
        first = false;
      }
      else
      {
        gx += ALPHA * (imu.g.x - gx);
        gy += ALPHA * (imu.g.y - gy);
        gz += ALPHA * (imu.g.z - gz);
        ax += ALPHA * (imu.a.x - ax);
        ay += ALPHA * (imu.a.y - ay);
        az += ALPHA * (imu.a.z - az);
      }

      drawHeader(showGyro ? "Gyroskop" : "Beschleunigungssensor");
      char buf[24];
      int x = showGyro ? (int)gx : (int)ax;
      int y = showGyro ? (int)gy : (int)ay;
      int z = showGyro ? (int)gz : (int)az;
      snprintf(buf, sizeof(buf), "X: %6d", x);
      u8g2.drawUTF8(2, 26, buf);
      snprintf(buf, sizeof(buf), "Y: %6d", y);
      u8g2.drawUTF8(2, 37, buf);
      snprintf(buf, sizeof(buf), "Z: %6d", z);
      u8g2.drawUTF8(2, 48, buf);
      drawFooter("A:zurück B:Gyro/Acc");
      oledSendBuffer();
    }
    delay(15);
  }
}

// ============================================================
// 5. Bumper
// ============================================================
void testBumper()
{
  BumpSensors bumpers;
  uint32_t leftPressedSinceMs = 0;
  uint32_t rightPressedSinceMs = 0;

  drawHeader("Bumper Kal.");
  u8g2.drawUTF8(2, 28, "Kalibrierung...");
  u8g2.drawUTF8(2, 40, "Nicht berühren!");
  oledSendBuffer();
  bumpers.marginPercentage = GAME_BUMPER_MARGIN_PERCENT;
  bumpers.calibrate();

  while (true)
  {
    // menuPollButtons(); replaced by
    menuPollInputs();
    if (menuGetPressedA()) return;

    bumpers.read();
    const uint32_t now = millis();
    const bool leftRaw = bumpers.leftIsPressed();
    const bool rightRaw = bumpers.rightIsPressed();

    if (!leftRaw)
      leftPressedSinceMs = 0;
    else if (leftPressedSinceMs == 0)
      leftPressedSinceMs = now;

    if (!rightRaw)
      rightPressedSinceMs = 0;
    else if (rightPressedSinceMs == 0)
      rightPressedSinceMs = now;

    const bool left =
      leftPressedSinceMs != 0 &&
      (uint32_t)(now - leftPressedSinceMs) >= GAME_BUMPER_CONFIRM_MS;
    const bool right =
      rightPressedSinceMs != 0 &&
      (uint32_t)(now - rightPressedSinceMs) >= GAME_BUMPER_CONFIRM_MS;

    drawHeader("Bumper");

    if (left)  { u8g2.drawBox(2,  14, 58, 36); u8g2.setDrawColor(0); }
    else         u8g2.drawFrame(2,  14, 58, 36);
    u8g2.drawUTF8(10, 35, left  ? "LINKS"  : "Links");
    u8g2.setDrawColor(1);

    if (right) { u8g2.drawBox(68, 14, 58, 36); u8g2.setDrawColor(0); }
    else         u8g2.drawFrame(68, 14, 58, 36);
    u8g2.drawUTF8(76, 35, right ? "RECHTS" : "Rechts");
    u8g2.setDrawColor(1);

    drawFooter("A:zurück");
    oledSendBuffer();
    delay(15);
  }
}

// ============================================================
// 6. TOF Distanzsensor
// ============================================================
void testTOF()
{
  bool present = false;
  uint32_t lastInit = 0;
  uint32_t lastDraw = 0;

  while (true)
  {
    // menuPollButtons(); replaced by
    menuPollInputs();
    if (menuGetPressedA()) return;

    uint32_t now = millis();

    // Sensor-Erkennung alle 500 ms
    if (now - lastInit >= 500)
    {
      lastInit = now;
      tof.init();
      present = (tof.getLastError() == 0);
      if (present)
      {
        tof.setBrightness(OPT3101Brightness::Adaptive);
        tof.setFrameTiming(32);
        tof.setChannel(1);
      }
    }

    if (now - lastDraw >= 50)
    {
      lastDraw = now;

      drawHeader("TOF Distanz");
      char buf[24];

      if (!present)
      {
        u8g2.drawUTF8(2, 28, "Sensor nicht");
        u8g2.drawUTF8(2, 40, "gefunden! (I2C)");
      }
      else
      {
        tof.sample();
        if (tof.amplitude > 35 && tof.distanceMillimeters > 0)
        {
          snprintf(buf, sizeof(buf), "%u mm", tof.distanceMillimeters);
          u8g2.drawUTF8(2, 28, buf);
          int barW = (int)((long)tof.distanceMillimeters * 124 / 500);
          if (barW > 124) barW = 124;
          u8g2.drawFrame(2, 32, 124, 12);
          u8g2.drawBox(2, 32, barW, 12);
          snprintf(buf, sizeof(buf), "Amp: %u", tof.amplitude);
          u8g2.drawUTF8(2, 51, buf);
        }
        else
        {
          u8g2.drawUTF8(2, 30, "Kein Signal");
          snprintf(buf, sizeof(buf), "Amp: %u", tof.amplitude);
          u8g2.drawUTF8(2, 44, buf);
        }
      }

      drawFooter("A:zurück");
      oledSendBuffer();
    }
    delay(15);
  }
}

// ============================================================
// 7. Motoren
// ============================================================
void testMotors()
{
  const int SPEED = PLATFORM_PWM(80);

  while (true)
  {
    drawHeader("Motoren");
    u8g2.drawUTF8(2, 20, "B: Links  C: Rechts");
    u8g2.drawUTF8(2, 31, "Stick: fahren");
    u8g2.drawUTF8(2, 42, "DPad U/D/L: fahren");
    drawFooter("A:zurück  C:Start");
    oledSendBuffer();

    // menuPollButtons(); replaced by
    menuPollInputs();
    if (menuGetPressedA()) return;
    if (menuGetPressedC()) break;
    delay(15);
  }

  Encoders::getCountsAndResetLeft();
  Encoders::getCountsAndResetRight();
  int32_t encL = 0, encR = 0;

  while (true)
  {
    uint16_t sw = menuGetCtrlSwitches();
    buttonAdvanced.update();   // Hardware-Tasten aktualisieren (sonst isPressed/wasPressed eingefroren)

    // Exit: Hardware-A halten ODER Controller-B (back) drücken
    if (menuGetPressedA())
    {
      motors.setSpeeds(0, 0);
      while (buttonAdvanced.isPressed(0)) { menuPollInputs(); delay(5); }
      return;
    }

    // Fahren per Hardware-Tasten
    bool bHeld = buttonAdvanced.isPressed(1);
    bool cHeld = buttonAdvanced.isPressed(2);

    int speedL = bHeld ? SPEED : 0;
    int speedR = cHeld ? SPEED : 0;

    // Linker Stick: Tank-Drive (Y=vor/zurück, X=Lenkung) – Deadzone ±15.
    // X invertiert, sodass Stick rechts = Maus rechts.
    // Quadratische Lenkkurve + reduzierte Verstaerkung: kleine Auslenkungen
    // bewirken sehr wenig Lenkung, damit Geradeausfahrt einfacher ist.
    int8_t lx, ly;
    menuGetCtrlStickL(lx, ly);
    if (lx > 15 || lx < -15 || ly > 15 || ly < -15)
    {
      int fwd  = -(int)ly * SPEED / 128;  // Y negativ = Stick oben = vorwärts
      int lxAbs = (lx < 0) ? -(int)lx : (int)lx;
      int lxSq  = ((int)lx * lxAbs) / 128;   // Vorzeichen erhalten, quadratische Skalierung
      int turn  = -lxSq * SPEED / 192;       // invertiert + gedaempft (~1/3 der vorigen Aggressivitaet)
      speedL = constrain(fwd - turn, -SPEED, SPEED);
      speedR = constrain(fwd + turn, -SPEED, SPEED);
    }
    else if (sw & CSW_D_PAD_UP)    { speedL =  SPEED; speedR =  SPEED; }
    else if (sw & CSW_D_PAD_DOWN)  { speedL = -SPEED; speedR = -SPEED; }
    else if (sw & CSW_D_PAD_LEFT)  { speedL = -SPEED; speedR =  SPEED; }

    motors.setSpeeds(speedL, speedR);

    encL += Encoders::getCountsAndResetLeft();
    encR += Encoders::getCountsAndResetRight();

    drawHeader("Motoren");
    char buf[28];

    snprintf(buf, sizeof(buf), "L:%4d  R:%4d", speedL, speedR);
    u8g2.drawUTF8(4, 22, buf);

    if (speedL != 0) { u8g2.drawBox(2,  25, 58, 12); u8g2.setDrawColor(0); }
    u8g2.drawUTF8(10, 35, "Links");
    u8g2.setDrawColor(1);
    if (speedL == 0) u8g2.drawFrame(2, 25, 58, 12);

    if (speedR != 0) { u8g2.drawBox(68, 25, 58, 12); u8g2.setDrawColor(0); }
    u8g2.drawUTF8(76, 35, "Rechts");
    u8g2.setDrawColor(1);
    if (speedR == 0) u8g2.drawFrame(68, 25, 58, 12);

    snprintf(buf, sizeof(buf), "EL:%-6ld ER:%-6ld", (long)encL, (long)encR);
    u8g2.drawUTF8(2, 48, buf);

    drawFooter("A:Stop  B(ctrl):zurück");
    oledSendBuffer();
    delay(30);
  }
}

// ============================================================
// 8. LEDs
// ============================================================
void testLEDs()
{
  static const LedColor colors[]     = { LED_RED, LED_GREEN, LED_BLUE, LED_YELLOW, LED_CYAN, LED_WHITE, LED_ORANGE, LED_OFF };
  static const char*    colorNames[] = { "Rot",   "Gruen",  "Blau",   "Gelb",    "Cyan",   "Weiss",  "Orange",   "Aus"   };
  const int numColors = 8;
  int colorIdx = 0;

  uint32_t lastDraw = 0;

  while (true)
  {
    // menuPollButtons(); replaced by
    menuPollInputs();
    if (menuGetPressedA())
    {
      ledClear();
      ledFlushToHardware();
      return;
    }
    if (menuGetPressedB()) colorIdx = (colorIdx + 1) % numColors;
    if (menuGetPressedC()) colorIdx = (colorIdx - 1 + numColors) % numColors;

    uint32_t now = millis();
    if (now - lastDraw >= 80)
    {
      lastDraw = now;

      ledSetAll(colors[colorIdx]);
      ledFlushToHardware();

      drawHeader("LEDs");
      u8g2.drawUTF8(2, 30, colorNames[colorIdx]);

      bool isOn = (colors[colorIdx].r || colors[colorIdx].g || colors[colorIdx].b);
      for (int i = 0; i < LED_COUNT; i++)
      {
        int x = 2 + i * 20;
        if (isOn) u8g2.drawBox(x, 34, 16, 10);
        else      u8g2.drawFrame(x, 34, 16, 10);
      }

      drawFooter("A:zurück B/C:Farbe");
      oledSendBuffer();
    }
    delay(15);
  }
}

// Letztes Selbsttest-Ergebnis (für Info-Screen)
static int  g_selftestPassed = 0;
static int  g_selftestTotal  = 0;
static bool g_selftestRan    = false;

// ============================================================
// Selbsttest – läuft beim Einschalten wenn aktiviert
// Alle 6 Ergebnisse gleichzeitig sichtbar (kein Scrollen).
// ============================================================
void selftest()
{
  // u8g2_font_5x8_tf: 5px breit, 8px Zeilenhöhe → 8 Zeilen in 64px
  // 6 Tests bei y = 8, 16, 24, 32, 40, 48  (plus Leerzeile am Ende)
  static const uint8_t* FONT   = u8g2_font_5x8_tf;
  static const int      LINE_H = 8;

  struct TestLine { char label[20]; bool pass; bool valid; };
  TestLine buf[6] = {};
  int totalTests = 0;
  int passCount  = 0;

  auto redraw = [&]() {
    u8g2.clearBuffer();
    u8g2.setFont(FONT);
    for (int i = 0; i < 6; i++) {
      if (!buf[i].valid) break;
      int baseline = (i + 1) * LINE_H;
      if (!buf[i].pass) {
        u8g2.drawBox(0, baseline - LINE_H + 1, 128, LINE_H);
        u8g2.setDrawColor(0);
      }
      char line[26];
      snprintf(line, sizeof(line), "%-18s%s", buf[i].label, buf[i].pass ? "OK" : "!!");
      u8g2.drawUTF8(2, baseline, line);
      if (!buf[i].pass) u8g2.setDrawColor(1);
    }
    oledSendBuffer();
  };

  auto addResult = [&](const char* label, bool ok) {
    if (totalTests >= 6) return;
    buf[totalTests].pass  = ok;
    buf[totalTests].valid = true;
    strncpy(buf[totalTests].label, label, 18);
    buf[totalTests].label[18] = '\0';
    if (ok) passCount++;
    totalTests++;
    redraw();
    delay(150);
  };

  // --- Tests ---
  addResult("Batterie",  mappingReadBatteryMv() >= 4300);

  imu.readGyro();
  addResult("Gyroskop",  imu.g.x != 0 || imu.g.y != 0 || imu.g.z != 0);

  imu.readMag();
  addResult("Kompass",   abs((int)imu.m.x) + abs((int)imu.m.y) > 50);

  {
    lineSensors.read();
    bool ok = false;
    for (int i = 0; i < 5; i++)
      if (lineSensors.rawSensorValues[i] > 50) { ok = true; break; }
    addResult("Linie", ok);
  }

  {
    tof.setChannel(1);
    tof.sample();
    addResult("TOF", tof.getLastError() == 0);
  }

  {
    BumpSensors bumpers;
    bumpers.marginPercentage = GAME_BUMPER_MARGIN_PERCENT;
    bumpers.calibrate();

    bool continuouslyPressed = true;
    const uint32_t confirmStartMs = millis();
    while ((uint32_t)(millis() - confirmStartMs) < GAME_BUMPER_CONFIRM_MS)
    {
      bumpers.read();
      if (!bumpers.leftIsPressed() && !bumpers.rightIsPressed())
        continuouslyPressed = false;
      delay(5);
    }
    addResult("Bumper", !continuouslyPressed);
  }

  // --- Ergebnis speichern (für Info-Screen) ---
  g_selftestPassed = passCount;
  g_selftestTotal  = totalTests;
  g_selftestRan    = true;

  // --- Melodie ---
  if (menuGetSelftestBeeperEnabled()) {
    Buzzer buzzer;
    if (passCount == totalTests)
      buzzer.play("!T200 L8 o4ceg>c2");  // aufsteigend: alles OK
    else
      buzzer.play("!T120 L4 o5c-. r8 c-");  // traurig: ein Test fehlgeschlagen
    while (buzzer.isPlaying()) delay(10);
  }

  // --- Warten auf A oder Timeout ---
  uint32_t deadline = millis() + 4000;
  while (millis() < deadline) {
    // menuPollButtons(); replaced by
    menuPollInputs();
    if (menuGetPressedA()) return;
    delay(20);
  }
  drawMenu();
}

// ============================================================
// 9. Info
// ============================================================

// Interne Systemwartungsroutine (nicht fuer den Endnutzer)
static void performMemoryCompaction()
{
  static const char TEXT[] =
    "I'd just like to interject for a moment. What you're"
    " referring to as Linux, is in fact, GNU/Linux, or as"
    " I've recently taken to calling it, GNU plus Linux."
    " Linux is not an operating system unto itself, but"
    " rather another free component of a fully functioning"
    " GNU system made useful by the GNU corelibs, shell"
    " utilities and vital system components comprising a"
    " full OS as defined by POSIX. Many computer users run"
    " a modified version of the GNU system every day,"
    " without realizing it. Through a peculiar turn of"
    " events, the version of GNU which is widely used today"
    " is often called \"Linux\", and many of its users are"
    " not aware that it is basically the GNU system,"
    " developed by the GNU Project. There really is a Linux,"
    " and these people are using it, but it is just a part"
    " of the system they use. Linux is the kernel: the"
    " program in the system that allocates the machine's"
    " resources to the other programs that you run. The"
    " kernel is an essential part of an operating system,"
    " but useless by itself; it can only function in the"
    " context of a complete operating system. Linux is"
    " normally used in combination with the GNU operating"
    " system: the whole system is basically GNU with Linux"
    " added, or GNU/Linux. All the so-called \"Linux\""
    " distributions are really distributions of GNU/Linux.";

  // Beethoven – Fuer Elise (WoO 59), a-moll
  static const char MELODY[] =
    "!T72 L16 o5 e d# e d# e o4 b d o5 c o4 a4 r4"
    " o4 c e a b4 r4 o4 e g# b o5 c4 r4"
    " o5 e d# e d# e o4 b d o5 c o4 a4 r4"
    " o4 c e a b4 r4 o4 e g# b o5 c4 r4"
    " o5 e d# e d# e o4 b d o5 c o4 a4 r4"
    " o4 c e a b4 r4 o4 e o5 c b o4 a1";

  // Zeilenumbruch (ASCII, 25 Zeichen pro Zeile, Font 5x8)
  const int COLS     = 25;
  const int MAX_LINES = 40;
  const int LINE_H    = 9;
  char lines[MAX_LINES][COLS + 1];
  int  numLines = 0;

  const char* src = TEXT;
  while (*src && numLines < MAX_LINES) {
    const char* p       = src;
    const char* lastSp  = nullptr;
    int         count   = 0;
    while (*p && count < COLS) {
      if (*p == ' ') lastSp = p;
      p++; count++;
    }
    int copyLen; const char* next;
    if (!*p)         { copyLen = (int)(p - src); next = p; }
    else if (lastSp) { copyLen = (int)(lastSp - src); next = lastSp + 1; }
    else             { copyLen = COLS; next = src + COLS; }
    if (copyLen > 0) {
      memcpy(lines[numLines], src, copyLen);
      lines[numLines][copyLen] = '\0';
      numLines++;
    }
    src = next;
  }

  Buzzer buzzer;
  buzzer.play(MELODY);

  int totalH = numLines * LINE_H;
  for (int scrollY = 64; scrollY > -(totalH + 10); scrollY--) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tf);
    for (int i = 0; i < numLines; i++) {
      int y = scrollY + i * LINE_H;
      if (y >= -LINE_H && y <= 70)
        u8g2.drawUTF8(2, y + 7, lines[i]);
    }
    oledSendBuffer();
    // menuPollButtons(); replaced by
    menuPollInputs();
    if (menuGetPressedA()) { buzzer.stopPlaying(); return; }
    delay(45);
  }
  buzzer.stopPlaying();
}

void testInfo()
{
  bool     needRedraw = true;
  uint32_t bcStart    = 0;
  bool     bcActive   = false;

  while (true)
  {
    // menuPollButtons(); replaced by
    menuPollInputs();
    if (menuGetPressedA()) return;

    // Easter-egg: B + C gleichzeitig 10 Sekunden halten
    bool bHeld = buttonAdvanced.isPressed(1);
    bool cHeld = buttonAdvanced.isPressed(2);
    if (bHeld && cHeld) {
      if (!bcActive) { bcStart = millis(); bcActive = true; }
      if (millis() - bcStart >= 10000) {
        performMemoryCompaction();
        bcActive   = false;
        needRedraw = true;
      }
    } else {
      bcActive = false;
    }

    if (!needRedraw) { delay(20); continue; }
    needRedraw = false;

    drawHeader("Info");

    u8g2.setFont(u8g2_font_6x10_tf);
#ifdef SW_VERSION
    u8g2.drawUTF8(2, 24, SW_VERSION);
#else
    u8g2.drawUTF8(2, 24, "v?.?/?.?.????");
#endif

#ifdef MOUSE_HYPER
    u8g2.drawUTF8(2, 35, "Typ: Hyper");
#else
    u8g2.drawUTF8(2, 35, "Typ: Normal");
#endif

    char testBuf[20];
    if (g_selftestRan)
      snprintf(testBuf, sizeof(testBuf), "Test: %d/%d %s",
               g_selftestPassed, g_selftestTotal,
               g_selftestPassed == g_selftestTotal ? "OK" : "FAIL");
    else
      snprintf(testBuf, sizeof(testBuf), "Test: --");
    u8g2.drawUTF8(2, 46, testBuf);

    drawFooter("A:zurück");
    oledSendBuffer();
  }
}
