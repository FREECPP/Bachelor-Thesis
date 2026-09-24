# Dezentrale Ghost-Interaktion

## Ziel

Mehrere physische Ghost-Roboter sollen dasselbe Labyrinth befahren, ohne
zusammenzustoßen oder dieselbe Zelle gleichzeitig zu belegen. Eine zentrale
Koordination ist durch die Projektvorgabe ausgeschlossen.

Die erste Ausbaustufe behandelt andere Ghosts als dynamische Wände. Ergänzend
wird die jeweils nächste geplante Zelle reserviert. Das ist einfacher als eine
vollständige Korridorreservierung und nutzt die vorhandene Solver-API:

- `solveClearDynamicObstacles()`
- `solveBlockCell(x, y)`
- `solvePlannedNextCell(x, y)`

## Aktueller Implementierungsstand

Die erste Ausbaustufe ist im Code umgesetzt und kompiliert erfolgreich.

### Neue Module und Schnittstellen

- `GhostInteraction.h` / `GhostInteraction.cpp`
  - speichert Positionen und Reservierungen anderer Ghosts
  - verwaltet Timeouts und Sequenznummern
  - wendet Peer-Zellen als dynamische Solver-Hindernisse an
  - entscheidet Konflikte anhand der lokalen ID
- `GhostGame.cpp`
  - sendet und erneuert eigene Reservierungen
  - wartet vor einem neuen Teilpfad kurz auf mögliche Vetos
  - gibt Reservierungen beim Warten, bei Fehlern und beim Verlassen frei
  - unterbricht keine bereits begonnene Linien- oder Kurvenfahrt
  - wartet bei Peer-Konflikten und plant periodisch neu
- `GameState.cpp`
  - leitet empfangene Ghost-Positionen an `GhostInteraction` weiter
  - verarbeitet Reservierungs-, Freigabe- und Replanning-Nachrichten
- `MicromouseSolving.cpp`
  - bietet einen nicht-fehlerhaften Planungsversuch für dynamisch blockierte
    Wege an

### Umgesetzter Laufzeitablauf

1. Jeder Ghost sendet weiterhin Position und Heading als Heartbeat.
2. Vor einer Planung werden frische Peer-Positionen als Wände gesetzt.
3. Reservierungen höher priorisierter Ghosts werden ebenfalls blockiert.
4. Der Solver plant bis zum nächsten Entscheidungsknoten.
5. Die unmittelbar nächste Zelle wird per `sendControlData()` reserviert.
6. Vor dem ersten Routenschritt und vor jeder Entscheidungszelle wartet der
   Ghost im sicheren Stillstand kurz auf ein mögliches Veto.
7. Ein höher priorisierter Ghost sendet bei einem Konflikt eine
   Replanning-Nachricht an den Reservierungsinhaber.
8. Bei gleichzeitig reservierter Zelle gewinnt die kleinere lokale ID. Bei
   gleichen lokalen IDs entscheidet die vollständige Roboter-ID.
9. Der andere Ghost plant mit der Konfliktzelle als dynamischem
    Hindernis neu. Ohne Alternativroute wartet er.
10. Korridorzellen laufen ohne Kommunikationswartezeit weiter. Vor der
    nächsten Entscheidungszelle hält der Executor eine Zelle vorher an.
11. Nach jeder erreichten Zelle wird die folgende Solver-Zelle reserviert.
12. Am Entscheidungsknoten beginnt die Planung erneut.

Ein durch andere Ghosts blockierter Weg ist kein Solverfehler. Erst wenn auch
ohne dynamische Peer-Hindernisse kein Pfad existiert, wird der normale
Solverfehler angezeigt.

### Aktuelle Zeitparameter

```text
Positions-Heartbeat:          400 ms
Peer-Position gültig:        2000 ms
Reservierungs-Heartbeat:      100 ms
Peer-Reservierung gültig:     900 ms
Replanning beim Warten:       250 ms
Veto-Fenster vor Start/Knoten: 350 ms
Commit-Ankündigung:            150 ms
```

### Reservierungslebenszyklus

Eine Reservierung wird:

- vor der Fahrt in die nächste Zelle angelegt
- während der Fahrt regelmäßig erneut gesendet
- nach Erreichen der Zelle auf die folgende Zelle weitergeschoben
- beim Warten auf einen Peer freigegeben
- bei einem Solver- oder Hindernisfehler freigegeben
- am Ende des Teilpfads freigegeben
- beim Verlassen des Spiels freigegeben

Sequenznummern verhindern, dass verspätete Reservierungen oder Freigaben einen
neueren Zustand überschreiben.

## Ausgetauschte Zustandsdaten

Jeder Ghost verwaltet pro erkanntem Peer:

- vollständige `robotId`
- aktuelle Zelle
- Heading
- Empfangszeitpunkt
- optional nächste geplante Zelle
- Empfangszeitpunkt der Reservierung

