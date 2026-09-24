#include "MicromousePersistence.h"
#include "MicromouseMazeFrame.h"
#include "MicromouseMappingState.h"
#include "drivers/FlashIAP.h"
#include <Arduino.h>
#include <stddef.h>
#include <string.h>

const uint32_t SAVED_MAZE_MAGIC = 0x4D415553;
const uint16_t SAVED_MAZE_VERSION = 4;
const uint32_t SAVED_SETTINGS_MAGIC = 0x53455454;  // "SETT"
const uint16_t SAVED_SETTINGS_VERSION = 8;

struct SavedMaze
{
  uint32_t magic;
  uint16_t version;
  uint16_t width;
  uint16_t height;
  uint16_t reserved;
  bool compassValid;
  int startWorldHeading;
  float startCompassDegrees;
  MazeCell cells[MAZE_W][MAZE_H];
  uint32_t checksum;
};

struct SavedSettingsFlash
{
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  SavedSettings data;
  uint32_t checksum;
};

struct SavedSettingsV6
{
  int mausModusIdx;
  int solveKurvenModusIdx;
  int goalX;
  int goalY;
  int magOffsetX;
  int magOffsetY;
  int selftestEnabled;
  int selftestBeeperEnabled;
  int gameLocalId;
  int startPosX;
  int startPosY;
};

struct SavedSettingsFlashV6
{
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  SavedSettingsV6 data;
  uint32_t checksum;
};

// V7: wie aktuelle SavedSettings, jedoch ohne die Turbo-Felder.
struct SavedSettingsV7
{
  int mausModusIdx;
  int solveKurvenModusIdx;
  int goalX;
  int goalY;
  int magOffsetX;
  int magOffsetY;
  int selftestEnabled;
  int selftestBeeperEnabled;
  int gameLocalId;
  int startPosX;
  int startPosY;
  int tofWallThresholdMm;
};

struct SavedSettingsFlashV7
{
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  SavedSettingsV7 data;
  uint32_t checksum;
};

// Turbo-Defaults fuer aeltere Settings-Versionen ohne Turbo-Felder.
const int TURBO_DURATION_DEFAULT_PERSIST = 5;
const int TURBO_COOLDOWN_DEFAULT_PERSIST = 10;
const int TURBO_FACTOR_DEFAULT_PERSIST   = 150;

uint32_t checksumBytes(const uint8_t *data, size_t size)
{
  uint32_t hash = 2166136261UL;

  for (size_t i = 0; i < size; i++)
  {
    hash ^= data[i];
    hash *= 16777619UL;
  }

  return hash;
}

uint32_t savedMazeAddress(mbed::FlashIAP &flash)
{
  uint32_t flashStart = flash.get_flash_start();
  uint32_t flashSize = flash.get_flash_size();
  uint32_t flashEnd = flashStart + flashSize;
  uint32_t sectorSize = flash.get_sector_size(flashEnd - 1);
  return flashEnd - sectorSize;
}

uint32_t savedSettingsAddress(mbed::FlashIAP &flash)
{
  uint32_t flashStart = flash.get_flash_start();
  uint32_t flashSize  = flash.get_flash_size();
  uint32_t flashEnd   = flashStart + flashSize;
  uint32_t sectorSize = flash.get_sector_size(flashEnd - 1);
  uint32_t addr       = flashEnd - 3 * sectorSize;

  if (addr < flashStart)
    return flashEnd - sectorSize;

  return addr;
}

bool saveMazeToFlash()
{
  mbed::FlashIAP flash;
  if (flash.init() != 0) return false;

  uint32_t addr = savedMazeAddress(flash);
  uint32_t sectorSize = flash.get_sector_size(addr);
  uint32_t pageSize = flash.get_page_size();

  if (sizeof(SavedMaze) > sectorSize)
  {
    flash.deinit();
    return false;
  }

  SavedMaze saved;
  memset(&saved, 0, sizeof(saved));

  saved.magic = SAVED_MAZE_MAGIC;
  saved.version = SAVED_MAZE_VERSION;
  saved.width = MAZE_W;
  saved.height = MAZE_H;
  saved.compassValid = startCompassValid;
  saved.startWorldHeading = startWorldHeading;
  saved.startCompassDegrees = startCompassDegrees;
  memcpy(saved.cells, maze, sizeof(maze));
  saved.checksum = checksumBytes((const uint8_t *)&saved, offsetof(SavedMaze, checksum));

  uint32_t writeSize = ((sizeof(SavedMaze) + pageSize - 1) / pageSize) * pageSize;
  uint8_t buffer[4096];
  if (writeSize > sizeof(buffer))
  {
    flash.deinit();
    return false;
  }

  memset(buffer, flash.get_erase_value(), writeSize);
  memcpy(buffer, &saved, sizeof(saved));

  bool ok = flash.erase(addr, sectorSize) == 0;
  if (ok)
    ok = flash.program(buffer, addr, writeSize) == 0;

  flash.deinit();
  return ok;
}

