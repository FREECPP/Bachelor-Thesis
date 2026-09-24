# Graph Report - RP2040_Software  (2026-06-11)

## Corpus Check
- 64 files · ~45,642 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 785 nodes · 1772 edges · 36 communities (33 shown, 3 thin omitted)
- Extraction: 69% EXTRACTED · 31% INFERRED · 0% AMBIGUOUS · INFERRED: 556 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `9db83daf`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- [[_COMMUNITY_Community 0|Community 0]]
- [[_COMMUNITY_Community 1|Community 1]]
- [[_COMMUNITY_Community 2|Community 2]]
- [[_COMMUNITY_Community 3|Community 3]]
- [[_COMMUNITY_Community 4|Community 4]]
- [[_COMMUNITY_Community 5|Community 5]]
- [[_COMMUNITY_Community 6|Community 6]]
- [[_COMMUNITY_Community 8|Community 8]]
- [[_COMMUNITY_Community 9|Community 9]]
- [[_COMMUNITY_Community 10|Community 10]]
- [[_COMMUNITY_Community 11|Community 11]]
- [[_COMMUNITY_Community 12|Community 12]]
- [[_COMMUNITY_Community 13|Community 13]]
- [[_COMMUNITY_Community 15|Community 15]]
- [[_COMMUNITY_Community 16|Community 16]]
- [[_COMMUNITY_Community 17|Community 17]]
- [[_COMMUNITY_Community 18|Community 18]]
- [[_COMMUNITY_Community 19|Community 19]]
- [[_COMMUNITY_Community 20|Community 20]]
- [[_COMMUNITY_Community 21|Community 21]]
- [[_COMMUNITY_Community 22|Community 22]]
- [[_COMMUNITY_Community 23|Community 23]]
- [[_COMMUNITY_Community 24|Community 24]]
- [[_COMMUNITY_Community 25|Community 25]]
- [[_COMMUNITY_Community 26|Community 26]]
- [[_COMMUNITY_Community 27|Community 27]]
- [[_COMMUNITY_Community 28|Community 28]]
- [[_COMMUNITY_Community 29|Community 29]]
- [[_COMMUNITY_Community 30|Community 30]]
- [[_COMMUNITY_Community 32|Community 32]]
- [[_COMMUNITY_Community 33|Community 33]]
- [[_COMMUNITY_Community 34|Community 34]]
- [[_COMMUNITY_Community 39|Community 39]]
- [[_COMMUNITY_Community 42|Community 42]]
- [[_COMMUNITY_Community 44|Community 44]]

## God Nodes (most connected - your core abstractions)
1. `oledSendBuffer()` - 44 edges
2. `inBounds()` - 28 edges
3. `solvingLoop()` - 27 edges
4. `testBatteryLife()` - 26 edges
5. `stopMotors()` - 25 edges
6. `ghostGameLoop()` - 25 edges
7. `ledSetAll()` - 23 edges
8. `ledFlushToHardware()` - 21 edges
9. `startSolvingInternal()` - 20 edges
10. `drawPacmanHUD()` - 19 edges

## Surprising Connections (you probably didn't know these)
- `menuAppLoop()` --calls--> `solveIsRandomMode()`  [INFERRED]
  src/menu/MenuAppLoop.cpp → src/MicromouseSolving.cpp
- `testBatteryLife()` --calls--> `solveIsAtError()`  [INFERRED]
  src/MicromouseHardwareTest.cpp → src/MicromouseSolving.cpp
- `enterGhostError()` --calls--> `solveLastError()`  [INFERRED]
  src/game/GhostGame.cpp → src/MicromouseSolving.cpp
- `selftest()` --calls--> `menuGetSelftestBeeperEnabled()`  [INFERRED]
  src/MicromouseHardwareTest.cpp → src/menu/MenuRender.cpp
- `doKarteSendenWireless()` --calls--> `mazeCopyCommFrameBytes()`  [INFERRED]
  src/menu/MenuActions.cpp → src/MicromouseMazeFrame.cpp

## Communities (36 total, 3 thin omitted)

