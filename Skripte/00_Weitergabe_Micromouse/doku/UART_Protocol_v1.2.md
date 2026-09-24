# UART protocol

The protocol for the communication between ESP32 and RP2040 (on board MCU).

Protocol version: 1.2

Protocol date: 11.05.2026

## Features

The protocol should cover following points:

- Controller support:
  
  - Inputs from a wireless controller (connected to ESP32), like stick positions, button events
  
  - Actions/Feedback to the controller, like rumble, led lights, ...

- ESP NOW networking:
  
  - Receive messages
  
  - Send messages (broad- and unicast)

- Data integrity:
  
  - Use of checksums to check for transmission errors
  
  - Use of timeouts for reset procedures

## UART Implementation

### Hardware side

Hardwarewise the following pins are used:

| RP2040          |               | ESP32     |
| --------------- | ------------- | --------- |
| UART0 TX @ GP28 | ------------- | RX @ IO16 |
| UART0 RX @ GP29 | ------------- | TX @ IO17 |

### Software side

Following code can be used to initialize the Serial objects with the Arduino framework:

**Micromouse (RP2040)**

```cpp
// Global variable
arduino::UART Serial2(/*TX*/28,/*RX*/29, /*RTS*/NC, /*CTS*/NC);

void setup(){
    Serial2.begin(9600, SERIAL_8N1);
}
```

**ESP32** 

```cpp
void setup(){
    Serial1.begin(9600, SERIAL_8N1, /*RX*/16, /*TX*/17);
}
```

A connection speed of **9600 BAUD** is used due to stability reasons.

## The Protocol

The ESP and the RP can both start to send data. There is no Master/Slave setup.

There are two layers to implement.

### Data Link Layer

This layer is used to verify and control the data. It's layer 2 in the OSI-Model. It consitis of following elements

| Start | Length  | Checksum | Data          | End |
| ----- | ------- | -------- | ------------- | --- |
| 136   | [0-255] | [0-255]  | max 255 bytes | 17  |

For every transmission an answer byte is required. This can be **ACK** (170) or **NAK** (240). After a NAK the transmission should be repeated.

#### Start byte

The startbyte is a control byte with the value of *0b10001000 = 136*.

#### Length byte

This byte stands for the length of the unstuffed data.

E.g. the series [1 2 3 4 5 6 170 8] should be send: length = 8  

#### Checksum byte

This byte is a unsigned 8-bit sum of the unstuffed data and acts as a checksum.

E.g. the series [1 2 3 4 5 6 170 8] has the checksum of 199

#### Data

This area contains the bytes to transmit (max length of 255). Because of the control bytes the data is modified with byte stuffing (every control byte gets replaced with escape byte and alternative byte):

| Description | Control-byte | Replacement sequence |
| ----------- | ------------ | -------------------- |
| Start       | 136          | 175 1                |
| End         | 17           | 175 2                |
| ACK         | 170          | 175 3                |
| NAK         | 240          | 175 4                |
| ESC         | 175          | 175 5                |

 Checksum and length are build on unstuffed *original* data.

#### End byte

The endbyte is a control byte with the value of *0b00010001 = 17*.

#### Checks

Following checks are performed:

- Checksum match: detects corrupted data

- Message length: length between start & stop byte and length byte

- De-stuffing errors

- Control bytes in data

- Bytes without start: start condition

- Timeout answer

For the communication following format is used:

| Length (Payload) | Checksum  | OpCode    | Value/Values (0-250) (=Payload) |
| ---------------- | --------- | --------- | ------------------------------- |
| 8 Bit            | 8 Bit sum | see table | 0-255 x 8 Bit                   |

After receiving one packet the checksum should be calculated and compared with the transmitted one. In case of difference the packet can be requested again with a NAK Bit.

After receiving the receiver should reply to the transmitter ACK (ACKnowledgement) or NAK (No AcKnowledgement). ACK and NAK should be send like every other message with the defined frame ( [Length][Checksum][OpCode][Payload (optional)]).

