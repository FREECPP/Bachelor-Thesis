#include "MicromouseMapping.h"
#include "MicromouseMappingState.h"

Graph mappedGraph;

void exportMazeToSerial()
{
  Serial.println();
  Serial.println("===== MAZE EXPORT START =====");
  Serial.print("compassAvailable=");
  Serial.print(compassAvailable ? 1 : 0);
  Serial.print(",startCompassValid=");
  Serial.print(startCompassValid ? 1 : 0);
  Serial.print(",startCompassDegrees=");
  Serial.print(startCompassDegrees, 1);
  Serial.print(",internalStartHeading=");
  Serial.print(dirName(START_HEADING));
  Serial.print(",worldStartHeading=");
  Serial.println(startCompassValid ? dirName(startWorldHeading) : "REL");
  Serial.println("internalX,internalY,worldX,worldY,knownRealN,knownRealE,knownRealS,knownRealW,wallRealN,wallRealE,wallRealS,wallRealW");

  for (int y = 0; y < MAZE_H; y++)
  {
    for (int x = 0; x < MAZE_W; x++)
    {
      int worldX;
      int worldY;
      internalToWorld(x, y, worldX, worldY);

      Serial.print(x);
      Serial.print(",");
      Serial.print(y);
      Serial.print(",");
      Serial.print(worldX);
      Serial.print(",");
      Serial.print(worldY);
      Serial.print(",");

      Serial.print(maze[x][y].known[internalDirFromWorld(NORTH)] ? 1 : 0);
      Serial.print(",");
      Serial.print(maze[x][y].known[internalDirFromWorld(EAST)] ? 1 : 0);
      Serial.print(",");
      Serial.print(maze[x][y].known[internalDirFromWorld(SOUTH)] ? 1 : 0);
      Serial.print(",");
      Serial.print(maze[x][y].known[internalDirFromWorld(WEST)] ? 1 : 0);
      Serial.print(",");

      Serial.print(maze[x][y].wall[internalDirFromWorld(NORTH)] ? 1 : 0);
      Serial.print(",");
      Serial.print(maze[x][y].wall[internalDirFromWorld(EAST)] ? 1 : 0);
      Serial.print(",");
      Serial.print(maze[x][y].wall[internalDirFromWorld(SOUTH)] ? 1 : 0);
      Serial.print(",");
      Serial.println(maze[x][y].wall[internalDirFromWorld(WEST)] ? 1 : 0);
    }
  }

  Serial.println("===== MAZE EXPORT END =====");
  Serial.println();
}

void exportGraphToSerial()
{
  Serial.println();
  Serial.println("===== GRAPH EXPORT START =====");
  Serial.println("fromInternalX,fromInternalY,toInternalX,toInternalY,fromWorldX,fromWorldY,toWorldX,toWorldY");

  for (int y = 0; y < MAZE_H; y++)
  {
    for (int x = 0; x < MAZE_W; x++)
    {
      if (maze[x][y].known[EAST] && !maze[x][y].wall[EAST])
      {
        int nx = x + 1;
        int ny = y;

        if (inBounds(nx, ny))
        {
          int fromWorldX;
          int fromWorldY;
          int toWorldX;
          int toWorldY;
          internalToWorld(x, y, fromWorldX, fromWorldY);
          internalToWorld(nx, ny, toWorldX, toWorldY);

          Serial.print(x);
          Serial.print(",");
          Serial.print(y);
          Serial.print(",");
          Serial.print(nx);
          Serial.print(",");
          Serial.print(ny);
          Serial.print(",");
          Serial.print(fromWorldX);
          Serial.print(",");
          Serial.print(fromWorldY);
          Serial.print(",");
          Serial.print(toWorldX);
          Serial.print(",");
          Serial.println(toWorldY);
        }
      }

      if (maze[x][y].known[NORTH] && !maze[x][y].wall[NORTH])
      {
        int nx = x;
        int ny = y + 1;

        if (inBounds(nx, ny))
        {
          int fromWorldX;
          int fromWorldY;
          int toWorldX;
          int toWorldY;
          internalToWorld(x, y, fromWorldX, fromWorldY);
          internalToWorld(nx, ny, toWorldX, toWorldY);

          Serial.print(x);
          Serial.print(",");
          Serial.print(y);
          Serial.print(",");
          Serial.print(nx);
          Serial.print(",");
          Serial.print(ny);
          Serial.print(",");
          Serial.print(fromWorldX);
          Serial.print(",");
          Serial.print(fromWorldY);
          Serial.print(",");
          Serial.print(toWorldX);
          Serial.print(",");
          Serial.println(toWorldY);
        }
      }
    }
  }

  Serial.println("===== GRAPH EXPORT END =====");
  Serial.println();
}

void buildGraphFromMaze()
{
  mappedGraph.edgeCount = 0;

  for (int y = 0; y < MAZE_H; y++)
  {
    for (int x = 0; x < MAZE_W; x++)
    {
      if (maze[x][y].known[EAST] && !maze[x][y].wall[EAST])
      {
        int nx = x + 1;
        int ny = y;

        if (inBounds(nx, ny))
          mappedGraph.edges[mappedGraph.edgeCount++] = {x, y, nx, ny};
      }

      if (maze[x][y].known[NORTH] && !maze[x][y].wall[NORTH])
      {
        int nx = x;
        int ny = y + 1;

        if (inBounds(nx, ny))
          mappedGraph.edges[mappedGraph.edgeCount++] = {x, y, nx, ny};
      }
    }
  }
}