### Community 0 - "Community 0"
Cohesion: 0.09
Nodes (61): startPacmanGame(), buildAndShowMazeFrame(), doBtConnect(), doEditMagOffsetX(), doEditMagOffsetY(), doKarteAnzeigen(), doKarteSendenSerial(), doKarteSendenWireless() (+53 more)

### Community 1 - "Community 1"
Cohesion: 0.05
Nodes (88): knownPassage(), beginDriveOneCell(), canMove(), requestCenterStopForQueuedTurn(), loop(), ledUpdate(), exportGraphToSerial(), exportMazeToSerial() (+80 more)

### Community 2 - "Community 2"
Cohesion: 0.09
Nodes (54): enterCaptureSequence(), gameGetPressedA(), gameGetPressedC(), gameLeave(), gameLoop(), leaveCaptureSequence(), chooseDirectionAtNode(), consumeQuitRequest() (+46 more)

### Community 3 - "Community 3"
Cohesion: 0.06
Nodes (73): gameBackgroundUpdate(), gameCbControlReceived(), gameCbPosReceived(), gameReportCapture(), getRoleFromId(), commitReservation(), enterGhostError(), ghostCanReplan() (+65 more)

### Community 4 - "Community 4"
Cohesion: 0.06
Nodes (13): drawCaptureScreen(), gameActive(), gameGetCtrlSwitches(), gameGetPressedB(), gameRoleColor(), playGameStartSound(), setup(), mappingRegisterBackgroundUpdate() (+5 more)

### Community 5 - "Community 5"
Cohesion: 0.09
Nodes (52): lineSeenByCenterSensors(), mappingGetHeading(), mappingGetStatus(), setLineSearchDir(), solveButtonAPressed(), solveButtonCPressed(), solveDisplayClear(), solveDisplayClearGraphics() (+44 more)

### Community 6 - "Community 6"
Cohesion: 0.05
Nodes (36): 1. Wechsel vom Menu ins Spiel, 1. Zielwahl, 2. Aktivierung des Game-State, 2. Routenplanung, 3. Aktive und vorgemerkte Route, 3. Laufender Spielbetrieb, 4. Verlassen des Game-State, 4. Zellenweise Ausfuehrung (+28 more)

### Community 8 - "Community 8"
Cohesion: 0.06
Nodes (33): 1.1 Komplett unbenutzte Symbole, 1.2 Nutzlose Indirektion, 1. Toter Code (sofort entfernbar), 2.1 Identische Strukturen in zwei Dateien, 2.2 Identische Funktionen in Mapping und Solving, 2.3 FNV-1a und Flash-Adressierung doppelt, 2.4 Doppelte Direction-Konstanten, 2.5 Doppelte `goalX`/`goalY` (+25 more)

### Community 9 - "Community 9"
Cohesion: 0.10
Nodes (3): begin(), sendControllerRumble(), update()

### Community 10 - "Community 10"
Cohesion: 0.09
Nodes (21): Aktuelle Zeitparameter, Aktueller Implementierungsstand, Ausgetauschte Zustandsdaten, Bekannte Grenze, code:text (Positions-Heartbeat:          400 ms), code:cpp (uint8_t localId = robotId & 0x1F;), code:text (kleinere lokale ID gewinnt), code:text (GC_GHOST_RESERVE_CELL = 8) (+13 more)

### Community 11 - "Community 11"
Cohesion: 0.08
Nodes (30): setOwnIdRole(), menuAppActive(), menuAppSetup(), applyCommRxLedSignal(), menuCbButtonPressed(), onGameControlReceived(), onMazeDataReceived(), onSubscribedReceived() (+22 more)

