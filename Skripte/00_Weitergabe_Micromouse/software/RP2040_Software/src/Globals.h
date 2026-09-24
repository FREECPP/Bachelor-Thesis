#pragma once
#include <Arduino.h>

// ── OLED-Busy-Flag (Main-Thread setzt während sendBuffer) ───────────────────
// Button-Thread überspringt c1BtnC.isPressed() (GP0!) während g_oledBusy,
// um Konflikte mit dem OLED D/C-Signal zu vermeiden.
extern volatile bool g_oledBusy;

// ── Abort-Flag: Button C gedrückt (persistent, wird von Mapping-Code gelöscht) ─
extern volatile bool g_abortRequested;

// ── Edge-triggered Button-Flags (Button-Thread setzt, pollButtonFifo löscht) ─
// TODO: Move to static in MicromouseMenu
extern volatile bool g_btnAPressed;  // ButtonA – auch für Solve-Modus
extern volatile bool g_btnBPressed;  // ButtonB
extern volatile bool g_btnCPressed;  // ButtonC – für Menü-Navigation

extern volatile uint8_t startPosX;
extern volatile uint8_t startPosY;
extern volatile uint8_t currentPosX;
extern volatile uint8_t currentPosY;
extern volatile uint8_t lastPosX;
extern volatile uint8_t lastPosY;


enum RP_STATE {
    RP_STATE_MENU,
    RP_STATE_GAME
};

extern volatile RP_STATE rpState;
extern volatile uint8_t robotId;
