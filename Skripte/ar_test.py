"""import cv2
import numpy as np
import trimesh
import os

# ==============================================================================
# 1. KALIBRIERUNGSDATEN & PARAMETER KONFIGURIEREN
# ==============================================================================
CALIB_FILE = "camera_calibration.npz"  # Dateipfad deiner .npz Datei
MARKER_SIZE = 0.05  # Physikalische Kantenlänge des Markers in Metern (z. B. 0.05m = 5 cm)
TARGET_ID = 0       # ID des Markers, über dem der Kristall schweben soll

# Laden der .npz-Datei & automatische Erkennung gängiger Variablen-Namen
data = np.load(CALIB_FILE)
camera_matrix = data.get("camera_matrix") if "camera_matrix" in data else data.get("mtx")
dist_coeffs = data.get("dist_coeffs") if "dist_coeffs" in data else data.get("dist")

if camera_matrix is None or dist_coeffs is None:
    raise KeyError(
        f"Konnte Kamera-Matrix oder Verzerrungskoeffizienten in '{CALIB_FILE}' nicht finden. "
        f"Enthaltene Schlüssel: {list(data.keys())}"
    )

#==============================================================================
# 2. 3D-GEOMETRIE AUS FUSION 360 LADEN (.obj-Datei)
# ==============================================================================
# Lade die OBJ-Datei
mesh = trimesh.load("Katze_Test.obj", force="mesh")

# Falls Fusion in Millimetern exportiert hat: in Meter umrechnen (mm -> m)
mesh.apply_scale(0.001)

# Vertices (3D-Punkte) und Faces (Dreiecke) für OpenCV vorbereiten
crystal_3d_pts = np.array(mesh.vertices, dtype=np.float32)
faces = mesh.faces.tolist()
edges = mesh.edges_unique

# 3D-Ecken des ArUco-Markers (Z=0 Ebene)
marker_3d_edges = np.array([
    [-MARKER_SIZE / 2,  MARKER_SIZE / 2, 0],
    [ MARKER_SIZE / 2,  MARKER_SIZE / 2, 0],
    [ MARKER_SIZE / 2, -MARKER_SIZE / 2, 0],
    [-MARKER_SIZE / 2, -MARKER_SIZE / 2, 0]
], dtype=np.float32)

# ==============================================================================
# 3. LIVE-STREAM & TRACKING LOOP
# ==============================================================================
# Zwingt OpenCV/FFmpeg, den Puffer zu ignorieren und sofort das Live-Bild zu zeigen
os.environ["OPENCV_FFMPEG_CAPTURE_OPTIONS"] = (
    "fflags;nobuffer|flags;low_delay|probesize;32|analyzeduration;0|sync;ext"
)
stream_url = "udp://@0.0.0.0:12345?fifo_size=100000&overrun_nonkey=1"
cap = cv2.VideoCapture(stream_url, cv2.CAP_FFMPEG)

# Zusätzlich den Puffer-Wert direkt in OpenCV auf 1 Frame limitieren
cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

if not cap.isOpened():
    print("Fehler: Kamera konnte nicht geöffnet werden.")
    exit()

window_name = "AR Red Crystal Tracking"
cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
cv2.startWindowThread()

dictionary = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_4X4_50)

try:
    parameters = cv2.aruco.DetectorParameters()
    detector = cv2.aruco.ArucoDetector(dictionary, parameters)
    legacy_mode = False
except AttributeError:
    parameters = cv2.aruco.DetectorParameters_create()
    legacy_mode = True

print("AR-Tracking gestartet. Drücke 'q' oder 'ESC' zum Beenden.")

while True:
    ret, frame = cap.read()
    if not ret:
        print("Konnte keinen Frame lesen.")
        break

    # Marker erkennen
    if not legacy_mode:
        corners, ids, _ = detector.detectMarkers(frame)
    else:
        corners, ids, _ = cv2.aruco.detectMarkers(frame, dictionary, parameters=parameters)

    if ids is not None:
        for i, marker_id in enumerate(ids.flatten()):
            if marker_id == TARGET_ID:
                # 2D-Ecken des erkannten Target-Markers extrahieren
                corners_2d = corners[i][0].astype(np.float32)

                # 3D-Pose (Rotation rvec & Translation tvec) berechnen
                success, rvec, tvec = cv2.solvePnP(
                    marker_3d_edges, corners_2d, camera_matrix, dist_coeffs
                )

                if success:

                    # 1. 3D-Punkte des geladenen Meshes auf 2D projizieren
                    img_pts, _ = cv2.projectPoints(
                        crystal_3d_pts, rvec, tvec, camera_matrix, dist_coeffs
                    )
                    img_pts = np.int32(img_pts.reshape(-1, 2))

                    # 2. Modell zeichnen (Halbtransparente Flächen)
                    overlay = frame.copy()
                    for face in faces:
                        pts = img_pts[face]
                        cv2.fillPoly(overlay, [pts], (20, 180, 20))

                    # Overlay über den Frame legen (40% Transparenz)
                    cv2.addWeighted(overlay, 0.4, frame, 0.6, 0, frame)

                    # 3. Kanten in Dunkelrot nachzeichnen
                    # Farbschema in BGR: (0, 0, 100) entspricht einem tiefen Dunkelrot
                    for edge in edges:
                        pt1 = tuple(img_pts[edge[0]])
                        pt2 = tuple(img_pts[edge[1]])
                        cv2.line(frame, pt1, pt2, (0, 100, 0), 1, cv2.LINE_AA)

                    # Markerumriss zur Kontrolle hervorheben
                    cv2.polylines(frame, [np.int32(corners_2d)], True, (0, 0, 255), 2)

    cv2.imshow(window_name, frame)

    key = cv2.waitKey(30) & 0xFF
    if key == ord('q') or key == 27:
        break

cap.release()
cv2.destroyAllWindows()
cv2.waitKey(1)
"""
import cv2
import numpy as np
import trimesh
import subprocess
import os

