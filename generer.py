"""Génère des thèmes sombres modernes pour Audacity 3.5 à partir du thème Dark officiel.

Entrées (base/) : dark.png (thème Dark extrait de lib-theme-resources.dll), names.txt
(ordre des couleurs), positions.json (emplacements des images, voir localiser.py).
Sortie : themes/<nom>/ImageCache.png.

Chaque image est traitée selon son rôle : fonds de boutons redessinés à plat et arrondis,
icônes de transport et curseurs de réglage redessinés, curseurs souris et logo intacts,
tout le reste recoloré par une rampe de gris teintée.
"""
import colorsys
import json
from pathlib import Path
from PIL import Image, ImageDraw

ICI = Path(__file__).parent
W, H = 440, 836
Y_COULEURS = 761
SUR = 4  # suréchantillonnage pour l'anticrénelage des formes

def hx(s):
    s = s.lstrip('#')
    return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4))

def mix(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))

THEMES = {
    'Minuit': dict(deep='#0b0f18', bg='#111723', surface='#182030', surface2='#212b3e', border='#2e3a52',
                   muted='#5f6d88', text_dim='#8593ad', text='#a3b0c6',
                   accent='#4da3ff', accent_light='#a6d0ff', rms='#2f7fd9', accent2='#ffb454'),
    # Contraste adouci : fonds moins noirs, textes et icônes en gris moyen
    'Graphite': dict(deep='#18181b', bg='#1e1e21', surface='#242428', surface2='#2d2d32', border='#3a3a40',
                     muted='#606067', text_dim='#85858c', text='#9d9da4',
                     accent='#3f9c90', accent_light='#7fbfb6', rms='#2d7d73', accent2='#c97a9f',
                     onde='#d9823b', onde_sel='#ecaa72', onde_rms='#a8612b'),
    'Amethyste': dict(deep='#0e0b14', bg='#14101c', surface='#1c1727', surface2='#262033', border='#372e49',
                      muted='#6c6184', text_dim='#9488ab', text='#b0a6c4',
                      accent='#a78bfa', accent_light='#d6c9ff', rms='#7c5ce0', accent2='#5eead4'),
    'Braise': dict(deep='#100c0a', bg='#17120f', surface='#201915', surface2='#2b221d', border='#3d3029',
                   muted='#7a685d', text_dim='#a8988c', text='#c4b5aa',
                   accent='#ff7a59', accent_light='#ffc2ad', rms='#d9542f', accent2='#facc15'),
}

def palette(p):
    p = {k: hx(v) for k, v in p.items()}
    # Forme d'onde : couleur propre si le thème en donne une, sinon l'accent
    p.setdefault('onde', p['accent'])
    p.setdefault('onde_sel', p['accent_light'])
    p.setdefault('onde_rms', p['rms'])
    p['sel'] = mix(p['surface2'], p['accent'], 0.22)
    p['sel_dark'] = mix(p['surface'], p['accent'], 0.14)
    # Rampe des gris : niveau d'origine (thème Dark) -> teinte cible
    p['ramp'] = [(0.00, p['deep']), (0.10, p['deep']), (0.20, p['bg']), (0.24, p['surface']),
                 (0.33, p['surface']), (0.40, p['surface2']), (0.50, p['border']),
                 (0.62, p['muted']), (0.80, p['text_dim']), (1.00, p['text'])]
    # Boutons : repos, survol, enfoncé, enfoncé survolé
    p['btn_up'] = mix(p['surface'], p['surface2'], 0.75)
    p['btn_hover'] = mix(p['surface2'], p['border'], 0.6)
    p['btn_down'] = mix(p['surface2'], p['accent'], 0.28)
    p['btn_down_hover'] = mix(p['surface2'], p['accent'], 0.38)
    p['icone_off'] = mix(p['surface2'], p['muted'], 0.55)
    # Textes (règle, indicateurs, libellés, noms de pistes) : plus foncés que les icônes
    p['police'] = mix(p['text'], p['muted'], 0.5)
    p['lecture'] = mix(hx('#5fbf7f'), p['text'], 0.25)
    p['enreg'] = mix(hx('#e05a5a'), p['text'], 0.15)
    return p