bool loadMazeFromFlash()
{
  mbed::FlashIAP flash;
  if (flash.init() != 0) return false;

  uint32_t addr = savedMazeAddress(flash);
  SavedMaze saved;
  bool ok = flash.read(&saved, addr, sizeof(saved)) == 0;
  flash.deinit();

  if (!ok) return false;
  if (saved.magic != SAVED_MAZE_MAGIC) return false;
  if (saved.version != SAVED_MAZE_VERSION) return false;
  if (saved.width != MAZE_W || saved.height != MAZE_H) return false;

  uint32_t checksum = checksumBytes((const uint8_t *)&saved, offsetof(SavedMaze, checksum));
  if (checksum != saved.checksum) return false;

  memcpy(maze, saved.cells, sizeof(maze));
  mouseX = START_X;
  mouseY = START_Y;
  heading = START_HEADING;
  stackTop = -1;

  startCompassValid = saved.compassValid;
  startWorldHeading = saved.startWorldHeading;
  startCompassDegrees = saved.startCompassDegrees;
  mazeSaved = true;
  mazeSaveAttempted = true;
  mapReady = true;
  return true;
}

bool saveSettings(const SavedSettings &s)
{
  mbed::FlashIAP flash;
  if (flash.init() != 0) return false;

  uint32_t addr       = savedSettingsAddress(flash);
  uint32_t sectorSize = flash.get_sector_size(addr);
  uint32_t pageSize   = flash.get_page_size();

  if (sizeof(SavedSettingsFlash) > sectorSize) { flash.deinit(); return false; }

  SavedSettingsFlash saved;
  memset(&saved, 0, sizeof(saved));
  saved.magic    = SAVED_SETTINGS_MAGIC;
  saved.version  = SAVED_SETTINGS_VERSION;
  saved.data     = s;
  saved.checksum = checksumBytes((const uint8_t *)&saved, offsetof(SavedSettingsFlash, checksum));

  uint32_t writeSize = ((sizeof(SavedSettingsFlash) + pageSize - 1) / pageSize) * pageSize;
  uint8_t buffer[4096];
  if (writeSize > sizeof(buffer)) { flash.deinit(); return false; }

  memset(buffer, flash.get_erase_value(), writeSize);
  memcpy(buffer, &saved, sizeof(saved));

  bool ok = flash.erase(addr, sectorSize) == 0;
  if (ok) ok = flash.program(buffer, addr, writeSize) == 0;

  flash.deinit();
  return ok;
}

bool loadSettings(SavedSettings &s)
{
  mbed::FlashIAP flash;
  if (flash.init() != 0) return false;

  uint32_t addr = savedSettingsAddress(flash);
  SavedSettingsFlash saved;
  bool ok = flash.read(&saved, addr, sizeof(saved)) == 0;
  if (!ok)
  {
    flash.deinit();
    return false;
  }
  if (saved.magic != SAVED_SETTINGS_MAGIC)
  {
    flash.deinit();
    return false;
  }

  if (saved.version == 6)
  {
    SavedSettingsFlashV6 oldSaved;
    ok = flash.read(&oldSaved, addr, sizeof(oldSaved)) == 0;
    flash.deinit();
    if (!ok) return false;

    uint32_t oldChecksum =
      checksumBytes((const uint8_t *)&oldSaved, offsetof(SavedSettingsFlashV6, checksum));
    if (oldChecksum != oldSaved.checksum) return false;

    s.mausModusIdx = oldSaved.data.mausModusIdx;
    s.solveKurvenModusIdx = oldSaved.data.solveKurvenModusIdx;
    s.goalX = oldSaved.data.goalX;
    s.goalY = oldSaved.data.goalY;
    s.magOffsetX = oldSaved.data.magOffsetX;
    s.magOffsetY = oldSaved.data.magOffsetY;
    s.selftestEnabled = oldSaved.data.selftestEnabled;
    s.selftestBeeperEnabled = oldSaved.data.selftestBeeperEnabled;
    s.gameLocalId = oldSaved.data.gameLocalId;
    s.startPosX = oldSaved.data.startPosX;
    s.startPosY = oldSaved.data.startPosY;
    s.tofWallThresholdMm = 100;
    s.turboDurationS = TURBO_DURATION_DEFAULT_PERSIST;
    s.turboCooldownS = TURBO_COOLDOWN_DEFAULT_PERSIST;
    s.turboFactorPct = TURBO_FACTOR_DEFAULT_PERSIST;
    return true;
  }

  if (saved.version == 7)
  {
    SavedSettingsFlashV7 oldSaved;
    ok = flash.read(&oldSaved, addr, sizeof(oldSaved)) == 0;
    flash.deinit();
    if (!ok) return false;

    uint32_t oldChecksum =
      checksumBytes((const uint8_t *)&oldSaved, offsetof(SavedSettingsFlashV7, checksum));
    if (oldChecksum != oldSaved.checksum) return false;

    s.mausModusIdx = oldSaved.data.mausModusIdx;
    s.solveKurvenModusIdx = oldSaved.data.solveKurvenModusIdx;
    s.goalX = oldSaved.data.goalX;
    s.goalY = oldSaved.data.goalY;
    s.magOffsetX = oldSaved.data.magOffsetX;
    s.magOffsetY = oldSaved.data.magOffsetY;
    s.selftestEnabled = oldSaved.data.selftestEnabled;
    s.selftestBeeperEnabled = oldSaved.data.selftestBeeperEnabled;
    s.gameLocalId = oldSaved.data.gameLocalId;
    s.startPosX = oldSaved.data.startPosX;
    s.startPosY = oldSaved.data.startPosY;
    s.tofWallThresholdMm = oldSaved.data.tofWallThresholdMm;
    s.turboDurationS = TURBO_DURATION_DEFAULT_PERSIST;
    s.turboCooldownS = TURBO_COOLDOWN_DEFAULT_PERSIST;
    s.turboFactorPct = TURBO_FACTOR_DEFAULT_PERSIST;
    return true;
  }

  flash.deinit();
  if (saved.version != SAVED_SETTINGS_VERSION) return false;

  uint32_t checksum = checksumBytes((const uint8_t *)&saved, offsetof(SavedSettingsFlash, checksum));
  if (checksum != saved.checksum) return false;

  s = saved.data;
  return true;
}

