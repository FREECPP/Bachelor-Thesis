//
// Created by robo on 22.05.26.
//

#include "UART_L3.h"

UART_L3::UART_L3() {
    EEPROM.begin(1);
    ownId = EEPROM.read(EEPROM_ID_POSITION);
}

void UART_L3::update() {
    uartL2.update();

    while (uartL2.rxList.length() > 0) {
        MESSAGE_LIST_ELEMENT * mle = uartL2.rxList.getFirstElement();
        switch (mle->message[0]) {
            case OPC_MAZE_DATA:
                if (cbSendMazeData!= nullptr) cbSendMazeData( mle->message[1], mle->length - 2/*-OpCode -Target*/, &mle->message[2]);
                break;
            case OPC_POS_DATA:
                if (cbSendPosData != nullptr) cbSendPosData(mle->message[1], mle->message[2], mle->message[3]);
                break;
            case OPC_CONNECT_NOW:
                if (cbConnectNow != nullptr) cbConnectNow();
                break;
            case OPC_CONNECT_BT_CONTROLLER:
                if (cbConnectBtController != nullptr) cbConnectBtController();
                break;
            case OPC_SET_ID:
                ownId = mle->message[1];
                EEPROM.write(EEPROM_ID_POSITION, ownId);
                EEPROM.commit();
                Serial.print("Setting new Id ");
                Serial.println(ownId);
                break;
            case OPC_GET_ID:
                uint8_t buffer[2];
                buffer[0] = OPC_GET_ID;
                buffer[1] = ownId;
                Serial.print("Getting Id ");
                Serial.println(ownId);
                uartL2.txList.addMessage(2, buffer);
                break;
            case OPC_SUBSCR_BT_CONTROLLER_DATA:
                if (cbSubscribeBtController != nullptr) cbSubscribeBtController(mle->message[1], mle->message[2], true);
                break;
            case OPC_UNSUBSCR_BT_CONTROLLER_DATA:
                if (cbSubscribeBtController != nullptr) cbSubscribeBtController(mle->message[1], mle->message[2], false);
                break;
            case OPC_BT_CONTROLLER_ACTION:
                if (cbControllerAction != nullptr) cbControllerAction(mle->message[1], &mle->message[2]);
                break;
            case OPC_GAME_CONTROL:
                if (cbSendControlData != nullptr) cbSendControlData(mle->length, mle->message);
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

void UART_L3::connectNowState( uint8_t stateId) {
    uint8_t buffer[2];
    buffer[0] = OPC_CONNECT_NOW_STATE;
    buffer[1] = stateId;
    uartL2.txList.addMessage(2, buffer);
}

void UART_L3::connectBtControllerState( uint8_t stateId) {
    uint8_t buffer[2];
    buffer[0] = OPC_CONNECT_BT_CONTROLLER_STATE;
    buffer[1] = stateId;
    uartL2.txList.addMessage(2, buffer);
}

void UART_L3::receiveMazeData(uint8_t length, uint8_t *data) {
    MESSAGE_LIST_ELEMENT * mle = MESSAGE_LIST::generateListElement(length +1);
    mle->message[0] = OPC_MAZE_DATA;
    for (uint8_t i = 0; i < length; i++) {
        mle->message[i+1] = data[i];
    }
    uartL2.txList.addMessage(mle);
}

void UART_L3::receivePosData(uint8_t senderId, uint8_t x, uint8_t y) {
    uint8_t buffer[4];
    buffer[0] = OPC_POS_DATA;
    buffer[1] = senderId;
    buffer[2] = x;
    buffer[3] = y;
    uartL2.txList.addMessage(4, buffer);
}

void UART_L3::receiveBtControllerData(uint8_t length, uint8_t *data) {
    MESSAGE_LIST_ELEMENT * mle = MESSAGE_LIST::generateListElement(length +1);
    mle->message[0] = OPC_CONTROLLER_DATA;
    for (uint8_t i = 0; i < length; i++) {
        mle->message[i+1] = data[i];
    }
    uartL2.txList.addMessage(mle);
}

void UART_L3::error(uint8_t errorId) {
    uint8_t buffer[2];
    buffer[0] = OPC_ERROR_NOW;
    buffer[1] = errorId;
    uartL2.txList.addMessage(2, buffer);
}

void UART_L3::receiveControlData(uint8_t length, uint8_t *data) {
    MESSAGE_LIST_ELEMENT * mle = MESSAGE_LIST::generateListElement(length +1);
    mle->message[0] = OPC_GAME_CONTROL;
    for (uint8_t i = 0; i < length; i++) {
        mle->message[i+1] = data[i];
    }
    uartL2.txList.addMessage(mle);
}

void UART_L3::forwardRaw(uint8_t length, uint8_t *data){
    uartL2.txList.addMessage(length, data);
}



// Callbacks
void UART_L3::registerCbConnectNow(void (*callback)()) {cbConnectNow = callback;}
void UART_L3::registerCbConnectBtController(void (*callback)()) {cbConnectBtController = callback;}
void UART_L3::registerCbSendMazeData(void (*callback)(uint8_t targetId, uint8_t length, uint8_t *data)) {cbSendMazeData = callback;}
void UART_L3::registerCbSendPosData(void (*callback)(uint8_t targetId, uint8_t x, uint8_t y)) {cbSendPosData = callback;}
void UART_L3::registerCbSubscribeController(void (*callback)(uint8_t functionId, uint8_t threshold, bool enable)) {cbSubscribeBtController = callback;}
void UART_L3::registerControllerAction(void (*callback)(uint8_t length, uint8_t *data)) { cbControllerAction = callback;}
void UART_L3::registerSendControlData(void (*callback)(uint8_t length, uint8_t *value)) {cbSendControlData = callback;}
