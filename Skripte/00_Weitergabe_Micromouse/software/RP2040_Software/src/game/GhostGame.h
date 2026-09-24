#ifndef GAME_GHOST_GAME_H
#define GAME_GHOST_GAME_H

#include <Arduino.h>

#include "GameState.h"

bool ghostGameInit(GAME_ROLES role);
bool ghostGameLoop();
void ghostGameLeave();
void ghostGameObservePacman(uint8_t senderId, uint8_t x, uint8_t y, int heading);
void ghostGameObservePeer(uint8_t senderId, uint8_t x, uint8_t y, int heading);
void ghostGameObserveReservation(uint8_t senderId,
                                 uint8_t x, uint8_t y,
                                 uint8_t sequence,
                                 bool committed);
void ghostGameObserveReservationRelease(uint8_t senderId, uint8_t sequence);
void ghostGameObserveReplan(uint8_t senderId,
                            uint8_t ownerId,
                            uint8_t sequence);
void ghostGameBackgroundUpdate();
void ghostGameBeginCapture();
bool ghostGameCaptureLoop();
bool ghostGameCaptureFailed();
const char *ghostGameCaptureLastError();
void ghostGamePrepareForHoming();
void ghostGameBeginReturnHome();
bool ghostGameReturnHomeLoop();
void ghostGameStartNextRound();
void ghostGameResetAfterTimeout();
void ghostGameManualHomeReset();
void ghostGameBroadcastPosition();

#endif
