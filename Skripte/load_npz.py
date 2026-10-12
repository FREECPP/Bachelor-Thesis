"""Auslesen der Kameramatrix sowie Verzerrungskoeffizienten

Nimmt eine .npz-Datei als String entgegen, und extrahiert dort die 
Kamera-Matrix, sowie die Verzerrungskoeffizienten, die Aufschluss über die 
Position der Kamera in relation zum Marker liefern. 
Die .npz-Datei ist das Ergebnis der Kamerakalibrierung.

Abhängigkeiten:
    numpy
"""

import numpy as np

def get_matrix_and_dist_coeffs(npz_path:str):
    """Auslesen der Kameramatrix sowie Verzerrungskoeffizienten

    Nimmt eine .npz-Datei als String entgegen, und extrahiert dort die 
    Kamera-Matrix, sowie die Verzerrungskoeffizienten, die Aufschluss über die 
    Position der Kamera in relation zum Marker liefern. 
    Die .npz-Datei ist das Ergebnis der Kamerakalibrierung.

    Args: 
        npz_path: Pfad zu der .npz-Datei die als Ergebnis aus der 
        Kamerakalibrierung hervorgeht, str

    Returns:
        camera_matrix: 3x3-Matrix, welche Brennweite und die Koordinate des 
        Bildmittelpunkts enthält
        dist_coeffs: Liste mit 5 Einträgen mit der radialen Verzerrung sowie der 
        tangentialen Verzerrung

    Raises: 
        KeyError: Wenn camera_matrix oder dist_coeffs leer ist
    """
    with np.load(npz_path) as data: 
        camera_matrix = data.get("camera_matrix") if "camera_matrix" in data else data.get("mtx")
        dist_coeffs = data.get("dist_coeffs") if "dist_coeffs" in data else data.get("dist")

        if camera_matrix is None or dist_coeffs is None:
            raise KeyError(
                f"Konnte Kamera-Matrix oder Verzerrungskoeffizienten in '{npz_path}' nicht finden. "
                f"Enthaltene Schlüssel: {list(data.keys())}"
            ) 
    return camera_matrix, dist_coeffs
