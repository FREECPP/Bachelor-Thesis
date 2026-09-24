//
// Created by robo on 22.05.26.
//

#include "PEER_LIST.h"

PEER_LIST::PEER_LIST() {

}

void PEER_LIST::addPeer(const uint8_t *macAddr, const uint8_t id) {
    // Generate new Element
    PEER *newPeer = (PEER *)malloc(sizeof(PEER));
    if (newPeer == nullptr) {
        Serial.println("PEER_LIST::addPeer couldn't malloc");
        return;
    }
    Serial.println("PEER_LIST::addPeer: nach malloc ");
    memset(newPeer, 0, sizeof(PEER));
    Serial.println("PEER_LIST::addPeer: nach memset ");
    newPeer->id = id;
    memcpy(newPeer->peerInfo.peer_addr, macAddr, 6);
    Serial.println("PEER_LIST::addPeer: nach memcpy ");
    newPeer->peerInfo.channel = ESP_NOW_CHANNEL_NUMBER;
    newPeer->peerInfo.encrypt = false;
    newPeer->peerInfo.ifidx = WIFI_IF_STA;
    esp_now_add_peer(&newPeer->peerInfo);
    Serial.println("PEER_LIST::addPeer: nach esp_now_add_peer ");

    // Add it to List
    newPeer->next = head;
    head = newPeer;
    Serial.println("PEER_LIST::addPeer: end of function ");
}

void PEER_LIST::changeId(uint8_t *macAddr, uint8_t id) {
    PEER * current = head;
    while (current != nullptr) {
        if (memcmp(current->peerInfo.peer_addr, macAddr, 6) == 0) {
            break;
        }
        current = current->next;
    }
    if (current == nullptr) {
        Serial.println("PEER_LIST::changeId couldn't find macAddress");
        return;
    }
    current->id = id;
}

void PEER_LIST::removePeer(uint8_t *macAddr) {
    PEER *prev = nullptr;
    PEER * current = head;
    while (current != nullptr) {
        if (memcmp(current->peerInfo.peer_addr, macAddr, 6) == 0) {
            break;
        }
        prev = current;
        current = current->next;
    }
    if (current == nullptr) {
        Serial.println("PEER_LIST::removePeer couldn't find macAddress");
        return;
    }
    esp_now_del_peer(current->peerInfo.peer_addr);
    if (prev != nullptr) {
        prev->next = current->next;
    }
    else {
        head = current->next;
    }
    free(current);
}

bool PEER_LIST::containsId(uint8_t id) {
    PEER * current = head;
    while (current != nullptr) {
        if (current->id == id) {
            return true;
        }
        current = current->next;
    }
    return false;
}

PEER *PEER_LIST::getPeer(uint8_t id) {
    PEER* current = head;
    while (current != nullptr) {
        if (current->id == id) {
            break;
        }
        current = current->next;
    }
    return current;
}

uint8_t PEER_LIST::getId(const uint8_t *macAddr) {
    PEER *current = head;
    while (current != nullptr) {
        if (memcmp(current->peerInfo.peer_addr, macAddr, 6) == 0) {
            return current->id;
        }
        current = current->next;
    }
    return 255;
}

uint8_t *PEER_LIST::getMac(uint8_t id) {
    PEER* wanted = getPeer(id);
    if (wanted == nullptr) {
        return broadcastMac;
    }
    return wanted->peerInfo.peer_addr;
}
