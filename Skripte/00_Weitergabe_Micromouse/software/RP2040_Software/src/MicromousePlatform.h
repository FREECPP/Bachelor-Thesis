#ifndef MICROMOUSE_PLATFORM_H
#define MICROMOUSE_PLATFORM_H

// =============================================================
// MicromousePlatform.h
//
// Compile-Time-Auswahl zwischen Pololu 3pi+ 2040 Mausvarianten.
// Aktivierung ueber platformio.ini build_flags:
//
//   (Default, kein Define)  -> 3pi+ Standard Edition (30:1 MP)
//   -D MOUSE_HYPER          -> 3pi+ Hyper Edition / HyperMouse (15:1 HPCB)
//
// Hardware-Unterschiede laut Pololu (docs/0J86/6.3):
//
//   Edition    Getriebe   Leerlauf-RPM   Top-Speed   Richtung
//   Standard   30:1 MP    720            1.5 m/s     normal
//   Hyper      15:1 HPCB  2100           ~4 m/s      INVERTIERT
//
//   "Inverted" = Getriebe-Output dreht entgegen Motor-Pinion.
//   Wird durch motors.flipLeftMotor(true) / flipRightMotor(true)
//   in mappingSetup() kompensiert (siehe MicromouseMapping.cpp).
//
// Pin-Belegung (OLED, Liniensensoren, Motoren, I2C) ist bei
// beiden Editionen IDENTISCH. Es gibt nur Unterschiede in:
//   - Motor-Drehrichtung      (per Library-API gefixt)
//   - Motor-Geschwindigkeit   (per PWM-Skalierung gefixt, siehe unten)
//
// =============================================================
// PWM-SKALIERUNG
// =============================================================
// Ziel: Beide Editionen erreichen bei IDENTISCHEN logischen
// Konstanten (LINE_BASE_SPEED, TURN_SPEED, SOLVE_FORWARD_SPEED, ...)
// dieselbe physikalische Geschwindigkeit. Damit bleiben Drehzeiten
// und Bogenkurven-Zeiten ueber beide Plattformen gleich kalibriert.
//
// Skalierungsfaktor = Standard-TopSpeed / Hyper-TopSpeed = 1.5 / 4.0
//                   = 0.375 = 3/8
//
// PLATFORM_PWM(p) skaliert einen 3pi+-Standard-Referenz-PWM-Wert
// auf die aktuelle Plattform. Integer-Arithmetik mit Rundung.
//
// ACHTUNG: Bei der Hyper-Edition werden viele Werte sehr klein
// (z.B. LINE_BASE_SPEED=50 -> 19). Pololu empfiehlt zwar max 50%
// PWM (=200) fuer die Hyper-Edition, der untere Anlauf-Bereich
// (PWM<15-20) kann aber wegen Cogging unzuverlaessig sein. Falls
// die Maus bei niedrigen PWMs stehenbleibt, kann der Skalierungs-
// faktor unten leicht erhoeht werden (z.B. NUM=20, DEN=40 = 0.5)
// und dafuer die Drehzeiten verkuerzt werden (Maus laeuft dann
// schneller als die Standard-Edition).
// =============================================================

// =============================================================
// PROBLEM des reinen "gleich-schnell"-Ansatzes:
// Bei einer Skalierung von 1.5/4 = 3/8 = 0.375 (= Top-Speed-
// Verhaeltnis) landen viele Werte unter 20 PWM. Die HPCB-Motoren
// der Hyper-Edition koppen in diesem Bereich (Cogging) und laufen
// nicht zuverlaessig an. Beobachtetes Symptom: Maus bewegt sich
// nicht.
//
// Loesung: Anheben auf 1/2 (PWM ca. 33 % hoeher als bei 3/8) und
// gleichzeitig alle GESCHWINDIGKEITS-PROPORTIONALEN Zeiten via
// PLATFORM_TIME_MS um denselben Faktor kuerzen, damit Drehwinkel
// und Streckenanteile pro Logik-Konstante identisch bleiben.
// Resultat: gleiche Bahn, gleiche Drehwinkel, aber die Hyper-Edition
// faehrt ca. 1.33 x schneller als die Standard – kein "exakt gleich
// schnell" mehr, dafuer zuverlaessiger Anlauf.
// =============================================================