def gris(p, v):
    r = p['ramp']
    for (a, ca), (b, cb) in zip(r, r[1:]):
        if v <= b:
            return mix(ca, cb, 0 if b == a else (v - a) / (b - a))
    return r[-1][1]

def recolore(p, rgb):
    r, g, b = (c / 255 for c in rgb)
    h, s, v = colorsys.rgb_to_hsv(r, g, b)
    lum = 0.2126 * r + 0.7152 * g + 0.0722 * b
    base_gris = gris(p, lum)
    if s < 0.15:
        return base_gris
    deg = h * 360
    if 15 <= deg <= 55 or 190 <= deg <= 265:
        # Orange et bleu du thème d'origine -> accent du thème
        # (éclat plafonné à celui de l'accent, saturation réduite : icônes moins criardes)
        ah, as_, av = colorsys.rgb_to_hsv(*(c / 255 for c in p['accent']))
        nr, ng, nb = colorsys.hsv_to_rgb(ah, min(1, s * 0.45 + as_ * 0.35), min(v, av))
        coul = mix(tuple(round(c * 255) for c in (nr, ng, nb)), base_gris, 0.25)
    else:
        # Rouge enregistrement, vert lecture, etc. : sens conservé, éclat réduit
        coul = mix(rgb, base_gris, 0.5)
    t = min(1, (s - 0.15) / 0.15)
    return mix(base_gris, coul, t)

# --- Dessin -------------------------------------------------------------------------

def toile(w, h):
    return Image.new('RGBA', (w * SUR, h * SUR), (0, 0, 0, 0))

def reduit(im, w, h):
    return im.resize((w, h), Image.LANCZOS)

def fond_bouton(w, h, coul, coin, rayon, marge):
    """Bouton plat : rectangle arrondi sur la couleur du support (coins opaques)."""
    im = Image.new('RGBA', (w * SUR, h * SUR), coin + (255,))
    d = ImageDraw.Draw(im)
    m = marge * SUR
    d.rounded_rectangle([m, m, w * SUR - 1 - m, h * SUR - 1 - m], radius=rayon * SUR, fill=coul + (255,))
    return reduit(im, w, h)

def glyphe(nom, w, h, coul):
    """Icônes de transport, dessinées dans une boîte 16x16."""
    im = toile(w, h)
    d = ImageDraw.Draw(im)
    s = SUR * w / 16
    P = lambda pts: [(x * s, y * s) for x, y in pts]
    c = coul + (255,)
    if nom == 'Play':
        d.polygon(P([(4.5, 2.5), (4.5, 13.5), (13.5, 8)]), fill=c)
    elif nom == 'Pause':
        d.rounded_rectangle(P([(4, 3), (6.8, 13)]), radius=0.8 * s, fill=c)
        d.rounded_rectangle(P([(9.2, 3), (12, 13)]), radius=0.8 * s, fill=c)
    elif nom == 'Stop':
        d.rounded_rectangle(P([(3.5, 3.5), (12.5, 12.5)]), radius=1.6 * s, fill=c)
    elif nom == 'Record':
        d.ellipse(P([(3, 3), (13, 13)]), fill=c)
    elif nom == 'Rewind':
        d.rounded_rectangle(P([(3, 3), (5, 13)]), radius=0.6 * s, fill=c)
        d.polygon(P([(13, 3), (13, 13), (5.5, 8)]), fill=c)
    elif nom == 'FFwd':
        d.rounded_rectangle(P([(11, 3), (13, 13)]), radius=0.6 * s, fill=c)
        d.polygon(P([(3, 3), (3, 13), (10.5, 8)]), fill=c)
    return reduit(im, w, h)

def pastille(w, h, coul, anneau):
    """Curseur de réglage : rond plein avec un fin anneau."""
    im = toile(w, h)
    d = ImageDraw.Draw(im)
    r = min(w, h) * SUR * 0.30
    cx, cy = w * SUR / 2, h * SUR / 2
    d.ellipse([cx - r - SUR, cy - r - SUR, cx + r + SUR, cy + r + SUR], fill=anneau + (255,))
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=coul + (255,))
    return reduit(im, w, h)

# --- Rôles des images ------------------------------------------------------------------

