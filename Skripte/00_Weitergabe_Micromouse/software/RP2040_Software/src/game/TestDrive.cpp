#include "../MicromouseMapping.h"
#include "TestDrive.h"
#include "GameState.h"
#include <COMMUNICATION_CODES.h>
#include <Pololu3piPlus2040.h>
#include "../UART_L3.h"
#include "GamePosition.h"

extern UART_L3 uartL3;
// Wie stark pro Encoder-Tick-Differenz gegengesteuert wird.
static const float STRAIGHT_KP = 0.1f;
static int   BASE_SPEED  = 60;
static bool    initialized = false;
static int32_t prevLeft  = 0;
static int32_t prevRight = 0;
static int s_posX = -1, s_posY = -1;
static int s_heading = 0; // 0 = Nord, 1 = Ost, 2 = Süd, 3 = West
static uint32_t s_lastPosTxMs = 0;

static void position(uint8_t length, uint8_t *data){
    if (length < 2 || data == nullptr)
        return;
    const int newX = data[0];
    const int newY = data[1];

    if(s_posX >=0){
        const int dx = newX - s_posX;
        const int dy = newY - s_posY;

        if(dx == 0 && dy == 1){s_heading = 0;}          // Norden
        else if(dx == 1 && dy == 0){s_heading = 1;}     // Osten
        else if(dx == 0 && dy == -1){s_heading = 2;}    // Süden
        else if(dx == -1 && dy == 0){s_heading = 3;}    // Westen
    } 
    s_posX = newX;
    s_posY = newY;
    
}

static void broadcastStartGame(){
    const uint8_t payload[] = {GC_START_GAME};
    for (uint8_t i = 0; i < 3; i++) {
        uartL3.sendControlData(255, sizeof(payload), payload);
        uartL3.update();
        delay(15);
    }
}

static void powerup(uint8_t length, uint8_t *data){
    Serial.println("Powerup wird ausgeführt");
    if (length < 1 || data == nullptr)
        return;

    if (data[0] == 1) {
        Serial.println("1 wurde festgestellt");
        BASE_SPEED = 60;
    }
    if (data[0] == 2){
        BASE_SPEED = 50;
    }
    uartL3.sendControllerRumble(20, 100, 100);
}

// Fährt geradeaus und korrigiert PWM-Differenz anhand der
// Encoder-Zaehlerdifferenz seit dem letzten Aufruf (P-Regler).
static void driveStraightCorrected()
{
    //static int32_t prevLeft  = 0;
    //static int32_t prevRight = 0;
    //static bool    initialized = false;

    const int32_t left  = Encoders::getCountsLeft();
    const int32_t right = Encoders::getCountsRight();

    if (!initialized) {
        prevLeft  = left;
        prevRight = right;
        initialized = true;
    }

    const int32_t deltaLeft  = left  - prevLeft;
    const int32_t deltaRight = right - prevRight;
    prevLeft  = left;
    prevRight = right;

    // Positiv = rechtes Rad dreht schneller -> Rechtsdrall -> rechts bremsen.
    const int32_t error = deltaRight - deltaLeft;
    const int correction = (int)(error * STRAIGHT_KP);

    int leftSpeed  = BASE_SPEED + correction;
    int rightSpeed = BASE_SPEED - correction;

    solveSetMotors(leftSpeed, rightSpeed);
}

bool testLoop(){
    uartL3.registerCbReceiveTestdriverData(powerup);
    uartL3.registerCbReceiveTestdriverDataPosition(position);

    broadcastStartGame(); // Geister starten

    while(true){
        uint16_t sw = gameGetCtrlSwitches();

        // Position regelmäßig senden(geister verwerfen sie nach 2 s)
        if (s_posX >= 0 && millis() - s_lastPosTxMs >= 400){
            s_lastPosTxMs = millis();
            uartL3.sendPosData(255, 
                gamePositionEncodeX((uint8_t)s_posX, s_heading),
                gamePositionEncodeY((uint8_t)s_posY, s_heading));
        }

        // Vorwärts fahren
        if ((sw & CSW_BUTTON_SHOULDER_R) && (sw & CSW_BUTTON_SHOULDER_L)){
            driveStraightCorrected();
        }
        // Rechts fahren
        else if (sw & CSW_BUTTON_SHOULDER_R){
            solveSetMotors(60,-60);
        }
        // Links fahren
        else if (sw & CSW_BUTTON_SHOULDER_L){
            solveSetMotors(-60,60);
        }
        else{
            stopMotors();
            initialized = false;
        }
    }
    

}
