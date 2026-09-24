//
// Created by robo on 22.05.26.
//

#include "UART_L2.h"

uint8_t UART_L2::generateChecksum(int length, const uint8_t *buffer) {
    uint8_t checksum = 0;
    for (int i = 0; i < length; i++) {
        checksum += buffer[i];
    }
    // Special case/Quick fix for end detection: Checksum mustn't be UART_L2_END aka 17
    if (checksum == UART_L2_END) checksum--;
    return checksum;
}

// Generate the byte stuffing and write the stuffed bytes (+ end byte) in txBuffer
int UART_L2::applyByteStuffing(int length, const uint8_t *buffer) {
    if (length > UART_L2_MAX_PAYLOAD) {
        Serial.print("ERROR: UART_L2::applyByteStuffing payload too large: ");
        Serial.println(length);
        txBufferLength = 0;
        return -1;
    }

    int index = 3;
    int replacements = length;
    for (int i = 0; i < length; i++) {
        switch (buffer[i]) {
            case UART_L2_START:
                txBuffer[index++] = UART_L2_ESC;
                txBuffer[index] = UART_L2_BS_START;
                break;
            case UART_L2_END:
                txBuffer[index++] = UART_L2_ESC;
                txBuffer[index] = UART_L2_BS_END;
                break;
            case UART_L2_ACK:
                txBuffer[index++] = UART_L2_ESC;
                txBuffer[index] = UART_L2_BS_ACK;
                break;
            case UART_L2_NAK:
                txBuffer[index++] = UART_L2_ESC;
                txBuffer[index] = UART_L2_BS_NAK;
                break;
            case UART_L2_ESC:
                txBuffer[index++] = UART_L2_ESC;
                txBuffer[index] = UART_L2_BS_ESC;
                break;
            default:
                txBuffer[index] = buffer[i];
                replacements--;
                break;
        }
        index++;
    }
    txBuffer[index] = UART_L2_END;
    txBufferLength = length + replacements + 4;
    return replacements;
}

// Diese Funktion liest die daten von rxBuffer und prüft diese nach länge, checksumme und destuff fehlern.
// Die decodierte nachricht wird der rxList hinten angehängt.
MESSAGE_LIST_ELEMENT *UART_L2::revertByteStuffing() {
    int headerLength = rxBuffer[1];
    uint8_t headerChecksum = rxBuffer[2];

    MESSAGE_LIST_ELEMENT *listElement = MESSAGE_LIST::generateListElement(headerLength);
    if (listElement == nullptr) {
        Serial.println("ERROR: UART_L2::revertByteStuffing generateListElement failed");
        sendNAK();
        return nullptr;
    }


    bool needNAK = false;
    int index_RX = 3;
    int replacements = 0;
    for (int i = 0; i < headerLength; i++) {
        if (rxBuffer[index_RX] == UART_L2_ESC) {
            switch (rxBuffer[++index_RX]) {
                case UART_L2_BS_START:
                    listElement->message[i] = UART_L2_START;
                    replacements++;
                    break;
                case UART_L2_BS_END:
                    listElement->message[i] = UART_L2_END;
                    replacements++;
                    break;
                case UART_L2_BS_ACK:
                    listElement->message[i] = UART_L2_ACK;
                    replacements++;
                    break;
                case UART_L2_BS_NAK:
                    listElement->message[i] = UART_L2_NAK;
                    replacements++;
                    break;
                case UART_L2_BS_ESC:
                    listElement->message[i] = UART_L2_ESC;
                    replacements++;
                    break;
                default:
                    Serial.println("ERROR: UART_L2::revertByteStuffing No such byte to de-stuff.");
                    needNAK = true;
                    break;
            }
        }
        else if (rxBuffer[index_RX] == UART_L2_START ||
            rxBuffer[index_RX] == UART_L2_END ||
            rxBuffer[index_RX] == UART_L2_ACK ||
            rxBuffer[index_RX] == UART_L2_NAK ||
            rxBuffer[index_RX] == UART_L2_ESC) {
            Serial.println("ERROR: UART_L2::revertByteStuffing Control sign in data area");
            needNAK = true;
            }
        else {
            listElement->message[i] = rxBuffer[index_RX];
        }

        if (needNAK) break;
        index_RX++;
    }

    // Length check
    if (headerLength != rxBufferLength - 4 - replacements && !needNAK) {
        Serial.print("Error from UART_L2::revertByteStuffing: header length does not match message length: Header=");
        Serial.print(headerLength);
        Serial.print(" rxBufferLength=");
        Serial.println(rxBufferLength);
        Serial.println("");
        Serial.println("rxBuffer dump:");
        for (int i = 0; i < rxBufferLength; i++) {
            Serial.print(rxBuffer[i], DEC);
            Serial.print(" ");
        }
        Serial.println("");
        needNAK = true;
    }

    // Checksum check
    if (headerChecksum != generateChecksum(headerLength, listElement->message) && !needNAK) {
        Serial.println("Error from UART_L2::read: header checksum does not match message checksum");
        needNAK = true;
    }

    if (needNAK) {
        // Löschen des Fehlerhaften bufferelements
        free(listElement->message);
        listElement->message = nullptr;
        free(listElement);
        //listElement = nullptr;

        // senden eines NAK
        sendNAK();
        return nullptr;
    }
    rxList.addMessage(listElement);
    sendACK();
    return listElement;
}


