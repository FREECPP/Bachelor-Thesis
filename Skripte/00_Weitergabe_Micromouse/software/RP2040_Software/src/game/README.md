# Game-Ordner

Dieser Ordner enthaelt die Spiellogik, die aktiv wird, sobald die
Haupt-State-Machine `rpState` auf `RP_STATE_GAME` wechselt. Das Menu soll hier
keine Spiel-Initialisierung mehr selbst ausfuehren, sondern nur den Wechsel in
den Game-State anstossen.

## Stand 8. Juni 2026

Die heutige Ueberarbeitung betrifft ausschliesslich die normalen
RP2040-Maeuse. Die Hyper-Maus liegt ausserhalb des betrachteten Game-Setups.

Umgesetzt wurden:

- Trennung von strategischer Zielwahl, Routenplanung und Motoransteuerung
- vollstaendige Zellrouten als `SolveRoutePlan`
- eigener nicht-blockierender `RouteMotionExecutor`
- vorausschauende Neuplanung an Entscheidungsknoten
- atomarer Austausch der Reststrecke ueber eine `Pending Route`
- encoderbasierte, nicht-blockierende Punktdrehungen
- gemeinsame zellenweise Navigation fuer Pacman und Ghosts
- Entfernung der ToF-Abhaengigkeit aus Solving und Game-Fahrt
- einstellbare und gespeicherte ToF-Wandschwelle fuer die Kartierung
- dezentrale Ghost-Kollisionsvermeidung mit Zellreservierungen
- Prioritaetsentscheidung und `pending`-/`committed`-Reservierungen
- erhoehte Bumper-Empfindlichkeit fuer die Pacman-Capture-Erkennung
- drei Pacman-Leben und koordinierte Return-to-Home-Runden

Der normale Firmwarestand wurde nach den Aenderungen mit
`pio run -e pico_normal` erfolgreich gebaut.

## Solving- und Fahrarchitektur

Die Ghost-Steuerung besteht nicht mehr aus einem Solver, der Planung und
Motorfahrt gleichzeitig ausfuehrt. Sie ist in aufeinander aufbauende Module
getrennt:

```text
Pacman-Position
      |
      v
GhostTargets
  strategisches Ziel fuer Rot/Pink
      |
      v
MicromouseSolving
  Dijkstra -> SolveRoutePlan[]
      |
      v
GhostGame
  aktive Route, Pending Route, Reservierungen, Replanning
      |
      v
RouteMotionExecutor
  naechste Zelle, Center-and-Stop, Pose-Update
      |
      v
navigationDriveCellBegin()/navigationDriveCellStep()
  Encoder-Drehung -> Linienfahrt -> Zellende
      |
      v
Motoren und Encoder
```

### 1. Zielwahl

`GhostTargets` bestimmt nur das strategische Ziel:

- Rot/Shadow verwendet Pacmans zuletzt gemeldete Zelle.
- Pink projiziert anhand von Pacmans Heading ein Ziel vor Pacman.
- Die Zielwahl steuert keine Motoren und veraendert keine Solver-Pose.

### 2. Routenplanung

`solveCreateRoutePlan()` plant mit Dijkstra von der aktuellen Solver-Pose zum
Ziel. `solveCreateRoutePlanFrom()` kann zusaetzlich von einer zukuenftigen
Zelle aus planen. Das Ergebnis ist ein `SolveRoutePlan`:

```cpp
struct SolveRoutePlan
{
  uint8_t x[SOLVE_ROUTE_MAX_CELLS];
  uint8_t y[SOLVE_ROUTE_MAX_CELLS];
  uint8_t length;
};
```

Die Route enthaelt die Startzelle und alle folgenden benachbarten Zellen.
Planung startet noch keine Fahrt. Peer-Positionen und relevante
Ghost-Reservierungen werden vorher als dynamische Solver-Hindernisse gesetzt.

Eine Entscheidungszelle wird durch `solveIsDecisionCell()` erkannt. Aktuell
gilt eine Zelle mit mindestens drei offenen Kanten als Entscheidungsknoten.

### 3. Aktive und vorgemerkte Route

Der `RouteMotionExecutor` besitzt:

