//
// Created by robo on 22.05.26.
//

#ifndef ESP32_UART_L3_H
#define ESP32_UART_L3_H
#include <UART_L2.h>
#include <COMMUNICATION_CODES.h>
#include <Arduino.h>
#include <EEPROM.h>

#define EEPROM_ID_POSITION 0


union CONVERTER{
    uint8_t b8[2];
    uint16_t b16;
};

// UART_L3 bildet eine komfortable Schnittstelle für die Kommunikation.
// In diesem Layer wird mittels vorangestelltem Opcode zwischen den nachrichten unterschieden.

class UART_L3 {
    UART_L2 uartL2;

    void (*cbConnectNow)() = nullptr;
    void (*cbConnectBtController)() = nullptr;
    void (*cbSendMazeData)(uint8_t targetId, uint8_t length, uint8_t *data) = nullptr;
    void (*cbSendPosData)(uint8_t targetId, uint8_t x, uint8_t y) = nullptr;
    void (*cbSubscribeBtController)(uint8_t functionId, uint8_t threshold, bool enable) = nullptr;
    void (*cbControllerAction)(uint8_t length, uint8_t *value) = nullptr;
    void (*cbSendControlData)(uint8_t length, uint8_t *value) = nullptr;

public:
    uint8_t ownId = 0;

    // Die Benamung ist vom RP2040 aus gedacht

    UART_L3();
    void update();
    void connectNowState(uint8_t stateId);
    void connectBtControllerState(uint8_t stateId);
    void receiveMazeData(uint8_t length, uint8_t *data);
    void receivePosData(uint8_t senderId, uint8_t x, uint8_t y);
    void receiveBtControllerData(uint8_t length, uint8_t *data);
    void error(uint8_t errorId);
    void receiveControlData(uint8_t length, uint8_t *data);

    void registerCbConnectNow(void (*callback)());
    void registerCbConnectBtController(void (*callback)());
    void registerCbSendMazeData(void (*callback)(uint8_t targetId, uint8_t length, uint8_t *data));
    void registerCbSendPosData(void (*callback)(uint8_t targetId, uint8_t x, uint8_t y));
    void registerCbSubscribeController(void (*callback)(uint8_t functionId, uint8_t threshold, bool enable));
    void registerControllerAction(void (*callback)(uint8_t length, uint8_t *data));
    void registerSendControlData(void (*callback)(uint8_t length, uint8_t *value));

    void forwardRaw(uint8_t length, uint8_t *data);


};


#endif //ESP32_UART_L3_H