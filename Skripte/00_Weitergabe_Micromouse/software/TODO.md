# TODO

## Kommunikation RP2040, ESP32 und ESP-NOW

Die zuletzt beobachteten Aussetzer beim Spielstart wurden durch eine veraltete
Firmware auf einem ESP32 verursacht. Die Kommunikation bleibt deshalb vorerst
unverändert. Bei einer spaeteren Ueberarbeitung sollten dennoch folgende
Schwachstellen behoben werden.

### Firmwarestand nachvollziehbar machen

- Firmwareversion oder Git-Commit beim Start seriell ausgeben.
- Optional die Version ueber UART an den RP2040 melden und dort anzeigen.
- Sicherstellen, dass alle Roboter mit derselben kompatiblen ESP-Firmware
  geflasht sind.

### ESP-NOW-Empfangsqueue synchronisieren

`espNowReceiveData()` veraendert die verketteten `MESSAGE_LIST`-Listen aus dem
asynchronen ESP-NOW-Callback, waehrend `processNow()` dieselben Listen liest und
Elemente freigibt. Die Liste ist nicht gegen parallele Zugriffe geschuetzt.

- Callback und Hauptschleife mit einer geeigneten Critical Section oder Queue
  synchronisieren.
- Vorzugsweise eine feste FreeRTOS-Queue oder einen Ringpuffer statt
  `malloc()`-basierter Listen im Callback verwenden.
- Im Callback keine langen Listen traversieren und keine seriellen Ausgaben
  ausloesen.

### Nachricht und Absender gemeinsam speichern

ESP-NOW-Nachricht und Absender-MAC werden momentan in zwei getrennten Listen
gespeichert. Schlaegt nur eine Allokation fehl, koennen beide Listen dauerhaft
gegeneinander verschoben sein.

- Nachricht, Laenge und MAC-Adresse in einem gemeinsamen Queue-Element ablegen.
- Rueckgabewerte beim Einreihen pruefen und Fehler zaehlen beziehungsweise
  melden.
- Die MAC-Adresse mit ihrer tatsaechlichen Laenge von sechs Bytes kopieren;
  momentan werden acht Bytes gelesen.

### Dynamische Nachrichtenlisten begrenzen

`MESSAGE_LIST` besitzt keine maximale Laenge und verwendet fuer jedes Element
mehrere dynamische Allokationen.

- Maximale Queue-Laenge und definiertes Verhalten bei Ueberlauf festlegen.
- Queue-Auslastung, verworfene Nachrichten und Allokationsfehler diagnostisch
  erfassbar machen.
- Langfristig feste Puffer verwenden, um Heap-Fragmentierung zu vermeiden.

### UART-ACK-Zustand absichern

`UART_L2::update()` dekrementiert `txAnswerOpen` bei jedem empfangenen ACK oder
NAK ohne zu pruefen, ob eine Antwort offen ist. Ein verspaetetes oder doppeltes
ACK kann den Zaehler negativ machen und weitere Sendungen blockieren.

- ACK und NAK nur bei offenem Sendevorgang akzeptieren.
- `txAnswerOpen` durch einen eindeutigen booleschen oder expliziten
  Zustandsautomaten ersetzen.
- Unerwartete ACKs und NAKs protokollieren, statt den Zustand zu veraendern.
- Timeout-Berechnungen ueber Differenzen ausfuehren, damit ein
  `millis()`-Ueberlauf korrekt behandelt wird.

### UART-Puffer und Wiederholungen robuster machen

- Vor dem Schreiben in `rxBuffer` dessen Grenze pruefen.
- Eine gesendete Nachricht erst nach erfolgreichem ACK endgueltig aus der
  Sendewarteschlange entfernen oder eine separate Kopie des aktiven Frames
  verwalten.
- Nach ausgeschoepften Wiederholungen den Nachrichtenverlust eindeutig melden.
- Fehlerzaehler nach erfolgreicher Kommunikation konsistent zuruecksetzen.

### ESP-NOW-Zustellung beobachtbar machen

Broadcast-Nachrichten besitzen keine anwendungsspezifische
Empfangsbestaetigung. Fehler von `esp_now_send()` werden derzeit weitgehend
ignoriert.

