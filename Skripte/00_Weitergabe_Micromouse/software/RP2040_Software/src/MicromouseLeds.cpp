#include "MicromouseLeds.h"
#include <Pololu3piPlus2040LEDs.h>

using namespace Pololu3piPlus2040;

// Wunsch-Puffer: Core 1 darf schreiben (volatile), Core 0 liest in ledFlushToHardware().
static volatile uint8_t g_ledR[LED_COUNT] = {};
static volatile uint8_t g_ledG[LED_COUNT] = {};
static volatile uint8_t g_ledB[LED_COUNT] = {};

static volatile uint8_t g_restoreR[LED_COUNT] = {};
static volatile uint8_t g_restoreG[LED_COUNT] = {};
static volatile uint8_t g_restoreB[LED_COUNT] = {};
static volatile bool g_leftBumperIndicator = false;
static volatile bool g_rightBumperIndicator = false;
static bool g_flashActive = false;
static uint32_t g_flashUntilMs = 0;
static LedColor g_flashColor = LED_OFF;

// NUR Core 0 – SPI0 auf GP3(TX)/GP6(SCK), geteilt mit OLED.
static RGBLEDs leds;

void ledSet(uint8_t index, LedColor color)
{
  if (index >= LED_COUNT) return;
  g_ledR[index] = color.r;
  g_ledG[index] = color.g;
  g_ledB[index] = color.b;
}

void ledSetAll(LedColor color)
{
  for (uint8_t i = 0; i < LED_COUNT; i++) ledSet(i, color);
}

void ledClear()       { ledSetAll(LED_OFF); }
void ledSetBumperIndicators(bool leftPressed, bool rightPressed)
{
  g_leftBumperIndicator = leftPressed;
  g_rightBumperIndicator = rightPressed;
}

void ledShowMapping() { ledSetAll(LED_BLUE); }
void ledShowSolving() { ledSetAll(LED_GREEN); }
void ledShowDone()    { ledSetAll(LED_WHITE); }
void ledShowError()   { ledSetAll(LED_RED); }
void ledShowIdle()    { ledClear(); }

void flashColor(LedColor flashColor, unsigned long duration_ms)
{
  if (!g_flashActive)
  {
    for (uint8_t i = 0; i < LED_COUNT; i++)
    {
      g_restoreR[i] = g_ledR[i];
      g_restoreG[i] = g_ledG[i];
      g_restoreB[i] = g_ledB[i];
    }
  }

  g_flashColor = flashColor;
  g_flashUntilMs = millis() + duration_ms;
  g_flashActive = true;
  ledFlushToHardware();
}

void ledFlushToHardware()
{
  static constexpr uint8_t FRONT_RIGHT_LED_INDEX = 3;
  static constexpr uint8_t FRONT_LEFT_LED_INDEX = 5;
  const bool useFlash = g_flashActive && (int32_t)(millis() - g_flashUntilMs) < 0;
  for (uint8_t i = 0; i < LED_COUNT; i++)
  {
    const bool showBumper =
      (i == FRONT_LEFT_LED_INDEX && g_leftBumperIndicator) ||
      (i == FRONT_RIGHT_LED_INDEX && g_rightBumperIndicator);
    const uint8_t r = showBumper ? LED_WHITE.r : (useFlash ? g_flashColor.r : g_ledR[i]);
    const uint8_t g = showBumper ? LED_WHITE.g : (useFlash ? g_flashColor.g : g_ledG[i]);
    const uint8_t b = showBumper ? LED_WHITE.b : (useFlash ? g_flashColor.b : g_ledB[i]);
    leds.set(i, RGB(r, g, b));
  }
  leds.show();
  // SharedSPI ist jetzt im LED-Modus (GP6=SCK, m_configuredForDisplay=false).
  // Pins NICHT manuell umschalten — das wuerde SharedSPI's internen State brechen.
  // oledSendBuffer() ruft ledSwitchSpiToDisplay() vor dem naechsten Frame.
}

void ledUpdate()
{
  if (!g_flashActive)
    return;

  if ((int32_t)(millis() - g_flashUntilMs) < 0)
    return;

  for (uint8_t i = 0; i < LED_COUNT; i++)
  {
    g_ledR[i] = g_restoreR[i];
    g_ledG[i] = g_restoreG[i];
    g_ledB[i] = g_restoreB[i];
  }
  g_flashActive = false;
  ledFlushToHardware();
}

void ledSwitchSpiToDisplay()
{
  // Loest SharedSPI::switchToDisplay() aus: GP2=SCK, GP6=SIO, 4 MHz,
  // und setzt m_configuredForDisplay=true — damit switchToLEDs() beim
  // naechsten leds.show() korrekt umschaltet statt still zu bleiben.
  static char dummy = 0;
  SharedSPI::getSharedSPI()->writeToDisplay(&dummy, 0, &dummy, 0);
}
