//
// Created by robo on 23.05.26.
//

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "PEER_LIST.h"

#include "COMMUNICATION_CODES.h"
#include "UART_L3.h"

extern UART_L3 uartL3;

static constexpr size_t NOW_RX_MAX_DATA_LENGTH = 250;
static constexpr UBaseType_t NOW_RX_QUEUE_LENGTH = 16;

struct NowRxMessage
{
    uint8_t mac[6];
    uint8_t length;
    uint8_t data[NOW_RX_MAX_DATA_LENGTH];
};

static QueueHandle_t nowRxQueue = nullptr;
static volatile uint32_t nowRxDropped = 0;

PEER_LIST peerList;

unsigned long timeoutNow = 0;

void espNowReceiveData(const uint8_t * mac, const uint8_t *incomingData, int len) {
    if (nowRxQueue == nullptr || mac == nullptr || incomingData == nullptr ||
        len <= 0 || len > (int)NOW_RX_MAX_DATA_LENGTH)
    {
        nowRxDropped++;
        return;
    }

    NowRxMessage message = {};
    memcpy(message.mac, mac, sizeof(message.mac));
    message.length = (uint8_t)len;
    memcpy(message.data, incomingData, len);
    if (xQueueSend(nowRxQueue, &message, 0) != pdTRUE)
        nowRxDropped++;
}

void initNowReceiveQueue()
{
    if (nowRxQueue == nullptr)
        nowRxQueue = xQueueCreate(NOW_RX_QUEUE_LENGTH, sizeof(NowRxMessage));
}

void processNow() {
    NowRxMessage message;
    while (nowRxQueue != nullptr &&
           xQueueReceive(nowRxQueue, &message, 0) == pdTRUE) {
        Serial.println("Received Now !");

        if (timeoutNow != 0) {      // during Connect process
            if (message.length == 2 && message.data[0] == NOW_CODE_BROAD_CONNECT) {
                uint8_t id = message.data[1];
                peerList.addPeer(message.mac, id);
                Serial.print("processNow: Received Broadcast connect from: MAC:");
                for (int i = 0; i < 6; i++) {
                    Serial.print(message.mac[i], HEX);
                    Serial.print(":");
                }
                Serial.print("  ID = ");
                Serial.println(id);

                timeoutNow = 0;

                uint8_t buffer[2];
                buffer[0] = NOW_CODE_UNI_CONNECT;
                buffer[1] = uartL3.ownId;
                esp_now_send(peerList.getMac(id), buffer, 2);
                continue;
            }
            if (message.length == 2 && message.data[0] == NOW_CODE_UNI_CONNECT) {
                peerList.addPeer(message.mac, message.data[1]);
                Serial.print("processNow: Received Uni connect from: MAC:");
                for (int i = 0; i < 6; i++) {
                    Serial.print(message.mac[i], HEX);
                    Serial.print(":");
                }
                Serial.print("  ID = ");
                Serial.println(message.data[1]);

                timeoutNow = 0;
                continue;
            }
        }
        switch (message.data[0]) {
            case OPC_MAZE_DATA:
                uartL3.receiveMazeData(message.length - 1, &message.data[1]);
                break;
            case OPC_POS_DATA:
                if (message.length >= 4)
                    uartL3.receivePosData(message.data[1], message.data[2], message.data[3]);
                break;
            case OPC_GAME_CONTROL:
                // Opcode-Byte ueberspringen: &message[1] = [senderId, payload...].
                // receiveControlData() stellt den Opcode selbst wieder voran,
                // sonst entsteht ein doppelter Opcode und alles ist um 1 Byte
                // verschoben (senderId wuerde als OPC_GAME_CONTROL gelesen).
                if (message.length >= 2)
                    uartL3.receiveControlData(message.length - 1, &message.data[1]);
                break;
            default:
                Serial.println("processNow: Received unknown message:");
                for (int i = 0; i < message.length; i++) {
                    Serial.print(message.data[i], DEC);
                    Serial.print(" ");
                }
                Serial.println("");

        }
    }

    static uint32_t lastReportedDrops = 0;
    const uint32_t dropped = nowRxDropped;
    if (dropped != lastReportedDrops) {
        Serial.print("ESP-NOW RX queue dropped messages: ");
        Serial.println(dropped);
        lastReportedDrops = dropped;
    }
}
