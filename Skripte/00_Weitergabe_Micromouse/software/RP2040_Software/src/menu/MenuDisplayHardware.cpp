#include "MenuInternal.h"

// ============================================================
// DISPLAY – Hardware SPI0
// Die Pololu-Board-Definition mappt den Default-SPI auf GP2/GP3.
//
// Pin-Belegung Pololu 3pi+ 2040:
//   GP3 = SPI0 TX  (MOSI/Data)  – automatisch
//   GP2 = SPI0 SCK (Clock)      – automatisch
//   GP1 = Reset
//   GP0 = DC (geteilt mit Button C!)
//   CS  = GND → U8X8_PIN_NONE
// ============================================================
U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI u8g2(
  U8G2_R0,
  /* cs=    */ U8X8_PIN_NONE,
  /* dc=    */ 0,
  /* reset= */ 1
);
