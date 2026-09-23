import glob
import os
import cv2
import numpy as np
from PIL import Image, ImageOps

# ==============================================================================
# 1. PARAMETER & BOARD DEFINIEREN                                              =
#   Hierraus wird ein ideales Abbild des Schachbretts erstellt                 =
# ==============================================================================

# verwendetes ArUco-Dicitionary angeben
dictionary = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_6X6_250)

# Board-Eigenschaften (Spalten, Zeilen, Quadratgroesse in m, Markergroesse in m)
board = cv2.aruco.CharucoBoard(
    (5, 7),
    squareLength=0.03972,
    markerLength=0.02000,
    dictionary=dictionary,
)

# Pfadnamen für Bildverwaltung
img_path = "bilder_für_kalibrierung/Pi_lens_position_4_0/"
output_dir = os.path.join(img_path, "detection_results")
os.makedirs(output_dir, exist_ok=True)  # Ordner fuer Kontrollbilder erstellen

# ChArUco-Detektor anlegen (um Schnittpunkte zu erkennnen) 
charuco_detector = cv2.aruco.CharucoDetector(board)

# Listen anlegen für Schnittpunkte, Ids und namen gültiger Bilder
all_charuco_corners = []
all_charuco_ids = []
valid_images = []
img_size = None

# ==============================================================================
# 2. BILDER EINLESEN & MERKMALE ERFASSEN                                       =
# ==============================================================================

# bilder aus Ordner holen
images = glob.glob(os.path.join(img_path, "*.jpeg")) + glob.glob(
    os.path.join(img_path, "*.jpg")
)

# Fehlerbehandlung falls keine Bilder im Ordner gefunden werden
if not images:
    print("Fehler: Keine Bilder im angegebenen Ordner gefunden!")
    exit()

print(f"{len(images)} Bilder gefunden, starte Verarbeitung...")


for fname in images:
    # Einheitliches Einlesen über PIL inklusive EXIF-Drehungskorrektur
    pil_img = Image.open(fname)
    pil_img = ImageOps.exif_transpose(pil_img)
    img = cv2.cvtColor(np.array(pil_img), cv2.COLOR_RGB2BGR)

    # In Graustufen umwandeln
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

    if img_size is None:
        img_size = (gray.shape[1], gray.shape[0])

    # ChArUco-Ecken erkennen (Koordianten (u,v) abspeichern, sowie IDs) ?????
    charuco_corners, charuco_ids, marker_corners, marker_ids = (
        charuco_detector.detectBoard(gray)
    )

    # Mindestens 4 Ecken fuer gueltige Mathematik erforderlich(PnP-Problem und Homographie-Berechnung)
    is_valid = charuco_corners is not None and len(charuco_corners) >= 4

    # Wenn das Bild als gültig klassifiziert wurde werden die Koordinaten, sowie die IDs abgespiechert
    if is_valid:
        all_charuco_corners.append(charuco_corners)
        all_charuco_ids.append(charuco_ids)
        valid_images.append(fname)  
        print(f" [OK] {os.path.basename(fname)}: {len(charuco_corners)} ChArUco-Ecken erkannt.")
    else:
        print(f" [ÜBERSPRUNGEN] {os.path.basename(fname)}: Zu wenige Ecken erkannt.")

# ==============================================================================
# Visualisierung & Kontrollbild abspeichern                                    = 
#   zum prüfen wie die Bilder vom Programm erkannt wurden, werden wird die     = 
#   Erkennung auf Kontrollbildern graphisch visualisiert                       =
# ==============================================================================
        
    img_annotated = img.copy()

    # ArUco-Marker zeichnen
    if marker_ids is not None:
        try:
            cv2.aruco.drawDetectedMarkers(
                img_annotated,
                marker_corners,
                marker_ids,
                borderColor=(0, 255, 0),
            )
        except cv2.error:
            pass

    # ChArUco-Ecken zeichnen 
    if (charuco_corners is not None and charuco_ids is not None and len(charuco_corners) > 0):
        try:
            # Reshape stellt sicher, dass die Punktanzahl fuer C++ übereinstimmt
            corners_reshaped = charuco_corners.reshape(-1, 1, 2)
            ids_reshaped = charuco_ids.reshape(-1, 1)

            cv2.aruco.drawDetectedCornersCharuco(
                image=img_annotated,
                charucoCorners=corners_reshaped,
                charucoIds=ids_reshaped,
                cornerColor=(255, 0, 0),
            )
        except cv2.error as e:
            # Stürzt bei Darstellungsproblemen nicht mehr ab, sondern führt die Kalibrierung fort
            pass

    # Sauberer Dateiname im Unterordner
    base_name = os.path.basename(fname)
    output_filename = os.path.join(output_dir, f"check_{base_name}")
    cv2.imwrite(output_filename, img_annotated) 


# ==============================================================================
# 3. ERST-KALIBRIERUNG
# ==============================================================================
if len(all_charuco_corners) < 5:
    print(
        "\nFehler: Zu wenige brauchbare Bilder für eine präzise Kalibrierung."
    )
    exit()

print("\nBerechne Erst-Kalibrierungsmatrix...")

all_obj_points = []
all_img_points = []

