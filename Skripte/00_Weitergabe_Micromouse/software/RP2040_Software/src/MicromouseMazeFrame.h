#ifndef MICROMOUSE_MAZE_FRAME_H
#define MICROMOUSE_MAZE_FRAME_H

#include "MicromouseMapping.h"
#include <Arduino.h>
#include <stddef.h>

const uint16_t MAZE_FRAME_MAX_NODES = MAZE_W * MAZE_H;
const uint16_t MAZE_FRAME_MAX_MAZE_BYTES = (MAZE_FRAME_MAX_NODES + 1) / 2;
const uint16_t MAZE_FRAME_MAX_BYTES = 2 + MAZE_FRAME_MAX_MAZE_BYTES;

struct MazeCommFrame
{
  uint8_t lengthX;
  uint8_t lengthY;
  uint8_t maze[MAZE_FRAME_MAX_MAZE_BYTES];
  uint16_t mazeLength;
};

bool mazeBuildCommFrame(MazeCommFrame &frame);
size_t mazeCopyCommFrameBytes(const MazeCommFrame &frame, uint8_t *out, size_t outCapacity);
bool mazeLoadCommFrameBytes(const uint8_t *data, size_t length);
void mazePrintCommFrameToSerial(const MazeCommFrame &frame);

#endif
