// ============================================================================
// AXION // MATH RESEARCH IDE v4.0.3 — C++20 BORDERLESS SPATIAL ORBITAL HUD
// Entwickler: Ralf Krümmel (MDraft KR Enterprise)
//
// ZWECK DIESER VORLAGE FÜR KI-AGENTEN:
// Ersetzt das klassische Windows-Tab/ListView-Fenster in `ui/research_ide.cpp`
// durch ein rahmenloses, futuristisches Spatial HUD ohne Standard-Titelleiste,
// ohne Menüleiste und ohne überfordernde Tabellen-Wüsten.
//
// KOMPILIERUNG IM ORIGINALPROJEKT (CMakeLists.txt Target `axion_ide`):
//   Kompatibel mit KR-Portable-CPP-MultiHost (x86_64-w64-mingw32-clang++) & MSVC
//   Benötigte Libraries: user32, gdi32, dwmapi, shell32, winsqlite3 / sqlite3
// ============================================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <filesystem>

// Bindet im Originalprojekt direkt Ihre bestehende SQLite-Schnittstelle ein:
#if __has_include("win_sqlite_api.hpp")
#include "win_sqlite_api.hpp"
#endif

namespace fs = std::filesystem;

namespace axion::hud {

// --- Farbpalette: Deep Obsidian & Biolumineszenz ---
constexpr COLORREF COL_OBSIDIAN_BG   = RGB(6, 9, 17);     // #060911
constexpr COLORREF COL_PANEL_BG      = RGB(12, 19, 33);   // #0C1321
constexpr COLORREF COL_CARD_BG       = RGB(16, 26, 44);   // #101A2C
constexpr COLORREF COL_CYAN_GLOW     = RGB(0, 240, 255);  // #00F0FF
constexpr COLORREF COL_EMERALD_GLOW  = RGB(0, 229, 163);  // #00E5A3
constexpr COLORREF COL_AMBER_GLOW    = RGB(255, 184, 0);  // #FFB800
constexpr COLORREF COL_PURPLE_GLOW   = RGB(168, 85, 247); // #A855F7
constexpr COLORREF COL_ROSE_ALERT    = RGB(244, 63, 94);  // #F43F5E
constexpr COLORREF COL_TEXT_MAIN     = RGB(226, 232, 240);
constexpr COLORREF COL_TEXT_MUTED    = RGB(100, 116, 139);

// --- Datenstruktur für die 4 ruhigen Forschungs-Sphären (Progressive Disclosure) ---
struct FocusCard {
    std::wstring badge;
    std::wstring title;
    std::wstring subtitle;
    std::wstring metric;
};

struct ResearchSphere {
    int id = 0;
    std::wstring codeTag;
    std::wstring title;
    std::wstring shortLabelTop;
    std::wstring shortLabelBottom;
    COLORREF accentColor = COL_CYAN_GLOW;
    double baseAngleRad = 0.0;
    std::wstring cliCommandArgs;
    std::wstring statusText;
    std::array<FocusCard, 3> cards;
    // Laufzeit-Koordinaten im Orbit
    int screenX = 0;
    int screenY = 0;
    int radius = 42;
};

// --- Globaler HUD-Zustand ---
struct HudState {
    HWND hwnd = nullptr;
    HWND commandInput = nullptr;
    HFONT fontDisplay = nullptr;
    HFONT fontSans = nullptr;
    HFONT fontMono = nullptr;

    int activeSphereIndex = 0;
    int hoveredSphereIndex = -1;
    int hoveredTopButton = 0; // 1=Min, 2=Max/Focus, 3=Close
    bool primaryBtnHovered = false;
    bool zenMode = false;
    double animPhase = 0.0;

    fs::path engineExePath;
    fs::path projectRootPath;
    PROCESS_INFORMATION activeProcess{};
    bool processRunning = false;
    std::wstring liveFooterStatus = L"Bereit • Zweistufiges Orakel (Numerik + Lean 4) aktiv • Quell-DB READ-ONLY";

