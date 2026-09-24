# src-Ordner

Dieser Ordner enthaelt die Firmware fuer den RP2040-Roboter. Die grobe
Architektur ist eine Haupt-State-Machine in `main.cpp`, die zwischen Menu und
Spiel unterscheidet. Gemeinsame Hardware-, Mapping-, Solver- und
Kommunikationsmodule liegen direkt in `src/`; groessere Funktionsbereiche sind
in Unterordner ausgelagert.

## Einstiegspunkt

### `main.cpp`

`main.cpp` ist der zentrale Arduino-Einstiegspunkt.

- initialisiert Serial, UART_L3, I2C und Menu
- ruft in jeder Loop `uartL3.update()`, `buttonAdvanced.update()` und
  `ledUpdate()` auf
- verwaltet die Haupt-State-Machine ueber `rpState`
- ruft beim Zustandswechsel `menuAppActive()`, `gameActive()` oder
  `gameLeave()` auf
- dispatcht danach an `menuAppLoop()` oder `gameLoop()`

Die Hauptzustaende stehen in `Globals.h`:

```cpp
enum RP_STATE {
    RP_STATE_MENU,
    RP_STATE_GAME
};
```

## Unterordner

### `menu/`

Enthaelt die Menu-Implementierung. `MicromouseMenu.cpp` ist nur noch der
oeffentliche Einstiegspunkt; die eigentliche Logik ist in mehrere Dateien in
`menu/` aufgeteilt.

Details: `src/menu/README.md`

### `game/`

Enthaelt die Spiel-State-Machine und die Rollenlogik fuer Pacman und Ghosts.
`GameState.cpp` ist der Einstieg in den Spielmodus und delegiert an
`PacmanGame.cpp` oder `GhostGame.cpp`.

Details: `src/game/README.md`

### `deleted/`

Enthaelt alte, bewusst aus dem aktiven Build geloeste Dateien mit der Endung
`.deleted`. Sie dienen nur noch als Referenz fuer spaetere Konzepte und werden
nicht kompiliert.

## Zentrale Module direkt in `src/`

### Globale Zustaende

- `Globals.h` / `Globals.cpp`: globale Runtime-Werte wie `rpState`,
  `robotId`, Startposition und gemeinsame Button-/Abort-Flags
- `MicromouseMappingState.h`: gemeinsamer Mapping-/Maze-Zustand

### Eingaben

- `ButtonAdvanced.h` / `ButtonAdvanced.cpp`: zentrale Button-Abstraktion mit
  einmalig konsumierbaren Edge-Events (`wasPressed`, `wasReleased`, `wasHeld`)
  und Callback-Anbindung fuer Menu/Game

### Kommunikation

- `UART_L3.h` / `UART_L3.cpp`: UART-Kommunikationslayer zum ESP bzw. zu anderen
  Robotern; transportiert Controller-, Maze- und Positionsdaten

### Mapping und Bewegung

- `MicromouseMapping.h` / `MicromouseMapping.cpp`: Mapping-Lauf,
  Linienverfolgung, Dreh-/Fahr-Primitives, Map-Aufbau und Kollisionslogik
- `MicromouseSolving.h` / `MicromouseSolving.cpp`: Solver fuer Zielnavigation
  auf Basis der bekannten Map
- `MicromouseMapExport.cpp`: Export der Map-/Graph-Daten
- `MicromouseMazeFrame.h` / `MicromouseMazeFrame.cpp`: Maze-Frame-Daten fuer
  Kommunikation und Anzeige

Wichtig fuer das Spiel: Pacman nutzt fuer die normale Zellenfahrt den
nicht-blockierenden Fast-Line-Follow-Stepper aus `MicromouseMapping`:

```cpp
lineFollowFastBegin(centerAndStop);
lineFollowFastStep();
```

Die klassischen Mapping-/Solver-Funktionen koennen weiterhin blockierende
Primitives wie `turnTo()` oder `followLineToNextIntersectionFast()` verwenden.

### Anzeige und LEDs

- `MicromouseGraphics.h` / `MicromouseGraphics.cpp`: OLED-Helfer und
  gemeinsame Zeichenfunktionen
- `MicromouseLeds.h` / `MicromouseLeds.cpp`: LED-Zustaende, Idle/Game-Farben
  und temporaeres `flashColor(...)` mit Rueckkehr zum vorherigen Zustand ueber
  `ledUpdate()`

### Persistenz und Einstellungen

- `MicromousePersistence.h` / `MicromousePersistence.cpp`: persistente
  Speicherung von Robot-ID, Maze, Kalibrierung und Spieleinstellungen
- `MicromousePlatform.h`: plattformnahe Definitionen und Hardware-Konstanten

### Tests und Diagnose

- `MicromouseHardwareTest.h` / `MicromouseHardwareTest.cpp`: Hardware-Test- und
  Diagnosefunktionen

## Datenfluss grob

1. `main.cpp` aktualisiert UART, Buttons und LEDs.
2. `rpState` entscheidet, ob Menu oder Game aktiv ist.
3. Im Menu werden Einstellungen, Mapping, Solving und Game-Start bedient.
4. Beim Game-Start setzt das Menu nur `rpState = RP_STATE_GAME`.
5. `gameActive()` initialisiert die aktive Rolle aus den oberen 3 Bits der
   `robotId`.
6. `gameLoop()` ruft die passende Rollenlogik auf.
7. Wenn eine Rollenlogik `true` zurueckgibt, wechselt `GameState` wieder nach
   `RP_STATE_MENU`.

## Hinweise fuer weitere Arbeit

- Neue Menu-Funktionen gehoeren in den passenden Owner unter `src/menu/`.
- Neue Spiellogik gehoert in `src/game/`.
- Gemeinsame Hardware- oder Fahr-Primitives sollten direkt in `src/` bleiben,
  solange sie von Menu, Mapping, Solver und Game gemeinsam genutzt werden.
- Alte Konzepte in `deleted/` nicht reaktivieren, ohne sie vorher an die neue
  `rpState`-/`GameState`-Struktur anzupassen.