BOUTONS = {  # préfixe -> état
    'UpButton': 'btn_up', 'HiliteUpButton': 'btn_hover',
    'DownButton': 'btn_down', 'HiliteButton': 'btn_down_hover',
    'MacUpButton': 'btn_up', 'MacHiliteUpButton': 'btn_hover',
    'MacDownButton': 'btn_down', 'MacHiliteButton': 'btn_down_hover',
}
TRANSPORT = ['Play', 'Pause', 'Stop', 'Record', 'Rewind', 'FFwd']

def role_bouton(nom):
    for pref in sorted(BOUTONS, key=len, reverse=True):
        if nom.startswith(pref):
            reste = nom[len(pref):]
            if reste in ('', 'Large', 'Small', 'Expand', 'ExpandSel'):
                return BOUTONS[pref], reste
    return None

def traite_image(p, nom, bloc, orig):
    w, h = bloc.size
    if nom.endswith('Cursor'):
        return None  # intacts
    if nom.startswith('Mac'):
        # Boutons propres au Mac, jamais affichés sous Windows : vidés pour alléger le PNG,
        # qui doit tenir dans la place du thème Sombre de la DLL
        return Image.new('RGBA', (w, h), (0, 0, 0, 0))
    rb = role_bouton(nom)
    if rb:
        etat, taille = rb
        coin_orig = orig.getpixel((0, 0))[:3]
        coin = p['surface'] if coin_orig == (64, 64, 64) else p['sel']
        rayon, marge = {'Large': (9, 3), 'Small': (6, 2), 'Expand': (4, 1), 'ExpandSel': (4, 1)}.get(
            taille, (8, 3))
        return fond_bouton(w, h, p[etat], coin, rayon, marge)
    base = nom.replace('Disabled', '')
    if base in TRANSPORT:
        off = nom.endswith('Disabled')
        coul = p['icone_off'] if off else {'Play': p['lecture'], 'Record': p['enreg']}.get(base, p['text'])
        return glyphe(base, w, h, coul)
    if nom.startswith('SliderThumb'):
        chaud = 'Hilited' in nom
        return pastille(w, h, p['accent_light'] if chaud else p['text'], p['surface'])
    # Recoloration générique
    out = bloc.copy()
    px = out.load()
    cache = {}
    for y in range(h):
        for x in range(w):
            c = px[x, y]
            if c[3] == 0:
                continue
            if c[:3] not in cache:
                cache[c[:3]] = recolore(p, c[:3])
            px[x, y] = cache[c[:3]] + (c[3],)
    return out