### Community 12 - "Community 12"
Cohesion: 0.10
Nodes (19): Anzeige und LEDs, code:cpp (enum RP_STATE {), code:cpp (lineFollowFastBegin(centerAndStop);), Datenfluss grob, `deleted/`, Eingaben, Einstiegspunkt, `game/` (+11 more)

### Community 13 - "Community 13"
Cohesion: 0.12
Nodes (17): code:cpp (#pragma once), code:cpp (bool inBounds(int x, int y);  // benötigt MAZE_W, MAZE_H), code:cpp (uint32_t flashChecksumFNV1a(const uint8_t *data, size_t size), code:bash (pio run -e pico_normal && pio run -e pico_hyper), Phase 2: Konsolidierung (Risiko: niedrig bis mittel), Schritt 2.10 – `comm()` und `g_uartL3` entfernen, Schritt 2.11 – `goalX/Y` Duplikation auflösen, Schritt 2.12 – Build-Test, dann commit (+9 more)

### Community 15 - "Community 15"
Cohesion: 0.14
Nodes (14): code:bash (pio run -e pico_normal && pio run -e pico_hyper), Phase 1: Toten Code entfernen (Risiko: sehr niedrig), Schritt 1.10 – Tote `UL3CB_*`-Callbacks in main.cpp aufräumen, Schritt 1.11 – Auskommentierten Code aufräumen, Schritt 1.12 – Build-Test, Schritt 1.1 – `src/main.txt` löschen, Schritt 1.2 – UART-RX-Ringpuffer entfernen, Schritt 1.3 – Unbenutzte Button-Flags entfernen (+6 more)

### Community 16 - "Community 16"
Cohesion: 0.14
Nodes (14): EncoderTurnState, active, lastProgress, lastProgressMs, lineSearchDir, settleUntilMs, settling, startedMs (+6 more)

### Community 17 - "Community 17"
Cohesion: 0.15
Nodes (13): SavedSettingsV7, gameLocalId, goalX, goalY, magOffsetX, magOffsetY, mausModusIdx, selftestBeeperEnabled (+5 more)

### Community 18 - "Community 18"
Cohesion: 0.24
Nodes (6): isPressed(), update(), validButtonId(), wasHeld(), wasPressed(), wasReleased()

### Community 19 - "Community 19"
Cohesion: 0.17
Nodes (12): SavedSettingsV6, gameLocalId, goalX, goalY, magOffsetX, magOffsetY, mausModusIdx, selftestBeeperEnabled (+4 more)

### Community 20 - "Community 20"
Cohesion: 0.18
Nodes (11): SavedMaze, cells, checksum, compassValid, height, magic, reserved, startCompassDegrees (+3 more)

### Community 21 - "Community 21"
Cohesion: 0.22
Nodes (9): Build-Verifikation, code:bash (pio run -e pico_normal), code:block14 (Bitte nach diesem Refactoring auf Hardware testen:), code:bash (find src -name "*.cpp" -o -name "*.h" | xargs wc -l | sort -), code:bash (graphify update .), Graphify-Update, Smoke-Test-Plan für Hardware (vom User auszuführen), Snapshot-Verifikation (+1 more)

### Community 22 - "Community 22"
Cohesion: 0.22
Nodes (9): code:cpp (enum class GameMode { NONE, DEMO_RANDOM, HUNT };), Phase 3: Umbenennungen (Risiko: niedrig), Schritt 3.1 – Wand-Kollisions-API umbenennen (nicht solve-spezifisch), Schritt 3.2 – Battery, Schritt 3.3 – `pollButtonFifo` umbenennen / konsolidieren, Schritt 3.4 – `MicromouseCore1` umbenennen, Schritt 3.5 – Pacman-Namen, Schritt 3.6 – Verschmelzung `g_inGameMode` + `g_huntActive` (+1 more)

### Community 23 - "Community 23"
Cohesion: 0.40
Nodes (10): checksumBytes(), loadBatteryTestResult(), loadMazeFromFlash(), loadSettings(), saveBatteryTestResult(), savedBatteryTestAddress(), savedMazeAddress(), savedSettingsAddress() (+2 more)

### Community 24 - "Community 24"
Cohesion: 0.33
Nodes (6): code:block11 (RAM:   [== ]  16.X% (used X bytes from 270336 bytes)), Phase 4: Datei-Aufspaltung (Risiko: mittel), Schritt 4.1 – `MicromouseMenu.cpp` aufteilen, Schritt 4.2 – `MicromouseMapping.cpp` aufteilen, Schritt 4.3 – Verzeichnis-Struktur erstellen, Schritt 4.4 – Build-Test

### Community 25 - "Community 25"
Cohesion: 0.33
Nodes (6): code:cpp (static void showNoMapMessage(const char* contextTitle) {), Phase 5: Polish (Risiko: niedrig), Schritt 5.1 – Magic Numbers in benannte Konstanten, Schritt 5.2 – String-Helper, Schritt 5.3 – Doku auslagern, Schritt 5.4 – Sprachen vereinheitlichen

### Community 26 - "Community 26"
Cohesion: 0.21
Nodes (15): drawReceivedMaze(), drawTwoLines(), menuGetSelftestBeeperEnabled(), menuShowMapStartHeading(), menuShowMapStartPrompt(), menuShowTemporaryMessage(), clearGraphics(), drawHLine() (+7 more)

### Community 27 - "Community 27"
Cohesion: 0.33
Nodes (6): SavedSettingsFlash, checksum, data, magic, reserved, version

### Community 28 - "Community 28"
Cohesion: 0.33
Nodes (6): SavedSettingsFlashV6, checksum, data, magic, reserved, version

### Community 29 - "Community 29"
Cohesion: 0.33
Nodes (6): SavedSettingsFlashV7, checksum, data, magic, reserved, version

### Community 30 - "Community 30"
Cohesion: 0.33
Nodes (6): SavedBatteryTestFlash, checksum, data, magic, reserved, version

### Community 39 - "Community 39"
Cohesion: 0.30
Nodes (13): canProjectFrom(), chooseBestNearbyTarget(), chooseClosestReachableTarget(), directionFromDelta(), ghostTargetsChooseTarget(), ghostTargetsObservePacman(), ghostTargetsReset(), manhattan() (+5 more)

### Community 42 - "Community 42"
Cohesion: 0.33
Nodes (5): Allgemeine Regeln für die KI, Code Review – RP2040_Software, Geschätzter Aufwand, Inhaltsverzeichnis, Teil 2 – KI-Refactoring-Instruktionen

### Community 44 - "Community 44"
Cohesion: 0.40
Nodes (5): ObservedGhost, id, known, x, y

## Knowledge Gaps
- **220 isolated node(s):** `recommendations`, `unwantedRecommendations`, `fromX`, `fromY`, `toX` (+215 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **3 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `oledSendBuffer()` connect `Community 0` to `Community 2`, `Community 3`, `Community 4`, `Community 5`, `Community 11`, `Community 26`?**
  _High betweenness centrality (0.039) - this node is a cross-community bridge._
- **Why does `EncoderTurnState` connect `Community 16` to `Community 1`?**
  _High betweenness centrality (0.025) - this node is a cross-community bridge._
- **Are the 41 inferred relationships involving `oledSendBuffer()` (e.g. with `testBattery()` and `battLifeError()`) actually correct?**
  _`oledSendBuffer()` has 41 INFERRED edges - model-reasoned connections that need verification._
- **Are the 23 inferred relationships involving `inBounds()` (e.g. with `exportGraphToSerial()` and `buildGraphFromMaze()`) actually correct?**
  _`inBounds()` has 23 INFERRED edges - model-reasoned connections that need verification._
- **Are the 13 inferred relationships involving `solvingLoop()` (e.g. with `solveButtonAPressed()` and `turnAbort()`) actually correct?**
  _`solvingLoop()` has 13 INFERRED edges - model-reasoned connections that need verification._
- **Are the 20 inferred relationships involving `testBatteryLife()` (e.g. with `menuPollInputs()` and `menuGetPressedA()`) actually correct?**
  _`testBatteryLife()` has 20 INFERRED edges - model-reasoned connections that need verification._
- **Are the 15 inferred relationships involving `stopMotors()` (e.g. with `testBatteryLife()` and `showCaughtScreen()`) actually correct?**
  _`stopMotors()` has 15 INFERRED edges - model-reasoned connections that need verification._