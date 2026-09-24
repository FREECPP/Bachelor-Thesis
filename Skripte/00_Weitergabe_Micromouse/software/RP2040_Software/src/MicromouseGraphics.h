#ifndef MICROMOUSE_GRAPHICS_H
#define MICROMOUSE_GRAPHICS_H

#include <Arduino.h>
#include <U8g2lib.h>

// Pixel-Buffer (128x64, 1 Byte pro 8 vertikale Pixel)
extern uint8_t graphics[1024];

// Zeiger auf das gemeinsame U8G2-Display-Objekt (in MicromouseMenu.cpp definiert)
extern U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI u8g2;

void clearGraphics();
void setPixel(uint8_t x, uint8_t y, bool value = true);
void drawHLine(uint8_t x1, uint8_t x2, uint8_t y);
void drawVLine(uint8_t x, uint8_t y1, uint8_t y2);
void drawSolidRectangle(uint8_t topLeftX, uint8_t topLeftY, uint8_t width, uint8_t height);

// Schreibt den Graphics-Buffer in den U8G2-Buffer und ruft sendBuffer() auf.
// Kann auch zusammen mit u8g2-Text-Elementen verwendet werden:
//   clearGraphics(); drawHLine(...); flushGraphicsToU8G2();
void flushGraphicsToU8G2();

// Thread-sicherer sendBuffer()-Wrapper: setzt g_oledBusy waehrend der
// Uebertragung, damit Core 1 ButtonC (GP0) in diesem Fenster nicht abtastet.
extern volatile bool g_oledBusy;
void oledSendBuffer();

// =============================================================
// WORKAROUND: GP0 ist auf dem Pololu 3pi+ 2040 GLEICHZEITIG
//   - OLED D/C-Signal (von U8G2 getrieben)
//   - Eingang fuer ButtonC
//
// Pololus ButtonC::isPressed() schaltet GP0 auf INPUT_PULLUP und
// liest nach nur 1 µs. Wenn U8G2 die Leitung als OUTPUT LOW
// zuruecklaesst, schafft der schwache interne Pull-up es in 1 µs
// nicht, die Leitung gegen die parasitaere Kapazitaet hochzuziehen
// → Button C wird faelschlich als "gedrueckt" gelesen → Mapping/
// Solving triggert sporadisch den C-Abort-Pfad → Motoren zittern.
//
// Diese Funktion setzt GP0 deshalb nach jeder U8G2-Operation
// explizit auf OUTPUT HIGH. So ist die Leitung bereits HIGH, wenn
// ButtonC::isPressed() sie 1 µs spaeter samplen will.
// MUSS nach jedem u8g2.sendBuffer() (und am Ende von Display-
// Bloecken) aufgerufen werden.
// =============================================================
void releaseOledDcForButtonC();

#endif
