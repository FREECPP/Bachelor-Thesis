import cv2
import numpy as np

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

# ==============================================================================
# 2. 3D-GEOMETRIE DES AR-KRISTALLS DEFINIEREN (in Metern, Markerzentriert)
# ==============================================================================
# 3D-Ecken des ArUco-Markers (Z=0 Ebene)
marker_3d_edges = np.array([
    [-MARKER_SIZE / 2,  MARKER_SIZE / 2, 0],
    [ MARKER_SIZE / 2,  MARKER_SIZE / 2, 0],
    [ MARKER_SIZE / 2, -MARKER_SIZE / 2, 0],
    [-MARKER_SIZE / 2, -MARKER_SIZE / 2, 0]
], dtype=np.float32)

# 3D-Punkte des Kristalls (Doppelpyramide/Oktaeder), schwebt auf der Z-Achse nach oben
h_bottom = MARKER_SIZE * 0.8  # Untere Spitze schwebt leicht über dem Marker
h_middle = MARKER_SIZE * 1.6  # Breiter Gürtel in der Mitte
h_top    = MARKER_SIZE * 2.4  # Obere Spitze
w        = MARKER_SIZE * 0.4  # Halbe Breite des Kristalls

crystal_3d_pts = np.array([
    [ 0,  0, h_top],     # Index 0: Obere Spitze
    [-w, -w, h_middle],  # Index 1: Mitte Links-Unten
    [ w, -w, h_middle],  # Index 2: Mitte Rechts-Unten
    [ w,  w, h_middle],  # Index 3: Mitte Rechts-Oben
    [-w,  w, h_middle],  # Index 4: Mitte Links-Oben
    [ 0,  0, h_bottom]   # Index 5: Untere Spitze
], dtype=np.float32)

# Kantenverbindungen (Indizes der Kristallpunkte)
edges = [
    (0, 1), (0, 2), (0, 3), (0, 4),  # Obere Pyramide
    (1, 2), (2, 3), (3, 4), (4, 1),  # Mittlerer Ring
    (5, 1), (5, 2), (5, 3), (5, 4)   # Untere Pyramide
]

# Polygon-Dreiecke für die Seitenflächen
faces = [
    [0, 1, 2], [0, 2, 3], [0, 3, 4], [0, 4, 1],  # Oben
    [5, 2, 1], [5, 3, 2], [5, 4, 3], [5, 1, 4]   # Unten
]

# ==============================================================================
# 3. LIVE-STREAM & TRACKING LOOP
# ==============================================================================
cap = cv2.VideoCapture(0)

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
                    # 3D-Punkte des Kristalls auf die 2D-Bildebene projizieren
                    img_pts, _ = cv2.projectPoints(
                        crystal_3d_pts, rvec, tvec, camera_matrix, dist_coeffs
                    )
                    img_pts = np.int32(img_pts.reshape(-1, 2))

                    # 1. Halbtransparente rote Flächen zeichnen
                    overlay = frame.copy()
                    for face in faces:
                        pts = img_pts[face]
                        cv2.fillPoly(overlay, [pts], (20, 20, 180)) # Dunkelrot als Basis
                    
                    # Overlay mit Originalbild blenden (40% Deckkraft)
                    cv2.addWeighted(overlay, 0.4, frame, 0.6, 0, frame)

                    # 2. Leuchtend rote Kanten darüberzeichnen
                    for edge in edges:
                        pt1 = tuple(img_pts[edge[0]])
                        pt2 = tuple(img_pts[edge[1]])
                        cv2.line(frame, pt1, pt2, (0, 0, 255), 2, cv2.LINE_AA)

                    # Markerumriss hervorheben
                    cv2.polylines(frame, [np.int32(corners_2d)], True, (0, 255, 0), 2)

    cv2.imshow(window_name, frame)

    key = cv2.waitKey(30) & 0xFF
    if key == ord('q') or key == 27:
        break

cap.release()
cv2.destroyAllWindows()
cv2.waitKey(1)