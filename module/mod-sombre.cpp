// mod-sombre : module Audacity 3.5.1 qui passe en sombre ce que le thème ne touche pas
// (barre de titre, barre de menus, menus déroulants, barre d'état, ascenseurs).
// Couleurs calées sur le thème Graphite.

#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <commctrl.h>
#include <string>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "comctl32.lib")

static const COLORREF FOND = RGB(0x1e, 0x1e, 0x21);
static const COLORREF SURVOL = RGB(0x2d, 0x2d, 0x32);
static const COLORREF TEXTE = RGB(0x9d, 0x9d, 0xa4);
static const COLORREF TEXTE_INACTIF = RGB(0x60, 0x60, 0x67);
static const COLORREF BORDURE = RGB(0x2d, 0x2d, 0x32);
static const COLORREF CHAMP = RGB(0x13, 0x13, 0x15);
static const COLORREF SUPPORT = RGB(0x24, 0x24, 0x28);   // fond des barres d'outils (clrMedium)
static const COLORREF CONTOUR = RGB(0x3a, 0x3a, 0x40);
static const COLORREF ACCENT = RGB(0x3f, 0x9c, 0x90);
static const int RAYON = 8;                               // diamètre de l'arrondi des coins

static HBRUSH gFond, gSurvol, gChamp;
static HHOOK gHook;

// API non documentées d'uxtheme (mode sombre Win32), par ordinal
enum PreferredAppMode { AppModeDefault, AllowDark, ForceDark, ForceLight };
using fnSetPreferredAppMode = PreferredAppMode(WINAPI *)(PreferredAppMode);
using fnAllowDarkModeForWindow = bool(WINAPI *)(HWND, bool);
using fnFlushMenuThemes = void(WINAPI *)();
static fnAllowDarkModeForWindow pAllowDarkModeForWindow;

// Structures des messages UAH (dessin de la barre de menus)
#define WM_UAHDRAWMENU 0x0091
#define WM_UAHDRAWMENUITEM 0x0092
typedef union { struct { DWORD cx, cy; } rgsizeBar[2]; struct { DWORD cx, cy; } rgsizePopup[4]; } UAHMENUITEMMETRICS;
typedef struct { DWORD rgcx[4]; DWORD fUpdateMaxWidths : 2; } UAHMENUPOPUPMETRICS;
typedef struct { HMENU hmenu; HDC hdc; DWORD dwFlags; } UAHMENU;
typedef struct { int iPosition; UAHMENUITEMMETRICS umim; UAHMENUPOPUPMETRICS umpm; } UAHMENUITEM;
typedef struct { DRAWITEMSTRUCT dis; UAHMENU um; UAHMENUITEM umi; } UAHDRAWMENUITEM;

static HFONT PoliceMenu(HWND hwnd)
{
    NONCLIENTMETRICSW ncm{ sizeof(ncm) };
    UINT dpi = GetDpiForWindow(hwnd);
    if (!SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, dpi ? dpi : 96))
        return nullptr;
    return CreateFontIndirectW(&ncm.lfMenuFont);
}

// Repeint la ligne claire que Windows trace sous la barre de menus
static void LigneSousMenu(HWND hwnd)
{
    MENUBARINFO mbi{ sizeof(mbi) };
    if (!GetMenu(hwnd) || !GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi))
        return;
    RECT client, fenetre;
    GetClientRect(hwnd, &client);
    MapWindowPoints(hwnd, nullptr, (POINT *)&client, 2);
    GetWindowRect(hwnd, &fenetre);
    OffsetRect(&client, -fenetre.left, -fenetre.top);
    RECT ligne = client;
    ligne.bottom = ligne.top;
    ligne.top--;
    HDC hdc = GetWindowDC(hwnd);
    FillRect(hdc, &ligne, gFond);
    ReleaseDC(hwnd, hdc);
}

