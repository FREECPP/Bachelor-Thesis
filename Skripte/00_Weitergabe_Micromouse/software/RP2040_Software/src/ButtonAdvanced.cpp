//
// Created by robo on 04.06.26.
//

#include "ButtonAdvanced.h"

ButtonAdvanced::ButtonAdvanced() {

}

static bool validButtonId(uint8_t id)
{
    return id < 3;
}

void ButtonAdvanced::setCallbackPressed(void (*callback)(uint8_t buttonNr)) {
    cbButtonPressed = callback;
}
void ButtonAdvanced::setCallbackReleased(void (*callback)(uint8_t buttonNr)) {
    cbButtonReleased = callback;
}
void ButtonAdvanced::setCallbackHold(void (*callback)(uint8_t buttonNr)) {
    cbButtonHold = callback;
}

void ButtonAdvanced::update() {
    // Yes, a bit repetitive, but the easiest solution because of different Button Classes

    // Button A
    bool btnAPressed = btnAdv_btnA.isPressed();
    uint8_t i = 0;
    if (btnAPressed && lastState[i] == BTN_RELEASED) {
        lastState[i] = millis();
        pressedEdge[i] = true;
        if (cbButtonPressed != nullptr) cbButtonPressed(i);
        Serial.println("ButtonAdvanced::update BtnA Pressed");
    }
    else if (btnAPressed && millis() >= lastState[i] + BTN_HOLD_TIME && lastState[i] != BTN_HOLD_SET) {
        lastState[i] = BTN_HOLD_SET;
        holdEdge[i] = true;
        if (cbButtonHold != nullptr) cbButtonHold(i);
    }
    else if (!btnAPressed && lastState[i] != BTN_RELEASED) {
        lastState[i] = BTN_RELEASED;
        releasedEdge[i] = true;
        if (cbButtonReleased != nullptr) cbButtonReleased(i);
    }

    // Button B
    bool btnBPressed = btnAdv_btnB.isPressed();
    i = 1;
    if (btnBPressed && lastState[i] == BTN_RELEASED) {
        lastState[i] = millis();
        pressedEdge[i] = true;
        if (cbButtonPressed != nullptr) cbButtonPressed(i);
    }
    else if (btnBPressed && millis() >= lastState[i] + BTN_HOLD_TIME && lastState[i] != BTN_HOLD_SET) {
        lastState[i] = BTN_HOLD_SET;
        holdEdge[i] = true;
        if (cbButtonHold != nullptr) cbButtonHold(i);
    }
    else if (!btnBPressed && lastState[i] != BTN_RELEASED) {
        lastState[i] = BTN_RELEASED;
        releasedEdge[i] = true;
        if (cbButtonReleased != nullptr) cbButtonReleased(i);
    }

    // Button C
    bool btnCPressed = btnAdv_btnC.isPressed();
    i = 2;
    if (btnCPressed && lastState[i] == BTN_RELEASED) {
        lastState[i] = millis();
        pressedEdge[i] = true;
        if (cbButtonPressed != nullptr) cbButtonPressed(i);
    }
    else if (btnCPressed && millis() >= lastState[i] + BTN_HOLD_TIME && lastState[i] != BTN_HOLD_SET) {
        lastState[i] = BTN_HOLD_SET;
        holdEdge[i] = true;
        if (cbButtonHold != nullptr) cbButtonHold(i);
    }
    else if (!btnCPressed && lastState[i] != BTN_RELEASED) {
        lastState[i] = BTN_RELEASED;
        releasedEdge[i] = true;
        if (cbButtonReleased != nullptr) cbButtonReleased(i);
    }
}

bool ButtonAdvanced::isPressed(uint8_t id) {
    if (!validButtonId(id)) return false;
    if (lastState[id] != BTN_RELEASED) return true;
    return false;
}

bool ButtonAdvanced::wasPressed(uint8_t id)
{
    if (!validButtonId(id)) return false;
    bool v = pressedEdge[id];
    pressedEdge[id] = false;
    return v;
}

bool ButtonAdvanced::wasReleased(uint8_t id)
{
    if (!validButtonId(id)) return false;
    bool v = releasedEdge[id];
    releasedEdge[id] = false;
    return v;
}

bool ButtonAdvanced::wasHeld(uint8_t id)
{
    if (!validButtonId(id)) return false;
    bool v = holdEdge[id];
    holdEdge[id] = false;
    return v;
}

void ButtonAdvanced::clearEvents()
{
    for (uint8_t i = 0; i < 3; i++)
    {
        pressedEdge[i] = false;
        releasedEdge[i] = false;
        holdEdge[i] = false;
    }
}
