#include "MenuInternal.h"

// ============================================================
// DISPLAY – Menue zeichnen
// ============================================================
static const int VISIBLE_ITEMS = 3;
static const int LINE_H        = 13;
static const int LIST_Y        = 14;

void drawMenu()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  // Header
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawUTF8(2, 10, currentMenu->title);
  if (uiState == UI_EDIT)
    u8g2.drawUTF8(86, 10, "[EDIT]");
  else if (g_lowBattery && (millis() / 500) % 2 == 0)
    u8g2.drawUTF8(86, 10, "!AKKU");
  else if (currentMenu == &mainMenu)
    u8g2.drawUTF8(70, 10, mausModi[mausModusIdx].name);
  if (g_btControllerConnected) {
    static const uint8_t BT_ICON[] = {0x04, 0x0C, 0x12, 0x0C, 0x0C, 0x12, 0x0C, 0x04};
    u8g2.drawXBM(120, 2, 5, 8, BT_ICON);
  }
  u8g2.setDrawColor(1);

  // Scroll-Fenster
  int scrollOffset = 0;
  if (selected >= VISIBLE_ITEMS)
    scrollOffset = selected - VISIBLE_ITEMS + 1;

  for (int i = 0; i < VISIBLE_ITEMS; i++)
  {
    int idx = i + scrollOffset;
    if (idx >= currentMenu->size) break;

    int  y   = LIST_Y + i * LINE_H;
    bool sel = (idx == selected);

    if (sel) { u8g2.drawBox(0, y, 128, LINE_H); u8g2.setDrawColor(0); }

    u8g2.drawUTF8(4, y + 9, currentMenu->items[idx].name);

    MenuItem& it = currentMenu->items[idx];

    if (it.type == ITEM_EDIT && it.var)
    {
      char buf[8];
      snprintf(buf, sizeof(buf), "%u", (unsigned)*it.var);
      u8g2.drawUTF8(105, y + 9, buf);
    }
    if (it.type == ITEM_CYCLE && it.optIndex && it.options)
    {
      const char* opt = it.options[*it.optIndex];
      u8g2.drawUTF8(128 - (int)strlen(opt) * 6 - 2, y + 9, opt);
    }
    if (it.type == ITEM_SUBMENU)
      u8g2.drawUTF8(120, y + 9, ">");

    u8g2.setDrawColor(1);
  }

  // Scrollbar
  if (currentMenu->size > VISIBLE_ITEMS)
  {
    int totalH = VISIBLE_ITEMS * LINE_H;
    int barH   = max(3, totalH * VISIBLE_ITEMS / currentMenu->size);
    int barY   = LIST_Y + (scrollOffset * totalH) / currentMenu->size;
    u8g2.drawBox(126, barY, 2, barH);
  }

  // Untere Hinweiszeile
  u8g2.drawLine(0, 53, 128, 53);
  if (uiState == UI_NAVIGATE)
    u8g2.drawUTF8(2, 63, "A:zurück B:v C:ok");
  if (uiState == UI_EDIT && activeItem)
    u8g2.drawUTF8(2, 63, "A:ok  B:--  C:++");

  oledSendBuffer();
}

bool menuGetSelftestBeeperEnabled() { return selftestBeeperEnabled != 0; }

// solveKurvenModusIdx: 0 = Kurve, 1 = Punkt.
bool menuGetUseCurves() { return solveKurvenModusIdx == 0; }

// ============================================================
// Hilfsfunktion: Zeichnet zwei Textzeilen zentriert
// ============================================================
static void drawTwoLines(const char* line1, const char* line2)
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawUTF8(2, 10, line1);
  u8g2.setDrawColor(1);

  if (line2 && line2[0] != '\0')
    u8g2.drawUTF8(2, 28, line2);

  oledSendBuffer();
}

// ============================================================
// Statusmeldungen fuer MicromouseMapping.cpp
// ============================================================
void menuShowTemporaryMessage(const char *line1, const char *line2)
{
  drawTwoLines(line1, line2 ? line2 : "");
  delay(700);
}

void menuShowMapStartPrompt()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawUTF8(2, 10, "MAP START");
  u8g2.setDrawColor(1);

  u8g2.drawUTF8(2, 26, "Place robot on");
  u8g2.drawUTF8(2, 38, "start cross");
  u8g2.drawUTF8(2, 50, "and keep still");

  oledSendBuffer();
}

void menuShowMapStartHeading(bool compassCaptured, const char *headingName)
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawUTF8(2, 10, "MAP START");
  u8g2.setDrawColor(1);

  u8g2.drawUTF8(2, 26, "Place robot on");
  u8g2.drawUTF8(2, 38, "start cross");

  u8g2.drawUTF8(2, 52, compassCaptured ? "World: " : "World: REL");
  if (compassCaptured && headingName)
    u8g2.drawUTF8(46, 52, headingName);

  oledSendBuffer();
}
