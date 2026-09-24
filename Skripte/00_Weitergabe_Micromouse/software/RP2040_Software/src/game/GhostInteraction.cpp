#include "GhostInteraction.h"

#include "GameState.h"

#include "../MicromouseMappingState.h"
#include "../MicromouseSolving.h"

struct GhostPeer
{
  bool positionKnown;
  uint8_t id;
  uint8_t x;
  uint8_t y;
  int heading;
  uint32_t positionSeenMs;

  bool reservationKnown;
  uint8_t reservedX;
  uint8_t reservedY;
  bool reservationSequenceKnown;
  uint8_t reservationSequence;
  bool reservationCommitted;
  uint32_t reservationSeenMs;
};

static constexpr uint32_t PEER_POSITION_TIMEOUT_MS = 2000;
static constexpr uint32_t PEER_RESERVATION_TIMEOUT_MS = 900;
static GhostPeer s_peers[5];
static uint8_t s_ownRobotId = 0;

static uint8_t localId(uint8_t id)
{
  return id & 0x1F;
}

static bool robotHasPriority(uint8_t candidateId, uint8_t otherId)
{
  const uint8_t candidateLocalId = localId(candidateId);
  const uint8_t otherLocalId = localId(otherId);
  if (candidateLocalId != otherLocalId)
    return candidateLocalId < otherLocalId;

  // Rollenbits liefern auch bei versehentlich gleichen lokalen IDs eine
  // eindeutige Reihenfolge und verhindern gegenseitiges Warten.
  return candidateId < otherId;
}

static int peerIndex(uint8_t senderId)
{
  const GAME_ROLES role = getRoleFromId(senderId);
  if (role == GAME_ROLES::ROLE_PACMAN)
    return 4;
  if (role < GAME_ROLES::ROLE_RED || role > GAME_ROLES::ROLE_BROWN)
    return -1;
  return (int)role - (int)GAME_ROLES::ROLE_RED;
}

static bool freshPosition(const GhostPeer &peer, uint32_t nowMs)
{
  return peer.positionKnown &&
         (uint32_t)(nowMs - peer.positionSeenMs) <= PEER_POSITION_TIMEOUT_MS;
}

static bool freshReservation(const GhostPeer &peer, uint32_t nowMs)
{
  return peer.reservationKnown &&
         (uint32_t)(nowMs - peer.reservationSeenMs) <=
           PEER_RESERVATION_TIMEOUT_MS;
}

static bool peerReservationHasPriority(const GhostPeer &peer)
{
  return robotHasPriority(peer.id, s_ownRobotId);
}

static bool sequenceIsNewer(uint8_t candidate, uint8_t current)
{
  return (int8_t)(candidate - current) > 0;
}

void ghostInteractionReset(uint8_t ownRobotId)
{
  s_ownRobotId = ownRobotId;
  for (uint8_t i = 0; i < 5; i++)
    s_peers[i] = {};
}

void ghostInteractionObservePosition(uint8_t senderId,
                                     uint8_t x, uint8_t y, int heading,
                                     uint32_t nowMs)
{
  if (senderId == s_ownRobotId || !inBounds(x, y))
    return;

  const int index = peerIndex(senderId);
  if (index < 0)
    return;

  GhostPeer &peer = s_peers[index];
  peer.positionKnown = true;
  peer.id = senderId;
  peer.x = x;
  peer.y = y;
  peer.heading = heading;
  peer.positionSeenMs = nowMs;

  if (peer.reservationKnown &&
      peer.reservedX == x && peer.reservedY == y)
  {
    peer.reservationKnown = false;
  }
}

bool ghostInteractionObserveReservation(uint8_t senderId,
                                        uint8_t x, uint8_t y,
                                        uint8_t sequence,
                                        bool committed,
                                        uint32_t nowMs)
{
  if (senderId == s_ownRobotId || !inBounds(x, y))
    return false;

  const int index = peerIndex(senderId);
  if (index < 0)
    return false;

  GhostPeer &peer = s_peers[index];
  if (peer.reservationSequenceKnown)
  {
    if (sequence == peer.reservationSequence)
    {
      if (!peer.reservationKnown)
        return false;
    }
    else if (!sequenceIsNewer(sequence, peer.reservationSequence))
    {
      return false;
    }
  }

  peer.id = senderId;
  peer.reservationKnown = true;
  peer.reservationSequenceKnown = true;
  peer.reservedX = x;
  peer.reservedY = y;
  peer.reservationSequence = sequence;
  peer.reservationCommitted = committed;
  peer.reservationSeenMs = nowMs;
  return true;
}

void ghostInteractionObserveRelease(uint8_t senderId,
                                    uint8_t sequence,
                                    uint32_t nowMs)
{
  (void)nowMs;
  if (senderId == s_ownRobotId)
    return;

  const int index = peerIndex(senderId);
  if (index < 0)
    return;

  GhostPeer &peer = s_peers[index];
  if (!peer.reservationSequenceKnown ||
      sequenceIsNewer(sequence, peer.reservationSequence))
  {
    peer.id = senderId;
    peer.reservationSequenceKnown = true;
    peer.reservationSequence = sequence;
    peer.reservationKnown = false;
    return;
  }

  if (peer.reservationSequence == sequence)
    peer.reservationKnown = false;
}

void ghostInteractionApplySolverBlocks(uint32_t nowMs)
{
  solveClearDynamicObstacles();

  for (uint8_t i = 0; i < 5; i++)
  {
    const GhostPeer &peer = s_peers[i];
    if (freshPosition(peer, nowMs))
      solveBlockCell(peer.x, peer.y);

    if (freshReservation(peer, nowMs) &&
        (peer.reservationCommitted ||
         peerReservationHasPriority(peer)))
    {
      solveBlockCell(peer.reservedX, peer.reservedY);
    }
  }
}

bool ghostInteractionCellOccupied(int x, int y, uint32_t nowMs)
{
  for (uint8_t i = 0; i < 5; i++)
  {
    const GhostPeer &peer = s_peers[i];
    if (freshPosition(peer, nowMs) && peer.x == x && peer.y == y)
      return true;
  }
  return false;
}

// True, wenn irgendein Peer kuerzlich eine Position ODER Reservierung gemeldet
// hat. Genutzt fuer den Einzel-Geist-Schnellpfad: ohne aktiven Peer gibt es
// keine Kollisionsgegner → das Veto-Fenster an Entscheidungszellen darf
// uebersprungen werden. Sobald ein zweiter Geist aktiv ist, greift wieder das
// volle Veto.
bool ghostInteractionAnyPeerActive(uint32_t nowMs)
{
  for (uint8_t i = 0; i < 5; i++)
  {
    const GhostPeer &peer = s_peers[i];
    if (freshPosition(peer, nowMs) || freshReservation(peer, nowMs))
      return true;
  }
  return false;
}

bool ghostInteractionShouldYieldCell(int x, int y, uint32_t nowMs)
{
  for (uint8_t i = 0; i < 5; i++)
  {
    const GhostPeer &peer = s_peers[i];
    if (!freshReservation(peer, nowMs))
      continue;
    if (peer.reservedX != x || peer.reservedY != y)
      continue;

    if (peer.reservationCommitted ||
        robotHasPriority(peer.id, s_ownRobotId))
      return true;
  }
  return false;
}

bool ghostInteractionOwnHasPriority(uint8_t peerId)
{
  return robotHasPriority(s_ownRobotId, peerId);
}
