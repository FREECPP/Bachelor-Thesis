
// ============================================================
// MicromouseMenu.cpp
//
// Einstiegspunkt fuer die Menue-Implementierung.
//
// Die eigentlichen Abschnitte liegen als normale .cpp-Dateien in
// src/menu/. Gemeinsame interne Symbole sind in menu/MenuInternal.h
// deklariert.
//
// main.cpp ruft nur noch menuAppSetup() in setup() und
// menuAppLoop() in loop() auf.
// ============================================================

#include "MicromouseMenu.h"
#include "menu/MenuInternal.h"

// Die Menue-Implementierung liegt in normalen Translation Units unter
// src/menu/. Diese Datei bleibt als stabiler Einstiegspunkt fuer bestehende
// Includes und Build-Regeln erhalten.
