//
// Created by robo on 14.05.26.
//

#ifndef ESP_SOFTWARE_MESSAGE_LIST_H
#define ESP_SOFTWARE_MESSAGE_LIST_H
#include <Arduino.h>

typedef struct MESSAGE_LIST_ELEMENT{
    uint8_t length;
    uint8_t *message;
    struct MESSAGE_LIST_ELEMENT *next;
} MESSAGE_LIST_ELEMENT;

class MESSAGE_LIST {
private:
    MESSAGE_LIST_ELEMENT * head = nullptr;
    int items = 0;

public:
    MESSAGE_LIST();
    int addMessage(uint8_t length, const uint8_t * message);
    int addMessage(MESSAGE_LIST_ELEMENT * newElement);
    int addMessageFront(uint8_t length, const uint8_t *message);
    static MESSAGE_LIST_ELEMENT* generateListElement(uint8_t length);
    int firstMessage(uint8_t *length, uint8_t **message);    // return = count of messages in list
    MESSAGE_LIST_ELEMENT* getFirstElement();
    void removeMessageFront();
    int length();
};


#endif //ESP_SOFTWARE_MESSAGE_LIST_H
