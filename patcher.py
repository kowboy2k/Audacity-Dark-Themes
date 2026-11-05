"""Remplace le thème « Sombre » intégré d'Audacity par un de nos thèmes.

Le thème Custom force menus et barre de titre en clair ; le thème Sombre intégré,
lui, les passe en sombre. Les octets du PNG sont écrits dans lib-theme-resources.dll
sous forme d'instructions `mov byte ptr [rsp+disp], imm8` : on remplace les imm8.

Usage : python patcher.py <Theme> <dll_source> <dll_sortie>
        python patcher.py --origine <dll>   (code 0 si la DLL porte le thème Sombre officiel)
"""
import hashlib
import re
import struct
import sys
from pathlib import Path

ICI = Path(__file__).parent
MOV = re.compile(rb'\xc6\x44\x24(.)(.)|\xc6\x84\x24(.{4})(.)', re.S)
DEBUT = re.compile(rb'\xc6\x44\x24.\x89\xc6\x44\x24.\x50\xc6\x44\x24.\x4e', re.S)

def decode(d, pos):
    """Renvoie {indice_octet: offset_fichier_de_l_imm8} pour un bloc d'image."""
    places, base = {}, None
    while (m := MOV.match(d, pos)):
        if m.group(1) is not None:
            disp, imm = m.group(1)[0], m.start(2)
        else:
            disp, imm = struct.unpack('<I', m.group(3))[0], m.start(4)
        base = disp if base is None else base
        places[disp - base] = imm
        pos = m.end()
    return places

def bloc_sombre(d):
    ref = (ICI / 'base' / 'dark.png').read_bytes()
    for m in DEBUT.finditer(d):
        places = decode(d, m.start())
        octets = bytes(d[places[i]] for i in range(len(places)))
        if octets[:len(ref)] == ref or octets.rstrip(b'\0') == ref.rstrip(b'\0'):
            return places
    # Déjà patché : le bloc sombre est le 2e (ordre classic, dark, high-contrast, light)
    return decode(d, list(DEBUT.finditer(d))[1].start())

def main(theme, src, dst):
    d = bytearray(Path(src).read_bytes())
    places = bloc_sombre(d)
    png = (ICI / 'base' / 'dark.png').read_bytes() if theme == 'Origine' \
        else (ICI / 'themes' / theme / 'ImageCache.png').read_bytes()
    n = len(places)
    if len(png) > n:
        sys.exit(f'PNG trop gros : {len(png)} > {n} octets')
    png = png + b'\0' * (n - len(png))
    for i, off in places.items():
        d[off] = png[i]
    assert bytes(d[places[i]] for i in range(n)) == png
    Path(dst).write_bytes(d)
    print(f'ok {theme} ({n} octets)')

# Empreinte du bloc Sombre tel que livré dans Audacity 3.5.1 (base/dark.png a été
# réenregistré à l'extraction : même image, octets différents)
SHA256_SOMBRE_351 = 'f6ff343c3381a4ab7aed31b702c2d38f1cef687f9c5950bff05cfb79538e7868'

def est_origine(chemin):
    """Vrai si la DLL contient le thème Sombre officiel d'Audacity 3.5.1 (jamais patchée)."""
    d = Path(chemin).read_bytes()
    for m in DEBUT.finditer(d):
        places = decode(d, m.start())
        bloc = bytes(d[places[i]] for i in range(len(places)))
        if hashlib.sha256(bloc).hexdigest() == SHA256_SOMBRE_351:
            return True
    return False

if __name__ == '__main__':
    if sys.argv[1] == '--origine':
        sys.exit(0 if est_origine(sys.argv[2]) else 1)
    main(*sys.argv[1:4])
