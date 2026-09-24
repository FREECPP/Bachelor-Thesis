#ifndef MICROMOUSE_MAZE_H
#define MICROMOUSE_MAZE_H

#include "MicromouseMapping.h"
#include <Arduino.h>

const int NORTH = 0;
const int EAST  = 1;
const int SOUTH = 2;
const int WEST  = 3;

struct MazeCell
{
  bool wall[4];
  bool known[4];
};

extern MazeCell maze[MAZE_W][MAZE_H];

void mazeClear();
void mazeSetWall(int x, int y, int direction, bool isWall);
void mazeSetWallLocal(int x, int y, int direction, bool isWall);
bool mazeHasWall(int x, int y, int direction);
bool mazeHasPassage(int x, int y, int direction);

int leftOf(int direction);
int rightOf(int direction);
int oppositeOf(int direction);
int dxDir(int direction);
int dyDir(int direction);
bool inBounds(int x, int y);

#endif
