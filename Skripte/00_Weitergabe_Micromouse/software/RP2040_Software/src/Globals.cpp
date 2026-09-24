#include "Globals.h"
#include <Pololu3piPlus2040Buttons.h>

using namespace Pololu3piPlus2040;

// ── Globals ───────────────────────────────────────────────────────────────────
volatile bool g_abortRequested = false;
volatile bool g_btnAPressed    = false;
volatile bool g_btnBPressed    = false;
volatile bool g_btnCPressed    = false;

volatile uint8_t startPosX = 0;
volatile uint8_t startPosY = 2;
volatile uint8_t currentPosX;
volatile uint8_t currentPosY;
volatile uint8_t lastPosX;
volatile uint8_t lastPosY;

volatile RP_STATE rpState;
volatile uint8_t robotId;
