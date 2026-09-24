# Code Review – RP2040_Software

**Analyse-Datum:** 2026-05-25
**Codebase-Umfang:** 12 `.cpp`-Dateien + 12 `.h`-Dateien, ca. **6.800 Zeilen Code**
**Dominierende Dateien:** `MicromouseMenu.cpp` (1703), `MicromouseMapping.cpp` (1458), `MicromouseSolving.cpp` (1151) – zusammen ~65 % des Gesamtcodes.

> Diese Datei dokumentiert eine systematische Code-Review. Sie ist in zwei Teile gegliedert: zuerst die **Befunde** (was problematisch ist), dann die **KI-Refactoring-Instruktionen** (wie ein zukünftiger Claude-Code-Lauf das schrittweise und sicher verbessern kann).

---

## Inhaltsverzeichnis

- [Teil 1 – Befunde](#teil-1--befunde)
  - [1. Toter Code (sofort entfernbar)](#1-toter-code-sofort-entfernbar)
  - [2. Echte Code-Duplikate](#2-echte-code-duplikate)
  - [3. Suboptimaler Code](#3-suboptimaler-code)
  - [4. Datei-Organisation](#4-datei-organisation)
  - [5. Umbenennungs-Vorschläge](#5-umbenennungs-vorschläge)
  - [6. Was ist gut?](#6-was-ist-gut)
- [Teil 2 – KI-Refactoring-Instruktionen](#teil-2--ki-refactoring-instruktionen)
  - [Allgemeine Regeln für die KI](#allgemeine-regeln-für-die-ki)
  - [Phase 1: Toten Code entfernen](#phase-1-toten-code-entfernen-risiko-sehr-niedrig)
  - [Phase 2: Konsolidierung](#phase-2-konsolidierung-risiko-niedrig-bis-mittel)
  - [Phase 3: Umbenennungen](#phase-3-umbenennungen-risiko-niedrig)
  - [Phase 4: Datei-Aufspaltung](#phase-4-datei-aufspaltung-risiko-mittel)
  - [Phase 5: Polish](#phase-5-polish-risiko-niedrig)
  - [Verifikations-Strategie](#verifikations-strategie)

---

# Teil 1 – Befunde

## 1. Toter Code (sofort entfernbar)

### 1.1 Komplett unbenutzte Symbole

| Symbol | Datei:Zeile | Status |
|---|---|---|
| `g_uartRxBuf[128]`, `g_uartRxHead`, `g_uartRxTail` | `MicromouseCore1.h:23-25` | UART-RX-Ringpuffer – nirgends benutzt |
| `uartRxRead()` | `MicromouseCore1.cpp:16` | Nie aufgerufen |
| `g_btnBPressed`, `g_btnCPressed` | `MicromouseCore1.h:18-19` | Deklariert, nie gelesen |
| `core1Start()` | `MicromouseCore1.cpp:25` | Leer (`{ }`) – Kommentar in `main.cpp` lügt ("RTOS-Thread") |
| `menuSetupDisplay()` | `MicromouseMenu.cpp:1100` | Leerer No-Op |
| `menuWaitReleaseA/B/C()` | `MicromouseMenu.cpp:1092-1094` | Nur `delay(30)` – wartet nicht auf Release |
| `solveWaitReleaseA/C()` | `MicromouseSolving.cpp:273-274` | Gibt sofort zurück (Flag schon gelöscht) – Funktionsname lügt |
| `g_uartL3Storage[]` | `MicromouseMenu.cpp:124` | `alignas`-Storage, nie beschrieben |
| `static uint8_t cursorX, cursorY` | `MicromouseMapping.cpp:1347` | Zugewiesen, nie gelesen |
| `RobotState state` + Enum | `MicromouseMapping.cpp:142-152` | Gesetzt, aber nirgends gelesen |
| `COLLISION_ABORT` | `MicromouseMenu.h:61` | Eigenkommentar: "aktuell nicht angeboten" |
| `serialEvent()` | `MicromouseMapping.cpp:1272` | Bei JEDEM Serial-Input wird Maze exportiert – Debug-Relikt |
| `UL3CB_receiveMazeData/PosData/BtControllerData` | `main.cpp:28-55` | Registrierung auskommentiert ("Done in Menu") |
| `main.txt` | `src/main.txt` | Legacy-Prototyp-Datei aus der Vor-Refactoring-Zeit |

### 1.2 Nutzlose Indirektion

`g_uartL3` (`MicromouseMenu.cpp:123`) ist nur `&uartL3` (`extern UART_L3 uartL3;` aus `main.cpp`).
Die `comm()`-Funktion (`MicromouseMenu.cpp:131`) ist ein dritter Wrapper darüber.
**Drei Indirektions-Ebenen für nichts** – `uartL3.foo()` würde reichen.

---

## 2. Echte Code-Duplikate

### 2.1 Identische Strukturen in zwei Dateien

**`Cell` vs `SolveCell`** – Felder komplett identisch:
- `MicromouseMappingState.h:17-25` (Cell)
- `MicromouseSolving.cpp:70-78` (SolveCell)

Der Kommentar in Solving sagt selbst: *"WICHTIG: Diese Strukturen muessen byte-identisch ... sein!"* – genau das Symptom für gefährliche Duplikation.

**`SavedMaze` vs `SolveSavedMaze`** – gleiches Problem:
- `MicromousePersistence.cpp:16-28`
- `MicromouseSolving.cpp:86-98`

Auch Magic & Version sind doppelt:
- `SAVED_MAZE_MAGIC=0x4D415553` / `SOLVE_SAVED_MAGIC=0x4D415553`
- `SAVED_MAZE_VERSION=3` / `SOLVE_SAVED_VERSION=3`

### 2.2 Identische Funktionen in Mapping und Solving

| In `MicromouseMapping.cpp` | Identisches Gegenstück in `MicromouseSolving.cpp` |
|---|---|
| `leftOf(d)`, `rightOf(d)`, `oppositeOf(d)` | gleiche `static inline` Versionen |
| `inBounds(x,y)` | `solveInBounds(x,y)` – byte-identisch |
| `turnLeft90/Right90/Around180()` | `solveTurnLeft90/Right90/Around180()` |
| `TURN_SPEED = PLATFORM_PWM(55)`, `TURN_90_TIME = PLATFORM_TIME_MS(430)` | `SOLVE_TURN_SPEED = PLATFORM_PWM(55)`, `SOLVE_TURN_90_TIME = PLATFORM_TIME_MS(430)` |

### 2.3 FNV-1a und Flash-Adressierung doppelt

- `checksumBytes()` in `MicromousePersistence.cpp:49`
- `solveChecksum()` in `MicromouseSolving.cpp:283`
- `savedMazeAddress()` in `MicromousePersistence.cpp:62`
- `solveSavedAddress()` in `MicromouseSolving.cpp:296`

Jeweils identische Implementierung.

### 2.4 Doppelte Direction-Konstanten

- `MicromouseMappingState.h:10-13` definiert `const int NORTH=0; EAST=1; SOUTH=2; WEST=3;`
- `MicromouseSolving.cpp:14-17` definiert das nochmal als `#define` mit Kommentar *"damit Solving keinen Mapping-Header einbinden muss"*

Das ist ein **Design-Fehler**. Ein gemeinsamer `MicromouseDirections.h` löst das Problem.

### 2.5 Doppelte `goalX`/`goalY`

- `MicromouseMenu.cpp:339-340`: `static int goalX = 0, goalY = 0;`
- `MicromouseSolving.cpp:159-160`: `static int goalX = 0, goalY = 0;`

Zwei unterschiedliche Variablen, die dieselbe Sache modellieren – fehleranfällig.

### 2.6 Doppelte Graph-Konstruktion

- `buildGraphFromMaze()` in `MicromouseMapExport.cpp:167`
- `solveBuildGraph()` in `MicromouseSolving.cpp:344`

Beide bauen einen ungerichteten Graphen aus den Maze-Daten – gleiche Logik.

### 2.7 GPIO-Pin-Restore-Block

Identische 8 Zeilen in:
- `MicromouseMenu.cpp:1334-1352`
- `MicromouseGraphics.cpp:71-86`

Sollte eine Helper-Funktion `restorePinFunctions()` sein.

### 2.8 Display-Header-Muster

`u8g2.clearBuffer()` kommt **28×** vor, `u8g2.drawBox(0, 0, 128, 12)` (Header-Balken) **17×**. Das Muster:
```cpp
u8g2.clearBuffer();
u8g2.setFont(u8g2_font_6x10_tf);
u8g2.drawBox(0, 0, 128, 12);
u8g2.setDrawColor(0);
u8g2.drawUTF8(2, 10, "TITEL");
u8g2.setDrawColor(1);
```
…wird Dutzende Male buchstabengleich wiederholt. Die `drawHeader()`/`drawFooter()`-Helper in `MicromouseHardwareTest.cpp:30-44` existieren schon – sind aber `static` und nicht wiederverwendbar.

---

## 3. Suboptimaler Code

### 3.1 Naming-Lügen (Funktionsname ≠ Verhalten)

| Symbol | Problem |
|---|---|
| `menuWaitReleaseA/B/C()` | Macht nur `delay(30)`, wartet nicht auf Release |
| `solveWaitReleaseA/C()` | Gibt sofort zurück (Flag wurde beim Lesen gelöscht) |
| `core1Start()` | Leer, Kommentar behauptet "RTOS-Thread starten" |
| `MicromouseCore1.h/cpp` | Dateiname suggeriert Core-1-Code – existiert nicht mehr |
| `setSolveWallCheck()` / `solveWallHitDetected()` / `markSolveWallHitTriggered()` | Werden auch von **Mapping** benutzt (`MicromouseMapping.cpp:1406`) – "solve"-Präfix falsch |
| `mappingReadBatteryMv()` | Hat mit Mapping nichts zu tun, nur Wrapper für `readBatteryMillivolts()` |
| `pollButtonFifo()` | Liest keine FIFO mehr (Thread weg), pollt direkt |

### 3.2 Schlechte Trennung der Verantwortlichkeiten

**`MicromouseMenu.cpp` (1703 Zeilen) enthält:**
- OLED-Display-Objekt-Definition
- Button-Polling
- Menü-Definitionen + Navigation
- Aktionshandler (`doKartierung`, `doSolvingZiel`, etc.)
- Hunt-/Pacman-Modus-Logik
- Empfangenes-Maze-Rendering inkl. Nibble-Decodierung
- Kollisions-Recovery-UI
- Kompass-Offset-Editor
- App-Setup + App-Loop

**`MicromouseMapping.cpp` (1458 Zeilen) enthält:**
- Globale Hardware-Instanzen (`motors`, `lineSensors`, `tof`, `imu`)
- Kompass/Magnetometer-Auswertung
- Linien-PID + Intersection-Detection
- Wand-Kollisions-Check (auch von Solving genutzt!)
- DFS-Algorithmus
- `solveDisplay*`-Wrapper (gehören eher zum Menu)
- Battery-Reader-Wrapper

### 3.3 `solveDisplay*`-Wrapper sind Anti-Pattern

`MicromouseMapping.cpp:1335-1373`: 10 Wrapper-Funktionen, die eine alte Pololu-OLED-API nachbauen, die nicht mehr verwendet wird. Sie liegen in **Mapping**, werden aber nur vom **Solving** per `extern` aufgerufen. Beispiel:
```cpp
void solveDisplayPrint(const char* s)  { u8g2.setFont(u8g2_font_6x10_tf); u8g2.print(s); }
```
Solving sollte entweder direkt U8G2 nutzen oder eine eigene saubere Display-Bridge bekommen.

### 3.4 Solving lädt sein eigenes Maze separat

`MicromouseSolving.cpp:106`: `static SolveCell solveMaze[MAZE_W][MAZE_H];` – eine **zweite Kopie** der Maze-Daten, geladen via Flash. Dabei liegt dieselbe Karte schon im RAM (`Cell maze[][]` von Mapping). Das verdoppelt den RAM-Verbrauch (~3 KB) und erzwingt Flash-Read selbst direkt nach dem Mapping.

### 3.5 Magic Numbers überall

- `delay(700)`, `delay(120)`, `delay(50)`, `delay(300)`, `delay(800)`, `delay(1000)` ohne benannte Konstanten
- RGB-Triplets `{51, 40, 0}`, `{51, 0, 0}` etc. in `MicromouseMenu.cpp:321-327`
- `MAX_CELL_DRIVE_TIME = 5000` – Begründung fehlt
- TOF-Sample-Anzahl, Schwellen, alle ohne Erklärung

### 3.6 String-Duplikate

`"Keine Karte!"` + `"Zuerst kartieren."` erscheint in vier `do*`-Aktionen wortgleich. Ein Helper `showNoMapMessage()` würde es einsparen.

### 3.7 O(V·E)-Dijkstra ohne Priority-Queue

`MicromouseSolving.cpp:391`: Pro Schritt wird die komplette Edge-Liste linear durchsucht (`Zeile 430`) – effektiv O(V·E) statt O((V+E)·log V). Für 50 Knoten unkritisch, aber unsauber.

### 3.8 Auskommentierter Code

- `MicromouseMenu.cpp:498`: `{"Info", ...}` Test-Submenu-Eintrag auskommentiert (Info ist im Hauptmenü)
- `MicromouseMenu.cpp:1324`: `// Serial.begin(115200); // Nix da, sowas kommt in die Main!!!!!!!!!!!!!!!!!!!!`

### 3.9 `MicromousePlatform.h` mischt Doku und Code

135 Zeilen davon sind ~110 Zeilen Kommentar. Inhaltlich gut, aber die Erklärung könnte in eine `docs/PLATFORM.md` ausgelagert werden – die Header-Datei selbst sollte 30 Zeilen kurz sein.

### 3.10 Inkonsistente Sprachen

Kommentare wechseln zwischen Deutsch (`"Zustandsmaschine"`, `"Fahr-Phase"`) und Englisch (`"Wait"`, `"Goal reached"`). UI-Strings sind teils Deutsch (`"Lösen"`), teils Englisch (`"SOLVE MODE"`, `"GOAL REACHED!"`). Konsequente Wahl wäre sauberer.

---

## 4. Datei-Organisation

### Aktuell problematisch

```
MicromouseMenu.cpp        ← 1703 Zeilen, 7+ Verantwortlichkeiten
MicromouseMapping.cpp     ← 1458 Zeilen, 6+ Verantwortlichkeiten
MicromouseSolving.cpp     ← 1151 Zeilen, OK aber viele Duplikate
MicromouseCore1.cpp       ← 25 Zeilen, mit veralteten Namen
```

### Vorschlag

```
src/
├── core/
│   ├── MicromouseDirections.h   ← NORTH/EAST/.., dxDir, leftOf, ...
│   ├── MicromouseFlash.h/cpp    ← FNV-Hash, Flash-Adressen, Sektoren
│   └── MicromouseTypes.h        ← gemeinsame Cell, SavedMaze, MagicNumbers
├── hardware/
│   ├── MicromouseHardware.h/cpp ← motors, lineSensors, tof, imu, battery
│   ├── MicromouseCompass.h/cpp
│   ├── MicromouseLineFollow.h/cpp  ← PID, intersectionSeen, wall check
│   └── MicromouseLeds.h/cpp
├── algo/
│   ├── MicromouseDFS.h/cpp      ← Mapping-Algorithmus
│   ├── MicromouseDijkstra.h/cpp ← Path-Planung
│   └── MicromouseGraph.h/cpp    ← einheitliches buildGraphFromMaze
├── ui/
│   ├── MicromouseGraphics.h/cpp
│   ├── MicromouseMenuFramework.h/cpp ← Menu/MenuItem Typen, drawMenu
│   ├── MicromouseMenuActions.cpp     ← doKartierung, doSolving, etc.
│   ├── MicromouseCollisionUI.h/cpp
│   ├── MicromouseHunt.h/cpp          ← Jagdmodus
│   ├── MicromouseRxMazeView.h/cpp
│   └── MicromouseHardwareTest.h/cpp
├── comm/
│   ├── MicromousePersistence.h/cpp
│   ├── MicromouseMazeFrame.h/cpp
│   └── UART_L3.h/cpp
├── platform/
│   └── MicromousePlatform.h
└── main.cpp
```

---

## 5. Umbenennungs-Vorschläge

| Vorher | Vorschlag | Grund |
|---|---|---|
| `MicromouseCore1.h/cpp` | `MicromouseSharedFlags.h/cpp` (oder löschen) | Kein Core-1-Code mehr |
| `core1Start()` | komplett löschen | Leer |
| `pollButtonFifo()` | konsolidieren mit `menuPollButtons()` | Doppelt, keine FIFO mehr |
| `setSolveWallCheck()` | `setWallCheck()` | Nicht solve-spezifisch |
| `solveWallHitDetected()` | `wallHitDetected()` | s.o. |
| `markSolveWallHitTriggered()` | `markWallHit()` | s.o. |
| `solveDisplay*` | `oledPrint*` und in Graphics verschieben | Display, nicht Solve |
| `mappingReadBatteryMv()` | `readBatteryMv()`, in Hardware-Modul | Hat mit Mapping nichts zu tun |
| `menuWaitReleaseA/B/C()` | löschen / korrekt implementieren | Lügt über Verhalten |
| `solveWaitReleaseA/C()` | löschen | Macht effektiv nichts |
| `g_uartL3` + `comm()` | direkt `uartL3` aus main.cpp | Sinnlose Indirektion |
| `mausModusIdx` | `pacmanGhostIdx` | Klarer |
| `mausModi[]` | `pacmanGhosts[]` | s.o. |
| `RobotState`, `state` | komplett löschen | Tot |
| `editIntFast()` | `editLargeRange()` | Was macht's "fast"? |
| `doKartierung`, `doSpielDemo`, … | konsistent englisch oder deutsch | Sprach-Mischmasch |
| `g_inGameMode` vs `g_huntActive` | ein gemeinsames `g_gameMode`-Enum | Zwei Variablen, ein Konzept |

---

## 6. Was ist gut?

Damit es nicht zu negativ klingt – diese Dinge sind solide:

- **`MicromousePlatform.h`**: PLATFORM_PWM / PLATFORM_TIME_MS Skalierung mit ausführlicher Begründung dokumentiert.
- **Kommentar-Qualität** ist bei kritischen Workarounds (OLED D/C vs. ButtonC, SPI-Pin-Restore, Klothoide-Kreis-Klothoide) **sehr gut** und MUSS beim Refactoring erhalten bleiben.
- **`MicromouseMazeFrame.cpp`** ist klein, fokussiert, klar.
- **`MicromousePersistence.cpp`** trennt die drei Flash-Sektoren (Maze / RxMaze / Settings) sauber.
- **`MicromouseLeds.cpp`** ist minimal und thread-safe entworfen.
- **`MicromouseGraphics.cpp`** Pixel-Buffer mit U8G2-Integration ist clever (Buffer-Layout-Kompatibilität ausgenutzt).
- **Bug-Fix-Kommentare** (Cogging, GP0/D/C, Pin-Funktionen-Restore) sind extrem wertvoll.

---

# Teil 2 – KI-Refactoring-Instruktionen

> **Dieser Teil ist als Prompt für eine zukünftige Claude-Code-Session konzipiert.** Wenn du eine KI bitten möchtest, das Projekt schrittweise zu verbessern, kannst du sie auf dieses Dokument verweisen und sagen: *"Arbeite die Phasen aus CODE_REVIEW.md ab, eine nach der anderen, und frag vor jeder neuen Phase nach Freigabe."*

## Allgemeine Regeln für die KI

1. **Niemals mehrere Phasen in einem Commit zusammenwerfen.** Jede Phase = mindestens ein eigener Commit (besser: pro nummeriertem Schritt ein Commit).
2. **Nach jedem Schritt kompilieren:** `pio run -e pico_normal` UND `pio run -e pico_hyper`. Beide Targets müssen grün bleiben.
3. **Verhalten darf sich nicht ändern.** Dies ist ein **reines Refactoring**, keine Funktionsänderung. Wenn du auf einen Bug stößt, melde ihn separat und repariere ihn nicht im selben Commit.
4. **Bestehende Kommentare nicht wegrationalisieren.** Die Bug-Fix-Kommentare (OLED-D/C-Workaround, GPIO-Pin-Restore, Cogging-Begründung, Platform-Skalierung) sind hart erarbeitetes Wissen. Sie MÜSSEN beim Verschieben von Code mitwandern.
5. **Bei jedem `grep`-Treffer fragen: "Wer nutzt das?"** Bevor etwas gelöscht wird, in **allen** `.cpp`/`.h`-Dateien suchen (nicht nur in der gerade bearbeiteten).
6. **Hardware-Tests sind blind.** Niemand kann den realen Roboter aus der KI-Session heraus testen. Daher: maximal konservativ refactorisieren, lieber zweimal nachdenken als einmal falsch verschieben.
7. **`graphify update .` nach jeder größeren Phase laufen lassen**, damit der Knowledge-Graph aktuell bleibt (siehe `CLAUDE.md`).
8. **Sprache:** Code-Kommentare auf Deutsch beibehalten (matched die bestehende Konvention im Großteil der Codebase). Neue UI-Strings auf Deutsch.

## Phase 1: Toten Code entfernen (Risiko: sehr niedrig)

**Vor jedem Löschen:** `grep -rn "<symbolname>" src/` ausführen und sicherstellen, dass nur die zu löschende Definition und ihre Forward-Deklaration auftauchen.

### Schritt 1.1 – `src/main.txt` löschen
- Komplette Legacy-Datei aus Vor-Refactoring-Zeit. Keine Referenzen aus dem Build-System.

### Schritt 1.2 – UART-RX-Ringpuffer entfernen
Betroffen: `MicromouseCore1.h`, `MicromouseCore1.cpp`
- `g_uartRxBuf[]`, `g_uartRxHead`, `g_uartRxTail`, `UART_RX_BUF_SIZE`, `uartRxRead()`
- **Verifikation:** `grep -rn "uartRx\|UART_RX_BUF" src/` muss leer sein.

### Schritt 1.3 – Unbenutzte Button-Flags entfernen
- `g_btnBPressed`, `g_btnCPressed` aus `MicromouseCore1.h/cpp`
- **Vorsicht:** `g_btnAPressed` und `g_abortRequested` BLEIBEN, die werden genutzt.

### Schritt 1.4 – Leere/lügende Funktionen entfernen
- `core1Start()` aus `MicromouseCore1.h/cpp` UND Aufruf in `main.cpp:75`
- `menuSetupDisplay()` aus `MicromouseMenu.h/cpp` UND Aufruf in `MicromouseMapping.cpp:1297`
- `menuWaitReleaseA/B/C()` aus `MicromouseMenu.h/cpp` (in Mapping nicht verwendet)
- `solveWaitReleaseA/C()` aus `MicromouseSolving.cpp` UND alle Aufrufstellen (`solveButtonAPressed()` löscht das Flag schon)

### Schritt 1.5 – `RobotState` Enum + `state`-Variable entfernen
- `MicromouseMapping.cpp:142-152` (Enum) und `:152` (`state = MAIN_MENU;`)
- Zuweisungen in `:1291` (`state = MAP_RUNNING;`) und `:1440` (`state = MAPPING_MENU;`)

### Schritt 1.6 – `g_uartL3Storage` entfernen
- `MicromouseMenu.cpp:124`: `alignas(UART_L3) static uint8_t g_uartL3Storage[...]`

### Schritt 1.7 – Dead-Statics in `solveDisplayGotoXY` entfernen
- `MicromouseMapping.cpp:1347`: `static uint8_t cursorX = 0, cursorY = 0;` (zugewiesen, nie gelesen)

### Schritt 1.8 – `COLLISION_ABORT` entfernen
- `MicromouseMenu.h:61` (nicht verwendet, Eigenkommentar bestätigt)

### Schritt 1.9 – `serialEvent()` entfernen
- `MicromouseMapping.cpp:1272-1276`. Diese Arduino-Callback ist ein Debug-Relikt: jedes Serial-Byte löst Maze-Export aus.
- **Hinweis:** Falls der User die Funktionalität noch braucht, sollte sie als sauberes Kommando `MAZE\n` aus dem Menü heraus erreichbar gemacht werden – nicht implizit per `serialEvent()`.

### Schritt 1.10 – Tote `UL3CB_*`-Callbacks in main.cpp aufräumen
- `main.cpp:28-55`: `UL3CB_receiveMazeData`, `UL3CB_receivePosData`, `UL3CB_receiveBtControllerData` sind definiert, ihre `register*`-Aufrufe sind auskommentiert.
- Entweder Funktionen löschen ODER Registrierung aktivieren (Empfehlung: löschen, denn Menu registriert eigene Callbacks).

### Schritt 1.11 – Auskommentierten Code aufräumen
- `MicromouseMenu.cpp:498`: `// {"Info", ...}` (verbliebener Test-Submenu-Eintrag)
- `MicromouseMenu.cpp:1324`: `// Serial.begin(115200); // Nix da, ...`
- `MicromouseMenu.cpp:680`: `// TODO: Change TargetId or leave it in broadcast` – entweder fixen oder löschen

### Schritt 1.12 – Build-Test
```bash
pio run -e pico_normal && pio run -e pico_hyper
```
**Beide grün → Phase 1 commit.**

---

## Phase 2: Konsolidierung (Risiko: niedrig bis mittel)

Jeder Schritt = eigener Commit. Nach jedem Schritt vollständig kompilieren.

### Schritt 2.1 – `MicromouseDirections.h` anlegen

Neue Header-Datei:
```cpp
#pragma once
#include <Arduino.h>

constexpr int NORTH = 0;
constexpr int EAST  = 1;
constexpr int SOUTH = 2;
constexpr int WEST  = 3;

constexpr int leftOf(int d)     { return (d + 3) & 3; }
constexpr int rightOf(int d)    { return (d + 1) & 3; }
constexpr int oppositeOf(int d) { return (d + 2) & 3; }

constexpr int dxDir(int d) { return d == EAST ? 1 : (d == WEST ? -1 : 0); }
constexpr int dyDir(int d) { return d == NORTH ? 1 : (d == SOUTH ? -1 : 0); }
```

Dann:
- Aus `MicromouseMappingState.h` die `NORTH/EAST/SOUTH/WEST` Konstanten und die freistehenden Direction-Helpers entfernen, durch `#include "MicromouseDirections.h"` ersetzen.
- Aus `MicromouseSolving.cpp` die `#define NORTH 0` etc. entfernen, `#include "MicromouseDirections.h"`.
- `MicromouseMapping.cpp::leftOf/rightOf/oppositeOf/dxDir/dyDir` entfernen.
- `MicromouseSolving.cpp::leftOf/rightOf/oppositeOf` (static inline) entfernen.

**Verifikation:** `grep -rn "leftOf\|rightOf\|oppositeOf\|dxDir\|dyDir" src/` darf nur Aufrufstellen finden, keine Definitionen mehr.

### Schritt 2.2 – `inBounds` konsolidieren

- In `MicromouseDirections.h` (oder einem `MicromouseGrid.h`) einmalig deklarieren:
  ```cpp
  bool inBounds(int x, int y);  // benötigt MAZE_W, MAZE_H
  ```
- Implementierung in `MicromouseMapping.cpp` belassen oder in ein neues `MicromouseGrid.cpp` ziehen.
- `MicromouseSolving.cpp::solveInBounds` löschen, durch `inBounds()` ersetzen.

### Schritt 2.3 – `MicromouseFlash.h/cpp` anlegen

Zwei Funktionen zentralisieren:
```cpp
uint32_t flashChecksumFNV1a(const uint8_t *data, size_t size);
uint32_t flashSectorAddress(mbed::FlashIAP &flash, int sectorsFromEnd);
```

Dann:
- `MicromousePersistence.cpp::checksumBytes` und `savedMazeAddress` / `savedRxMazeAddress` / `savedSettingsAddress` darauf umstellen.
- `MicromouseSolving.cpp::solveChecksum` und `solveSavedAddress` löschen, durch die zentralen Versionen ersetzen.

### Schritt 2.4 – `Cell` und `SolveCell` vereinigen

- `Cell` wird die einzige Struktur.
- `SolveCell` in `MicromouseSolving.cpp` löschen.
- Alle Vorkommen von `SolveCell` in `MicromouseSolving.cpp` durch `Cell` ersetzen.
- `MicromouseSolving.cpp` muss dann `#include "MicromouseMappingState.h"` (oder einen extrahierten Cell-Header) tun.

**Sicherheitscheck:** `sizeof(Cell)` darf sich nicht ändern (Flash-Format!). Vor und nach dem Schritt `Serial.println(sizeof(Cell))` in `setup()` einbauen und im Monitor verifizieren. Sonst gibt's Flash-Magic-Mismatch.

### Schritt 2.5 – `SavedMaze` und `SolveSavedMaze` vereinigen

- Vereinigte Struktur in `MicromousePersistence.h` (oder eigenem `MicromouseFlashFormat.h`).
- `SOLVE_SAVED_MAGIC` und `SOLVE_SAVED_VERSION` löschen, durch `SAVED_MAZE_MAGIC` / `SAVED_MAZE_VERSION` ersetzen.
- Solving's `solveLoadMazeFromFlash` nutzt die Persistence-API (`loadMazeFromFlash(target)` oder ähnlich).

**Vorsicht:** Vor diesem Schritt unbedingt sicherstellen, dass `Cell == SolveCell` (Schritt 2.4) schon abgeschlossen ist.

### Schritt 2.6 – Solving sollte Mapping's `maze[][]` mit-benutzen

Statt eigener `solveMaze[][]`-Kopie: Solving liest direkt aus `extern Cell maze[MAZE_W][MAZE_H]`.

**Warum trotzdem ggf. Flash-Reload nötig:** Wenn der Boot ohne Mapping-Lauf erfolgt, ist `maze[][]` leer. Daher: `solvingResetToStart()` ruft `loadMazeFromFlash()` (das schreibt in `maze[][]`), statt eine separate Kopie zu pflegen.

**Sparpotential:** ~3 KB RAM (signifikant auf RP2040!).

### Schritt 2.7 – Graph-Konstruktion vereinheitlichen

- `MicromouseMapExport.cpp::buildGraphFromMaze` UND `MicromouseSolving.cpp::solveBuildGraph` rufen denselben Algorithmus auf, jeweils mit anderer Edge-Ausgabe.
- Variante a) gemeinsame interne Funktion `iterateMazeEdges(callback)`.
- Variante b) ein einziges `buildGraph(Graph& out)` in `MicromouseGraph.cpp`, Solving nutzt es.

### Schritt 2.8 – GPIO-Pin-Restore-Helper

- Identischer Block in `MicromouseMenu.cpp:1334-1352` und `MicromouseGraphics.cpp:71-86`.
- Helper `restoreNonSpiPinFunctions()` in `MicromouseGraphics.cpp`.
- **WICHTIG:** Die ausführlichen Kommentare (warum diese Funktion existiert, was passiert wenn SPI die Pins überschreibt) MÜSSEN über den Helper wandern – sie sind hart erarbeitetes Bug-Wissen.

### Schritt 2.9 – `drawHeader/Footer` publik machen

- `MicromouseHardwareTest.cpp:30-44` hat `static void drawHeader(const char*)` und `drawFooter(const char*)`.
- In `MicromouseGraphics.h` als öffentliche API exportieren.
- Aufrufer in `MicromouseMenu.cpp` (28× `clearBuffer`, 17× `drawBox` Header-Pattern) nach und nach umstellen.

### Schritt 2.10 – `comm()` und `g_uartL3` entfernen

- `MicromouseMenu.cpp:123, 131-134`: `g_uartL3`, `comm()` löschen.
- `extern UART_L3 uartL3;` aus `main.cpp` direkt nutzen (Top of `MicromouseMenu.cpp`).
- Alle `comm()->foo()` → `uartL3.foo()`.
- `if (comm())` Checks entfallen (Referenz ist immer gültig).

### Schritt 2.11 – `goalX/Y` Duplikation auflösen

- In `MicromouseMenu.cpp` bleibt `goalX/Y` als Editor-Variable.
- In `MicromouseSolving.cpp` bleibt das `goalX/Y` ALS RUNTIME-STATE (vom Dijkstra gesetzt).
- ABER: Solving's `goalX/Y` wird nur intern verwendet. Statt mit dem Menü-`goalX` zu verwechseln, umbenennen: Solving's Variablen → `currentGoalX/Y`.

### Schritt 2.12 – Build-Test, dann commit
```bash
pio run -e pico_normal && pio run -e pico_hyper
```

---

## Phase 3: Umbenennungen (Risiko: niedrig)

Reine Renames – kein Logik-Change. Pro Symbol ein Commit.

### Schritt 3.1 – Wand-Kollisions-API umbenennen (nicht solve-spezifisch)
- `setSolveWallCheck` → `setWallCheck`
- `solveWallHitDetected` → `wallHitDetected`
- `markSolveWallHitTriggered` → `markWallHit`
- `g_solveWallCheck*` → `g_wallCheck*`
- `SOLVE_WALL_POLL_INTERVAL_MS` → `WALL_POLL_INTERVAL_MS`

### Schritt 3.2 – Battery
- `mappingReadBatteryMv()` → `readBatteryMv()`, raus aus Mapping, rein in ein neues `MicromouseBattery.h/cpp` (oder direkt nach `MicromouseHardware.cpp`).

### Schritt 3.3 – `pollButtonFifo` umbenennen / konsolidieren
- `pollButtonFifo()` und `menuPollButtons()` sind fast identisch. Eine Variante belassen, andere löschen.
- Empfehlung: `pollButtons()` bleibt, `menuPollButtons()` löschen (oder umgekehrt – Hauptsache eine Funktion).
- Im Header sichtbar machen (heute halb static, halb extern).

### Schritt 3.4 – `MicromouseCore1` umbenennen
- Datei → `MicromouseSharedFlags.h/cpp`
- Inhalt nach den Löschungen aus Phase 1.2/1.3/1.4 ist nur noch `g_abortRequested`, `g_btnAPressed`, `g_oledBusy` – passt zum neuen Namen.

### Schritt 3.5 – Pacman-Namen
- `mausModusIdx` → `pacmanGhostIdx`
- `mausModi[]` → `pacmanGhosts[]`
- `MausModusInfo` → `PacmanGhostInfo`
- `optModus[]` → `pacmanGhostNames[]`
- `MODUS_COUNT` → `PACMAN_GHOST_COUNT`

### Schritt 3.6 – Verschmelzung `g_inGameMode` + `g_huntActive`
```cpp
enum class GameMode { NONE, DEMO_RANDOM, HUNT };
static GameMode g_gameMode = GameMode::NONE;
```
- Alle Tests `g_inGameMode || g_huntActive` werden zu `g_gameMode != GameMode::NONE`.
- Alle Zuweisungen werden explizit.

### Schritt 3.7 – `editIntFast` umbenennen
- → `editLargeRangeInteger` (oder `editInt100Step`, je nach Geschmack).

---

## Phase 4: Datei-Aufspaltung (Risiko: mittel)

Größerer Aufwand. Vor diesem Schritt: Phasen 1-3 müssen sauber abgeschlossen sein, sonst potenzieren sich die Änderungen.

### Schritt 4.1 – `MicromouseMenu.cpp` aufteilen

**Reihenfolge wichtig** – von außen nach innen:

1. **Hunt-Modul herausziehen:**
   - Neue Datei: `MicromouseHunt.h/cpp`
   - Inhalt: `g_huntActive`, `g_huntHasFreshTarget`, `g_huntTargetX/Y`, `g_huntLastPlanned*`, `onPosDataReceived`, der Jagdmodus-Block in `menuAppLoop`.
   - Exportiert: `huntStart()`, `huntStop()`, `huntTick()`, `huntIsActive()`, `huntHandleNewTarget()`.
   - Aufrufe in `MicromouseMenu.cpp` durch diese API ersetzen.

2. **Empfangenes Maze (`drawReceivedMaze`, `rxMazeNibble`, `rotateNibbleClockwise`, `drawMazeNibbleWalls`, `onMazeDataReceived`) herausziehen:**
   - Neue Datei: `MicromouseRxMazeView.h/cpp`
   - Exportiert: `rxMazeViewDraw()`, `rxMazeViewHandleReceive(data, len)`, `rxMazeViewHasMap()`.

3. **Kollisions-Recovery-UI herausziehen:**
   - Neue Datei: `MicromouseCollisionUI.h/cpp`
   - Inhalt: `headingShortName`, `drawCollisionScreen`, `collisionRecoveryUI`, `mappingShowCollisionStop`.

4. **Aktionen (`doKartierung`, `doSolvingZiel`, `doSolvingZufaellig`, `doSpielDemo`, `doSpielPlay`, `doKarteAnzeigen`, `doKarteSendenWireless`, `doKarteSendenSerial`, `doEditMagOffsetX/Y`, `doResetMagOffsets`) herausziehen:**
   - Neue Datei: `MicromouseMenuActions.cpp` (keine eigene .h – die Funktionen werden nur von Menu-Definitionen verwendet, deren `MenuItem`-Tabellen die Funktionspointer halten).
   - Forward-Decls in der Tabelle bleiben in `MicromouseMenu.cpp`.

5. **Was in `MicromouseMenu.cpp` bleibt:**
   - U8G2-Display-Objekt-Definition
   - Button-Polling
   - `Menu`/`MenuItem` Strukturen
   - Menü-Definitionen (Tabellen)
   - Navigation (`goBack`, `enterItem`, `handleInput`, `drawMenu`)
   - `menuAppSetup`, `menuAppLoop`

### Schritt 4.2 – `MicromouseMapping.cpp` aufteilen

1. **Hardware-Globals herausziehen:**
   - Neue Datei: `MicromouseHardware.h/cpp`
   - `motors`, `lineSensors`, `tof`, `imu` als globale Instanzen.
   - Setup-Funktion `hardwareSetup()`.
   - `readBatteryMv()`.

2. **Kompass herausziehen:**
   - Neue Datei: `MicromouseCompass.h/cpp`
   - `compassAvailable`, `startCompassValid`, `startWorldHeading`, `startCompassDegrees`, `magOffsetX/Y`, `initCompass`, `captureStartCompassHeading`, `dirFromCompassAngle`, `compassSectorCenter`, `angularDistanceDeg`.

3. **Linien-Folge + Wand-Kollisions-Check herausziehen:**
   - Neue Datei: `MicromouseLineFollow.h/cpp`
   - `lineSeen`, `intersectionSeen`, `followLineStep`, `followLineToNextIntersection`, `followLineToNextIntersectionFast`, `centerRobotAfterIntersection`, `calibrateLineSensors`, der Wand-Check-Block.

4. **DFS-Logik herausziehen:**
   - Neue Datei: `MicromouseDFS.cpp`
   - `cellHasUnvisitedNeighbor`, `neighborUnvisited`, `directionScore`, `chooseUnvisitedNeighbor`, `moveToNeighbor`, `backtrackFastUntilUsefulCell`, `mappingStep`, Stack-Funktionen.

5. **`solveDisplay*`-Wrapper herausziehen:**
   - Entweder in `MicromouseSolving.cpp` integrieren (Solving nutzt direkt U8G2) ODER in ein kleines `MicromouseSolveBridge.h/cpp`.

6. **Was in `MicromouseMapping.cpp` bleibt:**
   - Maze-Daten-Globals (`maze[][]`, `mouseX/Y`, `heading`, Stack)
   - `initMaze`
   - `mappingSetup`, `mappingStartRun`, `mappingLoopStep`, `mappingAbortRun`, `mappingGetStatus`, `mappingGetMouse*`, `mappingSetPose`
   - `scanDirection`, `scanCurrentCell`, `setWall`, Lookahead-Funktionen
   - `drawLiveMap`
   - `turnLeft90`, `turnRight90`, `turnAround180`, `turnTo`

### Schritt 4.3 – Verzeichnis-Struktur erstellen

Nach 4.1 und 4.2 ist eine Strukturierung in Unterordner sinnvoll. Empfohlene Bewegung (siehe Abschnitt 4 in Teil 1):
- `src/core/`, `src/hardware/`, `src/algo/`, `src/ui/`, `src/comm/`, `src/platform/`
- `platformio.ini` braucht ggf. `src_filter` / `build_src_filter` – prüfen!

**Hinweis:** Diesen Schritt erst NACH 4.1 und 4.2 machen, sonst sind die `#include`-Pfade doppelt zu ändern.

### Schritt 4.4 – Build-Test
Nach jeder Teil-Aufspaltung beide Targets bauen. Speicher-Footprint vergleichen (`pio run -e pico_normal -v`):

```
RAM:   [== ]  16.X% (used X bytes from 270336 bytes)
Flash: [== ]  Y.Y% (used Y bytes from 2093056 bytes)
```

Sollte sich nur minimal ändern. Größere Abweichungen = etwas falsch gemacht.

---

## Phase 5: Polish (Risiko: niedrig)

### Schritt 5.1 – Magic Numbers in benannte Konstanten

Beispiele:
- `delay(700)` nach Display-Setup → `const uint32_t SPLASH_DURATION_MS = 700;`
- `delay(120)` nach Drehung → `const uint32_t TURN_REST_MS = 120;` (gibt es schon als `SOLVE_TURN_REST_MS` – konsolidieren)
- `MAX_CELL_DRIVE_TIME = 5000` mit Kommentar versehen, warum 5s.
- RGB-Werte in `pacmanGhosts[]` → benannte Konstanten `PACMAN_COLOR_YELLOW = {51, 40, 0}` etc.

### Schritt 5.2 – String-Helper

`MicromouseMenu.cpp` hat 4× das Muster "Keine Karte! / Zuerst kartieren.":
```cpp
static void showNoMapMessage(const char* contextTitle) {
  drawHeader(contextTitle);
  u8g2.drawUTF8(2, 28, "Keine Karte!");
  u8g2.drawUTF8(2, 40, "Zuerst kartieren.");
  oledSendBuffer();
  delay(1000);
}
```
Genutzt in `doSolvingZiel`, `doSolvingZufaellig`, `doSpielDemo`, `doSpielPlay`.

### Schritt 5.3 – Doku auslagern

`MicromousePlatform.h` hat ~110 Zeilen Erklärtext. Den Großteil nach `docs/PLATFORM.md` ziehen, im Header nur den minimalen Kontext (Verweis auf die Markdown-Datei + Skalierungs-Tabelle).

### Schritt 5.4 – Sprachen vereinheitlichen

Empfehlung: **Kommentare auf Deutsch belassen** (Mehrheits-Konvention), **UI-Strings konsequent auf Deutsch**.
- `"SOLVE MODE"` → `"LOESEN"`
- `"GOAL REACHED!"` → `"ZIEL ERREICHT!"`
- `"WALL HIT!"` → `"WAND BERUEHRT!"` oder `"KOLLISION!"`
- `"NO FLASH MAP"` → `"Keine Karte im Flash"`

---

## Verifikations-Strategie

Nach jeder Phase (am besten nach jedem Schritt):

### Build-Verifikation
```bash
pio run -e pico_normal
pio run -e pico_hyper
```
Beide grün, **gleiche** oder **leicht kleinere** Binärgröße.

### Smoke-Test-Plan für Hardware (vom User auszuführen)

Da die KI nicht selbst flashen/testen kann, sollte die KI nach jeder Phase dem User einen Testplan geben:

```
Bitte nach diesem Refactoring auf Hardware testen:
□ Boot ohne Karte: Menü startet, "SOLVE MODE" zeigt "NO FLASH MAP"
□ Mapping-Lauf: vollständig durchläuft (kleines 3×3-Labyrinth reicht)
□ Karte anzeigen: zeigt die gerade gemappte Karte
□ Solving ohne Random: fährt zum gesetzten Ziel
□ Solving Random: läuft endlos zwischen besuchten Zellen
□ Test-Menü: alle 9 Tests öffnen und A drücken (zurück)
□ Selbsttest: löst aus wenn aktiviert (alle 6 Tests durchlaufen)
□ Kompass-Offset-Editor: Werte ändern, speichern, neu booten - bleibt gespeichert
□ Karte → Senden → Wireless: ESP32 empfängt (LED grün)
□ Spiel → Demo: zufällige Ziele in Endlos-Schleife
□ Akku-Warnung: bei niedriger Spannung blinkt LED rot
```

### Snapshot-Verifikation

Vor und nach jeder Phase:
```bash
find src -name "*.cpp" -o -name "*.h" | xargs wc -l | sort -n
```
Erwartung: Zeilenzahl SINKT (durch Konsolidierung), Anzahl Dateien KANN STEIGEN (durch Aufspaltung).

### Graphify-Update
Nach jeder größeren Phase:
```bash
graphify update .
```
Dann `graphify-out/GRAPH_REPORT.md` ansehen – die God-Nodes sollten nach Phase 4 weniger zentralisiert sein (Aufspaltung reduziert Kopplung).

---

## Geschätzter Aufwand

| Phase | Geschätzte Zeit (KI) | Risiko | Lohnt sich? |
|---|---|---|---|
| Phase 1 (toter Code) | 1-2 Stunden | sehr niedrig | ✅ Klar |
| Phase 2 (Konsolidierung) | 4-6 Stunden | niedrig-mittel | ✅ Klar |
| Phase 3 (Renames) | 1-2 Stunden | niedrig | ✅ Klar |
| Phase 4 (Aufspaltung) | 6-10 Stunden | mittel | ⚠️ Nur wenn Codebase weiter wächst |
| Phase 5 (Polish) | 2-3 Stunden | niedrig | ⚙️ Optional |

**Quick Win:** Phasen 1+2+3 zusammen entfernen ca. **15-20 % der Codezeilen** ohne Funktionsverlust und machen die Codebase für zukünftige Features deutlich angenehmer. Phase 4 ist nur sinnvoll, wenn aktiv weiterentwickelt wird – ansonsten "if it ain't broke".
