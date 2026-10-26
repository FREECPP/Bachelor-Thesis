import socket, time

# Op-Code muss händisch eingetragen werden, da python die COMMUNICATION_CODES.h 
# nicht liest
OPC_TESTDRIVER_CONTROL = 15
OPC_TESTDRIVER_POSITION = 16

# Versucht TCP-Verbindung zum ESP32 aufzubauen, dieser ist nicht immer 
# Erreichbar, da sich Wifi wie auch Bluetooth die Funkantenne teilen daher wird 
# der Verbindungsaufbau 10x versucht
"""def connect(retries=5, delay=0.3):
    for i in range(retries):
        try:
            return socket.create_connection(("192.168.10.50", 4210), timeout=2)
        except OSError as e:
            print(f"Versuch {i+1} fehlgeschlagen: {e}")
            time.sleep(delay)
    raise ConnectionError("ESP32 nicht erreichbar") 
"""
def connect(timeout_total=6.0, delay=0.3):
    deadline = time.time() + timeout_total
    last_error = None
    while time.time() < deadline:
        try:
            return socket.create_connection(("192.168.10.50", 4210), timeout=1)
        except OSError as e:
            last_error = e
            time.sleep(delay)
    raise ConnectionError(f"Connect: ESP32 nicht erreichbar ({last_error})")

def send_packet(value):
    # Verbindung zu ESP32 herstellen
    s = connect()

    # Optionen der Verbindung werden gesetzt -> Normalerweise werden bei 
    # TCP kleine Pakete kurz gesammelt 
    # um sie gemeinsam zu senden (kann zu Verzögerungen führen)-> wird 
    # deaktiviert da wir nur kleine Pakete schicken
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    
    payload = bytes([OPC_TESTDRIVER_CONTROL, value]) 

    # Mit längenbyte anreichern damit Empfänger weiß, wann das Paket zu ende 
    # ist + senden
    s.sendall(bytes([len(payload)]) + payload)

   # time.sleep(1)     # ESP32 Zeit geben, das Paket zu lesen

    # Verbindung beenden
    s.close()

    # Melden das Skript durchgelaufen ist
    print("done")

def send_position(x, y):
    s = connect()
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    payload = bytes([OPC_TESTDRIVER_POSITION, x, y])
    s.sendall(bytes([len(payload)]) + payload)
    s.close()