- eine aktive Route mit aktuellem Zellindex
- optional eine `Pending Route`
- den Zustand, ob gerade eine Zellfahrt laeuft

Wenn die naechste Zelle eine Entscheidungskreuzung ist, plant `GhostGame`
bereits waehrend der Anfahrt mit `solveCreateRoutePlanFrom()` von dieser
zukuenftigen Pose aus neu. Eine neuere Pacman-Position kann diese vorgemerkte
Route bis zur Ankunft ersetzen.

Erst wenn die Kreuzungszelle vollstaendig erreicht ist, wird die Pending Route
atomar zur aktiven Route. Dadurch muss der Ghost keine alten Pacman-Positionen
in einer langen Warteschlange abfahren und eine laufende Zellfahrt wird
trotzdem nicht mitten im Feld abgebrochen.

### 4. Zellenweise Ausfuehrung

Der Executor startet immer genau den naechsten Zellschritt:

1. Fahrtrichtung zwischen aktueller und naechster Routenzelle bestimmen.
2. Falls noetig eine encoderbasierte Punktdrehung starten.
3. Danach die nicht-blockierende Linienfahrt starten.
4. `routeMotionStep()` zyklisch aufrufen.
5. Erst bei `CELL_REACHED` die Solver-Pose auf die neue Zelle setzen.
6. Reservierung freigeben beziehungsweise auf die Folgezelle verschieben.

Aufeinanderfolgende Geradeaus-Zellen koennen ohne kuenstlichen Zwischenstopp
durchfahren werden. Angehalten wird insbesondere:

- vor einem Richtungswechsel
- am Routenziel
- in der Zelle vor einer Entscheidungszelle, wenn dort eine
  Reservierungsfreigabe benoetigt wird
- bei einem Fahr- oder Encoderfehler

### 5. Encoderbasierte Drehung

`turnBegin()` und `turnStep()` ersetzen die fruehere rein zeitbasierte
Punktdrehung im Game-Betrieb. Beide Radencoder werden getrennt ausgewertet.
Ein bereits fertiges Rad wird gestoppt, waehrend das andere bis zur Toleranz
weiterdreht. Die Geschwindigkeit wird vor dem Ziel reduziert.

Die Drehung besitzt:

- Encoder-Sollwerte fuer 90 und 180 Grad
- Toleranzbereich
- Stall-Erkennung bei fehlendem Encoderfortschritt
- Zeitlimit als Fehlerabsicherung
- kurze Settle-Phase vor dem Aktualisieren des Headings

`turnTo()` existiert weiterhin als blockierender Kompatibilitaets-Wrapper fuer
alte Mapping-Ablaufe. Pacman und Ghosts verwenden im Game einschliesslich
Capture-Recovery und Home-Ausrichtung die nicht-blockierende
Begin-/Step-Schnittstelle. Der separate Victory-Neustart verwendet vorerst
noch den klassischen Solver synchron, ist aber nicht Teil des
Capture-/Return-Ablaufs.

### 6. Replanning ohne unnoetigen Kreuzungsstopp

Ist die geplante Ausfahrt aus einer Kreuzung geradeaus und steht keine weitere
Reservierungsfreigabe an, kann der Executor ohne Halt weiterfahren. Muss der
Ghost abbiegen, endet die Route oder folgt direkt eine weitere
Entscheidungszelle, wird noch waehrend der Anfahrt `Center-and-Stop`
angefordert.

Die Routenplanung selbst blockiert die Motorsteuerung nicht. Nur das
abschliessende Zentrieren nach einer erkannten Kreuzung ist weiterhin ein
kurzes blockierendes Mapping-Primitive.

## Dateien

### `GameState.h` / `GameState.cpp`

Zentraler Einstiegspunkt fuer den Spielmodus.

- definiert die Spielrollen (`GAME_ROLES`) und Spielzustaende (`GAME_STATE`)
- wertet die oberen 3 Bits der `robotId` als Rolle aus
- richtet beim Eintritt in den Game-State die Button- und UART-Callbacks ein
- initialisiert je nach Rolle Pacman oder Ghost
- dispatcht in `gameLoop()` an die passende Rollen-Loop
- setzt `rpState` zurueck auf `RP_STATE_MENU`, wenn eine Rollen-Loop beendet ist

