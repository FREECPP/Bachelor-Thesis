//
// Created by robo on 14.05.26.
//

#include "MESSAGE_LIST.h"

MESSAGE_LIST::MESSAGE_LIST(){

}

int MESSAGE_LIST::addMessage(uint8_t length, const uint8_t *message) {
    MESSAGE_LIST_ELEMENT * newElement = (MESSAGE_LIST_ELEMENT*) malloc(sizeof( MESSAGE_LIST_ELEMENT));
    if (newElement == nullptr) {
        Serial.println("MESSAGE_LIST::addMessage(): Memory Allocation Failed");
        return -1;
    }
    newElement->length = length;
    newElement->message = (uint8_t *) malloc(length);
    newElement->next = nullptr;
    if (newElement->message == nullptr) {
        Serial.println("MESSAGE_LIST::addMessage(): Memory Allocation Failed");
        free(newElement);
        return -1;
    }
    for (uint8_t i = 0; i < length; i++) {
        newElement->message[i] = message[i];
    }
    items++;

    if (head == nullptr) {
        head = newElement;
        return 0;
    }
    MESSAGE_LIST_ELEMENT * current = head;
    while (current->next != nullptr) {
        current = current->next;
    }
    current->next = newElement;
    return 0;
}

int MESSAGE_LIST::addMessage(MESSAGE_LIST_ELEMENT * newElement) {
    if (newElement == nullptr) {
        Serial.println("MESSAGE_LIST::addMessage(): newElement is nullptr");
        return -1;
    }
    items++;

    if (head == nullptr) {
        head = newElement;
        return 0;
    }
    MESSAGE_LIST_ELEMENT * current = head;
    while (current->next != nullptr) {
        current = current->next;
    }
    current->next = newElement;
    return 0;
}

int MESSAGE_LIST::addMessageFront(uint8_t length, const uint8_t *message) {
    if (message == nullptr) {
        return -1;
    }

    MESSAGE_LIST_ELEMENT *newElement = generateListElement(length);
    if (newElement == nullptr) {
        return -1;
    }

    for (uint8_t i = 0; i < length; i++) {
        newElement->message[i] = message[i];
    }

    newElement->next = head;
    head = newElement;
    items++;
    return 0;
}

MESSAGE_LIST_ELEMENT *MESSAGE_LIST::generateListElement(uint8_t length) {
    MESSAGE_LIST_ELEMENT * newElement = (MESSAGE_LIST_ELEMENT*) malloc(sizeof( MESSAGE_LIST_ELEMENT));
    if (newElement == nullptr) {
        Serial.println("MESSAGE_LIST::addMessage(): Memory Allocation Failed");
        return nullptr;
    }
    newElement->length = length;
    newElement->message = (uint8_t *) malloc(length);
    newElement->next = nullptr;
    if (newElement->message == nullptr) {
        Serial.println("MESSAGE_LIST::addMessage(): Memory Allocation Failed");
        free(newElement);
        return nullptr;
    }
    return newElement;
}

int MESSAGE_LIST::firstMessage(uint8_t *length, uint8_t **message) {
    if (items == 0) return 0;
    *length = head->length;
    *message = head->message;
    return items;
}

MESSAGE_LIST_ELEMENT * MESSAGE_LIST::getFirstElement() {
    return head;
}

void MESSAGE_LIST::removeMessageFront() {
    if (head == nullptr) return;
    MESSAGE_LIST_ELEMENT * toDelete = head;
    head = head->next;
    free(toDelete->message);
    free(toDelete);
    items--;
}

int MESSAGE_LIST::length() {
    return items;
}