Veraltete Daten dürfen den Solver nicht dauerhaft blockieren. Vorgeschlagene
Startwerte:

- Peer-Position: nach 1500 bis 2500 ms ungültig
- Zellreservierung: nach 600 bis 1000 ms ungültig
- Positions-Heartbeat: weiterhin etwa alle 400 ms

## Planungsablauf

Vor jeder Neuplanung:

1. Dynamische Hindernisse löschen.
2. Frische aktuelle Zellen aller anderen Ghosts blockieren.
3. Frische reservierte Folgezellen anderer Ghosts abhängig von der Priorität
   blockieren.
4. Strategisches Rollenziel bestimmen.
5. Pfad mit dem Solver planen.
6. Eigene nächste Zelle als Reservierung veröffentlichen.
7. Kurzes Veto-Fenster im Stillstand abwarten.
8. Bei einem Veto neu planen, sonst den Teilpfad ohne weitere
   Kommunikationspause fahren.

Nach Erreichen eines Entscheidungsknotens wird neu geplant.

## Konflikte und Priorität

Eine feste, deterministische Priorität verhindert, dass beide Ghosts
gleichzeitig aufeinander warten oder fahren.

Primär wird die lokale ID verwendet:

```cpp
uint8_t localId = robotId & 0x1F;
```

```text
kleinere lokale ID gewinnt
größere lokale ID wartet oder plant neu
```

Bei gleichen lokalen IDs entscheidet zusätzlich die vollständige `robotId`.
Die Rollenbits bilden damit einen deterministischen Tie-Breaker.

Die Prioritätsentscheidung gilt mindestens für:

- beide Ghosts reservieren dieselbe Zelle
- ein Ghost will in die aktuelle Zelle eines anderen fahren
- zwei Ghosts wollen ihre aktuellen Zellen tauschen
- ein Ghost reserviert die aktuelle Zelle eines anderen

Die Priorität gilt nur, solange Reservierungen noch `pending` sind. Nach dem
Veto-Fenster sendet der Gewinner seine Reservierung zunächst 150 ms als
`committed`, bleibt dabei aber noch stehen. Eine später eintreffende
Pending-Reservierung muss unabhängig von ihrer Priorität warten. Erst nach der
Commit-Ankündigung beginnt die Zellfahrt. Während der Fahrt kann die
Reservierung nicht mehr verdrängt werden.

Falls zwei Commit-Nachrichten während der Ankündigungsphase zusammentreffen,
stehen beide Roboter noch und die normale Priorität löst den Konflikt auf.
Damit kann ein Geradeausfahrer nicht mehr in eine Kreuzung einfahren, die ein
Abbieger bereits verbindlich belegt hat.

Ein niedriger priorisierter Ghost darf eine höher priorisierte Reservierung
nicht ignorieren. Der höher priorisierte Ghost darf dagegen neu planen, wenn
der andere Ghost eine Zelle bereits physisch belegt.

Beispiel mit lokalen IDs 0 und 1 an einer T-Kreuzung:

- Beide Ghosts reservieren gleichzeitig die mittlere Zelle.
- ID 0 erkennt die Reservierung von ID 1 und sendet ein Replanning-Veto.
- ID 0 darf nach Ablauf seines Veto-Fensters weiterfahren.
- ID 1 gibt seine Reservierung frei und plant um die mittlere Zelle herum.
- Existiert keine alternative Route, wartet ID 1 vor der Kreuzung.

## Verhalten beim Warten

Ein wartender Ghost:

- stoppt vor der Konfliktzelle
- behält seine aktuelle Zelle als Position bei
- sendet weiterhin Heartbeats
- plant in kurzen Abständen neu
- fährt weiter, sobald die Blockierung abgelaufen oder aufgehoben ist

Ein Konflikt darf nicht als Solverfehler behandelt werden. Der Ghost bleibt im
Spiel und wartet in einem eigenen Zustand beziehungsweise mit einem
`waitingForPeer`-Flag.

## Pacman-Ausnahme

Pacmans aktuelle Zelle darf weiterhin das Jagdziel sein. Ein Ghost hinter
Pacman hebt die Blockierung durch einen anderen Ghost jedoch nicht vollständig
auf.

Sichere Regel:

- Die aktuelle Zelle eines anderen Ghosts bleibt immer blockiert.
- Der jagende Ghost darf bis zu Pacmans aktueller Zelle planen.
- Er darf nicht durch die belegte Peer-Zelle hindurch planen.
- Nach Erreichen von Pacmans letzter bekannter Zelle wird neu geplant.

Damit kann ein Ghost Pacman verfolgen, ohne absichtlich auf einen dahinter
stehenden Ghost zuzufahren.

## Nachrichtenverlust

Reservierungen sind zeitlich begrenzte Hinweise und keine dauerhaften Locks.