Wichtige Funktionen:

- `gameActive()`: wird beim Wechsel Menu -> Game aufgerufen
- `gameLoop()`: wird im Hauptloop ausgefuehrt, solange `rpState == RP_STATE_GAME`
- `gameLeave()`: raeumt den Game-State beim Verlassen auf
- `gameGetPressedA/B/C()`: konsumierende Button-Abfragen ueber `ButtonAdvanced`
- `gameGetCtrlSwitches()`: liefert die aktuellen Controller-Switches
- `getRoleFromId()` / `setOwnIdRole()`: packen/lesen die Rolle in/aus der `robotId`

### `PacmanGame.h` / `PacmanGame.cpp`

Enthaelt die aktuell vollstaendigste Rollenlogik.

- initialisiert Pacmans Startposition, HUD, Leben und Muenzen
- nutzt `startPosX/startPosY` aus `Globals` als einstellbare Startposition
- liest und latcht Controller-Richtungen ueber `gameGetCtrlSwitches()`
- faehrt Pacman zellenweise durch das bekannte Labyrinth
- nutzt eine nicht-blockierende Linienfahrt ueber `lineFollowFastBegin()` /
  `lineFollowFastStep()`
- erkennt Siege, Neustart, Game Over und Rueckkehr ins Menu
- verarbeitet Capture-Ereignisse ueber Funk und ueber Pacmans eigene Bumper
- aktualisiert waehrend laufender Fahrten das HUD
- kalibriert die Bumper mit 20 Prozent Abstand zur Ruhe-Baseline, damit
  kurze oder seitliche Kontakte frueher als Capture erkannt werden

Die normale Pacman-Loop beendet das Spiel aktuell mit `A` oder `C`. Im
Victory-Screen gilt abweichend: `A` geht ins Menu, `C` startet neu.

Pacmans Fahrlogik ist an das originale Spielverhalten angenaehert:

- ein kurzer D-Pad-Impuls reicht, Pacman faehrt die gelatchte Richtung weiter
- loslassen stoppt nicht mehr; Pacman stoppt erst an einer Wand oder bei
  einer neuen gueltigen Richtung
- lange gerade Strecken werden ohne kuenstlichen Stopp nach der ersten Zelle
  gefahren
- neue Richtungen werden waehrend einer Zellenfahrt vorgemerkt
- wenn an der naechsten Kreuzung ein vorgemerkter Richtungswechsel moeglich
  ist, fordert Pacman fuer diese Kreuzung ein Center-and-Stop an

Die Linienfahrt und vorgeschaltete Punktdrehungen laufen ueber den gemeinsamen
`navigationDriveCellBegin()`-/`navigationDriveCellStep()`-Stepper.
Punktdrehungen werden dabei nicht-blockierend anhand der Motorencoder beendet.
Das Zentrieren nach einer Kreuzung ist weiterhin ein kurzes blockierendes
Primitive aus dem Mapping-Layer.

### `GhostGame.h` / `GhostGame.cpp`

Rollen-Loop fuer Ghost-Roboter.

- initialisiert den Ghost abhaengig von seiner Rolle auf einer Startposition
- nutzt ebenfalls `startPosX/startPosY`; die Blickrichtung bleibt rollenabhaengig
- baut den Solver-Graph aus der bekannten Map auf
- beobachtet Pacman-Positionsmeldungen
- fragt ueber `GhostTargets` ein Ziel ab
- plant mit dem Solver eine vollstaendige Zellroute zum strategischen Ziel
- uebergibt die Route als `SolveRoutePlan` an den `RouteMotionExecutor`
- arbeitet die Zellroute nicht-blockierend ab
- sendet die eigene Position inklusive Heading per `uartL3.sendPosData()` als
  Broadcast; die Heading-Bits liegen in den MSBs der Koordinatenbytes
- kalibriert die Bumper ebenfalls mit 20 Prozent Abstand zur Ruhe-Baseline
- reserviert vor der Fahrt die naechste geplante Zelle
- wartet vor dem ersten Routenschritt und vor jeder Entscheidungszelle auf
  Konfliktmeldungen