#if defined(MOUSE_HYPER)
  #define MOUSE_PLATFORM_NAME    "3pi+ Hyper"
  #define MOUSE_MOTOR_FLIP_LEFT  1
  #define MOUSE_MOTOR_FLIP_RIGHT 1
  // -----------------------------------------------------------
  // PWM-Skalierung: 7/8.
  //
  // Begruendung: Die HPCB-Motoren (15:1) der Hyper-Edition haben
  // eine hohe Anlauf-/Cogging-Schwelle. Empirisch:
  //   PWM 40 (Test-Menue)        → laeuft
  //   PWM 20 (1/2 * calibrate 40) → laeuft NICHT (Motor steht,
  //                                 Liniensensor-Kalibrierung
  //                                 schlaegt fehl → line timeout)
  // Mit 7/8 landen die kleinsten Fahrwerte ueber ~35 PWM:
  //   calibrate 40 → 35,  line_base 50 → 44,  turn 55 → 48
  // sodass die Motoren zuverlaessig anlaufen.
  //
  // Folge: Die Hyper faehrt 7/8 * (4.0/1.5) ≈ 2.33 x schneller als
  // die Standard-Edition. Schneller geht nur sauber mit Encodern
  // (Closed-Loop) – ohne die ist das der untere Rand fuer
  // verlaesslichen Anlauf.
  //
  // Falls die Motoren weiterhin nicht anlaufen: NUM erhoehen
  // (z.B. 1/1) und TIME entsprechend anpassen (siehe unten).
  // Falls die Maus zu schnell ist und die Linie verliert: NUM
  // senken – aber Cogging-Risiko beachten.
  // -----------------------------------------------------------
  #define PLATFORM_PWM_SCALE_NUM 7
  #define PLATFORM_PWM_SCALE_DEN 8
  // Zeit-Skalierung: 3/7. Haelt die Drehwinkel/Bahn-Abschnitte
  // identisch zur Standard-Edition. Konsistenz-Bedingung:
  //   PWM_SCALE * MOTOR_SPEED_RATIO * TIME_SCALE = 1
  //   (7/8)     * (8/3)             * (3/7)       = 1  ✓
  // MOTOR_SPEED_RATIO = TopSpeed_Hyper/TopSpeed_Standard = 4.0/1.5 = 8/3.
  #define PLATFORM_TIME_SCALE_NUM 3
  #define PLATFORM_TIME_SCALE_DEN 7
  // Encoder-Ziel pro Rad fuer eine 90-Grad-Punktdrehung. Ausgangswert fuer
  // die 15:1-Hyper-Edition; getrennt kalibrierbar von der Standard-Maus.
  #define PLATFORM_TURN_90_COUNTS 119
  // Weg von der ersten Kreuzungserkennung bis zum Drehpunkt der Maus.
  #define PLATFORM_INTERSECTION_CENTER_COUNTS 35
#else
  #define MOUSE_PLATFORM_NAME    "3pi+ Standard"
  #define MOUSE_MOTOR_FLIP_LEFT  0
  #define MOUSE_MOTOR_FLIP_RIGHT 0
  // Keine Skalierung (1:1).
  #define PLATFORM_PWM_SCALE_NUM  1
  #define PLATFORM_PWM_SCALE_DEN  1
  #define PLATFORM_TIME_SCALE_NUM 1
  #define PLATFORM_TIME_SCALE_DEN 1
  // Encoder-Ziel pro Rad fuer eine 90-Grad-Punktdrehung bei 30:1-Getriebe.
  #define PLATFORM_TURN_90_COUNTS 238
  // Etwa 20 mm bei der 30:1-Standard-Maus.
  #define PLATFORM_INTERSECTION_CENTER_COUNTS 70
#endif

// Zentraler Faktor fuer die normale Vorwaertsfahrt entlang der Linie.
// Drehungen, Zentrierung und Kalibrierung werden bewusst nicht skaliert.
#define DRIVE_SPEED_PERCENT 120

// Skaliert einen vorzeichenbehafteten PWM-Wert von der 3pi+-Standard-
// Referenz auf die aktuelle Plattform. Rundet zur naechsten Ganzzahl,
// damit kleine Werte nicht durch Integer-Truncation zu Null werden.
// Negative Werte werden korrekt symmetrisch gerundet.
#define PLATFORM_PWM(standard_pwm)                                        \
  ((int)((((long)(standard_pwm) * PLATFORM_PWM_SCALE_NUM)                 \
          + ((standard_pwm) >= 0 ? (PLATFORM_PWM_SCALE_DEN / 2)           \
                                 : -(PLATFORM_PWM_SCALE_DEN / 2)))        \
         / PLATFORM_PWM_SCALE_DEN))

#define PLATFORM_DRIVE_PWM(standard_pwm)                                  \
  PLATFORM_PWM((((standard_pwm) * DRIVE_SPEED_PERCENT) + 50) / 100)

// Skaliert geschwindigkeits-proportionale Zeiten (Drehzeiten,
// Bogenzeiten, Zellen-Durchfahrzeiten). Sensorberuhigungs-Zeiten
// (SCAN_SETTLE_TIME_MS, SOLVE_TURN_REST_MS etc.) sind NICHT mit
// diesem Makro skaliert, weil sie Hardware-Konstanten sind.
#define PLATFORM_TIME_MS(standard_ms)                                     \
  ((uint32_t)(((unsigned long)(standard_ms) * PLATFORM_TIME_SCALE_NUM     \
               + (PLATFORM_TIME_SCALE_DEN / 2))                           \
              / PLATFORM_TIME_SCALE_DEN))

#endif  // MICROMOUSE_PLATFORM_H