def couleurs(p):
    a, al, rms, a2 = p['accent'], p['accent_light'], p['rms'], p['accent2']
    return {
        'clrBlank': p['bg'], 'clrUnselected': p['surface'], 'clrSelected': p['sel'],
        'clrSample': p['onde'], 'clrSelSample': p['onde_sel'], 'clrRms': p['onde_rms'],
        'clrMuteSample': p['muted'], 'clrMuteRms': mix(p['muted'], p['border'], 0.5),
        'clrShadow': p['deep'], 'clrAboutBoxBackground': p['surface2'],
        'clrTrackPanelText': p['police'], 'clrLabelTrackText': p['police'],
        'clrRulerBackground': p['surface'], 'clrEnvelope': a2,
        'clrCursorPen': p['text'], 'clrRecordingPen': hx('#e8a0a0'), 'clrPlaybackPen': hx('#8fd4a4'),
        'clrRecordingBrush': p['enreg'], 'clrPlaybackBrush': p['lecture'],
        'clrRulerRecordingBrush': p['enreg'], 'clrRulerRecordingPen': hx('#a83a3a'),
        'clrRulerPlaybackBrush': p['lecture'], 'clrRulerPlaybackPen': hx('#3d8a58'),
        'clrTimeFont': a, 'clrTimeBack': p['deep'], 'clrTimeFontFocus': p['text'], 'clrTimeBackFocus': p['sel_dark'],
        'clrLabelTextNormalBrush': mix(p['surface2'], a, 0.35), 'clrLabelTextEditBrush': p['surface2'],
        'clrLabelUnselectedBrush': p['surface2'], 'clrLabelSelectedBrush': p['sel'],
        'clrLabelUnselectedPen': p['border'], 'clrLabelSelectedPen': a, 'clrLabelSurroundPen': p['deep'],
        'clrTrackFocus0': a, 'clrTrackFocus1': mix(a, p['surface'], 0.35), 'clrTrackFocus2': mix(a, p['surface'], 0.6),
        'clrSnapGuide': a2, 'clrTrackInfo': p['surface'], 'clrTrackInfoSelected': p['sel'],
        'clrLight': p['border'], 'clrMedium': p['surface'], 'clrDark': p['deep'],
        'clrLightSelected': mix(p['border'], a, 0.3), 'clrMediumSelected': p['sel'], 'clrDarkSelected': p['sel_dark'],
        'clrSyncLockSel': p['sel'], 'clrSelTranslucent': mix(p['surface'], a, 0.45), 'clrBlankSelected': p['sel_dark'],
        'clrSliderLight': p['surface2'], 'clrSliderMain': p['text_dim'], 'clrSliderDark': p['deep'],
        'clrTrackBackground': p['surface'], 'clrScrubRuler': p['surface2'], 'clrTimeHours': p['deep'],
        'clrFoxusBox': a, 'clrTrackNameText': p['police'], 'clrMidiZebra': p['surface2'], 'clrMidiLines': p['border'],
        'clrTextNegativeNumbers': al, 'clrSpectro1': p['surface'], 'clrSpectro1Sel': p['sel'],
        'clrClipAffordanceOutlinePen': p['deep'], 'clrClipAffordanceInactiveBrush': p['surface2'],
        'clrClipAffordanceActiveBrush': p['border'], 'clrClipAffordanceStroke': p['text'],
        'clrLabelTrackTextSelection': mix(p['surface2'], a, 0.5), 'clrClipNameText': p['police'],
        'clrClipNameTextSelection': mix(p['surface2'], a, 0.5), 'clrDropHintHighlight': a,
        'clrEffectListItemBackground': p['surface'], 'clrEffectListItemBorder': p['border'],
        'clrBeatSeparatorPen': p['surface2'], 'clrBarSeparatorPen': p['border'],
        'clrBeatFillStrongBrush': p['surface'], 'clrBeatFillWeakBrush': p['surface2'],
        'clrBeatFillStrongSelBrush': p['sel'], 'clrBeatFillWeakSelBrush': p['sel_dark'],
    }

def genere(nom, spec, dark, positions, noms):
    p = palette(spec)
    im = dark.copy()
    faits = set()
    # Images redessinées d'abord : une image identique (ex. CutPreview = Play) partage
    # leur emplacement et ne doit pas le recolorer à leur place
    prio = lambda n: (0 if n.replace('Disabled', '') in TRANSPORT or n.startswith('SliderThumb') else 1, n)
    for n, places in sorted(positions.items(), key=lambda kv: prio(kv[0])):
        if not places or len(places) > 4:  # image vide dans le thème Dark
            continue
        for x, y, w, h in places:
            if (x, y) in faits:
                continue
            faits.add((x, y))
            orig = dark.crop((x, y, x + w, y + h))
            neuf = traite_image(p, n, orig, orig)
            if neuf is not None:
                im.paste(neuf, (x, y))
    px, dk = im.load(), dark.load()
    table = couleurs(p)
    for i, n in enumerate(noms):
        x, y = (i % 36) * 12 + 1, Y_COULEURS + (i // 36) * 12 + 1
        c = table.get(n) or recolore(p, dk[x + 5, y + 5][:3])
        if c == (1, 1, 1):
            c = (2, 2, 2)
        for dy in range(10):
            for dx in range(10):
                px[x + dx, y + dy] = c + (255,)
    out = ICI / 'themes' / nom
    out.mkdir(parents=True, exist_ok=True)
    im.save(out / 'ImageCache.png', optimize=True)
    return im

if __name__ == '__main__':
    base = ICI / 'base'
    dark = Image.open(base / 'dark.png').convert('RGBA')
    positions = json.loads((base / 'positions.json').read_text())
    noms = (base / 'names.txt').read_text().split()
    for nom, spec in THEMES.items():
        genere(nom, spec, dark, positions, noms)
        print('ok', nom, (ICI / 'themes' / nom / 'ImageCache.png').stat().st_size)
