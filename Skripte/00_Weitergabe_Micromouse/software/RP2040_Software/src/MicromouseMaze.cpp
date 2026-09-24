#include "MicromouseMaze.h"

MazeCell maze[MAZE_W][MAZE_H];

int leftOf(int direction)
{
  return (direction + 3) % 4;
}

int rightOf(int direction)
{
  return (direction + 1) % 4;
}

int oppositeOf(int direction)
{
  return (direction + 2) % 4;
}

int dxDir(int direction)
{
  if (direction == EAST) return 1;
  if (direction == WEST) return -1;
  return 0;
}

int dyDir(int direction)
{
  if (direction == NORTH) return 1;
  if (direction == SOUTH) return -1;
  return 0;
}

bool inBounds(int x, int y)
{
  return x >= 0 && x < MAZE_W && y >= 0 && y < MAZE_H;
}

void mazeSetWallLocal(int x, int y, int direction, bool isWall)
{
  if (!inBounds(x, y) || direction < 0 || direction > 3) return;
  maze[x][y].wall[direction] = isWall;
  maze[x][y].known[direction] = true;
}

void mazeSetWall(int x, int y, int direction, bool isWall)
{
  if (!inBounds(x, y) || direction < 0 || direction > 3) return;

  mazeSetWallLocal(x, y, direction, isWall);

  const int nx = x + dxDir(direction);
  const int ny = y + dyDir(direction);
  if (inBounds(nx, ny))
    mazeSetWallLocal(nx, ny, oppositeOf(direction), isWall);
}

bool mazeHasWall(int x, int y, int direction)
{
  return inBounds(x, y) &&
         maze[x][y].known[direction] &&
         maze[x][y].wall[direction];
}

bool mazeHasPassage(int x, int y, int direction)
{
  if (!inBounds(x, y) || !maze[x][y].known[direction] || maze[x][y].wall[direction])
    return false;

  return inBounds(x + dxDir(direction), y + dyDir(direction));
}

void mazeClear()
{
  for (int x = 0; x < MAZE_W; x++)
  {
    for (int y = 0; y < MAZE_H; y++)
    {
      for (int direction = 0; direction < 4; direction++)
      {
        maze[x][y].wall[direction] = true;
        maze[x][y].known[direction] = false;
      }
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
}
