//
// Created by robo on 22.05.26.
//

#ifndef XBOX_ESP32_UART_L2_H
#define XBOX_ESP32_UART_L2_H

#include <Arduino.h>
#include "MESSAGE_LIST.h"

// Library settings
#define MAX_BUFFER 514
#define UART_L2_MAX_PAYLOAD     ((MAX_BUFFER - 4) / 2)
#define ERROR_TIMEOUT 1
#define ERROR_RECEIVE_NAK_NUMBER_EXCEEDED 2

// Protocol definitions
#define UART_SPEED      115200

#define UART_L2_ANS_TIMEOUT     100
#define UART_L2_RESEND_TRIES    5

// Control bytes
#define UART_L2_START   0b10001000      // Start byte
#define UART_L2_END     0b00010001      // End byte
#define UART_L2_ACK     0b10101010      // ACK byte
#define UART_L2_NAK     0b11110000      // NAK byte
#define UART_L2_ESC     0b10101111      // Escape sequence byte

// Byte stuffing codes
#define UART_L2_BS_START        1
#define UART_L2_BS_END          2
#define UART_L2_BS_ACK          3
#define UART_L2_BS_NAK          4
#define UART_L2_BS_ESC          5

#ifdef TARGET_SEED
#define L2_SERIAL Serial0
#endif
#ifdef TARGET_D1MINI
#define L2_SERIAL Serial1
#endif
#ifdef TARGET_WROOM2
#define L2_SERIAL Serial1
#endif
#ifdef TARGET_RP2040
#define L2_SERIAL Serial2
#endif


class UART_L2 {
private:
    // TX stuff
    uint8_t txBuffer[MAX_BUFFER] = {};
    int txBufferLength = 0;
    int txAnswerOpen = 0;
    unsigned long txLastSend = 0;
    uint8_t txResendTries = 0;

    // RX stuff
    uint8_t rxBuffer[MAX_BUFFER] = {};
    int rxBufferLength = 0;
    uint8_t rxNakCount = 0;

    // Management
    void (*cbStatusReport)(uint8_t errorCode) = nullptr;

public:
    bool failState = false;
    MESSAGE_LIST rxList;
    MESSAGE_LIST txList;

private:
    static uint8_t generateChecksum(int length, const uint8_t *buffer);
    int applyByteStuffing(int length, const uint8_t *buffer);
    MESSAGE_LIST_ELEMENT * revertByteStuffing();
    void sendACK();
    void sendNAK();
    void rewriteData();
    void write();

public:
    UART_L2();
    void registerCbStatusReport(void (*cb)(uint8_t));
    void update();
    void writePriority(uint8_t length, const uint8_t *message);




};


#endif //XBOX_ESP32_UART_L2_H
