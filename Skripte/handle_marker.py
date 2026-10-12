"""Wie soll mit dem ArUco-Marker umgegangen werden, sobald dieser erkannt wurde.

Zeichnet Objekte über die Marker und ermöglicht das Senden von Pakten wenn der 
Abstand zu einem Marker unter einen gewissen Wert fällt. 

Abhängigkeiten: 
    numpy, cv2, tcp_test

"""           
import numpy as np
import cv2
import tcp_test as tt

def handle_marker(corners,index,  crystal_3d_pts, marker_3d_edges,
                camera_matrix, dist_coeffs, frame,
                faces, edges):
    """Zeichnen des Objekts über den Marker. 

    Bestimmt zunächst aus Kamera-Matrix und Verzerrungskoeffizienten die Lage
    des Markers in relation zu der Kamera und Zeichnet anschließend basierend 
    auf den Informationen aus der OBJ-Datei das Objekt über den Marker. 

    Args: 
        corners: Eckpunkte aller erkannten Marker im Bild (Ausgabe von
            detectMarkers). Pro Marker ein Array der Form (1, 4, 2) mit den
            vier Ecken als Pixelkoordinaten (x, y).
        index: Position des Zielmarkers in corners (und ids). Wählt aus,
            welcher der erkannten Marker verarbeitet wird.
        crystal_3d_pts: Eckpunkte des 3D-Objekts, Array der Form (N, 3),float32.
            Koordinaten in Metern im Koordinatensystem des Markers.
        marker_3d_edges: Die vier Ecken des Markers in 3D, Array der 
            Form (4, 3),float32, in Metern. Liegen in der Ebene Z = 0 in der Reihenfolge
            oben links, oben rechts, unten rechts, unten links.
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

    Returns: 
        tvec[2][0]: Abstand zwischen Kamera und Marker in Metern (Tiefe in
            Blickrichtung der Kamera) als float. None, wenn die Lage des
            Markers nicht bestimmt werden konnte.

    """ 
    
    corners_2d = corners[index][0].astype(np.float32)

    # Position des Markers relativ zur Kamera bestimmen
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
        return float(tvec[2][0])

def send_packet_by_distance(dist, triggered, send_below=0.5, reset_above=0.6):
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
            tt.send_packet()
        except Exception:
            print("ESP kann nicht erreicht werden")
        return True
    if dist is None or dist > reset_above:
        return False
    return triggered