- wartet bei einer blockierten Route und plant periodisch neu
- gibt Reservierungen beim Warten, bei Fehlern, am Routenende und beim
  Verlassen des Spiels frei
- plant waehrend der Anfahrt auf Entscheidungsknoten bereits vom kommenden
  Knoten zur neuesten Pacman-Zielposition
- ersetzt die Reststrecke beim Erreichen des Entscheidungsknotens atomar

### `RouteMotionExecutor.h` / `RouteMotionExecutor.cpp`

Der Executor trennt die Fahrlogik von der Routenplanung:

- nimmt eine fertige Folge benachbarter Zellen entgegen
- startet pro Zelle den gemeinsamen
  `navigationDriveCellBegin()`-/`navigationDriveCellStep()`-Stepper
- aktualisiert die Solver-Pose erst nach einer vollstaendig erreichten Zelle
- meldet `CELL_REACHED`, `DONE` oder `ERROR` an `GhostGame`
- durchfaehrt aufeinanderfolgende Geradeaus-Zellen ohne Zwischenstopp
- stoppt vor Richtungswechseln, am Routenziel und bei Fehlern
- stoppt in der Zelle vor einer Entscheidungszelle, damit deren Reservierung
  vor der Einfahrt prioritaetsbasiert freigegeben werden kann
- kann eine `Pending Route` aufnehmen, deren Start die gerade angefahrene
  Entscheidungskreuzung ist
- faehrt ohne Halt weiter, wenn alte Fahrtrichtung und erster Schritt der
  Pending Route identisch sind
- fordert noch waehrend der Anfahrt ein Zentrieren an, wenn die Pending Route
  an der Kreuzung abbiegt oder dort endet

Der Fahrplan ist damit die Warteschlange zwischen Planung und Motoransteuerung:

`GhostTargets -> solveCreateRoutePlan() -> SolveRoutePlan -> RouteMotionExecutor`

Neue Pacman-Positionen brechen eine laufende Zellfahrt nicht ab. Ist die
naechste Zelle ein Entscheidungsknoten, wird die Route von dieser zukuenftigen
Pose aus neu berechnet und als Pending Route vorgemerkt. Bis zur Ankunft kann
eine noch neuere Pacman-Position diese Planung ersetzen. Schlaegt die
Neuplanung wegen eines Peer-Hindernisses fehl, bleibt die bisherige Route
unveraendert aktiv.

ToF-Messwerte werden waehrend Solver-, Pacman- und Ghost-Fahrten nicht
ausgewertet. Sie koennen daher keine Fahrt abbrechen oder verweigern. Der
ToF-Sensor wird nur bei der Kartierung zur Wanderkennung sowie im
Kalibrierungs- und Hardwaretest-Menue verwendet.

Unter `Hardware -> ToF Kalibrieren` wird die aktuelle Distanz zusammen mit der
einstellbaren Wandschwelle angezeigt. Unterhalb der Schwelle gilt die Messung
als `WAND`, oberhalb als `FREI`. Die Schwelle wird in Millimetern gespeichert
und beim Laden alter Einstellungen auf 100 mm initialisiert. Damit koennen
unterschiedliche ToF-Sensoren angepasst werden, ohne ihre Messwerte wieder in
die Game-Fahrt einzukoppeln.

Bedienung:

- `B`: Schwelle um 5 mm verringern
- `C`: Schwelle um 5 mm erhoehen
- `A`: Wert speichern und Menue verlassen

Der einstellbare Bereich reicht von 40 bis 200 mm. Die Live-Messung wird etwa
alle 150 ms aktualisiert.

Shadow/Rot nutzt aktuell direkt Pacmans gemeldete Zelle als Ziel. Neue
Pacman-Positionen werden ueber `senderId` gefiltert. Positionen und
Reservierungen anderer Ghosts werden fuer die dezentrale Kollisionsvermeidung
ausgewertet. Capture- und Rueckkehrlogik sind noch nicht vollstaendig neu
aufgebaut.

### `GhostInteraction.h` / `GhostInteraction.cpp`