// ============================================================
// Akku-Lebensdauer-Ergebnis
// Eigener Flash-Sektor: der freie Sektor zwischen Maze-Sektor [-1]
// und Settings-Sektor [-3], also bei flashEnd - 2*sectorSize.
// ============================================================
const uint32_t SAVED_BATTEST_MAGIC = 0x42415454;  // "BATT"
const uint16_t SAVED_BATTEST_VERSION = 1;

struct SavedBatteryTestFlash
{
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  BatteryTestResult data;
  uint32_t checksum;
};

uint32_t savedBatteryTestAddress(mbed::FlashIAP &flash)
{
  uint32_t flashStart = flash.get_flash_start();
  uint32_t flashSize  = flash.get_flash_size();
  uint32_t flashEnd   = flashStart + flashSize;
  uint32_t sectorSize = flash.get_sector_size(flashEnd - 1);
  uint32_t addr       = flashEnd - 2 * sectorSize;

  if (addr < flashStart)
    return flashEnd - sectorSize;

  return addr;
}

bool saveBatteryTestResult(const BatteryTestResult &r)
{
  mbed::FlashIAP flash;
  if (flash.init() != 0) return false;

  uint32_t addr       = savedBatteryTestAddress(flash);
  uint32_t sectorSize = flash.get_sector_size(addr);
  uint32_t pageSize   = flash.get_page_size();

  if (sizeof(SavedBatteryTestFlash) > sectorSize) { flash.deinit(); return false; }

  SavedBatteryTestFlash saved;
  memset(&saved, 0, sizeof(saved));
  saved.magic    = SAVED_BATTEST_MAGIC;
  saved.version  = SAVED_BATTEST_VERSION;
  saved.data     = r;
  saved.checksum = checksumBytes((const uint8_t *)&saved, offsetof(SavedBatteryTestFlash, checksum));

  uint32_t writeSize = ((sizeof(SavedBatteryTestFlash) + pageSize - 1) / pageSize) * pageSize;
  uint8_t buffer[4096];
  if (writeSize > sizeof(buffer)) { flash.deinit(); return false; }

  memset(buffer, flash.get_erase_value(), writeSize);
  memcpy(buffer, &saved, sizeof(saved));

  bool ok = flash.erase(addr, sectorSize) == 0;
  if (ok) ok = flash.program(buffer, addr, writeSize) == 0;

  flash.deinit();
  return ok;
}

bool loadBatteryTestResult(BatteryTestResult &r)
{
  mbed::FlashIAP flash;
  if (flash.init() != 0) return false;

  uint32_t addr = savedBatteryTestAddress(flash);
  SavedBatteryTestFlash saved;
  bool ok = flash.read(&saved, addr, sizeof(saved)) == 0;
  flash.deinit();

  if (!ok) return false;
  if (saved.magic != SAVED_BATTEST_MAGIC) return false;
  if (saved.version != SAVED_BATTEST_VERSION) return false;

  uint32_t checksum = checksumBytes((const uint8_t *)&saved, offsetof(SavedBatteryTestFlash, checksum));
  if (checksum != saved.checksum) return false;

  r = saved.data;
  return true;
}
