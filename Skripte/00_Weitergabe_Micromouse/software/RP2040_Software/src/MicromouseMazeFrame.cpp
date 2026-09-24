#include "MicromouseMazeFrame.h"
#include "MicromouseMappingState.h"
#include <string.h>

static bool hasPassage(int x, int y, int direction)
{
  return mazeHasPassage(x, y, direction);
}

static uint8_t encodeNodeNibble(int x, int y)
{
  uint8_t nibble = 0;

  if (hasPassage(x, y, NORTH)) nibble |= 0x8;
  if (hasPassage(x, y, EAST))  nibble |= 0x4;
  if (hasPassage(x, y, WEST))  nibble |= 0x2;
  if (hasPassage(x, y, SOUTH)) nibble |= 0x1;

  return nibble;
}

bool mazeBuildCommFrame(MazeCommFrame &frame)
{
  memset(&frame, 0, sizeof(frame));

  frame.lengthX = MAZE_W;
  frame.lengthY = MAZE_H;
  frame.mazeLength = (MAZE_W * MAZE_H + 1) / 2;

  for (int row = 0; row < MAZE_H; row++)
  {
    for (int col = 0; col < MAZE_W; col++)
    {
      const int y = MAZE_H - 1 - row;
      const uint8_t nibble = encodeNodeNibble(col, y);
      int nodeIndex = row * MAZE_W + col;
      int byteIndex = nodeIndex / 2;

      if ((nodeIndex & 1) == 0)
        frame.maze[byteIndex] |= (nibble << 4);
      else
        frame.maze[byteIndex] |= nibble;
    }
  }

  for (int x = 0; x < MAZE_W; x++)
  {
    mazeSetWallLocal(x, 0, SOUTH, true);
    mazeSetWallLocal(x, MAZE_H - 1, NORTH, true);
  }
  for (int y = 0; y < MAZE_H; y++)
  {
    mazeSetWallLocal(0, y, WEST, true);
    mazeSetWallLocal(MAZE_W - 1, y, EAST, true);
  }

  return true;
}

bool mazeLoadCommFrameBytes(const uint8_t *data, size_t length)
{
  if (!data || length < 2) return false;
  if (data[0] != MAZE_W || data[1] != MAZE_H) return false;

  const size_t mazeLength = (MAZE_W * MAZE_H + 1) / 2;
  if (length != 2 + mazeLength) return false;

  mazeClear();

  for (int row = 0; row < MAZE_H; row++)
  {
    for (int col = 0; col < MAZE_W; col++)
    {
      const int nodeIndex = row * MAZE_W + col;
      const uint8_t packed = data[2 + nodeIndex / 2];
      const uint8_t nibble = (nodeIndex & 1) ? (packed & 0x0F) : (packed >> 4);
      const int y = MAZE_H - 1 - row;

      mazeSetWall(col, y, NORTH, (nibble & 0x8) == 0);
      mazeSetWall(col, y, EAST,  (nibble & 0x4) == 0);
      mazeSetWall(col, y, WEST,  (nibble & 0x2) == 0);
      mazeSetWall(col, y, SOUTH, (nibble & 0x1) == 0);
    }
  }

  return true;
}

size_t mazeCopyCommFrameBytes(const MazeCommFrame &frame, uint8_t *out, size_t outCapacity)
{
  size_t frameLength = 2 + frame.mazeLength;
  if (!out || outCapacity < frameLength)
    return 0;

  out[0] = frame.lengthX;
  out[1] = frame.lengthY;
  memcpy(out + 2, frame.maze, frame.mazeLength);
  return frameLength;
}

void mazePrintCommFrameToSerial(const MazeCommFrame &frame)
{
  Serial.println();
  Serial.println("===== MAZE COMM FRAME START =====");
  Serial.print("length_x=");
  Serial.print(frame.lengthX);
  Serial.print(",length_y=");
  Serial.print(frame.lengthY);
  Serial.print(",maze_bytes=");
  Serial.println(frame.mazeLength);

  Serial.print("bytes=");
  Serial.print("0x");
  if (frame.lengthX < 16) Serial.print("0");
  Serial.print(frame.lengthX, HEX);
  Serial.print(" 0x");
  if (frame.lengthY < 16) Serial.print("0");
  Serial.print(frame.lengthY, HEX);

  for (uint16_t i = 0; i < frame.mazeLength; i++)
  {
    Serial.print(" 0x");
    if (frame.maze[i] < 16) Serial.print("0");
    Serial.print(frame.maze[i], HEX);
  }
  Serial.println();
  Serial.println("===== MAZE COMM FRAME END =====");
  Serial.println();
}
