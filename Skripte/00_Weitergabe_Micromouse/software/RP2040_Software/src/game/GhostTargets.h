#ifndef GAME_GHOST_TARGETS_H
#define GAME_GHOST_TARGETS_H

#include <Arduino.h>

#include "GameState.h"

void pinkGhostReset();
void pinkGhostObservePacman(uint8_t x, uint8_t y, int heading = -1);
void pinkGhostChooseTarget(uint8_t pacmanX, uint8_t pacmanY,
                           int ownX, int ownY,
                           int &targetX, int &targetY);

void ghostTargetsReset(GAME_ROLES role);
void ghostTargetsObservePacman(GAME_ROLES role, uint8_t x, uint8_t y, int heading = -1);
void ghostTargetsChooseTarget(GAME_ROLES role,
                              uint8_t pacmanX, uint8_t pacmanY,
                              int ownX, int ownY,
                              int &targetX, int &targetY);

#endif
