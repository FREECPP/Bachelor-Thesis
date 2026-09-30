//
// Created by robo on 05.06.26.
//

#ifndef RP2040_SOFTWARE_MICROMOUSEGAMESTATE_H
#define RP2040_SOFTWARE_MICROMOUSEGAMESTATE_H
#include <Arduino.h>

#define ROBOT_ID_BITS_PACMAN    0b00000000
#define ROBOT_ID_BITS_RED       0b00100000
#define ROBOT_ID_BITS_PINK      0b01000000
#define ROBOT_ID_BITS_CYAN      0b01100000
#define ROBOT_ID_BITS_BROWN     0b10000000
#define ROBOT_ID_BITS_TESTDRIVE 0b11000000

enum GAME_ROLES { // Hier werden die Rollen festgelegt
    ROLE_PACMAN,
    ROLE_RED,
    ROLE_PINK,
    ROLE_CYAN,
    ROLE_BROWN,
    TESTDRIVE
};

enum GAME_STATE {
    GS_IDLE_AT_HOME,
    GS_LOBBY,            // Pacman: "Druecke Xbox-Taste zum Starten" – vorfuehrsicher
    GS_RUNNING,
    GS_ERROR,
    GS_CATCHT,
    GS_RETREATING_TO_CELL,
    GS_WAITING_TO_RETURN,
    GS_RETURNING_TO_HOME,
    GS_MANUAL_HOME_CONFIRM
};

extern GAME_STATE currentGameState;

// Statemachine fuer den Spielmodus.
// gameActive() wird beim Wechsel RP_STATE_MENU -> RP_STATE_GAME aufgerufen.
void gameActive();
void gameLoop();
void gameLeave();

bool gameGetPressedA();
bool gameGetPressedB();
bool gameGetPressedC();
uint16_t gameGetCtrlSwitches();
void gameReportCapture(uint8_t ghostId, uint8_t captureX, uint8_t captureY);
void gameReportPacmanWin();

GAME_ROLES getRoleFromId(uint8_t id);
void setOwnIdRole(GAME_ROLES newRole, uint8_t id);

#endif //RP2040_SOFTWARE_MICROMOUSEGAMESTATE_H
