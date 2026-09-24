#ifndef GAME_POSITION_H
#define GAME_POSITION_H

#include <Arduino.h>

// The ESP transports both bytes unchanged. Store the two heading bits in the
// MSBs so the existing position message needs no protocol extension.
constexpr uint8_t GAME_POS_COORD_MASK = 0x0F;
constexpr uint8_t GAME_POS_HEADING_BIT = 0x80;

inline uint8_t gamePositionEncodeX(uint8_t x, int heading)
{
  return (uint8_t)((x & GAME_POS_COORD_MASK) |
                   ((((uint8_t)heading) & 0x01u) << 7));
}

inline uint8_t gamePositionEncodeY(uint8_t y, int heading)
{
  return (uint8_t)((y & GAME_POS_COORD_MASK) |
                   (((((uint8_t)heading) >> 1u) & 0x01u) << 7));
}

inline uint8_t gamePositionDecodeX(uint8_t encodedX)
{
  return encodedX & GAME_POS_COORD_MASK;
}

inline uint8_t gamePositionDecodeY(uint8_t encodedY)
{
  return encodedY & GAME_POS_COORD_MASK;
}

inline int gamePositionDecodeHeading(uint8_t encodedX, uint8_t encodedY)
{
  return ((encodedX & GAME_POS_HEADING_BIT) ? 1 : 0) |
         ((encodedY & GAME_POS_HEADING_BIT) ? 2 : 0);
}

#endif
