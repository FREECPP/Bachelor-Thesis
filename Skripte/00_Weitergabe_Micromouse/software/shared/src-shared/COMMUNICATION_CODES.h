#ifndef COMMUNICATION_CODES
#define COMMUNICATION_CODES

#include <Arduino.h>


// Global Op-Codes
const uint8_t OPC_MAZE_DATA = 1;
const uint8_t OPC_POS_DATA = 2;
const uint8_t OPC_CONNECT_NOW = 3;
const uint8_t OPC_CONNECT_NOW_STATE = 4;
const uint8_t OPC_CONNECT_BT_CONTROLLER = 5;
const uint8_t OPC_CONNECT_BT_CONTROLLER_STATE = 6;
const uint8_t OPC_CONTROLLER_DATA = 7;
const uint8_t OPC_ERROR_NOW = 8;
const uint8_t OPC_SET_ID = 9;
const uint8_t OPC_GET_ID = 10;
const uint8_t OPC_SUBSCR_BT_CONTROLLER_DATA = 11;
const uint8_t OPC_UNSUBSCR_BT_CONTROLLER_DATA = 12;
const uint8_t OPC_BT_CONTROLLER_ACTION = 13;
const uint8_t OPC_GAME_CONTROL = 14;        // Opcode, TargetId, GameControl Id, optional other stuff
const uint8_t OPC_TESTDRIVER_CONTROL = 15;


// Layer 3
const uint8_t CONN_SUCCESS = 0;
const uint8_t CONN_FAIL = 1;
const uint8_t NOW_ERR_WIRELESS = 2;
const uint8_t NOW_NO_ID_DECODE = 3;         // TargetId can't be decoded

// Game controll
const uint8_t GC_START_GAME = 0;
const uint8_t GC_STOP_GAME = 1;
const uint8_t GC_POWER_MODE = 2;
const uint8_t GC_NORMAL_MODE = 3;
const uint8_t GC_CATCHED_PACMAN = 4;
const uint8_t GC_CATCHED_GHOST = 5;
const uint8_t GC_RESET_POSITIONS = 6;
const uint8_t GC_CURRENT_SCORE = 7;         // Expects one byte of current Items
const uint8_t GC_PACMAN_WINS = 8;
const uint8_t GS_TIMER_TIMEOUT = 9;
const uint8_t GC_TIMER_TIMEOUT = GS_TIMER_TIMEOUT;  // Compatibility alias
const uint8_t GC_RETURNING = 11;


// Controller Stuff
const uint8_t CONTROLLER_STICK_L = 0;       // Ascending numbers; threshold values are stored in array with index of this values
const uint8_t CONTROLLER_STICK_R = 1;
const uint8_t CONTROLLER_SWITCHES = 2;
const uint8_t CONTROLLER_TRIGGER_L = 3;
const uint8_t CONTROLLER_TRIGGER_R = 4;

const uint8_t CONTROLLER_ACTION_RUMBLE = 100;       // Additional bytes: Duration ms, weakMagnitude, strongMagnitude

/* Switch codes; bit set is pressed*/
const uint16_t CSW_BUTTON_A             = 0b0000000000000001;
const uint16_t CSW_BUTTON_B             = 0b0000000000000010;
const uint16_t CSW_BUTTON_X             = 0b0000000000000100;
const uint16_t CSW_BUTTON_Y             = 0b0000000000001000;
const uint16_t CSW_BUTTON_SHOULDER_L    = 0b0000000000010000;
const uint16_t CSW_BUTTON_SHOULDER_R    = 0b0000000000100000;
const uint16_t CSW_THUMB_L              = 0b0000000001000000;
const uint16_t CSW_THUMB_R              = 0b0000000010000000;
const uint16_t CSW_BUTTON_TRIGGER_L     = 0b0000000100000000;
const uint16_t CSW_BUTTON_TRIGGER_R     = 0b0000001000000000;
const uint16_t CSW_BUTTON_HOME          = 0b0000010000000000;       // aka. X Button
const uint16_t CSW_BUTTON_BACK          = 0b0000100000000000;
const uint16_t CSW_D_PAD_UP             = 0b0001000000000000;
const uint16_t CSW_D_PAD_DOWN           = 0b0010000000000000;
const uint16_t CSW_D_PAD_RIGHT          = 0b0100000000000000;
const uint16_t CSW_D_PAD_LEFT           = 0b1000000000000000;


// ESP NOW Codes
const uint8_t NOW_CODE_BROAD_CONNECT = 42;
const uint8_t NOW_CODE_UNI_CONNECT = 43;


#endif  // COMMUNICATION_CODES
