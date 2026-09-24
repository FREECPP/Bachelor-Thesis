#include "Turbo.h"

#include "../MicromouseMapping.h"   // g_lineSpeedFactor
#include "../MicromouseLeds.h"
#include "../UART_L3.h"
#include <Pololu3piPlus2040Buzzer.h>

extern UART_L3 uartL3;
using namespace Pololu3piPlus2040;

// ============================================================
// Persistente Einstellungen (Defaults; werden beim Boot aus dem Flash
// ueberschrieben falls gespeicherte Werte gueltig sind).
// ============================================================
uint8_t turboDurationS = TURBO_DURATION_DEFAULT;
uint8_t turboCooldownS = TURBO_COOLDOWN_DEFAULT;
uint8_t turboFactorPct = TURBO_FACTOR_DEFAULT;

// ============================================================
// Interner Zustand
// ============================================================
namespace
{
    enum TurboState { TS_IDLE, TS_ACTIVE, TS_COOLDOWN };

    TurboState s_state         = TS_IDLE;
    uint32_t   s_phaseStartMs  = 0;   // Beginn von ACTIVE bzw. COOLDOWN
    uint32_t   s_rejectFlashEnd = 0;  // Ende des roten Reject-Blitzes (0 = inaktiv)

    Buzzer s_buzzer;

    const LedColor TURBO_PULSE_COLOR = LED_YELLOW;
    const LedColor TURBO_NORMAL_COLOR = LED_YELLOW;  // == PAC_LED_YELLOW-Idee
    constexpr uint32_t PULSE_PERIOD_MS = 130;        // schneller, aggressiver Puls
    constexpr uint32_t REJECT_FLASH_MS = 160;

    // Mario-"Stern"/Unverwundbarkeits-Thema, exakt aus dem MIDI extrahiert
    // (Lead-/Square-Spur, eine Loop-Einheit, +1 Oktave fuer klaren Piezo-Klang).
    // Laeuft in turboUpdate() bis zum Boost-Ende in Schleife.
    const char* TURBO_STAR_MELODY =
        "!T150 MS o5c8 c8 c8 o4d16 o5c8 c8 o4d16 o5c16 o4d16 o5c8 "
        "o4b8 b8 b8 c16 b8 b8 c16 b16 c16 b8";

    void applyFactor(float f)
    {
        g_lineSpeedFactor = f;
    }

    void startActive(uint32_t now)
    {
        s_state        = TS_ACTIVE;
        s_phaseStartMs = now;
        applyFactor((float)turboFactorPct / 100.0f);
        // Aktivierungs-Feedback: kraeftiges Rumble + Stern-Melodie starten
        // (wird in turboUpdate() bis zum Boost-Ende in Schleife gehalten).
        uartL3.sendControllerRumble(100, 255, 255);
        s_buzzer.play(TURBO_STAR_MELODY);
    }

    void endActiveToCooldown(uint32_t now)
    {
        applyFactor(1.0f);
        s_state        = TS_COOLDOWN;
        s_phaseStartMs = now;
        // Stern-Melodie stoppen.
        s_buzzer.stopPlaying();
        // LEDs zurueck auf normales Gelb.
        ledSetAll(TURBO_NORMAL_COLOR);
        ledFlushToHardware();
    }

    void doReject(uint32_t now)
    {
        // Anderes Rumble-Muster (kurz, nur schwacher Motor) + roter Blitz + Buzz.
        uartL3.sendControllerRumble(60, 255, 0);
        s_rejectFlashEnd = now + REJECT_FLASH_MS;
        ledSetAll(LED_RED);
        ledFlushToHardware();
        s_buzzer.play("!T180 o3 l16 g r16 g");
    }

    // Schneller Dreieck-Puls zwischen gedimmtem und vollem Turbo-Farbton.
    void pulseLeds(uint32_t now)
    {
        const uint32_t phase = (now - s_phaseStartMs) % PULSE_PERIOD_MS;
        const uint32_t half  = PULSE_PERIOD_MS / 2;
        uint32_t tri = (phase < half) ? phase : (PULSE_PERIOD_MS - phase);  // 0..half
        // Aggressiver Puls: nahezu aus (~3%) bis voll (100%).
        uint16_t bright = 8 + (uint16_t)((uint32_t)247 * tri / half);
        LedColor c = {
            (uint8_t)((uint16_t)TURBO_PULSE_COLOR.r * bright / 255),
            (uint8_t)((uint16_t)TURBO_PULSE_COLOR.g * bright / 255),
            (uint8_t)((uint16_t)TURBO_PULSE_COLOR.b * bright / 255),
        };
        ledSetAll(c);
        ledFlushToHardware();
    }
}

// ============================================================
// Oeffentliche API
// ============================================================
void turboReset()
{
    s_state          = TS_IDLE;
    s_phaseStartMs   = 0;
    s_rejectFlashEnd = 0;
    applyFactor(1.0f);
}

void turboCancel()
{
    // Aktiven Boost und Cooldown verwerfen -> sofort wieder einsatzbereit.
    turboReset();
}

void turboHandleButtonEdge(bool xRisingEdge)
{
    if (!xRisingEdge)
        return;

    const uint32_t now = millis();
    if (s_state == TS_IDLE)
        startActive(now);
    else
        doReject(now);   // ACTIVE oder COOLDOWN -> abgelehnt
}

void turboUpdate()
{
    const uint32_t now = millis();

    switch (s_state)
    {
        case TS_ACTIVE:
            if ((uint32_t)(now - s_phaseStartMs) >= (uint32_t)turboDurationS * 1000UL)
                endActiveToCooldown(now);
            else
            {
                if (now >= s_rejectFlashEnd)   // Reject-Flash hat Vorrang
                    pulseLeds(now);
                // Stern-Melodie ohne Unterbrechung loopen (z.B. nachdem ein
                // Muenz-Sound sie kurz uebertoent hat).
                if (!s_buzzer.isPlaying())
                    s_buzzer.play(TURBO_STAR_MELODY);
            }
            break;

        case TS_COOLDOWN:
            if ((uint32_t)(now - s_phaseStartMs) >= (uint32_t)turboCooldownS * 1000UL)
                s_state = TS_IDLE;
            break;

        case TS_IDLE:
            break;
    }
}

bool turboIsActive()
{
    return s_state == TS_ACTIVE;
}

float turboRemainingFraction()
{
    if (s_state != TS_ACTIVE || turboDurationS == 0)
        return 0.0f;
    const uint32_t elapsed = millis() - s_phaseStartMs;
    const uint32_t total   = (uint32_t)turboDurationS * 1000UL;
    if (elapsed >= total)
        return 0.0f;
    return 1.0f - (float)elapsed / (float)total;
}

bool turboOwnsLeds()
{
    return s_state == TS_ACTIVE || millis() < s_rejectFlashEnd;
}
