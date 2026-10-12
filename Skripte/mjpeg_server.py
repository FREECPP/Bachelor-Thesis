import os
import ssl
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import cv2

_HERE = os.path.dirname(os.path.abspath(__file__))
_CERT = os.path.join(_HERE, "cert.pem")
_KEY = os.path.join(_HERE, "key.pem")
_VR_PAGE = os.path.join(_HERE, "vr.html")

# Das zuletzt bereitgestellte Bild als fertig kodiertes JPEG.
# Der Lock sorgt dafür, dass Schreiben (Hauptprogramm) und Lesen (Server-Threads) sich nicht überlagern.
_latest_jpeg = None
_jpeg_lock = threading.Lock()

_server = None


def _get_jpeg():
    with _jpeg_lock:
        return _latest_jpeg


class _Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        path = self.path.split("?")[0]

        # Startseite: flache Ansicht im Browser plus Link zur VR-Seite
        if path == "/":
            self._send(200, "text/html",
                       b'<html><body style="margin:0;background:#000">'
                       b'<a href="/vr" style="color:#fff;font:2rem sans-serif">VR-Ansicht</a><br>'
                       b'<img src="/stream" style="width:100%"></body></html>')

        # WebXR-Seite (liegt als Datei neben diesem Skript)
        elif path == "/vr":
            with open(_VR_PAGE, "rb") as f:
                self._send(200, "text/html; charset=utf-8", f.read())

        # Einzelbild für die VR-Seite
        elif path == "/frame.jpg":
            jpg = _get_jpeg()
            if jpg is None:
                self._send(503, "text/plain", b"noch kein Bild")
            else:
                self._send(200, "image/jpeg", jpg, extra={"Cache-Control": "no-store"})

        # Endloser Strom einzelner JPEG-Bilder für die flache Ansicht
        elif path == "/stream":
            self.send_response(200)
            self.send_header("Content-Type", "multipart/x-mixed-replace; boundary=frame")
            self.end_headers()
            try:
                while True:
                    jpg = _get_jpeg()
                    if jpg is not None:
                        self.wfile.write(
                            b"--frame\r\nContent-Type: image/jpeg\r\nContent-Length: "
                            + str(len(jpg)).encode() + b"\r\n\r\n" + jpg + b"\r\n"
                        )
                    time.sleep(1 / 30)
            except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
                pass        # Browser hat die Verbindung geschlossen

        else:
            self._send(404, "text/plain", b"nicht gefunden")

    def _send(self, code, content_type, body, extra=None):
        try:
            self.send_response(code)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            for k, v in (extra or {}).items():
                self.send_header(k, v)
            self.end_headers()
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
            pass

    def log_message(self, *args):
        pass                # keine Konsolenausgabe pro Anfrage


def _ensure_cert():
    """Erzeugt beim ersten Start ein selbstsigniertes Zertifikat (WebXR verlangt HTTPS)."""
    if os.path.exists(_CERT) and os.path.exists(_KEY):
        return
    print("Erzeuge selbstsigniertes Zertifikat ...")
    subprocess.run(
        ["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
         "-keyout", _KEY, "-out", _CERT, "-days", "365", "-subj", "/CN=micromouse-ar"],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )


def start(port=8080, https=True):
    """Startet den Webserver in einem eigenen Thread. Blockiert das Hauptprogramm nicht."""
    global _server
    _server = ThreadingHTTPServer(("0.0.0.0", port), _Handler)
    scheme = "http"
    if https:
        _ensure_cert()
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(_CERT, _KEY)
        # Handshake erst im jeweiligen Handler-Thread, damit ein hängender Client den Server nicht blockiert
        _server.socket = ctx.wrap_socket(_server.socket, server_side=True,
                                         do_handshake_on_connect=False)
        scheme = "https"
    threading.Thread(target=_server.serve_forever, daemon=True).start()
    print(f"Stream läuft: {scheme}://<Mac-IP>:{port}   (VR: {scheme}://<Mac-IP>:{port}/vr)")


def update_frame(frame, quality=70):
    """Stellt ein neues Bild (BGR-Array von OpenCV) für den Stream bereit."""
    global _latest_jpeg
    ok, buf = cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, quality])
    if ok:
        with _jpeg_lock:
            _latest_jpeg = buf.tobytes()


def stop():
    """Beendet den Server (optional, am Programmende)."""
    if _server is not None:
        _server.shutdown()
