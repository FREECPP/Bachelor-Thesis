"""Zieht relevante Informationen aus einer OBJ-Datei um das Objekt anzeigen zu 
können.

Eine OBJ-Datei kann z.B in Fusion360 oder anderen CAD-Prgrammen erstellt werden.
Der Pfad des erstellten Objekts, muss dann nur dieser Funktion übergeben werden.
Ergebnis sind dann die Punkte, Flächen und Kanten mithilfe dessen das Objekt in 
openCV nachgezeichnet werden kann. 

Abhängigkeiten: 
    trimesh, numpy


"""
import trimesh
import numpy as np

def get_mesh_data(obj_path:str):
    """Zieht relevante Informationen aus einer OBJ-Datei um das Objekt anzeigen 
    zu können.

    Eine OBJ-Datei kann z.B in Fusion360 oder anderen CAD-Prgrammen erstellt 
    werden.
    Der Pfad des erstellten Objekts, muss dann nur dieser Funktion übergeben 
    werden.
    Ergebnis sind dann die Punkte, Flächen und Kanten mithilfe dessen das Objekt 
    in openCV nachgezeichnet werden kann.

    Args: 
        obj_path: Pfad zur OBJ-Datei, str

    Returns: 
        points: Punkte des Objekts also bsp. Eckpunkte etc.  
        faces: Flächen des Objekts
        edges: Kanten des Objekts
    """
    mesh = trimesh.load(obj_path, force="mesh")
    mesh.apply_scale(0.001)  # mm in Meter umrechnen

    points = np.array(mesh.vertices, dtype=np.float32)
    faces = mesh.faces.tolist()
    edges = mesh.edges_unique   
    return points, faces, edges
