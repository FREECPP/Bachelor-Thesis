//
// Created by robo on 22.05.26.
//

#include "UART_L3.h"

UART_L3::UART_L3() {}

void UART_L3::begin() {
    delay(1000); // Give the ESP32 a bit time to boot.
    uint8_t buffer[1];
    buffer[0] = OPC_GET_ID;
    uartL2.txList.addMessage(1, buffer);
    uartL2.update();
}

void UART_L3::update() {
    uartL2.update();

    while (uartL2.rxList.length() > 0) {
        MESSAGE_LIST_ELEMENT * mle = uartL2.rxList.getFirstElement();
        switch (mle->message[0]) {
            case OPC_MAZE_DATA:
                if (cbReceiveMazeData != nullptr) cbReceiveMazeData(mle->length - 1, &mle->message[1]);
                break;
            case OPC_POS_DATA:
                if (cbReceivePosData != nullptr) {
                    if (mle->length >= 5) {
                        cbReceivePosData(mle->message[2], mle->message[3], mle->message[4]);
                    } else {
                        cbReceivePosData(mle->message[1], mle->message[2], mle->message[3]);
                    }
                }
                break;
            case OPC_CONNECT_NOW_STATE:
                if (cbConnectNowState != nullptr) cbConnectNowState(mle->message[1]);
                break;
            case OPC_CONNECT_BT_CONTROLLER_STATE:
                if (cbConnectBtControllerState != nullptr) cbConnectBtControllerState(mle->message[1]);
                break;
            case OPC_CONTROLLER_DATA:
                if (cbReceiveBtControllerData != nullptr) cbReceiveBtControllerData(mle->length -1, &mle->message[1]);
                break;
            case OPC_ERROR_NOW:
                if (cbError !=nullptr) cbError(mle->message[1]);
                break;
            case OPC_GET_ID:
                Serial.print("Received new Id: ");
                Serial.println(mle->message[1], DEC);
                ownId = mle->message[1];
                ownIdKnown = true;
                break;
            case OPC_GAME_CONTROL:
                if (cbReceiveControlData != nullptr) cbReceiveControlData(mle->message[1], mle->length -2, &mle->message[2]);
                Serial.print("Ich habe ein Controlldata bekommen");
                break;
            case OPC_TESTDRIVER_CONTROL:
                if (cbReceiveTestdriverData != nullptr) cbReceiveTestdriverData(mle->length -1, &mle->message[1]);
                Serial.print("Ich habe Testdriverdata bekommen");
                break;
                
            default:
                Serial.println("UART_L3::update: Received unknown opcode:");
                for (uint8_t i = 0; i < mle->length; i++) {
                    Serial.print(mle->message[i], DEC);
                    Serial.print(" ");
                }
                Serial.println("");
                break;
        }
        uartL2.rxList.removeMessageFront();
    }
}

void UART_L3::connectNow() {
    uint8_t buffer[1];
    buffer[0] = OPC_CONNECT_NOW;
    uartL2.txList.addMessage(1, buffer);
}

void UART_L3::connectBtController() {
    uint8_t buffer[1];
    buffer[0] = OPC_CONNECT_BT_CONTROLLER;
    uartL2.txList.addMessage(1, buffer);
}

void UART_L3::sendMazeData(uint8_t targetId, uint8_t length, uint8_t *data) {
    MESSAGE_LIST_ELEMENT * mle = MESSAGE_LIST::generateListElement(length +2);
    mle->message[0] = OPC_MAZE_DATA;
    mle->message[1] = targetId;
    for (uint8_t i = 0; i < length; i++) {
        mle->message[i+2] = data[i];
    }
    uartL2.txList.addMessage(mle);
}

void UART_L3::sendPosData(uint8_t targetId, uint8_t x, uint8_t y) {
    uint8_t buffer[4];
    buffer[0] = OPC_POS_DATA;
    buffer[1] = targetId;
    buffer[2] = x;
    buffer[3] = y;
    uartL2.txList.addMessage(4, buffer);
}

void UART_L3::subscribeBtController(uint8_t functionId, uint8_t threshold, bool enable) {
    uint8_t buffer[3];
    if (enable) {
        buffer[0] = OPC_SUBSCR_BT_CONTROLLER_DATA;
    } else {
        buffer[0] = OPC_UNSUBSCR_BT_CONTROLLER_DATA;
    }

    buffer[1] = functionId;
    buffer[2] = threshold;
    uartL2.txList.addMessage(3, buffer);
}

void UART_L3::setId(uint8_t id) {
    ownId = id;
    ownIdKnown = true;
    uint8_t buffer[2];
    buffer[0] = OPC_SET_ID;
    buffer[1] = id;
    uartL2.txList.addMessage(2, buffer);
}

uint8_t UART_L3::getId() {return ownId;}
bool UART_L3::hasId() {return ownIdKnown;}

void UART_L3::sendControllerAction(uint8_t length, const uint8_t *data) {
    MESSAGE_LIST_ELEMENT * mle = MESSAGE_LIST::generateListElement(length +1);
    mle->message[0] = OPC_BT_CONTROLLER_ACTION;
    for (uint8_t i = 0; i < length; i++) {
        mle->message[i+1] = data[i];
    }
    uartL2.txList.addMessage(mle);
}

void UART_L3::sendControllerRumble(uint8_t duration, uint8_t magnitudeWeak, uint8_t magnitudeStrong) {
    uint8_t buffer[6];
    buffer[0] = OPC_BT_CONTROLLER_ACTION;
    buffer[1] = 4;
    buffer[2] = CONTROLLER_ACTION_RUMBLE;
    buffer[3] = duration;
    buffer[4] = magnitudeWeak;
    buffer[5] = magnitudeStrong;
    uartL2.writePriority(6, buffer);
    uartL2.update();
}

void UART_L3::sendControlData(uint8_t targetId, uint8_t length, const uint8_t *data) {
    MESSAGE_LIST_ELEMENT * mle = MESSAGE_LIST::generateListElement(length +2);      // Eigentliche Daten + Opcode + TargetId
    mle->message[0] = OPC_GAME_CONTROL;
    mle->message[1] = targetId;
    for (uint8_t i = 0; i < length; i++) {
        mle->message[i+2] = data[i];
    }
    uartL2.txList.addMessage(mle);
}




// Callbacks
void UART_L3::registerCbConnectNowState(void (*callback)(uint8_t stateId)) {cbConnectNowState = callback;}
void UART_L3::registerCbConnectBtControllerState(void (*callback)(uint8_t stateId)) {cbConnectBtControllerState = callback;}
void UART_L3::registerCbReceiveMazeData(void (*callback)(uint8_t length, uint8_t *data)) {cbReceiveMazeData = callback;}
void UART_L3::registerCbReceivePosData(void (*callback)(uint8_t id, uint8_t x, uint8_t y)) {cbReceivePosData = callback;}
void UART_L3::registerCbReceiveBtControllerData(void (*callback)(uint8_t length, uint8_t *data)) {cbReceiveBtControllerData = callback;}
void UART_L3::registerCbError(void (*callback)(uint8_t errorId)) {cbError = callback;}
void UART_L3::registerCbReceiveControlData(void (*callback)(uint8_t senderId, uint8_t length, uint8_t *data)) {cbReceiveControlData = callback;}
void UART_L3::registerCbReceiveTestdriverData(void(*callback)(uint8_t length, uint8_t *data)){cbReceiveTestdriverData = callback;}