static LRESULT CALLBACK ProcFenetre(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
{
    switch (msg) {
    case WM_UAHDRAWMENU: {
        auto *um = (UAHMENU *)lp;
        MENUBARINFO mbi{ sizeof(mbi) };
        if (!GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi))
            break;
        RECT fenetre;
        GetWindowRect(hwnd, &fenetre);
        OffsetRect(&mbi.rcBar, -fenetre.left, -fenetre.top);
        mbi.rcBar.top -= 1;
        FillRect(um->hdc, &mbi.rcBar, gFond);
        return TRUE;
    }
    case WM_UAHDRAWMENUITEM: {
        auto *it = (UAHDRAWMENUITEM *)lp;
        wchar_t texte[256] = {};
        MENUITEMINFOW mii{ sizeof(mii), MIIM_STRING };
        mii.dwTypeData = texte;
        mii.cch = 255;
        GetMenuItemInfoW(it->um.hmenu, it->umi.iPosition, TRUE, &mii);

        UINT etat = it->dis.itemState;
        bool chaud = etat & (ODS_HOTLIGHT | ODS_SELECTED);
        bool inactif = etat & (ODS_INACTIVE | ODS_DISABLED | ODS_GRAYED);
        FillRect(it->um.hdc, &it->dis.rcItem, chaud ? gSurvol : gFond);

        DWORD flags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;
        if (etat & ODS_NOACCEL)
            flags |= DT_HIDEPREFIX;
        HFONT police = PoliceMenu(hwnd);
        HGDIOBJ ancienne = police ? SelectObject(it->um.hdc, police) : nullptr;
        SetBkMode(it->um.hdc, TRANSPARENT);
        SetTextColor(it->um.hdc, inactif ? TEXTE_INACTIF : TEXTE);
        DrawTextW(it->um.hdc, texte, -1, &it->dis.rcItem, flags);
        if (police) {
            SelectObject(it->um.hdc, ancienne);
            DeleteObject(police);
        }
        return TRUE;
    }
    case WM_NCPAINT:
    case WM_NCACTIVATE: {
        LRESULT r = DefSubclassProc(hwnd, msg, wp, lp);
        LigneSousMenu(hwnd);
        return r;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, ProcFenetre, 1);
        break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK ProcBarreEtat(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
{
    switch (msg) {
    case WM_ERASEBKGND: {
        RECT r;
        GetClientRect(hwnd, &r);
        FillRect((HDC)wp, &r, gFond);
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT r;
        GetClientRect(hwnd, &r);
        FillRect(hdc, &r, gFond);
        HGDIOBJ ancienne = SelectObject(hdc, (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0));
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, TEXTE);
        HPEN stylo = CreatePen(PS_SOLID, 1, BORDURE);
        HGDIOBJ ancienStylo = SelectObject(hdc, stylo);
        int n = (int)SendMessageW(hwnd, SB_GETPARTS, 0, 0);
        for (int i = 0; i < n && i < 64; i++) {
            RECT part;
            if (!SendMessageW(hwnd, SB_GETRECT, i, (LPARAM)&part))
                continue;
            if (i > 0) {
                MoveToEx(hdc, part.left - 1, part.top + 2, nullptr);
                LineTo(hdc, part.left - 1, part.bottom - 2);
            }
            LRESULT info = SendMessageW(hwnd, SB_GETTEXTLENGTHW, i, 0);
            if (HIWORD(info) & SBT_OWNERDRAW)
                continue;
            std::wstring texte(LOWORD(info) + 1, L'\0');
            SendMessageW(hwnd, SB_GETTEXTW, i, (LPARAM)texte.data());
            part.left += 4;
            DrawTextW(hdc, texte.c_str(), -1, &part, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
        SelectObject(hdc, ancienStylo);
        DeleteObject(stylo);
        SelectObject(hdc, ancienne);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, ProcBarreEtat, 2);
        break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

// Écran de démarrage : image claire (XPM interne). Après le dessin normal, on relit les
// pixels et on inverse les gris (fond blanc -> fond sombre, texte foncé -> clair) ;
// les couleurs du logo restent telles quelles.
static bool gDemarrage = true;

static void InverserGris(HWND hwnd)
{
    RECT r;
    GetClientRect(hwnd, &r);
    int w = r.right, h = r.bottom;
    if (w <= 0 || h <= 0)
        return;
    HDC dc = GetDC(hwnd);
    BITMAPINFO bi{};
    bi.bmiHeader = { sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB };
    void *bits = nullptr;
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP dib = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (dib && bits) {
        HGDIOBJ ancien = SelectObject(mem, dib);
        BitBlt(mem, 0, 0, w, h, dc, 0, 0, SRCCOPY);
        auto *p = (BYTE *)bits;
        for (int i = 0; i < w * h; i++, p += 4) {
            int b = p[0], g = p[1], rr = p[2];
            int mx = max(rr, max(g, b)), mn = min(rr, min(g, b));
            if (mx == 0 || (mx - mn) * 100 / mx < 18) {
                int l = (rr * 2126 + g * 7152 + b * 722) / 10000;  // 0..255
                // l=255 (blanc) -> FOND, l=0 (noir) -> TEXTE
                p[2] = (BYTE)(GetRValue(TEXTE) + (GetRValue(FOND) - GetRValue(TEXTE)) * l / 255);
                p[1] = (BYTE)(GetGValue(TEXTE) + (GetGValue(FOND) - GetGValue(TEXTE)) * l / 255);
                p[0] = (BYTE)(GetBValue(TEXTE) + (GetBValue(FOND) - GetBValue(TEXTE)) * l / 255);
            }
        }
        BitBlt(dc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
        SelectObject(mem, ancien);
    }
    if (dib)
        DeleteObject(dib);
    DeleteDC(mem);
    ReleaseDC(hwnd, dc);
}

static LRESULT CALLBACK ProcDemarrage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
{
    switch (msg) {
    case WM_PAINT: {
        LRESULT r = DefSubclassProc(hwnd, msg, wp, lp);
        InverserGris(hwnd);
        return r;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, ProcDemarrage, 4);
        break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

// Contour arrondi : support autour, trait discret, intérieur couleur des champs
static void ContourArrondi(HDC dc, int w, int h)
{
    HBRUSH support = CreateSolidBrush(SUPPORT);
    RECT r{ 0, 0, w, h };
    FillRect(dc, &r, support);
    DeleteObject(support);
    HPEN stylo = CreatePen(PS_SOLID, 1, CONTOUR);
    HGDIOBJ ap = SelectObject(dc, stylo), ab = SelectObject(dc, gChamp);
    RoundRect(dc, 0, 0, w, h, RAYON, RAYON);
    SelectObject(dc, ap);
    SelectObject(dc, ab);
    DeleteObject(stylo);
}

// Bordure non cliente des champs et des compteurs de temps
static LRESULT CALLBACK ProcBordure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
{
    switch (msg) {
    case WM_NCPAINT: {
        RECT rw, rc;
        GetWindowRect(hwnd, &rw);
        GetClientRect(hwnd, &rc);
        MapWindowPoints(hwnd, nullptr, (POINT *)&rc, 2);
        OffsetRect(&rc, -rw.left, -rw.top);
        HDC dc = GetWindowDC(hwnd);
        ExcludeClipRect(dc, rc.left, rc.top, rc.right, rc.bottom);
        ContourArrondi(dc, rw.right - rw.left, rw.bottom - rw.top);
        ReleaseDC(hwnd, dc);
        return 0;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, ProcBordure, 5);
        break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void Chevron(HDC dc, HWND hwnd, const RECT &r, bool actif)
{
    int fleche = MulDiv(18, GetDpiForWindow(hwnd), 96);
    int cx = r.right - fleche / 2 - 2, cy = r.bottom / 2, d = MulDiv(3, GetDpiForWindow(hwnd), 96);
    HPEN stylo = CreatePen(PS_SOLID, max(1, d / 2), actif ? TEXTE : TEXTE_INACTIF);
    HGDIOBJ ap = SelectObject(dc, stylo);
    POINT v[3] = { { cx - d, cy - d / 2 }, { cx, cy + d / 2 + 1 }, { cx + d + 1, cy - d / 2 - 1 } };
    Polyline(dc, v, 3);
    SelectObject(dc, ap);
    DeleteObject(stylo);
}

// Liste générique wxComboCtrl (ex. « Secondes ») : wx la peint avec le thème clair ;
// on dessine le cadre et le chevron, le champ texte enfant se peint lui-même
static LRESULT CALLBACK ProcListeWx(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
{
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, IsWindowEnabled((HWND)lp) && msg == WM_CTLCOLOREDIT ? TEXTE : TEXTE_INACTIF);
        SetBkColor(hdc, CHAMP);
        return (LRESULT)gChamp;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT r;
        GetClientRect(hwnd, &r);
        ContourArrondi(hdc, r.right, r.bottom);
        Chevron(hdc, hwnd, r, IsWindowEnabled(hwnd));
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, ProcListeWx, 8);
        break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

// Case à cocher : carré arrondi foncé, rempli de l'accent avec une coche quand elle est cochée
static void DessineCase(HWND hwnd, HDC hdc)
{
    RECT r;
    GetClientRect(hwnd, &r);
    HBRUSH support = CreateSolidBrush(SUPPORT);
    FillRect(hdc, &r, support);
    DeleteObject(support);
    int dpi = GetDpiForWindow(hwnd), t = MulDiv(14, dpi, 96);
    int y = (r.bottom - t) / 2;
    bool coche = SendMessageW(hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool actif = IsWindowEnabled(hwnd);
    HPEN stylo = CreatePen(PS_SOLID, 1, coche ? ACCENT : CONTOUR);
    HBRUSH fond = CreateSolidBrush(coche ? (actif ? ACCENT : CONTOUR) : CHAMP);
    HGDIOBJ ap = SelectObject(hdc, stylo), ab = SelectObject(hdc, fond);
    RoundRect(hdc, 0, y, t, y + t, MulDiv(6, dpi, 96), MulDiv(6, dpi, 96));
    SelectObject(hdc, ap);
    SelectObject(hdc, ab);
    DeleteObject(stylo);
    DeleteObject(fond);
    if (coche) {
        HPEN trait = CreatePen(PS_SOLID, max(2, MulDiv(2, dpi, 96)), CHAMP);
        ap = SelectObject(hdc, trait);
        POINT v[3] = { { t * 3 / 14, y + t / 2 }, { t * 6 / 14, y + t * 10 / 14 }, { t * 11 / 14, y + t * 4 / 14 } };
        Polyline(hdc, v, 3);
        SelectObject(hdc, ap);
        DeleteObject(trait);
    }
    wchar_t texte[128] = {};
    GetWindowTextW(hwnd, texte, 127);
    if (texte[0]) {
        RECT rt{ t + MulDiv(6, dpi, 96), 0, r.right, r.bottom };
        HGDIOBJ af = SelectObject(hdc, (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0));
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, actif ? TEXTE : TEXTE_INACTIF);
        DrawTextW(hdc, texte, -1, &rt, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        SelectObject(hdc, af);
    }
}

static LRESULT CALLBACK ProcCase(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
{
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        DessineCase(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    // La case se repeint d'elle-même (sans WM_PAINT) quand on clique ou qu'elle change d'état
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK: case WM_KEYDOWN: case WM_KEYUP:
    case BM_SETCHECK: case BM_SETSTATE: case BM_CLICK: case WM_SETFOCUS: case WM_KILLFOCUS:
    case WM_ENABLE: case WM_SETTEXT: case WM_UPDATEUISTATE: case WM_MOUSEMOVE: case WM_MOUSELEAVE: {
        LRESULT res = DefSubclassProc(hwnd, msg, wp, lp);
        if (IsWindow(hwnd)) {
            HDC hdc = GetDC(hwnd);
            DessineCase(hwnd, hdc);
            ReleaseDC(hwnd, hdc);
        }
        return res;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, ProcCase, 9);
        break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

// Liste déroulante entièrement redessinée : fond foncé, coins arrondis, chevron
static LRESULT CALLBACK ProcListe(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
{
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, IsWindowEnabled(hwnd) ? TEXTE : TEXTE_INACTIF);
        SetBkColor(hdc, CHAMP);
        return (LRESULT)gChamp;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT r;
        GetClientRect(hwnd, &r);
        // Double tampon pour éviter le scintillement
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, r.right, r.bottom);
        HGDIOBJ ab = SelectObject(mem, bmp);
        ContourArrondi(mem, r.right, r.bottom);

        bool actif = IsWindowEnabled(hwnd);
        wchar_t texte[256] = {};
        int i = (int)SendMessageW(hwnd, CB_GETCURSEL, 0, 0);
        if (i >= 0 && SendMessageW(hwnd, CB_GETLBTEXTLEN, i, 0) < 255)
            SendMessageW(hwnd, CB_GETLBTEXT, i, (LPARAM)texte);
        else
            GetWindowTextW(hwnd, texte, 255);
        int fleche = MulDiv(18, GetDpiForWindow(hwnd), 96);
        RECT rt{ 8, 0, r.right - fleche, r.bottom };
        HGDIOBJ af = SelectObject(mem, (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0));
        SetBkMode(mem, TRANSPARENT);
        SetTextColor(mem, actif ? TEXTE : TEXTE_INACTIF);
        DrawTextW(mem, texte, -1, &rt, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(mem, af);

        Chevron(mem, hwnd, r, actif);

        BitBlt(hdc, 0, 0, r.right, r.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, ab);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ENABLE:
    case CB_SETCURSEL: {
        LRESULT res = DefSubclassProc(hwnd, msg, wp, lp);
        InvalidateRect(hwnd, nullptr, FALSE);
        return res;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, ProcListe, 6);
        break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

// Poignée de barre d'outils (Grabber) : un trait vertical discret au lieu du cadre strié
static LRESULT CALLBACK ProcPoignee(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
{
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT r;
        GetClientRect(hwnd, &r);
        HBRUSH support = CreateSolidBrush(SUPPORT);
        FillRect(hdc, &r, support);
        DeleteObject(support);
        int marge = max(4, r.bottom / 6), x = r.right / 2;
        RECT trait{ x, marge, x + 1, r.bottom - marge };
        HBRUSH b = CreateSolidBrush(CONTOUR);
        FillRect(hdc, &trait, b);
        DeleteObject(b);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, ProcPoignee, 7);
        break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void Sousclasser(HWND hwnd, SUBCLASSPROC proc, UINT_PTR id)
{
    DWORD_PTR rien;
    if (!GetWindowSubclass(hwnd, proc, id, &rien))
        SetWindowSubclass(hwnd, proc, id, 0);
}

// Parent des champs, listes et ascenseurs : couleurs des champs et fond du panneau
static LRESULT CALLBACK ProcParent(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR fond)
{
    switch (msg) {
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, IsWindowEnabled((HWND)lp) ? TEXTE : TEXTE_INACTIF);
        SetBkColor(hdc, CHAMP);
        return (LRESULT)gChamp;
    }
    case WM_ERASEBKGND:
        if (fond) {
            RECT r;
            GetClientRect(hwnd, &r);
            FillRect((HDC)wp, &r, gFond);
            return 1;
        }
        break;
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, ProcParent, 3);
        break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void SousClasserParent(HWND enfant, bool fond)
{
    HWND parent = GetParent(enfant);
    if (!parent)
        return;
    DWORD_PTR ancien = 0;
    if (GetWindowSubclass(parent, ProcParent, 3, &ancien)) {
        if (fond && !ancien)
            SetWindowSubclass(parent, ProcParent, 3, 1);
        return;
    }
    SetWindowSubclass(parent, ProcParent, 3, fond ? 1 : 0);
}

// Poignée : fenêtre wx étroite (grabberWidth = 10 px) et haute, sans enfant, avec un
// libellé (« Grabber » traduit)
static bool EstPoignee(HWND hwnd)
{
    RECT r;
    GetWindowRect(hwnd, &r);
    int w = r.right - r.left, h = r.bottom - r.top;
    int lim = MulDiv(14, GetDpiForWindow(hwnd), 96);
    return w > 0 && w <= lim && h >= 2 * w && !GetWindow(hwnd, GW_CHILD) && GetWindowTextLengthW(hwnd) > 0;
}

// wxComboCtrl : fenêtre wx basse et large dont le seul enfant est un champ texte
static bool EstListeWx(HWND hwnd)
{
    HWND enfant = GetWindow(hwnd, GW_CHILD);
    if (!enfant || GetWindow(enfant, GW_HWNDNEXT))
        return false;
    wchar_t c[16] = {};
    GetClassNameW(enfant, c, 16);
    RECT r;
    GetWindowRect(hwnd, &r);
    int w = r.right - r.left, h = r.bottom - r.top;
    return !lstrcmpiW(c, L"Edit") && h > 0 && h <= MulDiv(40, GetDpiForWindow(hwnd), 96) && w > 2 * h;
}

static void Assombrir(HWND hwnd)
{
    wchar_t classe[64] = {};
    GetClassNameW(hwnd, classe, 64);
    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    // Seulement dans la fenêtre de projet (celle qui a la barre de menus)
    HWND racine = GetAncestor(hwnd, GA_ROOT);
    bool projet = racine && GetMenu(racine);

    // Écran de démarrage : seule fenêtre « toujours devant », sans barre de titre ni
    // barre des tâches, créée avant AppInitialized ; on prend sa fenêtre enfant
    if (gDemarrage && racine && racine != hwnd && GetParent(hwnd) == racine) {
        LONG rs = GetWindowLongW(racine, GWL_STYLE), rx = GetWindowLongW(racine, GWL_EXSTYLE);
        if ((rx & WS_EX_TOPMOST) && (rx & WS_EX_TOOLWINDOW) && (rs & WS_CAPTION) != WS_CAPTION) {
            DWORD_PTR rien;
            if (!GetWindowSubclass(hwnd, ProcDemarrage, 4, &rien))
                SetWindowSubclass(hwnd, ProcDemarrage, 4, 0);
            return;
        }
    }

    if ((style & WS_CAPTION) == WS_CAPTION && !(style & WS_CHILD)) {
        if (pAllowDarkModeForWindow)
            pAllowDarkModeForWindow(hwnd, true);
        BOOL oui = TRUE;
        DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &oui, sizeof(oui));
        COLORREF c = FOND, t = TEXTE;
        DwmSetWindowAttribute(hwnd, 35 /*DWMWA_CAPTION_COLOR*/, &c, sizeof(c));
        DwmSetWindowAttribute(hwnd, 36 /*DWMWA_TEXT_COLOR*/, &t, sizeof(t));
        DWORD_PTR rien;
        if (!GetWindowSubclass(hwnd, ProcFenetre, 1, &rien))
            SetWindowSubclass(hwnd, ProcFenetre, 1, 0);
    }
    else if (!lstrcmpiW(classe, STATUSCLASSNAMEW)) {
        DWORD_PTR rien;
        if (!GetWindowSubclass(hwnd, ProcBarreEtat, 2, &rien))
            SetWindowSubclass(hwnd, ProcBarreEtat, 2, 0);
    }
    else if (!lstrcmpiW(classe, L"ScrollBar")) {
        if (pAllowDarkModeForWindow)
            pAllowDarkModeForWindow(hwnd, true);
        SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
        // Le panneau qui porte l'ascenseur horizontal laisse voir son fond clair à gauche
        if (projet && !(style & SBS_VERT))
            SousClasserParent(hwnd, true);
    }
    else if (projet && !lstrcmpiW(classe, L"ComboBox")) {
        if (pAllowDarkModeForWindow)
            pAllowDarkModeForWindow(hwnd, true);
        SetWindowTheme(hwnd, L"DarkMode_CFD", nullptr);  // pour la liste qui se déroule
        Sousclasser(hwnd, ProcListe, 6);
    }
    else if (projet && !lstrcmpiW(classe, L"Edit")) {
        if (pAllowDarkModeForWindow)
            pAllowDarkModeForWindow(hwnd, true);
        SetWindowTheme(hwnd, L"DarkMode_CFD", nullptr);
        SousClasserParent(hwnd, false);
        wchar_t parent[32] = {};
        GetClassNameW(GetParent(hwnd), parent, 32);
        LONG ex = GetWindowLongW(hwnd, GWL_EXSTYLE);
        if (lstrcmpiW(parent, L"ComboBox") && ((style & WS_BORDER) || (ex & (WS_EX_CLIENTEDGE | WS_EX_STATICEDGE))))
            Sousclasser(hwnd, ProcBordure, 5);
    }
    else if (projet && !lstrcmpiW(classe, L"Button") &&
             ((style & BS_TYPEMASK) == BS_CHECKBOX || (style & BS_TYPEMASK) == BS_AUTOCHECKBOX)) {
        Sousclasser(hwnd, ProcCase, 9);
    }
    else if (projet && !wcsncmp(classe, L"wxWindow", 8) && (style & WS_CHILD) && EstListeWx(hwnd)) {
        Sousclasser(hwnd, ProcListeWx, 8);
    }
    else if (projet && !wcsncmp(classe, L"wxWindow", 8) && (style & WS_CHILD) && EstPoignee(hwnd)) {
        Sousclasser(hwnd, ProcPoignee, 7);
    }
    else if (projet && !wcsncmp(classe, L"wxWindow", 8) && (style & WS_CHILD) &&
             !(style & (WS_VSCROLL | WS_HSCROLL)) &&
             ((style & WS_BORDER) || (GetWindowLongW(hwnd, GWL_EXSTYLE) & (WS_EX_CLIENTEDGE | WS_EX_STATICEDGE)))) {
        Sousclasser(hwnd, ProcBordure, 5);  // compteurs de temps et autres cadres
    }
    else if (projet && !lstrcmpiW(classe, UPDOWN_CLASSW)) {
        if (pAllowDarkModeForWindow)
            pAllowDarkModeForWindow(hwnd, true);
        SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
    }
}

static LRESULT CALLBACK Crochet(int code, WPARAM wp, LPARAM lp)
{
    if (code == HC_ACTION) {
        auto *m = (CWPRETSTRUCT *)lp;
        if (m->message == WM_CREATE && m->lResult != -1)
            Assombrir(m->hwnd);
    }
    return CallNextHookEx(gHook, code, wp, lp);
}

static BOOL CALLBACK Enfant(HWND hwnd, LPARAM) { Assombrir(hwnd); return TRUE; }
static BOOL CALLBACK Fenetre(HWND hwnd, LPARAM)
{
    Assombrir(hwnd);
    EnumChildWindows(hwnd, Enfant, 0);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
    return TRUE;
}

static void Demarrer()
{
    gFond = CreateSolidBrush(FOND);
    gSurvol = CreateSolidBrush(SURVOL);
    gChamp = CreateSolidBrush(CHAMP);
    if (HMODULE ux = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)) {
        auto setMode = (fnSetPreferredAppMode)GetProcAddress(ux, MAKEINTRESOURCEA(135));
        auto flush = (fnFlushMenuThemes)GetProcAddress(ux, MAKEINTRESOURCEA(136));
        pAllowDarkModeForWindow = (fnAllowDarkModeForWindow)GetProcAddress(ux, MAKEINTRESOURCEA(133));
        if (setMode)
            setMode(ForceDark);
        if (flush)
            flush();
    }
    gHook = SetWindowsHookExW(WH_CALLWNDPROCRET, Crochet, nullptr, GetCurrentThreadId());
    EnumThreadWindows(GetCurrentThreadId(), Fenetre, 0);
}

static void Arreter()
{
    if (gHook)
        UnhookWindowsHookEx(gHook);
    gHook = nullptr;
}

extern "C" {
__declspec(dllexport) const wchar_t *GetVersionString() { return L"3.5.1"; }

__declspec(dllexport) int ModuleDispatch(int type)
{
    switch (type) {
    case 0: Demarrer(); break;                                        // ModuleInitialize
    case 1: Arreter(); break;                                         // ModuleTerminate
    case 2: gDemarrage = false; EnumThreadWindows(GetCurrentThreadId(), Fenetre, 0); break; // AppInitialized
    case 4: EnumThreadWindows(GetCurrentThreadId(), Fenetre, 0); break;                     // ProjectInitialized
    }
    return 1;
}
}
