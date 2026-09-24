#include "MenuInternal.h"

// ============================================================
// KOLLISIONS-RECOVERY-UI
// ============================================================

static const char* headingShortName(int h)
{
  switch (((h % 4) + 4) % 4)
  {
    case 0: return "N";
    case 1: return "E";
    case 2: return "S";
    case 3: return "W";
    default: return "?";
  }
}

// Zeichnet den Hauptscreen (oder den Edit-Screen, je nach editField).
//   editField = -1 → Hauptscreen mit A/B/C-Belegung
//   editField =  0 → Edit-Mode, Cursor auf X
//   editField =  1 → Edit-Mode, Cursor auf Y
//   editField =  2 → Edit-Mode, Cursor auf Heading
static void drawCollisionScreen(int x, int y, int heading, int editField)
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawStr(2, 10, editField < 0 ? "KOLLISION!" : "POSE EDIT");
  u8g2.setDrawColor(1);

  // Pose-Zeile mit optionalem Cursor
  char buf[24];
  snprintf(buf, sizeof(buf), "X:%d  Y:%d  %s",
           x, y, headingShortName(heading));
  u8g2.drawStr(2, 26, buf);

  if (editField >= 0)
  {
    // Cursor-Marker unter dem editierten Feld.
    // Spalten: "X:" ab Pixel 2; "Y:" startet nach "X:%d  " (10-14 px je nach X)
    // Pragmatisch: feste Cursor-Positionen statt exakter Breite.
    static const int colX[3] = {2, 46, 92};
    u8g2.drawStr(colX[editField], 36, "^");
  }

  // Unteres Drittel: Aktionen, je nach Modus.
  u8g2.drawLine(0, 41, 128, 41);
  if (editField < 0)
  {
    u8g2.drawStr(2, 51, "A:weiter   B:edit");
    u8g2.drawStr(2, 62, "C:Reset (0,0,E)");
  }
  else
  {
    u8g2.drawStr(2, 51, "A:naechstes Feld");
    u8g2.drawStr(2, 62, "B:+   C:-");
  }

  oledSendBuffer();
}

CollisionAction collisionRecoveryUI(int &x, int &y, int &heading)
{
  // Edit-Modus: Cursor wandert durch X → Y → Heading → zurueck.
  int editField = -1;  // -1 = Hauptscreen
  drawCollisionScreen(x, y, heading, editField);

  while (true)
  {
    buttonAdvanced.update();
    // pollButtonFifo();

    if (editField < 0)
    {
      // Hauptscreen
      if (menuGetPressedA())
      {
        // Weiter mit (evtl. editierter) Pose
        return COLLISION_CONTINUE;
      }
      if (menuGetPressedB())
      {
        editField = 0;
        drawCollisionScreen(x, y, heading, editField);
      }
      else if (menuGetPressedC())
      {
        // Hardcoded-Reset auf Startposition
        x = 0;
        y = 0;
        heading = 1;  // EAST
        return COLLISION_RESET;
      }
    }
    else
    {
      // Edit-Modus
      if (menuGetPressedA())
      {
        // Zum naechsten Feld – oder zurueck zum Hauptscreen.
        editField++;
        if (editField > 2) editField = -1;
        drawCollisionScreen(x, y, heading, editField);
      }
      else if (menuGetPressedB())
      {
        // + 1 mit Wrap-around auf gueltigen Bereich
        if (editField == 0)      x = (x + 1) % MAZE_W;
        else if (editField == 1) y = (y + 1) % MAZE_H;
        else                     heading = (heading + 1) % 4;
        drawCollisionScreen(x, y, heading, editField);
      }
      else if (menuGetPressedC())
      {
        // - 1 mit Wrap-around
        if (editField == 0)      x = (x + MAZE_W - 1) % MAZE_W;
        else if (editField == 1) y = (y + MAZE_H - 1) % MAZE_H;
        else                     heading = (heading + 3) % 4;
        drawCollisionScreen(x, y, heading, editField);
      }
    }

    delay(20);
  }
}

void mappingShowCollisionStop(int x, int y, int heading)
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawStr(2, 10, "KOLLISION!");
  u8g2.setDrawColor(1);

  char buf[24];
  snprintf(buf, sizeof(buf), "X:%d  Y:%d  %s",
           x, y, headingShortName(heading));
  u8g2.drawStr(2, 26, buf);

  u8g2.drawStr(2, 40, "Mapping abgebr.");
  u8g2.drawStr(2, 52, "Karte bleibt im RAM");
  u8g2.drawLine(0, 54, 128, 54);
  u8g2.drawStr(2, 63, "C = Menue");

  oledSendBuffer();
}