### Length

This byte defines the number of payload bytes. For protocol version 1.0 it's only possible to send up to 255 payload bytes (should be enought, ESP NOW doesn't support more than 250 bytes anyways...).

Because of the extra header (OpCode + Length) and checksum every message is 3 bytes longer than the length byte.

### Checksum

The checksum is calculated as the sum of all bytes (OpCode, length, Payload) except the checksum itself. It should be interpreted as an unsigned 8-bit integer (uint8_t).

### OpCode

| Code | Name                  | Values [bytes]         | Description                                         |
| ---- | --------------------- | ---------------------- | --------------------------------------------------- |
| 11   | Stick L               | 2 [X, Y]               | Left Stick                                          |
| 12   | Stick R               | 2 [X, Y]               | Right Stick                                         |
| 13   | L2 Trigger            | 1                      | Left 2 Trigger (analog)                             |
| 14   | R2 Trigger            | 1                      | Right 2 Trigger (analog)                            |
| 15   | SW_Set                | 2                      | Switches set, see below                             |
| 16   | Gyroscope             | 3 [X, Y, Z]            |                                                     |
| 31   | RGB LED               | 3 [RGB]                |                                                     |
| 32   | Player LED            | 1                      | Player indicator LED                                |
| 33   | Rumble                | 1                      | 0                                                   |
|      |                       |                        |                                                     |
| 101  | Send broadcast        | 0-250 (message)        | Send message to all ESPs                            |
| 102  | Send unicast          | 1 (target) 0-250 (msg) | Send to one ESP.                                    |
| 110  | Msg received          | 0-255 (message)        | Message from other                                  |
|      |                       |                        |                                                     |
| 200  | ACK                   |                        | Acknowledgement                                     |
| 201  | NAK                   | 1 error code           | No Acknowledgement                                  |
| 211  | Enable Subscription   | 2 [ID, threshhold]     | Enable change notification of element (ID = OpCode) |
| 212  | Disable Subscription  | ID                     | Disable change notification                         |
| 213  | Reset Subscriptions   |                        | Disable all change notifications                    |
| 214  | Request current value | ID                     | Request message (ID = OpCode)                       |
| 215  | Ping                  |                        | Sends ping, expects answer (216)                    |
| 216  | Ping answer           |                        | Answer to Ping (215)                                |

**SW_Set**

Bit meanings (Pressed when 1) first Byte:

1. L1 Trigger

2. R1 Trigger

3. Share button

4. Options button

5. PS button

6. Mute button

7. Left stick button

8. Right stick button

Bit meanings (Pressed when 1) second Byte:

1. Cross up

2. Cross right

3. Cross down

4. Cross left

5. Triangle

6. Circle

7. X

8. Square

**Error codes**

MSB set: request last message again

| Code | Name           | Description               | Example        |
| ---- | -------------- | ------------------------- | -------------- |
| 1    | Checksum       | Checksum does not match   |                |
| 2    | Invalid OpCode | Opcode invalid            | OpCode 5       |
| 3    | Invalid value  | Value range doesn't match | Player LED 200 |
| 4    | Invalid length | Unexpected value length   |                |

### Payload / Values

Here is the opcode dependent data located. 

## Further concepts

Ideas behind the protocol

### Subscription

The protocol supports subscription to changes. This ist especially handy for controller inputs. The RP2040 can subscribe to parameters and get messages when something changes. The threshold specifies the amount of the change for analog values and is ignored for digital ones.

### Timeouts and safety

There are following timeouts and procedures:

- **Length timeout** (receiver side): if the message is not finnished after **100 ms** the NAK (invalid length) with resend request should be sent. A maximum of ***5 retrys*** is allowed before going into error state. 

- **Answer timeout** (transmitter side): if the communication partner doesn't answer at all in **100 ms** goto error state and retry connection every second.

- **Checksum & ACK system**: If something goes wrong during the communication the checksum should catch some of the errors.
