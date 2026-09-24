#include "GhostTargets.h"

#include "../MicromouseMappingState.h"

static bool s_hasLastPacmanPos = false;
static uint8_t s_lastPacmanX = 0;
static uint8_t s_lastPacmanY = 0;
static int s_lastPacmanDir = -1;
static uint8_t s_dirConfidence = 0;

static int directionFromDelta(int dx, int dy)
{
  if (abs(dx) >= abs(dy) && dx > 0) return EAST;
  if (abs(dx) >= abs(dy) && dx < 0) return WEST;
  if (dy > 0) return NORTH;
  if (dy < 0) return SOUTH;
  return -1;
}

static bool usableTargetCell(int x, int y)
{
  return inBounds(x, y);
}

static bool knownPassage(int x, int y, int dir)
{
  const int nx = x + dxDir(dir);
  const int ny = y + dyDir(dir);
  return inBounds(x, y) &&
         inBounds(nx, ny) &&
         maze[x][y].known[dir] &&
         !maze[x][y].wall[dir] &&
         maze[nx][ny].known[oppositeOf(dir)] &&
         !maze[nx][ny].wall[oppositeOf(dir)];
}

static bool canProjectFrom(int x, int y, int dir)
{
  return knownPassage(x, y, dir);
}

static int manhattan(int ax, int ay, int bx, int by)
{
  int dx = ax - bx;
  int dy = ay - by;
  if (dx < 0) dx = -dx;
  if (dy < 0) dy = -dy;
  return dx + dy;
}

static bool reachableFrom(int startX, int startY, int targetX, int targetY)
{
  if (!inBounds(startX, startY) || !inBounds(targetX, targetY))
    return false;
  if (startX == targetX && startY == targetY)
    return true;

  bool visited[MAZE_W][MAZE_H] = {{false}};
  int queueX[MAZE_W * MAZE_H];
  int queueY[MAZE_W * MAZE_H];
  int readIndex = 0;
  int writeIndex = 0;

  visited[startX][startY] = true;
  queueX[writeIndex] = startX;
  queueY[writeIndex] = startY;
  writeIndex++;

  while (readIndex < writeIndex)
  {
    const int x = queueX[readIndex];
    const int y = queueY[readIndex];
    readIndex++;

    for (int dir = NORTH; dir <= WEST; dir++)
    {
      if (!knownPassage(x, y, dir))
        continue;

      const int nx = x + dxDir(dir);
      const int ny = y + dyDir(dir);
      if (visited[nx][ny])
        continue;
      if (nx == targetX && ny == targetY)
        return true;

      visited[nx][ny] = true;
      queueX[writeIndex] = nx;
      queueY[writeIndex] = ny;
      writeIndex++;
    }
  }

  return false;
}

static bool chooseBestNearbyTarget(int predictedX, int predictedY,
                                   int ownX, int ownY,
                                   int &targetX, int &targetY)
{
  int bestX = targetX;
  int bestY = targetY;
  int bestScore = 10000;
  bool found = false;

  for (int y = 0; y < MAZE_H; y++)
  {
    for (int x = 0; x < MAZE_W; x++)
    {
      if (!usableTargetCell(x, y))
        continue;
      if (!reachableFrom(ownX, ownY, x, y))
        continue;

      const int predictionDist = manhattan(x, y, predictedX, predictedY);
      if (predictionDist > 2)
        continue;

      const int score = predictionDist * 4 + manhattan(ownX, ownY, x, y);
      if (!found || score < bestScore)
      {
        found = true;
        bestScore = score;
        bestX = x;
        bestY = y;
      }
    }
  }

  if (!found)
    return false;

  targetX = bestX;
  targetY = bestY;
  return true;
}

static bool chooseClosestReachableTarget(int wantedX, int wantedY,
                                         int ownX, int ownY,
                                         int &targetX, int &targetY)
{
  int bestScore = 10000;
  bool found = false;

  for (int y = 0; y < MAZE_H; y++)
  {
    for (int x = 0; x < MAZE_W; x++)
    {
      if (!reachableFrom(ownX, ownY, x, y))
        continue;

      const int score =
        manhattan(x, y, wantedX, wantedY) * (MAZE_W + MAZE_H) +
        manhattan(ownX, ownY, x, y);
      if (!found || score < bestScore)
      {
        found = true;
        bestScore = score;
        targetX = x;
        targetY = y;
      }
    }
  }

  return found;
}

