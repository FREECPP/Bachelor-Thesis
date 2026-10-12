import cv2
import numpy as np
import trimesh
import subprocess
import os
import tcp_test as tt
import load_npz as npz
import load_3d_geometry as geo
import handle_marker as hm
import mjpeg_server

# ==============================================================================
# 1. KALIBRIERUNGSDATEN & PARAMETER KONFIGURIEREN
# ==============================================================================
CALIB_FILE = "camera_calibration.npz"  # Dateipfad deiner .npz Datei
MARKER_SIZE = 0.05  # Physikalische Kantenlänge des Markers in Metern (z. B. 0.05m = 5 cm)
TARGET_ID = 0       # ID des Markers, über dem das Objekt schweben soll
WIDTH = 1280
HEIGHT = 720

# Laden der .npz-Datei & automatische Erkennung gängiger Variablen-Namen
camera_matrix, dist_coeffs = npz.get_matrix_and_dist_coeffs(CALIB_FILE)

# ==============================================================================
# 2. 3D-GEOMETRIE AUS FUSION 360 LADEN (.obj-Datei)
# ==============================================================================
OBJ_FILE = "Ar_test_obj.obj"
crystal_3d_pts, faces, edges = geo.get_mesh_data(OBJ_FILE)

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
# ---- Startet mpv im Hintergrund ----
# Befehl der ausfgeführt werden soll
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

# Erstellen eines Subprozesses mit dem oben definierten Befehl
process = subprocess.Popen(
    mpv_cmd,                    # Befehl von oben 
    stdout=subprocess.PIPE,     # Videodaten werden nicht aufs Terminal gebracht sondern landen in einer Pipe die über process.stdout.read(frame_bytes) ausgelsen wird 
    stderr=subprocess.DEVNULL,  # Fehlermeldungen werden unterdrückt 
    bufsize=10**7               # Großer Buffer soll verhindern, das die Pipe vollläuft
)

# ---- OpenCV-Fenster erstellen ----
window_name = "AR Red Crystal Tracking (mpv Backend)"
cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)     # Es wird ein Normales Fenster erzeugt, das vergrößert oder verkleinert werden kann

# ----  ArUco Detector einrichten ----
dictionary = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_4X4_50)   #Gibt an welches Dictionary genutzt wird  

# Verzweigung die prüft welche cv2 Version genutzt wird und demensprechend einen detector erstellt oder nicht 
try:
    parameters = cv2.aruco.DetectorParameters()
    detector = cv2.aruco.ArucoDetector(dictionary, parameters)
    legacy_mode = False
except AttributeError:
    parameters = cv2.aruco.DetectorParameters_create()
    legacy_mode = True

print("AR-Tracking mit mpv-Backend gestartet. Drücke 'q' oder 'ESC' zum Beenden.")

frame_bytes = WIDTH * HEIGHT * 3

marker_was_visible = False
triggered = False

# ---- MJPEG-server starten unter dem Port 8080 ----
# Besonders geeignet wenn der Stream auf einem Gerät läuft auf dem ich nicht viel coden kann sondern als Anzeigemedium nur einen Browser habe (VR-Brille, Handy, Tablet)
mjpeg_server.start(8080)

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
        corners, ids, _ = detector.detectMarkers(frame) # Zwei Arrays werden zurückgegeben eins für die gefundenen Ecken(Corners) und eins für die ID's dabei stehen jeweils zugehörige Ecken und ID's am selben Indexplatz
    else:
        corners, ids, _ = cv2.aruco.detectMarkers(frame, dictionary, parameters=parameters) # Zwei Arrays werden zurückgegeben eins für die gefundenen Ecken(Corners) und eins für die ID's dabei stehen jeweils zugehörige Ecken und ID's am selben Indexplatz

    dist = None
    if ids is not None:
        for i, marker_id in enumerate(ids.flatten()):
            if marker_id == TARGET_ID:
                
               dist = hm.handle_marker(corners, i, crystal_3d_pts, marker_3d_edges,
                                 camera_matrix, dist_coeffs, frame, faces, edges)
    triggered = hm.send_packet_by_distance(dist, triggered)

    mjpeg_server.update_frame(frame)
    cv2.imshow(window_name, frame)

    # Da mpv bereits das Timing übernimmt, reicht cv2.waitKey(1) völlig aus
    key = cv2.waitKey(1) & 0xFF
    if key == ord('q') or key == 27:
        break

process.terminate()
cv2.destroyAllWindows()