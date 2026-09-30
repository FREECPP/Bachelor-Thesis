#include "../MicromouseMapping.h"
#include "TestDrive.h"
#include "GameState.h"
#include <COMMUNICATION_CODES.h>
#include <Pololu3piPlus2040.h>
#include "../UART_L3.h"

extern UART_L3 uartL3;
// Wie stark pro Encoder-Tick-Differenz gegengesteuert wird.
static const float STRAIGHT_KP = 0.1f;
static int   BASE_SPEED  = 60;
static bool    initialized = false;
static int32_t prevLeft  = 0;
static int32_t prevRight = 0;


static void test(uint8_t length, uint8_t *data){
    Serial.println("test wird ausgeführt");
    if (length < 1 || data == nullptr)
        return;

    if (data[0] == 1) {
        Serial.println("1 wurde festgestellt");
        BASE_SPEED = 60;
    }
    if (data[0] == 2){
        BASE_SPEED = 200;
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
    uartL3.registerCbReceiveTestdriverData(test);
    while(true){
        uint16_t sw = gameGetCtrlSwitches();

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