Kapselt die dezentrale Interaktion zwischen mehreren Ghost-Robotern.

- speichert aktuelle Positionen und Zellreservierungen anderer Ghosts
- verwirft veraltete Positionen und Reservierungen ueber Timeouts
- behandelt belegte Peer-Zellen als dynamische Solver-Hindernisse
- blockiert Reservierungen hoeher priorisierter Ghosts bei der Neuplanung
- blockiert `committed` Reservierungen unabhaengig von der Prioritaet
- loest gleichzeitige Zellreservierungen ueber die lokale Roboter-ID auf:
  kleinere lokale ID gewinnt
- nutzt bei gleichen lokalen IDs die vollstaendige Roboter-ID als
  deterministischen Tie-Breaker
- nutzt Sequenznummern, damit verspaetete Reservierungen und Freigaben keinen
  neueren Zustand ueberschreiben

Die Kollisionsvermeidung reserviert derzeit nur die unmittelbar folgende Zelle,
nicht die gesamte Route oder den ganzen Korridor. Details zu Nachrichtenformaten,
Timeouts, Konfliktfaellen und bekannten Grenzen stehen in
`GHOST_INTERACTION.md`.

### `GhostTargets.h` / `GhostTargets.cpp`

Kapselt die Zielwahl der Ghost-Rollen.

Aktueller Stand:

- Standardziel fuer Ghosts ist Pacmans aktuelle Position
- Shadow/Rot nutzt dadurch exakt Pacmans aktuelle Zelle
- Pink hat eine eigene Strategie: Pacmans mitgesendete Bewegungsrichtung wird
  verwendet, dann wird ein Ziel vor Pacman projiziert
- Pink projiziert nur ueber bekannte offene Passagen; Ersatz-Ziele muessen vom
  aktuellen Ghost-Standort ueber den Solver-Graph erreichbar sein
- die Zielwahl ist von der eigentlichen Ghost-Bewegung getrennt

Neue Ghost-Strategien sollten hier ergaenzt werden, wenn sie sich nur in der
Target-Wahl unterscheiden.

## Ablauf

### 1. Wechsel vom Menu ins Spiel

Das Menu setzt nur:

```cpp
rpState = RP_STATE::RP_STATE_GAME;
```

Die zentrale Haupt-State-Machine in `main.cpp` erkennt den Wechsel und ruft
`gameActive()` auf.

### 2. Aktivierung des Game-State

`gameActive()`:

1. registriert Game-spezifische Button- und UART-Callbacks
2. setzt Button-/Abort-Flags zurueck
3. liest die aktive Rolle aus `robotId`
4. initialisiert die passende Rollenlogik:
   - `ROLE_PACMAN` -> `startPacmanGame()` -> `pacmanGameInit()`
   - Ghost-Rollen -> `ghostGameInit(activeRole)`

Die Startkoordinaten kommen aus `startPosX/startPosY` und werden unter
`Einstellung -> Spieleinst.` im Menu editiert und gespeichert.

Wenn die Initialisierung fehlschlaegt, bleibt `gameInitialized == false`. Dann
kann der User mit `A` oder `C` wieder ins Menu zurueck.

### 3. Laufender Spielbetrieb

Solange `rpState == RP_STATE_GAME`, ruft der Hauptloop `gameLoop()` auf.

`gameLoop()` dispatcht nach Rolle:

```cpp
ROLE_PACMAN -> pacmanGameLoop()
ROLE_RED    -> ghostGameLoop()
ROLE_PINK   -> ghostGameLoop()
ROLE_CYAN   -> ghostGameLoop()
ROLE_BROWN  -> ghostGameLoop()
```

Die Rollen-Loops geben `true` zurueck, wenn der Spielmodus beendet werden soll.
`GameState` setzt dann:

```cpp
rpState = RP_STATE_MENU;
```

### 4. Verlassen des Game-State

Beim Wechsel zurueck ins Menu ruft die Haupt-State-Machine `gameLeave()` auf.
Dort werden Motoren gestoppt, LEDs auf Idle gesetzt, Abort-Flags geloescht und
Ghost-spezifischer Zustand zurueckgesetzt.