# Konvertiere ChArUco-Ecken in 3D-Objektpunkte und 2D-Bildpunkte
for corners, ids in zip(all_charuco_corners, all_charuco_ids):
    # 2D-Koordinaten corners wird über matchImagePoints auf die 3D-Koordinaten gemapped damit die zuordung zwischen realer Weltkoordinaten und Bildkoordinate besteht
    obj_pts, img_pts = board.matchImagePoints(corners, ids)
    all_obj_points.append(obj_pts)
    all_img_points.append(img_pts)

# Erst-Kalibrierung durchführen (ermitteln der Kameraposition rvecs, tvecs und den intrinsischen Parametern)
(
    init_error,
    camera_matrix,
    dist_coeffs,
    rvecs,
    tvecs,
) = cv2.calibrateCamera(
    objectPoints=all_obj_points,
    imagePoints=all_img_points,
    imageSize=img_size,
    cameraMatrix=None,
    distCoeffs=None,
)

print(f"Ursprünglicher Gesamtwert (RMS): {init_error:.4f} Pixel")


# ==============================================================================
# 4. BILDER MIT HOHEM FEHLER FILTERN (AUSSREISSER-REDUKTION)                   =
#   Optimierung versucht den Fehler über alle Bilder hinweg, möglichst gering  = 
#   zu halten. Befindet sich ein schlechtes Bild unter 20 guten, verzieht sich =
#   das Modell für alle anderen Bilder um den großen Fehler auszugleichen.     = 
#   Um eine verschlechterung der Gesammtkalibrierung auszuschließen, werden    =
#   die besonders schlechten Bilder hier aussortiert                           =    
# ==============================================================================
MAX_ALLOWED_ERROR = 5.0  # Schwellenwert in Pixeln (1.0 px ist ein guter Zielwert fuer wissenschaftliche Arbeiten)

filtered_obj_points = []
filtered_img_points = []
kept_image_names = []

print(
    f"\nFiltere Bilder mit einem Fehler > {MAX_ALLOWED_ERROR:.2f} Pixeln..."
)

for i in range(len(all_obj_points)):

    # 3D-Punkte zurück ins Bild projizieren anhand der in der Erst-Kalibrierung berechneten Parameter
    # An dieser Stelle sollten die Tatsächlichen Schnittpunkte liegen wenn die Kalibrierung optimal wäre
    img_pts_reproj, _ = cv2.projectPoints(
        all_obj_points[i], rvecs[i], tvecs[i], camera_matrix, dist_coeffs
    )

    # Mittleren euklidischen Abstand (Fehler) in Pixeln pro Punkt berechnen
    pts_true = all_img_points[i].reshape(-1, 2) # Tatsächliche Punkte
    pts_proj = img_pts_reproj.reshape(-1, 2) # Projezierte Punkte

    # Unterschied zwischen Tatsächlichen Punkten und Projezierten Punkten berechnen (Für jeden Schnittpunkt im Bild -> Anschließend Durchschnitt von allen nehmen)
    error = np.mean(np.linalg.norm(pts_true - pts_proj, axis=1))

    img_name = os.path.basename(valid_images[i])
    
    # Fallunterscheidung ob das Bild genau genug ist um in die Kalibrierung mit einbezogen zu werden
    if error <= MAX_ALLOWED_ERROR:
        filtered_obj_points.append(all_obj_points[i])
        filtered_img_points.append(all_img_points[i])
        kept_image_names.append(valid_images[i])
        print(f" [BEHALTEN]    {img_name}: {error:.4f} px")
    else:
        print(f" [AUSSORTIERT] {img_name}: {error:.4f} px")


# ==============================================================================
# 5. ZWEIT-KALIBRIERUNG (RE-CALIBRATION) & SPEICHERN                           =
# ==============================================================================

# Prüfen ob genügen Bilder für eine aussagekräftige Kalibrierung vorliegen
if len(filtered_obj_points) >= 5:
    (
        final_error,
        camera_matrix,
        dist_coeffs,
        rvecs,
        tvecs,
    ) = cv2.calibrateCamera(
        objectPoints=filtered_obj_points,
        imagePoints=filtered_img_points,
        imageSize=img_size,
        cameraMatrix=None,
        distCoeffs=None,
    )

    print("\n==================================================")
    print("NEUKALIBRIERUNG ERFOLGREICH!")
    print(f"Neuer Gesamtfehler (RMS): {final_error:.4f} Pixel")
    print(
        f"Verwendete Bilder:       {len(filtered_obj_points)} von {len(all_obj_points)}"
    )
    print("==================================================")

    # Ergebnis-Matrix abspeichern
    np.savez(
        "camera_calibration.npz",
        camera_matrix=camera_matrix,
        dist_coeffs=dist_coeffs,
        reprojection_error=final_error,
    )
    print("\nKalibrierung erfolgreich in 'camera_calibration.npz' gespeichert.")
else:
    print(
        "\nFehler: Nach dem Filtern bleiben weniger als 5 Bilder für die Neukalibrierung übrig."
    )
    print(
        "Empfehlung: Erhöhe 'MAX_ALLOWED_ERROR' etwas oder erstelle schärfere Fotos."
    )