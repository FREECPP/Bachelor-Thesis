//
// Created by robo on 23.05.26.
//
#include <UART_L3.h>
#include <Bluepad32.h>

extern int threshold[5];
extern ControllerPtr controller;
extern UART_L3 uartL3;

int stickL[2] = {0, 0};
int stickR[2] = {0, 0};
uint16_t switches = 0;
int triggerL = 0;
int triggerR = 0;

unsigned long timeoutBt = 0;

void onConnected(ControllerPtr ctl) {
    if (controller == nullptr) {
        controller = ctl;
        Serial.println("onConnected: Controller verbunden");
        BP32.enableNewBluetoothConnections(false);
        uartL3.connectBtControllerState(CONN_SUCCESS);
        timeoutBt = 0;
        return;
    }
    Serial.println("onConnected: neuer Controller wird ignoriert");


}

void onDisconnected(ControllerPtr ctl) {
    if (controller == ctl) {
        Serial.println("onDisconnected");
        controller = nullptr;
        uartL3.connectBtControllerState(CONN_FAIL);
    }
}

void controllerHandling() {
    if (threshold[CONTROLLER_STICK_L] != -1 && (    // StickL
        controller->axisX() > stickL[0] + threshold[CONTROLLER_STICK_L]*4 || controller->axisX() < stickL[0] - threshold[CONTROLLER_STICK_L]*4||
        controller->axisY() > stickL[1] + threshold[CONTROLLER_STICK_L]*4 || controller->axisY() < stickL[1] - threshold[CONTROLLER_STICK_L]*4 )) {
        static unsigned long txLastSend = 0;
        if (txLastSend + 20 < millis()) {
            txLastSend = millis();
            uint8_t buffer[3];
            buffer[0] = CONTROLLER_STICK_L;
            buffer[1] = controller->axisX()/4;
            buffer[2] = controller->axisY()/4;
            uartL3.receiveBtControllerData(3, buffer);

            stickL[0] = controller->axisX();
            stickL[1] = controller->axisY();
        }
    }

    if (threshold[CONTROLLER_STICK_R] != -1 && (    // StickR
        controller->axisRX() > stickR[0] + threshold[CONTROLLER_STICK_R]*4 || controller->axisRX() < stickR[0] - threshold[CONTROLLER_STICK_R]*4||
        controller->axisRY() > stickR[1] + threshold[CONTROLLER_STICK_R]*4 || controller->axisRY() < stickR[1] - threshold[CONTROLLER_STICK_R]*4 )) {
        static unsigned long txLastSend = 0;
        if (txLastSend + 20 < millis()) {
            txLastSend = millis();
            uint8_t buffer[3];
            buffer[0] = CONTROLLER_STICK_R;
            buffer[1] = controller->axisRX()/4;
            buffer[2] = controller->axisRY()/4;
            uartL3.receiveBtControllerData(3, buffer);

            stickR[0] = controller->axisRX();
            stickR[1] = controller->axisRY();
        }
    }

    // Switch states
    if (threshold[CONTROLLER_SWITCHES] != -1 ) {
        uint16_t newSwitches = controller->buttons(); // Contains positions 0-9
        newSwitches |= controller->dpad() << 12;      // Dpad data to corresponding position
        // Leuchtende Xbox/Guide-Taste = Bluepad32 miscSystem() (NICHT miscHome();
        // miscHome ist auf Xbox-Controllern die Menue-Taste). Wird in der
        // Pacman-Lobby als Start-Taste ausgewertet (GameState GS_LOBBY).
        if (controller->miscSystem()) newSwitches |= CSW_BUTTON_HOME;
        if (controller->miscBack()) newSwitches |= CSW_BUTTON_BACK;

        if (switches != newSwitches) {
            switches = newSwitches;
            uint8_t buffer[3];
            buffer[0] = CONTROLLER_SWITCHES;
            CONVERTER converter{};
            converter.b16 = switches;
            buffer[1] = converter.b8[0];
            buffer[2] = converter.b8[1];
            uartL3.receiveBtControllerData(3, buffer);
        }
    }

    // Trigger L
    if (threshold[CONTROLLER_TRIGGER_L] != -1 && (    // TriggerL
        controller->brake() > triggerL + threshold[CONTROLLER_TRIGGER_L]*4 || controller->brake() < triggerL - threshold[CONTROLLER_TRIGGER_L]*4)) {
        static unsigned long txLastSend = 0;
        if (txLastSend + 20 < millis()) {
            txLastSend = millis();
            uint8_t buffer[2];
            buffer[0] = CONTROLLER_TRIGGER_L;
            buffer[1] = controller->brake()/4;
            uartL3.receiveBtControllerData(2, buffer);
            triggerL = controller->brake();
        }
    }

    // Trigger R
    if (threshold[CONTROLLER_TRIGGER_R] != -1 && (    // TriggerL
        controller->throttle() > triggerR + threshold[CONTROLLER_TRIGGER_R]*4 || controller->throttle() < triggerR - threshold[CONTROLLER_TRIGGER_R]*4)) {
        static unsigned long txLastSend = 0;
        if (txLastSend + 20 < millis()) {
            txLastSend = millis();
            uint8_t buffer[2];
            buffer[0] = CONTROLLER_TRIGGER_R;
            buffer[1] = controller->throttle()/4;
            uartL3.receiveBtControllerData(2, buffer);
            triggerR = controller->throttle();
        }
    }
}