## Eingaben und Kommunikation

### Buttons

Im Game-State nutzt `ButtonAdvanced` eigene Callbacks aus `GameState.cpp`.

- `A`: setzt sofort `g_abortRequested`, wird von `gameGetPressedA()` einmalig konsumiert
- `B`: wird von `gameGetPressedB()` einmalig konsumiert
- `C`: setzt sofort `g_abortRequested`, wird von `gameGetPressedC()` einmalig konsumiert

`g_abortRequested` ist weiterhin wichtig fuer Solver-/Rueckfahrten. Pacmans
normale Zellbewegung inklusive Punktdrehung laeuft ueber den
nicht-blockierenden Navigations-Stepper.

### Controller

Controller-Switches werden per UART-Subscribe empfangen und in `gameCtrlSw`
zwischengespeichert. Rollenlogik liest sie ueber `gameGetCtrlSwitches()`.

### Positionsdaten

Empfangene Positionsdaten werden in `GameState.cpp` in Koordinaten und Heading
dekodiert und rollenabhaengig weitergereicht:

- Pacman-Positionen gehen an `ghostGameObservePacman()` und anschliessend an
  `GhostTargets`.
- Ghost-Positionen gehen auf Pacman-Robotern an `pacmanGameObserveGhost()`.
- Ghost-Positionen gehen auf Ghost-Robotern an `ghostGameObservePeer()` und
  werden von `GhostInteraction` als dynamische Hindernisse verwendet.

### Ghost-Reservierungen

Ghosts tauschen ueber `sendControlData()` drei Nachrichtentypen aus:

- `GC_GHOST_RESERVE_CELL`: reserviert die naechste geplante Zelle
- `GC_GHOST_RELEASE_CELL`: gibt die eigene Reservierung frei
- `GC_GHOST_REPLAN`: fordert den Besitzer einer konfliktbehafteten
  Reservierung zur Neuplanung auf

Vor dem ersten Schritt einer neuen Route und vor jeder Entscheidungszelle
wartet der Ghost 350 ms auf ein moegliches Veto. Bei einem Konflikt gewinnt
die kleinere lokale ID; bei Gleichstand entscheidet die vollstaendige
Roboter-ID. Der andere Ghost bleibt in der Zelle vor der Kreuzung stehen und
versucht nach 250 ms erneut zu planen. Nach jeder erreichten Zelle wird die
Reservierung auf die folgende Routenzelle weitergeschoben. Reine
Korridorzellen verursachen dabei keine zusaetzliche Pause.

Nach dem Veto-Fenster wird die Reservierung als `committed` fuer weitere
150 ms angekuendigt, bevor die Zellfahrt beginnt. Eine bereits committed
Kreuzung kann nicht mehr durch einen spaeter eintreffenden, hoeher
priorisierten Ghost verdraengt werden. Treffen zwei Commit-Meldungen noch
waehrend dieser Stillstandsphase zusammen, entscheidet weiterhin die
Prioritaet. Fuer dieses Verhalten muessen alle Ghosts die neue
Reservierungsnachricht mit Zustandsbyte verwenden.

Der Zustandsablauf einer Kreuzungsreservierung ist:

```text
keine Reservierung
      |
      v
pending, 350 ms Veto-Fenster
      |
      +---- Konflikt/verlorene Prioritaet ----> warten und neu planen
      |
      v
committed, 150 ms Ankuendigung im Stillstand
      |
      +---- simultaner Commit ----> Prioritaet entscheidet
      |
      v
Zellfahrt laeuft, Reservierung ist unverdrängbar
      |
      v
Zelle erreicht -> Freigabe oder Reservierung der Folgezelle
```

Diese Commit-Phase schliesst insbesondere den Fall, dass ein Ghost bereits
abbiegend auf eine Kreuzung zufaehrt und ein spaeter eintreffender, eigentlich
hoeher priorisierter Ghost geradeaus durchfahren will. Sobald die Einfahrt
committed ist, muss der spaetere Ghost warten.

## ToF-Verwendung

Die ToF-Auswertung ist bewusst von der Game-Fahrt getrennt:

