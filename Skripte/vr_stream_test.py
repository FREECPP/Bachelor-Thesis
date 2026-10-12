from flask import Flask, Response
import cv2

app = Flask(__name__)
cap = cv2.VideoCapture("udp://0.0.0.0:12345", cv2.CAP_FFMPEG)

def frames():
    while True:
        ok, frame = cap.read()
        if not ok:
            continue
        # hier dein angereichertes/stabilisiertes Bild einsetzen
        ok, jpg = cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, 70])
        yield (b"--frame\r\nContent-Type: image/jpeg\r\n\r\n" + jpg.tobytes() + b"\r\n")

@app.route("/")
def index():
    return '<body style="margin:0;background:#000"><img src="/video" style="width:100%"></body>'

@app.route("/video")
def video():
    return Response(frames(), mimetype="multipart/x-mixed-replace; boundary=frame")

app.run(host="0.0.0.0", port=8000, threaded=True)