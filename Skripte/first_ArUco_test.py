import cv2

def generate_aruco_marker():
    # 1. Wörterbuch auswählen (DICT_4X4_50 ist sehr robust und ideal für den Einstieg)
    dictionary = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_4X4_50)

    # 2. Marker-ID und Bildgröße festlegen
    marker_id = 0       # ID des Markers (0 bis 49 bei DICT_4X4_50)
    marker_size = 600   # Auflösung in Pixeln (600x600 px)

    # 3. Marker generieren (Kompatibel mit allen OpenCV-Versionen)
    try:
        marker_image = cv2.aruco.generateImageMarker(dictionary, marker_id, marker_size)
    except AttributeError:
        marker_image = cv2.aruco.drawMarker(dictionary, marker_id, marker_size)

    # 4. Weißen Rand hinzufügen (Quiet Zone / Padding für Display-Anzeige)
    border_size = 60
    marker_with_border = cv2.copyMakeBorder(
        marker_image,
        border_size, border_size, border_size, border_size,
        cv2.BORDER_CONSTANT,
        value=[255, 255, 255]
    )

    # 5. Bild speichern
    filename = f"aruco_marker_id_{marker_id}.png"
    cv2.imwrite(filename, marker_with_border)
    print(f"Marker wurde erfolgreich unter '{filename}' gespeichert.")

def detect_aruco_marker():
    dictionary = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_4X4_50)

    # ArUco-Detektor initialisieren (OpenCV 4.7+)
    try:
        parameters = cv2.aruco.DetectorParameters()
        detector = cv2.aruco.ArucoDetector(dictionary, parameters)
        legacy_mode = False
    except AttributeError:
        parameters = cv2.aruco.DetectorParameters_create()
        legacy_mode = True

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        # Marker im aktuellen Frame suchen
        if not legacy_mode:
            corners, ids, rejected = detector.detectMarkers(frame)
        else:
            corners, ids, rejected = cv2.aruco.detectMarkers(frame, dictionary, parameters=parameters)

        # Wenn ein Marker erkannt wurde
        if ids is not None:
            # 1. Ränder des Markers grün einzeichnen
            cv2.aruco.drawDetectedMarkers(frame, corners, ids)
            
            # 2. Ausrichtung der ecken anzeigen (Erste Ecke ist oben links = Orientierung)
            for corner in corners:
                top_left = tuple(corner[0][0].astype(int))
                cv2.circle(frame, top_left, 8, (0, 0, 255), -1) # Rot punkt zeigt Orientierung oben-links

        cv2.imshow("ArUco Tracking Test", frame)

def debug_script():
    print("1. Versuche Kamera zu öffnen...")
    cap = cv2.VideoCapture(0)

    if not cap.isOpened():
        print("Fehler: Kamera konnte nicht geöffnet werden! Falscher Index?")
        exit()

    print("2. Kamera geöffnet. Lese ersten Frame...")
    ret, frame = cap.read()

    if ret:
        print("3. Frame erfolgreich empfangen! Zeige Fenster...")
        cv2.imshow("Test", frame)
        cv2.waitKey(0) # Wartet auf einen Tastendruck
    else:
        print("Fehler: Kein Frame empfangen.")

    cap.release()
    cv2.destroyAllWindows()
    print("4. Beendet.")
'''
source = 0

cap = cv2.VideoCapture(source)

if not cap.isOpened():
    print("Fehler: Kamera-Stream konnte nicht geöffnet werden.")
    exit()

while True: 
    ret, frame = cap.read()
    if not ret: 
        print("Frame konnte nicht empfangen werden")
        break

    cv2.imshow("IPhone Live-Feed", frame)
    
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

detect_aruco_marker()

cap.release()
cv2.destroyAllWindows()
'''
import cv2

# 1. Kamera öffnen
cap = cv2.VideoCapture(0)

if not cap.isOpened():
    print("Fehler: Kamera konnte nicht geöffnet werden.")
    exit()

# 2. Fenster erstellen & macOS-Event-Thread aktivieren (verhindert Lade-Maus)
window_name = "ArUco Tracking Test"
cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
cv2.startWindowThread() 

# 3. ArUco Dictionary & Detektor initialisieren
dictionary = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_4X4_50)

try:
    parameters = cv2.aruco.DetectorParameters()
    detector = cv2.aruco.ArucoDetector(dictionary, parameters)
    legacy_mode = False
except AttributeError:
    parameters = cv2.aruco.DetectorParameters_create()
    legacy_mode = True

print("Tracking gestartet. Drücke 'q' im Videofenster zum Beenden.")

while True:
    ret, frame = cap.read()
    if not ret:
        print("Konnte keinen Frame lesen.")
        break

    # Marker im Frame suchen
    if not legacy_mode:
        corners, ids, _ = detector.detectMarkers(frame)
    else:
        corners, ids, _ = cv2.aruco.detectMarkers(frame, dictionary, parameters=parameters)

    # Wenn Marker gefunden wurden, grün einzeichnen
    if ids is not None:
        cv2.aruco.drawDetectedMarkers(frame, corners, ids)

    # Frame anzeigen
    cv2.imshow(window_name, frame)

    # 30 ms Pause für reibungsloses Zeichnen unter macOS (Beenden mit 'q')
    key = cv2.waitKey(30) & 0xFF
    if key == ord('q') or key == 27:  # 'q' oder ESC-Taste
        break

# Ressourcen freigeben
cap.release()
cv2.destroyAllWindows()
cv2.waitKey(1)  # Schließt das Fenster sauber unter macOS
