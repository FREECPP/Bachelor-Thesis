#include <Arduino.h>
#include <Bluepad32.h>
#include "UART_L3.h"
#include "PEER_LIST.h"
#include <esp_wifi.h>
#include <esp_now.h>
#include <btstack_run_loop.h>
#include <btstack_run_loop_freertos.h>
extern "C" {
#include <uni_hid_device.h>
}

#include "../../../../../../../.platformio/packages/toolchain-riscv32-esp/riscv32-esp-elf/include/c++/8.4.0/ratio"

UART_L3 uartL3;

ControllerPtr controller;
// Thresholds for StickL, StickR, Switches, TriggerL, TriggerR
int threshold[5] = {-1, -1, -1, -1, -1};
extern unsigned long timeoutBt;

extern PEER_LIST peerList;
extern unsigned long timeoutNow;

void controllerHandling();
void processNow();

static bool pendingRumble = false;
static uint16_t pendingRumbleDurationMs = 0;
static uint8_t pendingRumbleWeak = 0;
static uint8_t pendingRumbleStrong = 0;
static uint16_t activeRumbleDurationMs = 0;
static uint8_t activeRumbleWeak = 0;
static uint8_t activeRumbleStrong = 0;
static bool rumbleCallbackQueued = false;
static btstack_context_callback_registration_t rumbleCallbackRegistration;

static void playRumbleOnBtstackThread(void *context) {
    (void)context;
    rumbleCallbackQueued = false;

    if (controller == nullptr || !controller->isConnected()) {
        Serial.println("Rumble skipped: no controller");
        return;
    }

    uni_hid_device_t *device = uni_hid_device_get_instance_for_idx(controller->index());
    if (device != nullptr && device->report_parser.play_dual_rumble != nullptr) {
        device->report_parser.play_dual_rumble(device, 0, activeRumbleDurationMs,
                                               activeRumbleWeak, activeRumbleStrong);
        uni_hid_device_send_queued_reports(device);
        return;
    }

    controller->playDualRumble(0, activeRumbleDurationMs,
                               activeRumbleWeak, activeRumbleStrong);
    btstack_run_loop_freertos_trigger();
}

static void serviceControllerRumble() {
    if (!pendingRumble)
        return;

    pendingRumble = false;
    activeRumbleDurationMs = pendingRumbleDurationMs;
    activeRumbleWeak = pendingRumbleWeak;
    activeRumbleStrong = pendingRumbleStrong;

    if (rumbleCallbackQueued)
        return;

    rumbleCallbackQueued = true;
    rumbleCallbackRegistration.callback = playRumbleOnBtstackThread;
    rumbleCallbackRegistration.context = nullptr;
    btstack_run_loop_execute_on_main_thread(&rumbleCallbackRegistration);
    btstack_run_loop_freertos_trigger();
}

// ── Callbacks ────────────────────────────────────────────────────────────────

void onConnected(ControllerPtr ctl);        // BtController
void onDisconnected(ControllerPtr ctl);     // BtController

void espNowReceiveData(const uint8_t * mac, const uint8_t *incomingData, int len);
void initNowReceiveQueue();

