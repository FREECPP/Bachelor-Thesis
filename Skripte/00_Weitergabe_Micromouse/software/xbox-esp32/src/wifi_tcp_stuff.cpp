#include <Arduino.h>
#include <WiFi.h>
#include "secrets.h"
#include "UART_L3.h"
#include <COMMUNICATION_CODES.h>

extern UART_L3 uartL3;

static WiFiServer tcpServer(TCP_PORT);
static WiFiClient tcpClient;

// Nachrichten Puffer
static uint8_t    tcpBuf[256];

// Wie viele Bytes der aktuellen Nachricht liegen schon im Puffer
static uint16_t   tcpFill = 0;

static bool serverStarted = false;

void initWifiTcp() {

    // Feste IP vergeben
    IPAddress ip(192, 168, 10, 50);

    // Gateway und Subnet festlegen
    IPAddress gateway(192, 168, 10, 1);
    IPAddress subnet(255, 255, 255, 0);

    // Wifi-Konfiguration anwenden
    WiFi.config(ip, gateway, subnet);        // MUSS vor begin() stehen

    // Sorgt dafür das ein automatisches Neustarten möglich ist
    WiFi.setAutoReconnect(true);

    // Startet die Verbindung
    WiFi.begin(WIFI_SSID, WIFI_PASS);        // blockiert nicht

    // Startet den Server (Verbindungen werden angenommen sobald mit WLAN verbunden)
   //tcpServer.begin();
}

// Behandlungen von Nachrichten -> bestehen immer aus einem Frame
static void handleTcpFrame(uint8_t *data, uint8_t len) {

    // Abbrechen wenn Frame leer ist
    if (len == 0) return;
    Serial.printf("TCP: Frame len=%d opcode=%d\n", len, data[0]);
    // Erstes Byte wird immer angeschaut da dort der OP-Code steht und dieser angibt was zu tun ist
    switch (data[0]) {                        // Whitelist
        case OPC_CONTROLLER_DATA:
        case OPC_GAME_CONTROL:
        case OPC_MAZE_DATA:
        case OPC_POS_DATA:
        case OPC_TESTDRIVER_CONTROL:
        // Daten die Empfangen werden, werden direkt über UART Weitergeleitet
            uartL3.forwardRaw(len, data);
            break;
        default:
        // Entspricht das erste Byte keinem bekannten OP-Code, wird es auf der Konsole ausgegeben
            Serial.printf("TCP: unerlaubter Opcode %d\n", data[0]);
    }
}

void processTcp() {
    
    // Wenn keine WLAN-Verbindung besteht, sofort abbrechen 
    if(WiFi.status() != WL_CONNECTED) { serverStarted = false; return; }
    if (!serverStarted) { tcpServer.begin(); serverStarted = true; }
   

    // liefert einen wartenden Client (ist ein neuer da wird ein alter ggf. entfernt)
    WiFiClient c = tcpServer.available();     // neuer Client?
    if (c) {
        if (tcpClient) tcpClient.stop();
        tcpClient = c;
        tcpClient.setNoDelay(true);
        tcpFill = 0;
        Serial.println("TCP: Client verbunden");
    }

    // Solange Client verbunden und Daten vorhanden sind, diese in den Puffer schreiben
    while (tcpClient && (tcpClient.connected() || tcpClient.available())) {

        if(!tcpClient.available()) break;

        // nächstes Byte auslesen und in b speichern
        uint8_t b = tcpClient.read();

        // Byte im Buffer unter nächstem Index speichern
        tcpBuf[tcpFill] = b;

        // Index erhöhen damit nächstes Byte auf die nächste Position geschrieben wird
        tcpFill = tcpFill + 1;

        if (tcpFill > tcpBuf[0]) {            // Längenbyte + Nutzdaten komplett
            handleTcpFrame(&tcpBuf[1], tcpBuf[0]);
            tcpFill = 0;
        }
    }
}