# ==============================================================================
# 1. KALIBRIERUNGSDATEN & PARAMETER KONFIGURIEREN
# ==============================================================================
CALIB_FILE = "camera_calibration.npz"  # Dateipfad deiner .npz Datei
MARKER_SIZE = 0.05  # Physikalische Kantenlänge des Markers in Metern (z. B. 0.05m = 5 cm)
TARGET_ID = 0       # ID des Markers, über dem das Objekt schweben soll
WIDTH = 1280
HEIGHT = 720

# Laden der .npz-Datei & automatische Erkennung gängiger Variablen-Namen
data = np.load(CALIB_FILE)
camera_matrix = data.get("camera_matrix") if "camera_matrix" in data else data.get("mtx")
dist_coeffs = data.get("dist_coeffs") if "dist_coeffs" in data else data.get("dist")

if camera_matrix is None or dist_coeffs is None:
    raise KeyError(
        f"Konnte Kamera-Matrix oder Verzerrungskoeffizienten in '{CALIB_FILE}' nicht finden. "
        f"Enthaltene Schlüssel: {list(data.keys())}"
    )

# ==============================================================================
# 2. 3D-GEOMETRIE AUS FUSION 360 LADEN (.obj-Datei)
# ==============================================================================
mesh = trimesh.load("Ar_test_obj.obj", force="mesh")
mesh.apply_scale(0.001)  # mm in Meter umrechnen

crystal_3d_pts = np.array(mesh.vertices, dtype=np.float32)
faces = mesh.faces.tolist()
edges = mesh.edges_unique

# 3D-Ecken des ArUco-Markers (Z=0 Ebene)
marker_3d_edges = np.array([
    [-MARKER_SIZE / 2,  MARKER_SIZE / 2, 0],
    [ MARKER_SIZE / 2,  MARKER_SIZE / 2, 0],
    [ MARKER_SIZE / 2, -MARKER_SIZE / 2, 0],
    [-MARKER_SIZE / 2, -MARKER_SIZE / 2, 0]
], dtype=np.float32)

# ==============================================================================
# 3. MPV BACKEND STREAM & AR-TRACKING LOOP
# ==============================================================================
# Startet mpv im Hintergrund mit Ihren exakten Low-Latency Parametern
mpv_cmd = [
    'mpv',
    'udp://@0.0.0.0:12345',
    '--no-cache',
    '--untimed',
    '--profile=low-latency',
    '--vf=format=bgr24',       # Wandelt direkt in OpenCV-kompatibles BGR um
    '--of=rawvideo',           # Rohdaten-Ausgabe
    '--o=-'                    # Ausgabe über stdout
]

process = subprocess.Popen(
    mpv_cmd, 
    stdout=subprocess.PIPE, 
    stderr=subprocess.DEVNULL, 
    bufsize=10**7
)

window_name = "AR Red Crystal Tracking (mpv Backend)"
cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)

dictionary = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_4X4_50)

try:
    parameters = cv2.aruco.DetectorParameters()
    detector = cv2.aruco.ArucoDetector(dictionary, parameters)
    legacy_mode = False
except AttributeError:
    parameters = cv2.aruco.DetectorParameters_create()
    legacy_mode = True

print("AR-Tracking mit mpv-Backend gestartet. Drücke 'q' oder 'ESC' zum Beenden.")

frame_bytes = WIDTH * HEIGHT * 3

while True:
    # Exakt die Bytes für ein einzelnes Frame aus dem mpv-Datenstrom lesen
    raw_frame = process.stdout.read(frame_bytes)
    
    if len(raw_frame) != frame_bytes:
        print("Stream-Ende oder Verbindung unterbrochen.")
        break

    # In ein OpenCV NumPy-Array umwandeln
    frame = np.frombuffer(raw_frame, dtype=np.uint8).reshape((HEIGHT, WIDTH, 3)).copy()

    # Marker erkennen
    if not legacy_mode:
        corners, ids, _ = detector.detectMarkers(frame)
    else:
        corners, ids, _ = cv2.aruco.detectMarkers(frame, dictionary, parameters=parameters)

    if ids is not None:
        for i, marker_id in enumerate(ids.flatten()):
            if marker_id == TARGET_ID:
                corners_2d = corners[i][0].astype(np.float32)

                success, rvec, tvec = cv2.solvePnP(
                    marker_3d_edges, corners_2d, camera_matrix, dist_coeffs
                )

                if success:
                    img_pts, _ = cv2.projectPoints(
                        crystal_3d_pts, rvec, tvec, camera_matrix, dist_coeffs
                    )
                    img_pts = np.int32(img_pts.reshape(-1, 2))

                    overlay = frame.copy()
                    for face in faces:
                        pts = img_pts[face]
                        cv2.fillPoly(overlay, [pts], (20, 20, 180))

                    cv2.addWeighted(overlay, 0.4, frame, 0.6, 0, frame)

                    for edge in edges:
                        pt1 = tuple(img_pts[edge[0]])
                        pt2 = tuple(img_pts[edge[1]])
                        cv2.line(frame, pt1, pt2, (0, 0, 100), 1, cv2.LINE_AA)

                    cv2.polylines(frame, [np.int32(corners_2d)], True, (0, 255, 0), 2)

    cv2.imshow(window_name, frame)

    # Da mpv bereits das Timing übernimmt, reicht cv2.waitKey(1) völlig aus
    key = cv2.waitKey(1) & 0xFF
    if key == ord('q') or key == 27:
        break

process.terminate()
cv2.destroyAllWindows()