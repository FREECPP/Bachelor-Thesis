//
// Created by robo on 22.05.26.
//

#ifndef RP2040_UART_L3_H
#define RP2040_UART_L3_H
#include <UART_L2.h>
#include <COMMUNICATION_CODES.h>

// UART_L3 bildet eine komfortable Schnittstelle für die Kommunikation.
// In diesem Layer wird mittels vorangestelltem Opcode zwischen den nachrichten unterschieden.

class UART_L3 {
    UART_L2 uartL2;

    void (*cbConnectNowState)(uint8_t stateId) = nullptr;
    void (*cbConnectBtControllerState)(uint8_t stateId) = nullptr;
    void (*cbReceiveMazeData)(uint8_t length, uint8_t *data) = nullptr;
    void (*cbReceivePosData)(uint8_t senderId, uint8_t x, uint8_t y) = nullptr;
    void (*cbReceiveBtControllerData)(uint8_t length, uint8_t *data) = nullptr;
    void (*cbError)(uint8_t errorId) = nullptr;
    void (*cbReceiveControlData)(uint8_t senderId, uint8_t length, uint8_t *data) = nullptr;

    uint8_t ownId = 0;      // Real id is stored on ESP
    bool ownIdKnown = false;
    public:

    UART_L3();
    void begin();
    void update();
    void connectNow();
    void connectBtController();
    void sendMazeData(uint8_t targetId, uint8_t length, uint8_t *data);
    void sendPosData(uint8_t targetId, uint8_t x, uint8_t y);
    void subscribeBtController(uint8_t functionId, uint8_t threshold, bool enable);
    void setId(uint8_t id);
    uint8_t getId();
    bool hasId();
    void sendControllerAction(uint8_t length, const uint8_t *data);
    void sendControllerRumble(uint8_t duration /*Max 100*/, uint8_t magnitudeWeak, uint8_t magnitudeStrong);
    void sendControlData(uint8_t targetId, uint8_t length, const uint8_t *data);

    void registerCbConnectNowState(void (*callback)(uint8_t stateId));
    void registerCbConnectBtControllerState(void (*callback)(uint8_t stateId));
    void registerCbReceiveMazeData(void (*callback)(uint8_t length, uint8_t *data));
    void registerCbReceivePosData(void (*callback)(uint8_t senderId, uint8_t x, uint8_t y));
    void registerCbReceiveBtControllerData(void (*callback)(uint8_t length, uint8_t *data));
    void registerCbError(void (*callback)(uint8_t errorId));
    void registerCbReceiveControlData(void (*callback)(uint8_t senderId, uint8_t length, uint8_t *data));


};


#endif //RP2040_UART_L3_H
