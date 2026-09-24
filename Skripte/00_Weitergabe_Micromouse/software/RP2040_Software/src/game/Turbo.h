#pragma once
#include <Arduino.h>

// ============================================================
// Turbo-Button (Pacman)
//
// Per Xbox-Taste X aktiviert der Spieler waehrend des Pacman-Spiels
// fuer eine einstellbare Zeit einen Geschwindigkeits-Boost auf den
// Geraden. Danach folgt eine Cooldown-Phase, in der X ignoriert wird
// (Anti-Spam). Alle drei Parameter sind persistent (SavedSettings).
//
// Zustaende: IDLE -> ACTIVE (turboDurationS) -> COOLDOWN (turboCooldownS) -> IDLE
// Zeitmessung: Wall-Clock (millis) ab Tastendruck.
//
// Der Boost wirkt ausschliesslich ueber g_lineSpeedFactor (siehe
// MicromouseMapping) auf die Geradeaus-Linienfahrt – nicht auf Drehungen
// oder das Zentrieren an Kreuzungen.
// ============================================================

// --- Persistente Einstellungen (werden ueber das Menue editiert und in
//     SavedSettings gespeichert). ---
extern uint8_t turboDurationS;   // Boost-Dauer in Sekunden          (1..30)
extern uint8_t turboCooldownS;   // Cooldown in Sekunden             (0..60)
extern uint8_t turboFactorPct;   // Geschwindigkeitsfaktor in Prozent (110..200)

// Voreinstellungen / Grenzen (auch fuer Menue und Settings-Validierung).
constexpr uint8_t TURBO_DURATION_DEFAULT = 5;
constexpr uint8_t TURBO_COOLDOWN_DEFAULT = 10;
constexpr uint8_t TURBO_FACTOR_DEFAULT   = 150;
constexpr uint8_t TURBO_DURATION_MIN = 1,   TURBO_DURATION_MAX = 30;
constexpr uint8_t TURBO_COOLDOWN_MIN = 0,   TURBO_COOLDOWN_MAX = 60;
constexpr uint8_t TURBO_FACTOR_MIN   = 110, TURBO_FACTOR_MAX   = 200;

// Vollstaendig zuruecksetzen (IDLE, Faktor 1.0). Beim Spielstart aufrufen.
void turboReset();

// Aktiven Turbo sofort abbrechen und Cooldown loeschen (bei Capture).
void turboCancel();

// Mit der steigenden Flanke der X-Taste aufrufen (true = gerade gedrueckt).
// IDLE -> Turbo starten; ACTIVE/COOLDOWN -> Reject-Feedback.
void turboHandleButtonEdge(bool xRisingEdge);

// Einmal pro Pacman-Loop-Zyklus aufrufen. Treibt Timer, LED-Puls,
// Reject-Flash und setzt/raumt g_lineSpeedFactor.
void turboUpdate();

// true solange der Boost aktiv ist.
bool turboIsActive();

// 1.0 -> 0.0 ueber das aktive Fenster (fuer den schwindenden HUD-Balken).
float turboRemainingFraction();

// true solange der Turbo die LEDs steuert (aktiver Puls oder Reject-Flash);
// der Aufrufer soll dann seine eigenen LED-Updates aussetzen.
bool turboOwnsLeds();