- Kartierung: Distanz bestimmt anhand von `tofWallThresholdMm`, ob eine Wand
  vorhanden ist.
- Kalibriermenue: Live-Distanz und Wandschwelle werden angezeigt und editiert.
- Hardwaretest: Sensorfunktion kann weiterhin geprueft werden.
- Solver/Pacman/Ghosts: keine ToF-Abfrage fuer Stoppen, Freigeben oder
  Routenplanung.

Schwankende Lichtverhaeltnisse oder unterschiedliche ToF-Sensoren koennen
damit die Fahrt im bekannten Labyrinth nicht mehr verweigern.

## Capture-Erkennung

Pacman und Ghosts verwenden die optischen Bumper nur dann als Capture-Signal,
wenn die per Funk bekannten Koordinaten ebenfalls zu einer Begegnung passen.
Die Bumper werden beim Spielstart kalibriert.

`marginPercentage` ist auf 20 Prozent eingestellt. Die
Bibliothek berechnet die Schwelle als:

```text
threshold = baseline * (100 + marginPercentage) / 100
```

Der Abstand zur Ruhe-Baseline reduziert Fehlausloesungen durch Schwankungen
der optischen Messwerte. Die Koordinatenpruefung bleibt bestehen, um
Fehlausloesungen ohne Ghost/Pacman-Begegnung zusaetzlich zu vermeiden.

Ein einzelner Messwert oberhalb dieser Schwelle reicht nicht fuer einen
Capture. Der Bumper muss mindestens 75 ms durchgehend gedrueckt gemeldet
werden, waehrend dieselbe Begegnungszelle weiterhin durch frische
Positionsdaten bestaetigt ist. Kurze optische oder vibrationsbedingte
Messspitzen werden dadurch verworfen.

Zur Diagnose wird das ungefilterte Bumper-Signal waehrend des Spiels direkt
auf den vorderen LEDs angezeigt: linker Bumper auf der linken Front-LED,
rechter Bumper auf der rechten Front-LED. Weiss bedeutet, dass die Bibliothek
den jeweiligen Bumper aktuell als gedrueckt meldet. Diese Anzeige ist
unabhaengig von Koordinatenabgleich und 75-ms-Capture-Bestaetigung.

Wiederholte `GC_RETURN_GRANT`-Nachrichten fuer dieselbe Runde starten die
Rueckfahrt nicht erneut. Der Empfangscallback merkt einen neuen Grant nur vor;
die Bewegungsinitialisierung erfolgt anschliessend im normalen Game-Loop.
Dadurch laufen keine blockierenden Dreh- oder Linienoperationen innerhalb
eines UART-Callbacks. Eine Rueckwaerts-Recovery wird ausserdem nur ausgefuehrt,
wenn der Ghost beim Capture bereits messbar in eine Fahrzelle eingefahren war.

## Leben und Return-to-Home

Pacman startet mit drei Leben. Nach einem Capture stoppt `GameState` die
Jagdphase und Pacman erstellt aus den zuletzt empfangenen Ghost-Positionen eine
Rückkehrwarteschlange in Rollenreihenfolge:

```text
Shadow -> Speedy -> Bashful -> Pokey -> Pacman
```

Nach abgeschlossenem Encoder-Rückzug verharrt jeder Roboter mindestens zwei
Sekunden an der wiederhergestellten Position. Pacman sendet danach
`GC_HOMING_START` mit der aktuellen Rundensequenz. Erst dieses Signal gibt die
Homing-Phase frei.

Nicht aktive Ghost-Rollen werden übersprungen. Pacman vergibt anschließend
über `GC_RETURN_GRANT` immer nur einem Ghost Fahrrecht. Der Ghost plant mit dem
normalen Solver zu seiner lokal gespeicherten Startposition und verwendet
weiterhin Folgezellreservierungen sowie die Positionen wartender Ghosts als
dynamische Hindernisse. Wartende Ghosts senden deshalb ihren
Positions-Heartbeat auch während der Rückkehrphase weiter.