void pinkGhostReset()
{
  s_hasLastPacmanPos = false;
  s_lastPacmanX = 0;
  s_lastPacmanY = 0;
  s_lastPacmanDir = -1;
  s_dirConfidence = 0;
}

void pinkGhostObservePacman(uint8_t x, uint8_t y, int heading)
{
  int observedDir = -1;

  if (s_hasLastPacmanPos && (x != s_lastPacmanX || y != s_lastPacmanY))
  {
    observedDir = directionFromDelta((int)x - (int)s_lastPacmanX,
                                     (int)y - (int)s_lastPacmanY);
  }

  if (heading >= 0 && heading < 4)
    observedDir = heading;

  if (observedDir >= 0)
  {
    if (observedDir == s_lastPacmanDir)
    {
      if (s_dirConfidence < 8)
        s_dirConfidence++;
    }
    else
    {
      s_lastPacmanDir = observedDir;
      s_dirConfidence = 1;
    }
  }

  s_lastPacmanX = x;
  s_lastPacmanY = y;
  s_hasLastPacmanPos = true;
}

void pinkGhostChooseTarget(uint8_t pacmanX, uint8_t pacmanY,
                           int ownX, int ownY,
                           int &targetX, int &targetY)
{
  targetX = pacmanX;
  targetY = pacmanY;

  if (s_lastPacmanDir < 0)
    return;

  const int distToPacman = manhattan(ownX, ownY, pacmanX, pacmanY);
  int wantedSteps = 2;
  if (distToPacman >= 6) wantedSteps = 4;
  else if (distToPacman >= 3) wantedSteps = 3;
  if (s_dirConfidence >= 3 && wantedSteps < 4)
    wantedSteps++;

  int projectedX = pacmanX;
  int projectedY = pacmanY;
  int bestAheadX = pacmanX;
  int bestAheadY = pacmanY;
  bool foundAhead = false;

  for (int step = 1; step <= wantedSteps; step++)
  {
    if (!canProjectFrom(projectedX, projectedY, s_lastPacmanDir))
      break;

    projectedX += dxDir(s_lastPacmanDir);
    projectedY += dyDir(s_lastPacmanDir);
    bestAheadX = projectedX;
    bestAheadY = projectedY;
    foundAhead = true;
  }

  if (foundAhead)
  {
    targetX = bestAheadX;
    targetY = bestAheadY;
    return;
  }

  const int predictedX = (int)pacmanX + dxDir(s_lastPacmanDir) * wantedSteps;
  const int predictedY = (int)pacmanY + dyDir(s_lastPacmanDir) * wantedSteps;
  chooseBestNearbyTarget(predictedX, predictedY, ownX, ownY, targetX, targetY);
}

void ghostTargetsReset(GAME_ROLES role)
{
  if (role == GAME_ROLES::ROLE_PINK)
    pinkGhostReset();
}

void ghostTargetsObservePacman(GAME_ROLES role, uint8_t x, uint8_t y, int heading)
{
  if (role == GAME_ROLES::ROLE_PINK)
    pinkGhostObservePacman(x, y, heading);
}

void ghostTargetsChooseTarget(GAME_ROLES role,
                              uint8_t pacmanX, uint8_t pacmanY,
                              int ownX, int ownY,
                              int &targetX, int &targetY)
{
  targetX = pacmanX;
  targetY = pacmanY;

  if (role == GAME_ROLES::ROLE_PINK)
    pinkGhostChooseTarget(pacmanX, pacmanY, ownX, ownY, targetX, targetY);

  if (!reachableFrom(ownX, ownY, targetX, targetY))
    chooseClosestReachableTarget(targetX, targetY,
                                 ownX, ownY, targetX, targetY);
}
