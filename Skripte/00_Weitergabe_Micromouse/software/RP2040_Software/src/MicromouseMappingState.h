#ifndef MICROMOUSE_MAPPING_STATE_H
#define MICROMOUSE_MAPPING_STATE_H

#include "MicromouseMaze.h"
#include <Arduino.h>

const int START_X = 0;
const int START_Y = 0;

const int START_HEADING = EAST;

extern int mouseX;
extern int mouseY;
extern int heading;
extern int stackTop;

extern bool mazePrinted;
extern bool mazeSaved;
extern bool mazeSaveAttempted;
extern bool mapReady;

extern bool compassAvailable;
extern bool startCompassValid;
extern int startWorldHeading;
extern float startCompassDegrees;

const char* dirName(int d);
int worldDxForDir(int d);
int worldDyForDir(int d);
int worldDirFromInternal(int internalDir);
int internalDirFromWorld(int worldDir);
void internalToWorld(int x, int y, int &worldX, int &worldY);

#endif