Nach Erreichen der Home-Position sendet der Ghost `GC_AT_HOME`. Erst danach
erteilt Pacman dem nächsten Teilnehmer Fahrrecht. Grants und At-Home-Meldungen
werden alle 500 ms wiederholt und enthalten eine Rundensequenz, sodass
Paketverlust oder verspätete Nachrichten keine parallele Fahrt auslösen.

Pacman fährt zuletzt nach Hause. Bei verbleibenden Leben startet
`GC_ROUND_START` die nächste Runde, wobei Münzen und Restleben erhalten
bleiben. Nach dem dritten Capture beendet `GC_GAME_OVER` den Spielmodus auf
allen Robotern.

Sammelt Pacman alle Münzen, sendet er `GC_PACMAN_WINS`. Die Ghosts stoppen
ihre Verfolgung und fahren wie beim Capture zuerst encoderbasiert zur letzten
bestätigten Zelle zurück. Erst danach verwenden sie die geordnete Heimfahrt
über `GC_RETURN_GRANT`. Pacman fährt wiederum zuletzt nach Hause; danach
beendet `GC_GAME_OVER` den Spielmodus auf allen Robotern.

Vor jeder Zellbewegung speichert der Navigationslayer die Encoderstände beider
Räder und das Heading der Ausgangszelle. Bei `GC_CATCHED_PACMAN` stoppen
Pacman und alle Ghosts ihre aktuelle Bewegung und wechseln lokal in den
Capture-Rückzugszustand. Während der Zellbewegung werden die absoluten
Encoderstände beider Räder alle 100 ms gespeichert. Beim Capture wird ein
zusätzlicher letzter Messpunkt aufgenommen. Der Rückzug arbeitet diese
Messpunkte rückwärts ab und reproduziert damit die tatsächliche Radtrajektorie
von Linienkorrekturen, Punktdrehungen und einrädrigen Bögen in umgekehrter
Reihenfolge.

Der Rückzug benötigt keine Kreuzungserkennung und keine freie Strecke vor dem
Roboter. Erst nach erreichtem Encoderstand wird die bekannte Pose der
Ausgangszelle wieder in Mapping und Solver gesetzt. Anschließend beginnt die
bestehende geordnete Heimfahrt über `GC_RETURN_GRANT`.

Der Rückzug ist nicht blockierend und besitzt:

- Encoder-Trajektorie beider Räder in 100-ms-Intervallen
- reduzierte Geschwindigkeit kurz vor dem Encoderziel
- automatische Prüfung der Encoderpolarität
- Stall- und Gesamt-Timeout
- kontrollierten Fehlerzustand statt einer Endlosschleife

Ein fehlender oder ungueltiger Capture-Snapshot gilt nicht als erfolgreiche
Rueckfahrt. Nur wenn beide Encoder nachweislich noch am Zellursprung stehen,
darf der Rueckzug ohne Motorbewegung abgeschlossen werden. Dadurch startet das
Homing nicht mehr mit einer nur logisch behaupteten, physisch falschen Pose.

Jeder physische Roboter muss eine eigene, freie Home-Zelle konfiguriert haben.
Mehrere Roboter können nicht dieselbe Home-Zelle belegen.

## Offene Punkte

- Die Ghost-Kollisionsvermeidung muss noch mit mehreren realen Robotern,
  Paketverlust und stark verzoegerten Nachrichten getestet werden.
- Gegenverkehr in einem laengeren, anfangs freien Korridor wird durch die
  Reservierung nur einer Folgezelle nicht vollstaendig verhindert.
- Vollstaendige Korridorreservierungen und ein kontrollierter Rueckzug zur
  letzten Kreuzung sind noch nicht implementiert.
- Bei einem Hindernis- oder Solverfehler bleibt der Ghost mit einer
  Fehlermeldung stehen, bis A, B oder C gedrueckt wird.
- Capture und Return-to-Home müssen noch mit mehreren realen Robotern und
  absichtlich verlorenen Funkpaketen getestet werden.
- Der Mapping-Modus nutzt fuer seine synchronen Ablaufe weiterhin den
  blockierenden `turnTo()`-Kompatibilitaets-Wrapper; intern arbeitet auch
  dieser mit dem encoderbasierten Dreh-Stepper.
- Das Zentrieren nach einer Kreuzung ist noch blockierend.
