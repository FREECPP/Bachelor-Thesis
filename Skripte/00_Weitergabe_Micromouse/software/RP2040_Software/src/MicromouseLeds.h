#pragma once
#include <Arduino.h>

#define LED_COUNT 6

struct LedColor { uint8_t r, g, b; };

constexpr LedColor LED_OFF    = {  0,   0,   0};
constexpr LedColor LED_RED    = {255,   0,   0};
constexpr LedColor LED_GREEN  = {  0, 255,   0};
constexpr LedColor LED_BLUE   = {  0,   0, 255};
constexpr LedColor LED_YELLOW = {255, 200,   0};
constexpr LedColor LED_CYAN   = {  0, 255, 255};
constexpr LedColor LED_WHITE  = {255, 255, 255};
constexpr LedColor LED_ORANGE = {255,  80,   0};

// Threadsafe: Core 1 darf schreiben, Core 0 liest + überträgt.
void ledSet(uint8_t index, LedColor color);
void ledSetAll(LedColor color);
void ledClear();
void ledSetBumperIndicators(bool leftPressed, bool rightPressed);

void ledShowMapping();   // alle blau   – Mapping läuft
void ledShowSolving();   // alle grün   – Solving läuft
void ledShowDone();      // alle weiß   – fertig
void ledShowError();     // alle rot    – Fehler
void ledShowIdle();      // alle aus    – Menü / Idle
void flashColor(LedColor flashColor, unsigned long duration_ms);
void ledUpdate();

// NUR Core 0 aufrufen, NACH dem letzten sendBuffer() des Zyklus.
// Überträgt den Wunsch-State an die Hardware (SPI0/GP3/GP6).
void ledFlushToHardware();

// Schaltet SharedSPI intern auf Display-Modus (GP2=SCK, 4 MHz).
// Muss vor jedem u8g2.sendBuffer() aufgerufen werden, damit nach einem
// LED-Flush der SharedSPI-State wieder konsistent ist.
void ledSwitchSpiToDisplay();
