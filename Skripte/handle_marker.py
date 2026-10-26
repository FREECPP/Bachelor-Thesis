"""Wie soll mit dem ArUco-Marker umgegangen werden, sobald dieser erkannt wurde.

Zeichnet Objekte über die Marker und ermöglicht das Senden von Pakten wenn der 
Abstand zu einem Marker unter einen gewissen Wert fällt. 

Abhängigkeiten: 
    numpy, cv2, tcp_test, json

"""           
import numpy as np
import cv2
import tcp_test as tt
import json

MIN_DEPTH = 0.03 # Objektpunkte näher als 3 cm an der Kamera weden nicht 
# gezeichnet
MAZE_W, MAZE_H = 10, 5

def estimate_marker_pose(corners, index, marker_3d_edges, camera_matrix,
                         dist_coeffs):
    """
    Args: 
        corners: Eckpunkte aller erkannten Marker im Bild (Ausgabe von
            detectMarkers). Pro Marker ein Array der Form (1, 4, 2) mit den
            vier Ecken als Pixelkoordinaten (x, y).
        index: Position des Zielmarkers in corners (und ids). Wählt aus,
            welcher der erkannten Marker verarbeitet wird.
        marker_3d_edges: Die vier Ecken des Markers in 3D, Array der 
            Form (4, 3),float32, in Metern. Liegen in der Ebene Z = 0 in der Reihenfolge
            oben links, oben rechts, unten rechts, unten links.
        camera_matrix: Kamera-Matrix (3, 3) mit Brennweiten und Bildmittelpunkt
            in Pixeln, aus der Kalibrierung.
        dist_coeffs: Verzerrungskoeffizienten (1, 5) in der Reihenfolge
            [k1, k2, p1, p2, k3], aus derselben Kalibrierung.

    Returns: 
        rvec: Wie der Marker zur Kamera gedreht ist
        tvec: Wie der Marker zur Kamera verschoben ist
        distance: Abstand zwischen Kamera und Marker in Metern (Tiefe in
            Blickrichtung der Kamera) als float. None, wenn die Lage des
            Markers nicht bestimmt werden konnte.
        corners_2d:Die vier Ecken eines einzelnen Markers im Kamerabild in 
            Pixeln 

    """
    
    corners_2d = corners[index][0].astype(np.float32)

    # Position des Markers relativ zur Kamera bestimmen
    success, rvec, tvec = cv2.solvePnP(
        marker_3d_edges, corners_2d, camera_matrix, dist_coeffs,
        flags=cv2.SOLVEPNP_IPPE_SQUARE
    )

    if not success: 
        return None

    distance = float(tvec[2][0])

    return rvec, tvec, distance, corners_2d
    

def draw_object(frame, rvec, tvec, corners_2d, crystal_3d_pts, faces, edges,
                 camera_matrix, dist_coeffs):
    """
    Args: 
        crystal_3d_pts: Eckpunkte des 3D-Objekts, Array der Form (N, 3),float32.
            Koordinaten in Metern im Koordinatensystem des Markers.
        camera_matrix: Kamera-Matrix (3, 3) mit Brennweiten und Bildmittelpunkt
            in Pixeln, aus der Kalibrierung.
        dist_coeffs: Verzerrungskoeffizienten (1, 5) in der Reihenfolge
            [k1, k2, p1, p2, k3], aus derselben Kalibrierung.
        frame: Aktuelles Kamerabild als BGR-Array (Höhe, Breite, 3), uint8.
            Wird direkt verändert, das Objekt wird hineingezeichnet.
        faces: Dreiecke des 3D-Objekts als Liste von Listen, jede mit drei
            Indizes in crystal_3d_pts.
        edges: Kanten des 3D-Objekts, Array der Form (E, 2) mit je zwei
            Indizes in crystal_3d_pts.
        corners_2d: Die vier Ecken eines einzelnen Markers im Kamerabild in 
            Pixeln

    Returns: 
        
    """
    # Tiefe aller Objektpunkte im Kamerakoordinatendaten. Liegt einer zu nah an
    # der Kamera oder dahinter, wird nicht gezeichnet (sonst absurde Pixelwerte)
    rot, _ = cv2.Rodrigues(rvec)
    cam_pts = crystal_3d_pts @ rot.T + tvec.reshape(1,3)
    if cam_pts[:, 2].min() < MIN_DEPTH:
        return 

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

def send_packet_by_distance(dist, triggered, value, send_below=0.5, reset_above=0.6):
    """Sendet ein Netzwerkpaket an den ESP32 abhängig vom Abstand zum Marker.
    
    Wenn sich der Roboter einem Marker nähert sollen verschiedene Aktionen 
    ausgelöst werden, der Roboter sammelt Powerups quasi ein. 

    Args: 
        dist: gemessener Abstand in Metern oder None (kein Marker).
        triggered: ob für diese Annäherung schon gesendet wurde.

    Returns: 
        triggered: Gibt den neuen Zustand von triggered zurück.
    """
   
    if dist is not None and dist < send_below and not triggered:
        try:
            tt.send_packet(value)
        except Exception as e:
            print(f"ESP kann nicht erreicht werden{e}")
        return True
    if dist is None or dist > reset_above:
        return False
    return triggered

def load_labyrinth_config(path: str) -> dict:
    """Einmal beim Start aufrufen. Wirft ValueError bei ungültiger Konfiguration."""
    with open(path, "r", encoding="utf-8") as file:
        data = json.load(file)
    factor = data.get("factor")
    if not isinstance(factor, (int, float)) or factor <= 0:
        raise ValueError(f"factor muss > 0 sein, ist {factor!r}")
    for key, value in data.get("markers", {}).items():
        if len(value) != 3 or value[2] not in ("n", "e", "s", "w"):
            raise ValueError(f"Marker {key}: ungültiger Eintrag {value!r}")
    return data

def calc_pac_pos(rvec, tvec, marker_id, config: dict):
    """Zelle (x, y) der Kamera aus der Pose eines Wandmarkers.

    Der Buchstabe im Marker-Eintrag gibt an, in welche Himmelsrichtung die
    Vorderseite des Markers zeigt (x = Osten, y = Norden).
    """
    key = str(int(marker_id))
    if key not in config["markers"]:
        raise ValueError(key)
    mx, my, direction = config["markers"][key]
    factor = config["factor"]

    rot, _ = cv2.Rodrigues(rvec)          # Wandelt rvec (Rotationsvektor) in eine 3x3 Rotationsmatrix um -> Ermöglicht direktes verrechnen mit Punkten durch das Skalarprodukt
    cam = (-rot.T @ tvec).ravel()        # setzt die Punkte aus dem Kamerakoordinatensystem in das Markerkoordinatensystem (x rechts, y oben, z aus der Wand) -> rvec und tvec gehen ausgehend vom Kamerakoordinatensystem

    match direction:
        case "n": 
            offset = (cam[0],  -cam[2])
        case "e": 
            offset = ( -cam[2],  -cam[0])
        case "s": 
            offset = ( -cam[0], cam[2])
        case "w": 
            offset = (cam[2], cam[0])

    x = round((mx * factor + offset[0]) / factor)
    y = round((my * factor + offset[1]) / factor)
    if not (0 <= x < MAZE_W and 0 <= y < MAZE_H):
        raise ValueError(f"Zelle ({x}, {y}) außerhalb des Labyrinths")
    return x, y