void UL3CB_connectNow() {
    Serial.println("UL3CB_connectNow");
    timeoutNow = millis();
    uint8_t buffer[2];
    buffer[0] = NOW_CODE_BROAD_CONNECT;
    buffer[1] = uartL3.ownId;
    esp_now_send(broadcastAddress, buffer, 2);

}
void UL3CB_connectBtController() {
    Serial.println("UL3CB_connectBtController");
    BP32.forgetBluetoothKeys();
    BP32.enableNewBluetoothConnections(true);
    timeoutBt = millis();
}
void UL3CB_sendMazeData(uint8_t targetId, uint8_t length, uint8_t *data) {
    Serial.println("UL3CB_sendMazeData");
    if (targetId == 255) {      // Broadcast
        uint8_t buffer[255];
        buffer[0] = OPC_MAZE_DATA;
        for (int i = 0; i < length; i++) {
            buffer[i + 1] = data[i];
        }
        esp_now_send(broadcastAddress, buffer, length+1);
        return;
    }
    if (peerList.containsId(targetId) == false) {
        Serial.println("UL3CB_sendMazeData: ID not found");
        uartL3.error(NOW_NO_ID_DECODE);
        return;
    }
    esp_now_send(peerList.getMac(targetId), data, length);

}
void UL3CB_sendPosData(uint8_t targetId, uint8_t x, uint8_t y) {
    Serial.println("UL3CB_sendPosData");
    if (peerList.containsId(targetId) == false && targetId != 255) {
        Serial.println("UL3CB_sendPosData: ID not found");
        uartL3.error(NOW_NO_ID_DECODE);
        return;
    }
    uint8_t buffer[4];
    buffer[0] = OPC_POS_DATA;
    buffer[1] = uartL3.ownId;
    buffer[2] = x;
    buffer[3] = y;
    if (targetId == 255) esp_now_send(broadcastAddress, buffer, 4);
    else esp_now_send(peerList.getMac(targetId), buffer, 4);
}
void UL3CB_subscribeBtController(uint8_t functionId, uint8_t newThreshold, bool enable) {
    Serial.println("UL3CB_subscribeBtController");
    if (functionId > 4) return;
    threshold[functionId] = enable ? newThreshold : -1;
}
void UL3CB_controllerAction(uint8_t length, uint8_t *data) {
    Serial.println("UL3CB_controllerAction");
    if (length < 4 || data == nullptr) return;

    uint8_t functionId = data[0];
    if (functionId == CONTROLLER_ACTION_RUMBLE) {
        pendingRumbleDurationMs = (uint16_t)data[1] * 4;
        pendingRumbleWeak = data[2];
        pendingRumbleStrong = data[3];
        pendingRumble = true;
    }
}

void UL3CB_sendControlData(uint8_t length, uint8_t *data) {// Opcode, TargetId, GameControl Id, optional other stuff

    Serial.println("UL3CB_sendControlData");
    uint8_t targetId = data[1];
    data[1] = uartL3.ownId;     // Replace target id with own id
    if (targetId == 255) {      // Broadcast
        esp_now_send(broadcastAddress, data, length);
        return;
    }
    if (peerList.containsId(targetId) == false) {
        Serial.println("UL3CB_sendMazeData: ID not found");
        uartL3.error(NOW_NO_ID_DECODE);
        return;
    }
    esp_now_send(peerList.getMac(targetId), data, length);

}

// ── Setup ────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    //while (!Serial) delay(10);
    Serial.println("Welcome to the ESP Communication program!");

    BP32.setup(&onConnected, &onDisconnected);
    BP32.enableNewBluetoothConnections(false);

    // Gespeicherte BT-Keys löschen → immer frisch pairen
    uartL3.registerCbConnectNow(UL3CB_connectNow);
    uartL3.registerCbConnectBtController(UL3CB_connectBtController);
    uartL3.registerCbSendMazeData(UL3CB_sendMazeData);
    uartL3.registerCbSendPosData(UL3CB_sendPosData);
    uartL3.registerCbSubscribeController(UL3CB_subscribeBtController);
    uartL3.registerControllerAction(UL3CB_controllerAction);
    uartL3.registerSendControlData(UL3CB_sendControlData);

    // ESP Now stuff
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_err_t ret = esp_now_init();
    if (ret != ESP_OK) {
        Serial.printf("esp_now_init failed: %s\n", esp_err_to_name(ret));
    }
    initNowReceiveQueue();
    esp_now_register_recv_cb(esp_now_recv_cb_t(espNowReceiveData));
    /* Add broadcastAddress */
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = ESP_NOW_CHANNEL_NUMBER;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

}

// ── Loop ─────────────────────────────────────────────────────────────────────

void loop() {

    BP32.update();
    if (controller != nullptr) {
        controllerHandling();
    }

    if (timeoutBt != 0 && millis() > (timeoutBt + 10000) ) {
        timeoutBt = 0;
        uartL3.connectBtControllerState(CONN_FAIL);
        BP32.enableNewBluetoothConnections(false);
        Serial.println("Bluetooth connection: TIMEOUT!");
    }

    if (timeoutNow != 0 && millis() > (timeoutNow + 10000) ) {
        timeoutNow = 0;
        uartL3.connectNowState(CONN_FAIL);
        Serial.println("ESP-NOW connection: TIMEOUT!");
    }


    uartL3.update();
    serviceControllerRumble();
    processNow();

}
