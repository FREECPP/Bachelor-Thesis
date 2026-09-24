#include "MicromouseGraphics.h"
#include "MicromouseLeds.h"
#include <string.h>
#include <hardware/gpio.h>

uint8_t graphics[1024];
volatile bool g_oledBusy = false;

void clearGraphics()
{
  memset(graphics, 0, sizeof(graphics));
}

void setPixel(uint8_t x, uint8_t y, bool value)
{
  if (x >= 128 || y >= 64) return;

  if (value)
    graphics[x + (y >> 3) * 128] |= 1 << (y & 7);
  else
    graphics[x + (y >> 3) * 128] &= ~(1 << (y & 7));
}

void drawHLine(uint8_t x1, uint8_t x2, uint8_t y)
{
  if (x2 < x1) { uint8_t t = x1; x1 = x2; x2 = t; }
  for (uint8_t x = x1; x <= x2; x++) setPixel(x, y, true);
}

void drawVLine(uint8_t x, uint8_t y1, uint8_t y2)
{
  if (y2 < y1) { uint8_t t = y1; y1 = y2; y2 = t; }
  for (uint8_t y = y1; y <= y2; y++) setPixel(x, y, true);
}

void drawSolidRectangle(uint8_t topLeftX, uint8_t topLeftY, uint8_t width, uint8_t height)
{
  if (topLeftX >= 128 || topLeftY >= 64) return;
  if (width  > 128 - topLeftX) width  = 128 - topLeftX;
  if (height > 64  - topLeftY) height = 64  - topLeftY;

  for (uint8_t y = topLeftY; y < topLeftY + height; y++)
    for (uint8_t x = topLeftX; x < topLeftX + width; x++)
      setPixel(x, y, true);
}

// Kopiert den 128×64-Pixelbuffer byteweise in den U8G2-RAM und sendet ihn.
// Das U8G2-Tile-Format ist identisch zu unserem Buffer-Layout:
//   Byte-Index = col + (row >> 3) * 128, Bit = row & 7
// → direktes Überschreiben über u8g2.getBufferPtr() ist möglich.
void flushGraphicsToU8G2()
{
  // U8G2 Full-Frame-Buffer: getBufferPtr() liefert den internen Puffer.
  // Größe: 128 * 64 / 8 = 1024 Byte – identisch zu graphics[].
  uint8_t* buf = u8g2.getBufferPtr();
  if (buf)
    memcpy(buf, graphics, 1024);

  oledSendBuffer();
}

void oledSendBuffer()
{
  ledSwitchSpiToDisplay();  // SharedSPI auf Display-Modus: GP2=SCK, m_configuredForDisplay=true
  g_oledBusy = true;
  u8g2.sendBuffer();
  g_oledBusy = false;
  // mbed::SPI kann GP16/18/19 bei jedem write()-Aufruf erneut auf GPIO_FUNC_SPI setzen
  // (_acquire()-Mechanismus oder Erst-Initialisierung beim ersten Transfer).
  // PIO-Liniensensoren brauchen GPIO_FUNC_PIO0 – sofort nach jedem sendBuffer() wiederherstellen.
  gpio_set_function(16, GPIO_FUNC_PIO0);
  gpio_set_function(17, GPIO_FUNC_PIO0);
  gpio_set_function(18, GPIO_FUNC_PIO0);
  gpio_set_function(19, GPIO_FUNC_PIO0);
  // Motor-Pins ebenfalls schuetzen: GP14/15 sind PWM-Slice-Outputs der
  // mbed::PwmOut-Instanzen in Pololu3piPlus2040Motors (rightPWM=p14,
  // leftPWM=p15). GP10/11 sind die Direction-Pins (SIO). Falls die
  // mbed::SPI-Hardware-Init diese Pin-Funktionen versehentlich anfasst,
  // ohne diese Wiederherstellung wuerden die Motoren waehrend des
  // Mappings (drawLiveMap pro Zelle) blockieren, obwohl sie im Test-
  // Menue noch laufen.
  gpio_set_function(14, GPIO_FUNC_PWM);   // rightPWM
  gpio_set_function(15, GPIO_FUNC_PWM);   // leftPWM
  gpio_set_function(10, GPIO_FUNC_SIO);   // rightDirectionPin
  gpio_set_function(11, GPIO_FUNC_SIO);   // leftDirectionPin
  releaseOledDcForButtonC();
}

// Setzt GP0 nach U8G2-Operationen auf OUTPUT HIGH, damit
// ButtonC::isPressed() die Leitung beim 1-µs-Sample sofort HIGH
// findet (kein langsames Pull-up-Charging gegen parasitaere C).
// Siehe ausfuehrliche Erklaerung im Header.
void releaseOledDcForButtonC()
{
  pinMode(0, OUTPUT);
  digitalWrite(0, HIGH);
}