void UART_L2::sendACK() {
    L2_SERIAL.write(UART_L2_ACK);
    rxBufferLength = 0;
    rxNakCount = 0;
}

void UART_L2::sendNAK() {
    L2_SERIAL.write(UART_L2_NAK);
    rxBufferLength = 0;
    rxNakCount++;
    if (rxNakCount > UART_L2_RESEND_TRIES) {
        Serial.println("UART_L2::sendNAK: NAK count exceeds limit");
        if (cbStatusReport != nullptr) cbStatusReport(ERROR_RECEIVE_NAK_NUMBER_EXCEEDED);
    }
}

void UART_L2::rewriteData() {
    txResendTries++;
    if (txResendTries > UART_L2_RESEND_TRIES) {
        Serial.println("ERROR: UART_L2::rewriteData exceeded resend tries");
        failState = true;
        txAnswerOpen = 0;
        return;
    }
    L2_SERIAL.write(txBuffer, txBufferLength);
    txAnswerOpen = 1;
    txLastSend = millis();
}

void UART_L2::write() {
    MESSAGE_LIST_ELEMENT * message = txList.getFirstElement();
    if ( message  == nullptr ) return;
    txBuffer[0] = UART_L2_START;
    txBuffer[1] = message->length;
    txBuffer[2] = generateChecksum(message->length, message->message);
    applyByteStuffing(message->length, message->message);

    L2_SERIAL.write(txBuffer, txBufferLength);
    txLastSend = millis();
    txResendTries = 0;
    txAnswerOpen = 1;
    txList.removeMessageFront();
}

// public:

UART_L2::UART_L2() {
#ifdef TARGET_D1MINI
    L2_SERIAL.begin(UART_SPEED, SERIAL_8N1, 16, 17);
#endif

#ifdef TARGET_WROOM2
    L2_SERIAL.begin(UART_SPEED, SERIAL_8N1, 18, 17);
#endif

#ifdef TARGET_SEED
    L2_SERIAL.begin(UART_SPEED, SERIAL_8N1); //, 44 /*D7*/, 43 /*D6*/);
    //L2_SERIAL.begin(UART_SPEED, SERIAL_8N1, 7,6);
#endif

#ifdef TARGET_RP2040
    L2_SERIAL.begin(UART_SPEED, SERIAL_8N1);
#endif
}

void UART_L2::registerCbStatusReport(void (*cb)(uint8_t)) {
    cbStatusReport = cb;
}

void UART_L2::update() {
    // RX part

    while (L2_SERIAL.available()) {
        uint8_t byte = L2_SERIAL.read();
        if (byte == UART_L2_ACK && rxBufferLength == 0) {
            if (txAnswerOpen == 0) {
                Serial.println("UART_L2::update: unexpected ACK");
                continue;
            }
            txAnswerOpen = 0;
            txResendTries = 0;
            failState = false;
            continue;
        }
        if (byte == UART_L2_NAK && rxBufferLength == 0) {
            if (txAnswerOpen == 0) {
                Serial.println("UART_L2::update: unexpected NAK");
                continue;
            }
            rewriteData();
            continue;
        }
        if (rxBufferLength >= MAX_BUFFER) {
            Serial.println("ERROR: UART_L2::update receive buffer overflow");
            sendNAK();
            break;
        }
        if (rxBufferLength == 0 && byte != UART_L2_START) {
            Serial.print("Error from UART_L2::update expected Start byte but received ");
            Serial.println(byte, DEC);
            sendNAK();
            break;
        }
        rxBuffer[rxBufferLength] = byte;
        rxBufferLength++;

        if (byte == UART_L2_END) {
            revertByteStuffing();
            // rxBufferLength = 0;  // Wird bereits in sendACK/NAK zurückgesetzt.
        }

    }

    if (txAnswerOpen != 0 &&
        (uint32_t)(millis() - txLastSend) >= UART_L2_ANS_TIMEOUT) {
        rewriteData();
    }
    if (txAnswerOpen == 0) {
        write();
    }
}

void UART_L2::writePriority(uint8_t length, const uint8_t *message) {
    txList.addMessageFront(length, message);
}

