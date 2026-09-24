#ifndef GAME_GHOST_INTERACTION_H
#define GAME_GHOST_INTERACTION_H

#include <Arduino.h>

// 0..8 are assigned in COMMUNICATION_CODES.h.
constexpr uint8_t GC_GHOST_RESERVE_CELL = 16;
constexpr uint8_t GC_GHOST_RELEASE_CELL = 17;
constexpr uint8_t GC_GHOST_REPLAN = 18;
constexpr uint8_t GC_RETURN_GRANT = 19;
constexpr uint8_t GC_AT_HOME = 20;
constexpr uint8_t GC_ROUND_START = 21;
constexpr uint8_t GC_GAME_OVER = 22;
constexpr uint8_t GC_HOMING_START = 23;
constexpr uint8_t GHOST_RESERVATION_PENDING = 0;
constexpr uint8_t GHOST_RESERVATION_COMMITTED = 1;

void ghostInteractionReset(uint8_t ownRobotId);
void ghostInteractionObservePosition(uint8_t senderId,
                                     uint8_t x, uint8_t y, int heading,
                                     uint32_t nowMs);
bool ghostInteractionObserveReservation(uint8_t senderId,
                                        uint8_t x, uint8_t y,
                                        uint8_t sequence,
                                        bool committed,
                                        uint32_t nowMs);
void ghostInteractionObserveRelease(uint8_t senderId,
                                    uint8_t sequence,
                                    uint32_t nowMs);

void ghostInteractionApplySolverBlocks(uint32_t nowMs);
bool ghostInteractionCellOccupied(int x, int y, uint32_t nowMs);
// True, wenn irgendein Peer kuerzlich Position/Reservierung gemeldet hat
// (Einzel-Geist-Schnellpfad: ohne aktiven Peer kein Veto noetig).
bool ghostInteractionAnyPeerActive(uint32_t nowMs);
bool ghostInteractionShouldYieldCell(int x, int y, uint32_t nowMs);
bool ghostInteractionOwnHasPriority(uint8_t peerId);

#endif
