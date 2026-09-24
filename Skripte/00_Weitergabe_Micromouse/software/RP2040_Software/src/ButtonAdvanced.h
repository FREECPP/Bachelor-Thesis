//
// Created by robo on 04.06.26.
//

#ifndef RP2040_SOFTWARE_BUTTONADVANCED_H
#define RP2040_SOFTWARE_BUTTONADVANCED_H
#include <Pololu3piPlus2040.h>

#define BTN_HOLD_TIME 1000
#define BTN_RELEASED  0
#define BTN_HOLD_SET  0xFFFFFFFF        // 4 Bytes is the max size of ulong


// Stores last states. When not pressed: BTN_RELEASED
//                     When pressed: Press event millis();



class ButtonAdvanced {
    ButtonA btnAdv_btnA;        // ButtonNr = 0
    ButtonB btnAdv_btnB;        // ButtonNr = 1
    ButtonC btnAdv_btnC;        // ButtonNr = 2

    unsigned long lastState[3] = {0,0,0};
    bool pressedEdge[3] = {false, false, false};
    bool releasedEdge[3] = {false, false, false};
    bool holdEdge[3] = {false, false, false};
    void (*cbButtonPressed)(uint8_t buttonNr) = nullptr;
    void (*cbButtonReleased)(uint8_t buttonNr) = nullptr;
    void (*cbButtonHold)(uint8_t buttonNr) = nullptr;

    public:
    ButtonAdvanced();
    void setCallbackPressed(void (*callback)(uint8_t buttonNr));
    void setCallbackReleased(void (*callback)(uint8_t buttonNr));
    void setCallbackHold(void (*callback)(uint8_t buttonNr));
    void update();
    bool isPressed(uint8_t id);
    bool wasPressed(uint8_t id);
    bool wasReleased(uint8_t id);
    bool wasHeld(uint8_t id);
    void clearEvents();
};


#endif //RP2040_SOFTWARE_BUTTONADVANCED_H