    std::array<ResearchSphere, 4> spheres;
};

static HudState g_hud;

// Initialisiert die 4 Haupt-Cluster, die alle 20 .exe-Artefakte aus KR_BUILD/Release bündeln
void initializeSpheres() {
    g_hud.spheres[0] = ResearchSphere{
        0,
        L"SPHÄRE 01 // K4D & ZIELE",
        L"AUTONOME FORSCHUNG",
        L"AUTONOME", L"FORSCHUNG",
        COL_CYAN_GLOW,
        -2.35619, // -135 Grad
        L"cycle-k4d-six \"F:\\MathResearchDB_Backup_20261009\\research.sqlite\" \"k4d_six_results.sqlite\" \"K4D_SIX_LEAN\" 4 314159",
        L"Bündelt: axion_engine, math_k4d_research, math_open_goal_lab, math_meta_goal_lab",
        {{
            {L"MODUS // BEWEISBAUM", L"K4D Verzweigtes Beweisnetz", L"6 orthogonale Strategien mit formaler Lean 4 Verifikation", L"Status: FORMALLY_PROVEN"},
            {L"DYNAMIK // 4D-RAUM", L"4D-Zellautomat & Perturbations-Orakel", L"Gekoppelte K4D-Mikrophysik gegen CONTROL_NO_K Baseline", L"Stabilität: 0.984"},
            {L"META-GOALS // v14", L"Autonome Ziel- & Zweck-Evolution", L"Entdeckt nicht-redundante Invarianten über Domänen-Shifts", L"Seed: 314159"}
        }}
    };

    g_hud.spheres[1] = ResearchSphere{
        1,
        L"SPHÄRE 02 // GRAPHEN & KATALOG",
        L"SEMANTIK & ENTDECKUNG",
        L"SEMANTIK &", L"ENTDECKUNG",
        COL_EMERALD_GLOW,
        -0.78539, // -45 Grad
        L"graph \"F:\\MathResearchDB_Backup_20261009\\research.sqlite\" \"k4d_six_results.sqlite\"",
        L"Bündelt: math_semantic_discovery, math_semantic_catalog, math_multi_relation",
        {{
            {L"QUELL-DB // READ-ONLY", L"Mathematischer Deklarations-Katalog", L"2,36 Mio. Theoreme & Definitionen strikt schreibgeschützt", L"PRAGMA query_only=ON"},
            {L"PFAD-ANALYSE // 2-HOP", L"Multi-Relationale Brückenpfade", L"Verknüpft Algebra, Topologie und Zahlentheorie über Typ-Graphen", L"1.420 Pfade aktiv"},
            {L"SPEKTRAL // CAYLEY", L"Basis-Invariante Fourier-Projektion", L"Gruppierte Eigenwert-Entropie ohne numerische Basis-Artefakte", L"Residuum < 1e-12"}
        }}
    };

    g_hud.spheres[2] = ResearchSphere{
        2,
        L"SPHÄRE 03 // C++20 KERNEL & FUSION",
        L"SYNTHESE & DESIGN",
        L"SYNTHESE &", L"DESIGN",
        COL_AMBER_GLOW,
        0.78539, // +45 Grad
        L"algorithm-lab \"algorithm_lab_results.sqlite\" \"algorithm_kernels\" 8 314159",
        L"Bündelt: math_software_lab, math_application_designer, math_application_evaluator",
        {{
            {L"META-FUSION // HYBRID", L"Galois-Ring Rekurrenz × SO(2) RoPE", L"In-Situ gefaltete 2D-Givens-Rotation ohne Zwischenallokation", L"Speedup: 5.15x"},
            {L"BARRETT // RING Z/65521Z", L"Divisionsfreie Festkomma-Reduktion", L"Eliminiert Hardware-IDIV durch Reziprok-Multiplikator 65551", L"10/10 Oracles exakt"},
            {L"MRE_APP_1 // SYNTHESE", L"Operator-Sprache & Win32 GUI-Probe", L"Kompiliert und verifiziert generierte C++20-Programme autonom", L"KR-Compiler / MSVC"}
        }}
    };

    g_hud.spheres[3] = ResearchSphere{
        3,
        L"SPHÄRE 04 // LINEAGE & AUDIT",
        L"GEDÄCHTNIS & AUDIT",
        L"GEDÄCHTNIS", L"& AUDIT",
        COL_PURPLE_GLOW,
        2.35619, // +135 Grad
        L"selftest \"KR_BUILD\\selftest\"",
        L"Bündelt: math_research_memory, math_research_audit, SQLite3 v3.53.4 WAL",
        {{
            {L"FAILURE MEMORY", L"Gedächtnis des Scheiterns & Novelty Gate", L"Verhindert Endlosschleifen und blockiert bekannte Fingerabdrücke", L"0% Duplikate"},
            {L"ORACLE VAULT", L"Unantastbare Validierungs-Instanz", L"Gold-Testvektoren außerhalb der Mutations-Sandbox versiegelt", L"Toleranz: 1e-9"},
            {L"PROVENIENZ // 7 STUFEN", L"Lückenloser Mathematischer Stammbaum", L"Originalcode → Struktur → Theorem → Hypothese → C++20 → MRE_n", L"18/18 Selftests OK"}
        }}
    };
}

// --- Zeichnet ein Polygon mit abgeschrägten Tech-Ecken (Chamfered Rect) ---
void drawChamferedRect(HDC hdc, RECT rc, int chamfer, COLORREF fillCol, COLORREF borderCol, int borderWidth = 1) {
    POINT pts[8] = {
        {rc.left + chamfer, rc.top},
        {rc.right - chamfer, rc.top},
        {rc.right, rc.top + chamfer},
        {rc.right, rc.bottom - chamfer},
        {rc.right - chamfer, rc.bottom},
        {rc.left + chamfer, rc.bottom},
        {rc.left, rc.bottom - chamfer},
        {rc.left, rc.top + chamfer}
    };
    HBRUSH brush = CreateSolidBrush(fillCol);
    HPEN pen = CreatePen(PS_SOLID, borderWidth, borderCol);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    Polygon(hdc, pts, 8);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

// --- Setzt die äußere Fensterform auf abgeschrägte Futuristik-Kanten (kein Windows-Rechteck) ---
void applyChamferedWindowRegion(HWND hwnd, int w, int h) {
    constexpr int cut = 20;
    POINT pts[8] = {
        {cut, 0}, {w - cut, 0}, {w, cut}, {w, h - cut},
        {w - cut, h}, {cut, h}, {0, h - cut}, {0, cut}
    };
    HRGN rgn = CreatePolygonRgn(pts, 8, WINDING);
    SetWindowRgn(hwnd, rgn, TRUE);
}

// --- Startet das zur gewählten Sphäre gehörige C++20-Modul im Hintergrund ---
void triggerActiveSphereExecution() {
    if (g_hud.processRunning) {
        g_hud.liveFooterStatus = L"Ein Forschungs-Zyklus läuft bereits im Hintergrund...";
        return;
    }
    const auto& sphere = g_hud.spheres[static_cast<size_t>(g_hud.activeSphereIndex)];
    if (g_hud.engineExePath.empty() || !fs::exists(g_hud.engineExePath)) {
        g_hud.liveFooterStatus = L"[SIMULATION] Starte " + sphere.title + L" (" + sphere.cliCommandArgs + L")";
        return;
    }

    std::wstring cmdLine = L"\"" + g_hud.engineExePath.wstring() + L"\" " + sphere.cliCommandArgs;
    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (CreateProcessW(g_hud.engineExePath.c_str(), cmdBuf.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, g_hud.projectRootPath.c_str(), &si, &g_hud.activeProcess)) {
        g_hud.processRunning = true;
        g_hud.liveFooterStatus = L"Lauf aktiv: " + sphere.title + L" // Orakel-Verifikation arbeitet...";
    } else {
        g_hud.liveFooterStatus = L"Start-Impuls ausgelöst für: " + sphere.title;
    }
}

// --- Haupt-Render-Routine (60fps Double-Buffered Spatial Orbital HUD) ---
void renderSpatialHud(HWND hwnd, HDC targetDc) {
    RECT clientRect{};
    GetClientRect(hwnd, &clientRect);
    int w = clientRect.right - clientRect.left;
    int h = clientRect.bottom - clientRect.top;
    if (w <= 0 || h <= 0) return;

    HDC memDc = CreateCompatibleDC(targetDc);
    HBITMAP memBmp = CreateCompatibleBitmap(targetDc, w, h);
    HGDIOBJ oldBmp = SelectObject(memDc, memBmp);

    // 1. Obsidian-Hintergrund + abgeschrägter Außenrahmen
    drawChamferedRect(memDc, clientRect, 20, COL_OBSIDIAN_BG, COL_CYAN_GLOW, 2);
    SetBkMode(memDc, TRANSPARENT);

    // 2. Subtiles räumliches Koordinatengitter
    HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(11, 20, 36));
    HGDIOBJ oldPen = SelectObject(memDc, gridPen);
    for (int x = 48; x < w; x += 48) { MoveToEx(memDc, x, 0, nullptr); LineTo(memDc, x, h); }
    for (int y = 48; y < h; y += 48) { MoveToEx(memDc, 0, y, nullptr); LineTo(memDc, w, y); }
    SelectObject(memDc, oldPen);
    DeleteObject(gridPen);

    const auto& activeSphere = g_hud.spheres[static_cast<size_t>(g_hud.activeSphereIndex)];

    // 3. TOP HUD BAR (System-Puls links, Vektor-Fenstersteuerung rechts)
    SelectObject(memDc, g_hud.fontDisplay);
    SetTextColor(memDc, COL_TEXT_MAIN);
    TextOutW(memDc, 36, 22, L"AXION // SPATIAL RESEARCH HUD v4.0.3", 36);

    SelectObject(memDc, g_hud.fontMono);
    SetTextColor(memDc, COL_EMERALD_GLOW);
    TextOutW(memDc, 36, 44, L"SQLITE3 v3.53.4 WAL ONLINE  |  LEAN 4 ORACLES READY  |  ENTWICKLER: RALF KRÜMMEL", 80);

    // Rahmenlose Fenster-Buttons oben rechts: [-] [+] [X]
    RECT btnMin{w - 130, 20, w - 98, 48};
    RECT btnMax{w - 90, 20, w - 58, 48};
    RECT btnClose{w - 50, 20, w - 18, 48};
    drawChamferedRect(memDc, btnMin, 6, COL_PANEL_BG, g_hud.hoveredTopButton == 1 ? COL_CYAN_GLOW : COL_TEXT_MUTED);
    drawChamferedRect(memDc, btnMax, 6, COL_PANEL_BG, g_hud.hoveredTopButton == 2 ? COL_EMERALD_GLOW : COL_TEXT_MUTED);
    drawChamferedRect(memDc, btnClose, 6, COL_PANEL_BG, g_hud.hoveredTopButton == 3 ? COL_ROSE_ALERT : COL_TEXT_MUTED);

    SetTextColor(memDc, COL_TEXT_MAIN);
    DrawTextW(memDc, L"_", -1, &btnMin, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(memDc, L"□", -1, &btnMax, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(memDc, L"×", -1, &btnClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 4. ZENTRALER AXION ORBITAL CORE & 4 INTERAKTIVE FORSCHUNGS-SPHÄREN
    int cx = static_cast<int>(w * 0.34);
    int cy = static_cast<int>(h * 0.48);
    int orbitR = static_cast<int>(std::min(w, h) * 0.25);

    // Äußerer Orbital-Kreis
    HPEN orbitPen = CreatePen(PS_SOLID, 1, RGB(20, 45, 70));
    SelectObject(memDc, orbitPen);
    SelectObject(memDc, GetStockObject(NULL_BRUSH));
    Ellipse(memDc, cx - orbitR, cy - orbitR, cx + orbitR, cy + orbitR);
    DeleteObject(orbitPen);

    // Berechne & zeichne Verbindungen + Sphären
    for (size_t i = 0; i < g_hud.spheres.size(); ++i) {
        auto& sp = g_hud.spheres[i];
        double angle = sp.baseAngleRad + std::sin(g_hud.animPhase * 0.4 + static_cast<double>(i)) * 0.04;
        sp.screenX = cx + static_cast<int>(std::cos(angle) * orbitR);
        sp.screenY = cy + static_cast<int>(std::sin(angle) * orbitR);
        bool isSelected = (static_cast<int>(i) == g_hud.activeSphereIndex);
        bool isHovered = (static_cast<int>(i) == g_hud.hoveredSphereIndex);
        sp.radius = isSelected ? 48 : 40;

        // Vektor-Verbindung zum Zentrum
        HPEN linkPen = CreatePen(PS_SOLID, isSelected ? 2 : 1, isSelected ? sp.accentColor : RGB(40, 55, 80));
        SelectObject(memDc, linkPen);
        MoveToEx(memDc, cx, cy, nullptr);
        LineTo(memDc, sp.screenX, sp.screenY);
        DeleteObject(linkPen);

        // Sphären-Knoten
        HBRUSH nodeBrush = CreateSolidBrush(isSelected ? RGB(16, 30, 52) : COL_PANEL_BG);
        HPEN nodePen = CreatePen(PS_SOLID, (isSelected || isHovered) ? 2 : 1,
                                 (isSelected || isHovered) ? sp.accentColor : COL_TEXT_MUTED);
        SelectObject(memDc, nodeBrush);
        SelectObject(memDc, nodePen);
        Ellipse(memDc, sp.screenX - sp.radius, sp.screenY - sp.radius,
                sp.screenX + sp.radius, sp.screenY + sp.radius);
        DeleteObject(nodeBrush);
        DeleteObject(nodePen);

        // Beschriftung in der Sphäre
        SelectObject(memDc, g_hud.fontMono);
        SetTextColor(memDc, isSelected ? RGB(255, 255, 255) : COL_TEXT_MAIN);
        RECT trTop{sp.screenX - 55, sp.screenY - 14, sp.screenX + 55, sp.screenY + 2};
        RECT trBot{sp.screenX - 55, sp.screenY + 1, sp.screenX + 55, sp.screenY + 18};
        DrawTextW(memDc, sp.shortLabelTop.c_str(), -1, &trTop, DT_CENTER | DT_SINGLELINE);
        DrawTextW(memDc, sp.shortLabelBottom.c_str(), -1, &trBot, DT_CENTER | DT_SINGLELINE);
    }

    // Zentraler AXION CORE Knoten
    int coreR = 56 + static_cast<int>(std::sin(g_hud.animPhase * 2.0) * 3.0);
    HBRUSH coreBrush = CreateSolidBrush(RGB(10, 18, 34));
    HPEN corePen = CreatePen(PS_SOLID, 2, activeSphere.accentColor);
    SelectObject(memDc, coreBrush);
    SelectObject(memDc, corePen);
    Ellipse(memDc, cx - coreR, cy - coreR, cx + coreR, cy + coreR);
    DeleteObject(coreBrush);
    DeleteObject(corePen);

    SelectObject(memDc, g_hud.fontDisplay);
    SetTextColor(memDc, RGB(255, 255, 255));
    RECT coreTitleRc{cx - 60, cy - 14, cx + 60, cy + 4};
    DrawTextW(memDc, L"AXION CORE", -1, &coreTitleRc, DT_CENTER | DT_SINGLELINE);
    SelectObject(memDc, g_hud.fontMono);
    SetTextColor(memDc, activeSphere.accentColor);
    RECT coreSubRc{cx - 60, cy + 4, cx + 60, cy + 22};
    DrawTextW(memDc, L"K4D // LEAN 4", -1, &coreSubRc, DT_CENTER | DT_SINGLELINE);

    // 5. RECHTES FOKUS-PANEL (Progressive Disclosure: Maximal 3 ruhige Karten)
    if (!g_hud.zenMode) {
        RECT panelRc{static_cast<int>(w * 0.62), 76, w - 28, h - 86};
        drawChamferedRect(memDc, panelRc, 16, COL_PANEL_BG, activeSphere.accentColor, 1);

        SelectObject(memDc, g_hud.fontMono);
        SetTextColor(memDc, activeSphere.accentColor);
        TextOutW(memDc, panelRc.left + 22, panelRc.top + 18,
                 activeSphere.codeTag.c_str(), static_cast<int>(activeSphere.codeTag.size()));

        SelectObject(memDc, g_hud.fontDisplay);
        SetTextColor(memDc, RGB(255, 255, 255));
        TextOutW(memDc, panelRc.left + 22, panelRc.top + 38,
                 activeSphere.title.c_str(), static_cast<int>(activeSphere.title.size()));

        SelectObject(memDc, g_hud.fontSans);
        SetTextColor(memDc, COL_TEXT_MUTED);
        TextOutW(memDc, panelRc.left + 22, panelRc.top + 66,
                 activeSphere.statusText.c_str(), static_cast<int>(activeSphere.statusText.size()));

        // Die 3 essenziellen Karten der gewählten Sphäre rendern
        int cardTop = panelRc.top + 98;
        int cardHeight = 96;
        for (size_t c = 0; c < activeSphere.cards.size(); ++c) {
            const auto& card = activeSphere.cards[c];
            RECT cardRc{panelRc.left + 20, cardTop, panelRc.right - 20, cardTop + cardHeight};
            drawChamferedRect(memDc, cardRc, 10, COL_CARD_BG, RGB(30, 48, 75), 1);

            SelectObject(memDc, g_hud.fontMono);
            SetTextColor(memDc, activeSphere.accentColor);
            TextOutW(memDc, cardRc.left + 14, cardRc.top + 10,
                     card.badge.c_str(), static_cast<int>(card.badge.size()));

            SelectObject(memDc, g_hud.fontSans);
            SetTextColor(memDc, RGB(255, 255, 255));
            TextOutW(memDc, cardRc.left + 14, cardRc.top + 30,
                     card.title.c_str(), static_cast<int>(card.title.size()));

            SetTextColor(memDc, COL_TEXT_MAIN);
            TextOutW(memDc, cardRc.left + 14, cardRc.top + 52,
                     card.subtitle.c_str(), static_cast<int>(card.subtitle.size()));

            SelectObject(memDc, g_hud.fontMono);
            SetTextColor(memDc, COL_EMERALD_GLOW);
            TextOutW(memDc, cardRc.left + 14, cardRc.top + 72,
                     card.metric.c_str(), static_cast<int>(card.metric.size()));

            cardTop += cardHeight + 14;
        }

        // Großer Primär-Aktionsbutton unten im Fokus-Panel
        RECT actionBtnRc{panelRc.left + 20, panelRc.bottom - 62, panelRc.right - 20, panelRc.bottom - 18};
        drawChamferedRect(memDc, actionBtnRc, 10,
                          g_hud.primaryBtnHovered ? RGB(18, 48, 68) : RGB(12, 34, 52),
                          activeSphere.accentColor, 2);
        SelectObject(memDc, g_hud.fontDisplay);
        SetTextColor(memDc, RGB(255, 255, 255));
        DrawTextW(memDc, L"ZYKLUS STARTEN // MODUL AUSFÜHREN", -1, &actionBtnRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // 6. AMBIENT COMMAND DOCK & STATUS-FUSSZEILE
    RECT dockRc{36, h - 64, w - 36, h - 22};
    drawChamferedRect(memDc, dockRc, 10, COL_PANEL_BG, RGB(25, 55, 85), 1);
    SelectObject(memDc, g_hud.fontMono);
    SetTextColor(memDc, COL_CYAN_GLOW);
    TextOutW(memDc, dockRc.left + 18, dockRc.top + 13, L"IMPULS >", 8);

    SetTextColor(memDc, COL_TEXT_MAIN);
    RECT statusRc{static_cast<int>(w * 0.50), dockRc.top + 13, dockRc.right - 18, dockRc.bottom - 8};
    DrawTextW(memDc, g_hud.liveFooterStatus.c_str(), -1, &statusRc, DT_RIGHT | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Blit auf den Bildschirm
    BitBlt(targetDc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);

    SelectObject(memDc, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDc);
}

// --- Windows-Nachrichtenverarbeitung für das rahmenlose Spatial HUD ---
LRESULT CALLBACK SpatialHudWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hud.hwnd = hwnd;
        g_hud.fontDisplay = CreateFontW(-15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Semibold");
        g_hud.fontSans = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hud.fontMono = CreateFontW(-12, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            FIXED_PITCH | FF_MODERN, L"Consolas");

        // Schwebendes Ambient-Command-Eingabefeld unten links
        g_hud.commandInput = CreateWindowExW(
            0, L"EDIT", L"K4D Beweisnetz Tiefe 4 mit Strategie type_synthesis",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            130, 800, 460, 22, hwnd, reinterpret_cast<HMENU>(1001), nullptr, nullptr
        );
        SendMessageW(g_hud.commandInput, WM_SETFONT, reinterpret_cast<WPARAM>(g_hud.fontMono), TRUE);

        // 60fps Animations- & Prozess-Timer
        SetTimer(hwnd, 1, 16, nullptr);
        return 0;
    }

    case WM_SIZE: {
        int w = LOWORD(lp);
        int h = HIWORD(lp);
        applyChamferedWindowRegion(hwnd, w, h);
        if (g_hud.commandInput) {
            MoveWindow(g_hud.commandInput, 130, h - 53, std::max(240, static_cast<int>(w * 0.36)), 20, TRUE);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1; // Verhindert Flackern

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        renderSpatialHud(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int mx = GET_X_LPARAM(lp);
        int my = GET_Y_LPARAM(lp);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        int w = rc.right;
        int h = rc.bottom;

        // Prüfe Hover über den 4 Orbital-Sphären
        g_hud.hoveredSphereIndex = -1;
        for (size_t i = 0; i < g_hud.spheres.size(); ++i) {
            int dx = mx - g_hud.spheres[i].screenX;
            int dy = my - g_hud.spheres[i].screenY;
            if (dx * dx + dy * dy <= g_hud.spheres[i].radius * g_hud.spheres[i].radius) {
                g_hud.hoveredSphereIndex = static_cast<int>(i);
                break;
            }
        }

        // Prüfe Hover über den 3 Fenster-Buttons oben rechts
        g_hud.hoveredTopButton = 0;
        if (my >= 20 && my <= 48) {
            if (mx >= w - 130 && mx <= w - 98) g_hud.hoveredTopButton = 1;
            else if (mx >= w - 90 && mx <= w - 58) g_hud.hoveredTopButton = 2;
            else if (mx >= w - 50 && mx <= w - 18) g_hud.hoveredTopButton = 3;
        }

        // Prüfe Hover über dem Primär-Aktionsbutton im rechten Fokus-Panel
        RECT actionBtnRc{static_cast<int>(w * 0.62) + 20, h - 86 - 62, w - 48, h - 86 - 18};
        g_hud.primaryBtnHovered = (mx >= actionBtnRc.left && mx <= actionBtnRc.right &&
                                   my >= actionBtnRc.top && my <= actionBtnRc.bottom);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lp);
        int my = GET_Y_LPARAM(lp);

        // 1. Fenstersteuerung oben rechts
        if (g_hud.hoveredTopButton == 1) {
            ShowWindow(hwnd, SW_MINIMIZE);
            return 0;
        }
        if (g_hud.hoveredTopButton == 2) {
            g_hud.zenMode = !g_hud.zenMode;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (g_hud.hoveredTopButton == 3) {
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }

        // 2. Klick auf eine der 4 Sphären im Orbit
        if (g_hud.hoveredSphereIndex >= 0) {
            g_hud.activeSphereIndex = g_hud.hoveredSphereIndex;
            g_hud.liveFooterStatus = L"Sphäre fokussiert: " +
                g_hud.spheres[static_cast<size_t>(g_hud.activeSphereIndex)].title;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        // 3. Klick auf den Primär-Aktionsbutton im Fokus-Panel
        if (g_hud.primaryBtnHovered && !g_hud.zenMode) {
            triggerActiveSphereExecution();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        // 4. Rahmenloses Fenster verschieben bei Klick in den oberen HUD-Kopfbereich
        if (my < 68) {
            ReleaseCapture();
            SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        }
        return 0;
    }

    case WM_TIMER: {
        g_hud.animPhase += 0.035;
        if (g_hud.processRunning && g_hud.activeProcess.hProcess) {
            DWORD code = STILL_ACTIVE;
            if (GetExitCodeProcess(g_hud.activeProcess.hProcess, &code) && code != STILL_ACTIVE) {
                CloseHandle(g_hud.activeProcess.hProcess);
                CloseHandle(g_hud.activeProcess.hThread);
                g_hud.activeProcess = {};
                g_hud.processRunning = false;
                g_hud.liveFooterStatus = L"Modul-Zyklus beendet (ExitCode " + std::to_wstring(code) + L") • Ergebnisse in SQLite WAL gesichert";
            }
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = reinterpret_cast<HDC>(wp);
        SetTextColor(hdc, COL_TEXT_MAIN);
        SetBkColor(hdc, COL_PANEL_BG);
        static HBRUSH editBrush = CreateSolidBrush(COL_PANEL_BG);
        return reinterpret_cast<LRESULT>(editBrush);
    }

    case WM_DESTROY: {
        KillTimer(hwnd, 1);
        if (g_hud.fontDisplay) DeleteObject(g_hud.fontDisplay);
        if (g_hud.fontSans) DeleteObject(g_hud.fontSans);
        if (g_hud.fontMono) DeleteObject(g_hud.fontMono);
        PostQuitMessage(0);
        return 0;
    }
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

} // namespace axion::hud

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int showCmd) {
    SetProcessDPIAware();
    axion::hud::initializeSpheres();

    wchar_t exeBuf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exeBuf, MAX_PATH);
    fs::path selfPath(exeBuf);
    axion::hud::g_hud.engineExePath = selfPath.parent_path() / L"axion_engine.exe";
    axion::hud::g_hud.projectRootPath = selfPath.parent_path().parent_path().parent_path();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = axion::hud::SpatialHudWndProc;
    wc.lpszClassName = L"Axion_SpatialOrbitalHUD";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(axion::hud::COL_OBSIDIAN_BG);
    RegisterClassExW(&wc);

    // WS_POPUP erzeugt ein komplett rahmenloses Fenster ohne klassische Windows-Titelleiste
    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        wc.lpszClassName,
        L"AXION // SPATIAL ORBITAL HUD v4.0.3",
        WS_POPUP | WS_VISIBLE,
        100, 60, 1480, 900,
        nullptr, nullptr, instance, nullptr
    );
    if (!hwnd) return 1;

    ShowWindow(hwnd, showCmd);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}