- Rueckgabewerte von `esp_now_send()` auswerten.
- Einen Sendestatus-Callback registrieren und Fehler zaehlen.
- Fuer wichtige Ereignisse wie Spielstart Sequenznummern, Quittierungen oder
  zeitlich begrenzte Wiederholungen vorsehen.
- Doppelte Ereignisse auf Empfaengerseite anhand der Sequenznummer
  unterdruecken.

### Diagnose verbessern

- Empfangene Game-Control-Nachrichten mit Opcode, Absender, Laenge und
  Ablehnungsgrund optional protokollieren.
- Queue-Laengen, UART-Wiederholungen, unerwartete ACKs und verworfene
  ESP-NOW-Pakete ueber einen Diagnosemodus sichtbar machen.
- Zwischen Funkempfang auf dem ESP32, UART-Weiterleitung und Annahme durch die
  RP2040-Zustandslogik unterscheiden.

## Geister fluessig durch Kreuzungen fahren

Geister fahren fluessig durch Abbiegungen (rollender Bogen statt Stopp-und-Dreh)
und durch Entscheidungszellen. Die **vorausschauende Reservierung** ist im
**Jagd-Loop** (`ghostGameLoop`) jetzt umgesetzt: das Veto-Fenster laeuft schon
in der Korridor-Anfahrt ab, sodass die Kreuzung beim Eintreffen meist committed
ist und ohne Halt durchfahren wird. Reine Optimierung mit sicherem Fallback
(Default bleibt "anhalten + Veto", nur bei committeter Freigabe geloescht).

OFFEN:
- **Hardware-Test mit >= 2 Geistern**: verifizieren, dass nie zwei Geister
  dieselbe Kreuzung committen (Priority-Yield im Pending-Fenster) und dass das
  Durchrollen bei realer Korridor-Fahrzeit (> ~500 ms) greift; sonst Fallback-
  Halt. Veto-/Commit-Fenster ggf. nachziehen.
- **Heimfahr-Loop** (`GS_RETURNING_TO_HOME` / die zweite Fahr-Schleife in
  `ghostGameLoop`): nutzt noch das alte Gate ohne Vorausschau. Fuer konsistent
  fluessige Heimfahrten dort dieselbe Logik anwenden.

### Umgesetztes Design (Referenz)

- Naechste Entscheidungszelle eine Korridorzelle frueher reservieren; EIN
  Reservierungs-Slot. Korridorzellen ueber die Positionen geschuetzt (heutige
  Korridor-Reservierungen bieten ohnehin keinen gegenseitigen Ausschluss).
- Gate haelt nur, wenn die Maus UNMITTELBAR in die reservierte Zelle einfaehrt
  (`s_reservationX/Y == routeMotionNextCell`); Veto-Fortschritt + Yield laufen
  immer auf der reservierten Zelle.
- `centerAndStop` Default "anhalten"; per `navigationDriveCellSetCenterStop(false)`
  geloescht, sobald die reservierte Entscheidungszelle committed ist und gerade
  die Korridorzelle davor gefahren wird (`routeMotionFollowingCell == reserviert`).

### Edge Cases absichern

- Muss-Weichen-Fall: wird waehrend der Anfahrt ein hoeher priorisierter Peer
  erkannt, an der letzten Korridorzelle vor der Kreuzung anhalten (Puffer) und
  `waitForPeer()`, bevor die Entscheidungszelle betreten wird.
- Kurze Anfahrt: ist das Veto-Fenster beim Ankommen noch nicht abgelaufen,
  Rest-Halt an der Korridorzelle (verkuerzt, sicher).
- Zwei Entscheidungszellen direkt hintereinander (kein Korridor-Puffer): kein
  Pipelining moeglich → Fallback auf den heutigen Halt.
- Replan waehrend der Anfahrt: die vorausschauende Reservierung verwerfen/neu
  setzen (haengt an `stageRouteUpdateAtNextDecision`).
- Mit >= 2 Geistern auf Hardware verifizieren, dass nie zwei Geister dieselbe
  Kreuzung committen.