- Jede Meldung enthält implizit den Absender über `senderId`.
- Neue Meldungen ersetzen den vorherigen Zustand dieses Peers.
- Abgelaufene Positionen und Reservierungen werden ignoriert.
- Eine empfangene neue Position hebt eine alte, bereits erreichte Reservierung
  auf.
- Eigene Broadcasts werden ignoriert.
- Reservierungen und Replanning-Nachrichten werden als Broadcast gesendet.
- Eine aktive Reservierung wird alle 100 ms wiederholt.
- Replanning-Vetos werden vor dem ersten Routenschritt und vor
  Entscheidungszellen umgesetzt. Der Executor hält dafür in der vorherigen
  Zelle; eine bereits laufende Zellfahrt wird nicht unterbrochen.

## ControlData-Format

Die erste Ausbaustufe verwendet drei `sendControlData()`-Nachrichten:

```text
GC_GHOST_RESERVE_CELL = 16
Payload: [16, x, y, sequence, state]

state = 0: pending
state = 1: committed

GC_GHOST_RELEASE_CELL = 17
Payload: [17, sequence]

GC_GHOST_REPLAN = 18
Payload: [18, ownerId, sequence]
```

Die Absender-ID wird vom Kommunikationslayer als `senderId` geliefert und
nicht zusätzlich in den Payload geschrieben. Die Sequenznummer verhindert,
dass ein verspätetes Paket eine neuere Reservierung überschreibt.

`ownerId` bezeichnet den Ghost, der seine Route neu planen soll. Dadurch
können alle Ghosts dieselbe Broadcast-Nachricht empfangen, aber nur der
Reservierungsinhaber mit der passenden Sequenznummer wertet das Veto aus.

Eine aktive Reservierung wird regelmäßig erneut gesendet. Nach jeder
erreichten Zelle reserviert der Ghost die folgende Solver-Zelle. Beim Warten,
bei einem Fehler, beim Erreichen des Entscheidungsknotens und beim Verlassen
des Spiels wird die Reservierung freigegeben.

Empfänger akzeptieren weiterhin vier Byte lange Reservierungspakete und
behandeln sie als `pending`. Die Kollisionssicherheit des Commit-Verfahrens ist
jedoch nur gegeben, wenn alle beteiligten Ghosts die fünf Byte lange Nachricht
auswerten.

## Bekannte Grenze

Positionen plus Folgezellreservierungen verhindern nicht zuverlässig, dass
zwei Ghosts gleichzeitig von entgegengesetzten Seiten in einen längeren,
anfangs leeren Korridor einfahren.

Treffen sie sich dort, kann keiner am anderen vorbeifahren. Mögliche spätere
Erweiterungen sind:

- Reservierung eines vollständigen Korridors zwischen Entscheidungsknoten
- Rückzug des niedrig priorisierten Ghosts zur letzten Kreuzung
- feste Einbahnrichtungen für besonders kritische Korridore

Die erste Ausbaustufe sollte dieses Restrisiko protokollieren und den Ghost
kontrolliert stoppen, statt die Kollision fortzusetzen.

## Empfohlene Implementierungsreihenfolge

Bereits implementiert:

1. Ghost-Positionsmeldungen auf Ghost-Robotern empfangen und speichern.
2. Peer-Zellen vor jeder Planung mit `solveBlockCell()` sperren.
3. Konflikte mit der unmittelbar nächsten Zelle vor Fahrtbeginn prüfen.
4. Warten und periodisches Replanning.
5. Austausch der nächsten geplanten Zelle über `sendControlData()`.
6. Priorität anhand der lokalen ID.
7. Broadcast-Veto vor dem Start eines neuen Teilpfads.

Noch praktisch zu prüfen beziehungsweise zu ergänzen:

1. Zelltausch und gleichzeitige Reservierung derselben Zelle testen.
2. Verhalten bei Paketverlust und stark verzögerten Paketen testen.
3. Mehrere Ghosts gleichzeitig auf realer Hardware testen.

Bewusst zurückgestellt:

- gleiche lokale IDs beim Spielstart erkennen
- Gegenverkehr in langen Korridoren vollständig lösen
- Korridorreservierungen

## Testfälle

- Zwei Ghosts planen dieselbe Kreuzung.
- Zwei Ghosts wollen dieselbe Zielzelle.
- Zwei Ghosts wollen ihre Zellen tauschen.
- Ein Ghost steht, während ein anderer von hinten ankommt.
- Ein Ghost sendet keine Heartbeats mehr.
- Pacman steht zwischen zwei Ghosts.
- Pacman biegt ab, nachdem beide Ghosts auf ihn zugefahren sind.
- Zwei Ghosts fahren gleichzeitig in einen langen Korridor ein.
- Zwei Ghosts besitzen dieselbe lokale ID, aber unterschiedliche Rollen.
