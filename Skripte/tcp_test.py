import socket, time

OPC_TESTDRIVER_CONTROL = 15

def connect(retries=10, delay=0.5):
    for i in range(retries):
        try:
            return socket.create_connection(("192.168.10.50", 4210), timeout=2)
        except OSError as e:
            print(f"Versuch {i+1} fehlgeschlagen: {e}")
            time.sleep(delay)
    raise SystemExit("ESP32 nicht erreichbar")

s = connect()
s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

payload = bytes([OPC_TESTDRIVER_CONTROL, 1])
s.sendall(bytes([len(payload)]) + payload)

time.sleep(1)     # ESP32 Zeit geben, das Paket zu lesen
s.close()
print("done")
