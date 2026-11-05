"""Repère chaque image de base/components dans base/dark.png (toutes ses occurrences).

Sortie : base/positions.json = {nom: [[x, y, w, h], ...]}. Une image identique à une
autre (ex. ZoomIn/ZoomOut dans le thème Dark) a donc plusieurs emplacements.
"""
import json
from pathlib import Path
import numpy as np
from PIL import Image

ICI = Path(__file__).parent

def main():
    D = np.array(Image.open(ICI / 'base' / 'dark.png').convert('RGBA')).astype(np.int16)
    H, W = D.shape[:2]
    pos = {}
    for f in sorted((ICI / 'base' / 'components').glob('*.png')):
        c = np.array(Image.open(f).convert('RGBA')).astype(np.int16)
        h, w = c.shape[:2]
        m = c[..., 3] > 0
        if not m.any():  # image vide dans ce thème (ex. Options)
            pos[f.stem] = []
            continue
        # Premier pixel opaque comme ancre, puis comparaison complète
        ay, ax = map(int, np.argwhere(m)[0])
        cand = np.argwhere((D[ay:H - h + 1 + ay, ax:W - w + 1 + ax] == c[ay, ax]).all(-1))
        trouves = []
        for y, x in cand:
            if np.abs(D[y:y + h, x:x + w][m] - c[m]).max() <= 2:
                trouves.append([int(x), int(y), w, h])
        pos[f.stem] = trouves
    (ICI / 'base' / 'positions.json').write_text(json.dumps(pos, indent=0))
    print(len(pos), 'images ;', 'introuvables :', [n for n, v in pos.items() if not v])

if __name__ == '__main__':
    main()
