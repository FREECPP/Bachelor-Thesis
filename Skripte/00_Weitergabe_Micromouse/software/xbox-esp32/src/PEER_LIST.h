//
// Created by robo on 22.05.26.
//

#ifndef XBOX_ESP32_PEER_LIST_H
#define XBOX_ESP32_PEER_LIST_H
#include <Arduino.h>
#include <esp_wifi.h>
#include <esp_now.h>

#define ESP_NOW_CHANNEL_NUMBER 0
const uint8_t broadcastAddress[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

typedef struct PEER {
    esp_now_peer_info_t peerInfo;
    uint8_t id;
    struct PEER *next;
}PEER;

class PEER_LIST {
    PEER *head = nullptr;
    uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    public:
    PEER_LIST();
    void addPeer(const uint8_t *macAddr, uint8_t id);
    void changeId(uint8_t *macAddr, uint8_t id);
    void removePeer(uint8_t *macAddr);
    bool containsId(uint8_t id);
    PEER *getPeer(uint8_t id);
    uint8_t getId(const uint8_t *macAddr);
    uint8_t *getMac(uint8_t id);
};


#endif //XBOX_ESP32_PEER_LIST_H