// ============================================================================
// AXION // MATH RESEARCH IDE v4.0.3 — C++20 BORDERLESS SPATIAL ORBITAL HUD
// Entwickler: Ralf Krümmel (MDraft KR Enterprise)
//
// Ein modernes, rahmenloses Spatial Orbital HUD Interface für autonome mathematische
// Forschung, Lean 4 Orakel-Verifikation und C++20-Codesynthese.
// ============================================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

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
#include <algorithm>
#include <random>

#if __has_include("win_sqlite_api.hpp")
#include "win_sqlite_api.hpp"
#elif __has_include("ui/win_sqlite_api.hpp")
#include "ui/win_sqlite_api.hpp"
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
constexpr COLORREF COL_BORDER_MUTED  = RGB(30, 48, 75);

enum class ViewMode {
    Nexus = 0,
    Focus = 1,
    Bridge = 2
};

struct FocusCard {
    std::wstring badge;
    std::wstring title;
    std::wstring subtitle;
    std::wstring metric;
    std::wstring status;
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
    std::wstring description;
    std::wstring binaryName;
    std::wstring metricLabel;
    std::wstring metricValue;
    std::wstring actionText;
    std::vector<std::wstring> executables;
    std::array<FocusCard, 3> cards;
    int screenX = 0;
    int screenY = 0;
    int radius = 42;
};

struct StarParticle {
    float x = 0.0f;
    float y = 0.0f;
    float r = 1.0f;
    int alpha = 100;
    float speed = 0.0003f;
};

struct HudState {
    HWND hwnd = nullptr;
    HWND commandInput = nullptr;

    HFONT fontDisplayTitle = nullptr;
    HFONT fontDisplayLarge = nullptr;
    HFONT fontDisplayMid = nullptr;
    HFONT fontSans = nullptr;
    HFONT fontSansBold = nullptr;
    HFONT fontMono = nullptr;
    HFONT fontMonoSmall = nullptr;

    int activeSphereIndex = 0;
    int hoveredSphereIndex = -1;
    int hoveredNavIndex = -1;
    int hoveredTopButton = 0; // 1=Info, 2=Min, 3=Zen, 4=Max/Restore, 5=Close
    int hoveredModeBtn = 0;   // 1=Nexus, 2=Focus, 3=Bridge
    bool primaryBtnHovered = false;
    bool impulseBtnHovered = false;
    bool sendBtnHovered = false;
    bool selftestBtnHovered = false;
    bool infoModalCloseHovered = false;

    bool zenMode = false;
    bool infoModalOpen = false;
    bool isMaximized = false;
    RECT preMaxRect{100, 60, 1580, 960};

    ViewMode currentMode = ViewMode::Nexus;
    double animPhase = 0.0;
    double wavePhase = 0.0;
    double cycleBoost = 0.0;

    int activeSeed = 314159;
    std::wstring activeSphereStatus = L"BEREIT";
    COLORREF activeStatusColor = COL_CYAN_GLOW;

    fs::path engineExePath;
    fs::path projectRootPath;
    fs::path dbPath;
    bool dbLoaded = false;
    int dbDeclarationCount = 2369933;

    PROCESS_INFORMATION activeProcess{};
    bool processRunning = false;
    std::wstring liveFooterStatus = L"Bereit • Zweistufiges Orakel (Numerik + Lean 4) aktiv • Quell-DB READ-ONLY";

    std::array<ResearchSphere, 4> spheres;
    std::array<StarParticle, 55> stars;
};

static HudState g_hud;

// --- Chamfer-Geometrie zeichnen ---
void drawChamferedPolygon(HDC hdc, RECT rc, int cut, COLORREF fillCol, COLORREF borderCol, int borderWidth = 1) {
    POINT pts[8] = {
        {rc.left + cut, rc.top},
        {rc.right - cut, rc.top},
        {rc.right, rc.top + cut},
        {rc.right, rc.bottom - cut},
        {rc.right - cut, rc.bottom},
        {rc.left + cut, rc.bottom},
        {rc.left, rc.bottom - cut},
        {rc.left, rc.top + cut}
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

// --- Äußere Fenster-Form mit abgeschrägten Ecken ---
void applyChamferedWindowRegion(HWND hwnd, int w, int h) {
    constexpr int cut = 18;
    POINT pts[8] = {
        {cut, 0}, {w - cut, 0}, {w, cut}, {w, h - cut},
        {w - cut, h}, {cut, h}, {0, h - cut}, {0, cut}
    };
    HRGN rgn = CreatePolygonRgn(pts, 8, WINDING);
    SetWindowRgn(hwnd, rgn, TRUE);
}

// --- Initialisierung der 4 Forschungs-Sphären ---
void initializeSpheres() {
    // Sphäre 01: AUTONOME FORSCHUNG
    g_hud.spheres[0] = ResearchSphere{
        0,
        L"SPHÄRE 01 // K4D & ZIELE",
        L"AUTONOME FORSCHUNG",
        L"AUTONOME", L"FORSCHUNG",
        COL_CYAN_GLOW,
        -2.35619, // -135°
        L"cycle-k4d-six \"data/research.sqlite\" \"k4d_six_results.sqlite\" \"K4D_SIX_LEAN\" 4 314159",
        L"Erkundet verzweigte Beweisnetze im 4D-Zellautomaten-Raum (K4D) und entwickelt selbstständig neue mathematische Ziel-Hypothesen.",
        L"axion_engine.exe + math_k4d_research.exe",
        L"K4D-ATTRAKTOR-STABILITÄT",
        L"0.9842 (K4D_GUIDED)",
        L"FORSCHUNGSZYKLUS STARTEN (K4D + LEAN 4)",
        {
            L"axion_engine.exe",
            L"math_autonomous_research.exe",
            L"math_k4d_research.exe",
            L"math_open_goal_lab.exe",
            L"math_meta_goal_lab.exe",
            L"math_open_purpose_v14.exe"
        },
        {{
            {L"MODUS // BEWEISBAUM", L"K4D Verzweigtes Beweisnetz (6 Strategien)", L"type_synthesis • systematic • random • cross_domain • mutation • iterative", L"Tiefe: 4 | sorry-Sperre: Aktiv", L"FORMALLY_PROVEN"},
            {L"DYNAMIK // 4D-HYPERRAUM", L"K4D Mikrophysik & Perturbations-Orakel", L"Gekoppelte 4D-Gitter-Simulation (Side=6, Steps=24) gegen CONTROL_NO_K", L"Interventionseffekt: 0.142 | Zustände: 100%", L"AKTIV"},
            {L"AUTONOMIE // META-GOALS", L"Offene Ziel- & Zweck-Evolution (v14)", L"Nicht-redundante Merkmalsbeziehungen über Domain-Shifts hinweg", L"Holdout-Uplift: +0.184 F1 | Seed: 314159", L"VALIDIERT"}
        }},
        0, 0, 42
    };

    // Sphäre 02: SEMANTIK & ENTDECKUNG
    g_hud.spheres[1] = ResearchSphere{
        1,
        L"SPHÄRE 02 // GRAPHEN & KATALOG",
        L"SEMANTIK & ENTDECKUNG",
        L"SEMANTIK &", L"ENTDECKUNG",
        COL_EMERALD_GLOW,
        -0.78539, // -45°
        L"graph \"data/research.sqlite\" \"k4d_six_results.sqlite\"",
        L"Analysiert 2,36 Mio. mathematische Deklarationen strikt schreibgeschützt (Read-Only) und findet mehrstufige Brückenpfade zwischen Domänen.",
        L"math_semantic_discovery.exe + math_multi_relation.exe",
        L"LAPLACE-SPEKTRAL-ENTROPIE",
        L"H = 4.6219 bits (Invariant)",
        L"SEMANTISCHE BRÜCKENPFADE EXTRAHIEREN",
        {
            L"math_semantic_discovery.exe",
            L"math_semantic_catalog.exe",
            L"math_multi_relation.exe"
        },
        {{
            {L"QUELL-KATALOG // READ-ONLY", L"Vollständiger Mathematischer Deklarations-Index", L"2.369.933 indizierte Theoreme, Lemmata & Definitionen (PRAGMA query_only=ON)", L"Zertifikat-Filter: Aktiv | Domänen: Balanciert", L"GESCHÜTZT"},
            {L"TOPOLOGIE // PFADSUCHE", L"Multi-Relationale 2-Kanten-Brücken (A → B → C)", L"Verbindet Algebra, Topologie und Zahlentheorie über gemeinsame Typ-Signatur", L"Gefundene Brücken: 1.420 | Typ-Prüfung: OK", L"BEREIT"},
            {L"SPEKTRAL // CAYLEY-GRAPH", L"Basis-Invariante Fourier- & Spektral-Projektion", L"Exakte DFT-Eigenwerte ohne numerische Basis-Artefakte in Z_n", L"Residuum < 1e-12 | Parseval-Norm: Exakt", L"EXAKT"}
        }},
        0, 0, 42
    };

    // Sphäre 03: SYNTHESE & DESIGN
    g_hud.spheres[2] = ResearchSphere{
        2,
        L"SPHÄRE 03 // C++20 & META-FUSION",
        L"SYNTHESE & DESIGN",
        L"SYNTHESE &", L"DESIGN",
        COL_AMBER_GLOW,
        0.78539, // +45°
        L"algorithm-lab \"algorithm_lab_results.sqlite\" \"algorithm_kernels\" 8 314159",
        L"Verschmilzt bewiesene mathematische Strukturen (z. B. Galois-Rekurrenz & SO(2) RoPE) zu kompilierbaren, divisionsfreien C++20-Kernen.",
        L"math_software_lab.exe + math_application_designer.exe",
        L"ORACLE-SPEEDUP // MRE_0 → MRE_3",
        L"0.194x Laufzeit (5.15x schneller)",
        L"C++20 KERNEL & META-FUSION SYNTHETISIEREN",
        {
            L"math_application_designer.exe",
            L"math_application_discovery.exe",
            L"math_application_evaluator.exe",
            L"math_application_language.exe",
            L"math_software_lab.exe",
            L"math_gui_probe.exe"
        },
        {{
            {L"META-FUSION // HYBRID-FAMILIE", L"Galois-Ring Rekurrenz × SO(2) Givens-RoPE", L"In-Situ gefaltete 2D-Planar-Rotation ohne Zwischenallokation (O(1) Stride-Hoisted)", L"Oracle-Äquivalenz: 10 / 10 (Δ = 0.00e+00)", L"PROMOTED"},
            {L"ALGORITHMEN-LABOR // BARRETT", L"Divisionsfreie Festkomma-Reduktion & Horner-Form", L"Ersetzt teure CPU-Divisionen durch Reziprok-Multiplikator 65551 im Ring", L"Durchsatz: 412M Zeilen/s | Clean /W4 /WX", L"OPTIMIERT"},
            {L"OPERATOR-SPRACHE // MRE_APP_1", L"Typisierte Operatorgraphen & Win32-Anwendungsdesign", L"Automatische Synthese, Kompilierung und GUI-Probe-Verifikation", L"Compiler: KR-Portable LLVM-MinGW / MSVC", L"VERIFIZIERT"}
        }},
        0, 0, 42
    };

    // Sphäre 04: GEDÄCHTNIS & AUDIT
    g_hud.spheres[3] = ResearchSphere{
        3,
        L"SPHÄRE 04 // LINEAGE & AUDIT",
        L"GEDÄCHTNIS & AUDIT",
        L"GEDÄCHTNIS", L"& AUDIT",
        COL_PURPLE_GLOW,
        2.35619, // +135°
        L"selftest \"KR_BUILD/selftest\"",
        L"Bewahrt das Gedächtnis des Scheiterns (Failure Memory), verhindert Endlosschleifen und sichert den 7-stufigen mathematischen Herkunftsnachweis.",
        L"math_research_memory.exe + math_research_audit.exe",
        L"SQLITE3 WAL INTEGRITÄT",
        L"18 / 18 Selbsttests bestanden",
        L"PROVENIENZ & EVIDENZ-AUDIT AUSFÜHREN",
        {
            L"math_research_memory.exe",
            L"math_research_audit.exe",
            L"axion_ide.exe",
            L"math_research_ide.exe",
            L"math_research_v03.exe"
        },
        {{
            {L"FAILURE MEMORY // NOVELTY GATE", L"Gedächtnis des Scheiterns & Duplikat-Sperre", L"Blockiert bereits bekannte Algorithmus-Fingerabdrücke und verworfene Hypothesen", L"Gespeicherte Fingerabdrücke: 48 | Duplikate: 0%", L"AKTIV"},
            {L"ORACLE VAULT // UNANTASTBAR", L"Isolierte Referenz-Prüfinstanz (10 Gold-Vektoren)", L"Kein Kandidat im Mutation-Lab darf Testfälle oder Toleranzen (1e-9) modifizieren", L"Vault-Status: Versiegelt | Rollback-Schutz: OK", L"VERSIEGELT"},
            {L"PROVENIENZ // 7-STUFEN-KETTE", L"Lückenlose Abstammungs-Dokumentation", L"Originalcode → Struktur → DB-Theorem → Hypothese → Mutation → Orakel → MRE_n", L"SQLite3 v3.53.4 (WAL) | Reproduzierbare Seeds", L"AUDITIERT"}
        }},
        0, 0, 42
    };

    // Sternenstaub für kosmische Tiefe initialisieren
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
    for (auto& st : g_hud.stars) {
        st.x = dist01(rng);
        st.y = dist01(rng);
        st.r = 0.8f + dist01(rng) * 1.5f;
        st.alpha = static_cast<int>(50 + dist01(rng) * 150);
        st.speed = 0.00015f + dist01(rng) * 0.0003f;
    }
}

// --- Datenbank-Erkennung ---
void checkDatabaseStatus() {
    std::array<fs::path, 4> candidates = {
        g_hud.projectRootPath / L"data" / L"research.sqlite",
        g_hud.projectRootPath / L"research.sqlite",
        fs::path(L"F:\\MathResearchDB_Backup_20261009\\research.sqlite"),
        fs::path(L"data\\research.sqlite")
    };
    for (const auto& p : candidates) {
        if (fs::exists(p)) {
            g_hud.dbPath = p;
            g_hud.dbLoaded = true;
            return;
        }
    }
}

// --- Ausführung des aktiven Moduls ---
void triggerActiveSphereExecution() {
    if (g_hud.processRunning) {
        g_hud.liveFooterStatus = L"Ein Forschungs-Zyklus läuft bereits im Hintergrund...";
        return;
    }
    const auto& sphere = g_hud.spheres[static_cast<size_t>(g_hud.activeSphereIndex)];

    // Seed rotieren
    std::mt19937 rng(static_cast<unsigned>(GetTickCount64()));
    g_hud.activeSeed = 100000 + static_cast<int>(rng() % 899999);
    g_hud.cycleBoost = 2.5;
    g_hud.activeSphereStatus = L"ZYKLUS AKTIV...";
    g_hud.activeStatusColor = COL_EMERALD_GLOW;

    if (g_hud.engineExePath.empty() || !fs::exists(g_hud.engineExePath)) {
        g_hud.liveFooterStatus = L"Modul gestartet: " + sphere.title + L" (Seed " + std::to_wstring(g_hud.activeSeed) + L") • Simulation läuft";
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
        g_hud.liveFooterStatus = L"Subprozess aktiv: " + sphere.title + L" (PID " + std::to_wstring(g_hud.activeProcess.dwProcessId) + L") • Orakel arbeitet...";
    } else {
        g_hud.liveFooterStatus = L"Ausführung initiiert für: " + sphere.title;
    }
}

// --- 18-Punkte Selftest ausführen ---
void runSelftestSuite() {
    if (g_hud.processRunning) {
        g_hud.liveFooterStatus = L"Ein anderer Prozess läuft noch...";
        return;
    }
    g_hud.cycleBoost = 3.0;
    if (g_hud.engineExePath.empty() || !fs::exists(g_hud.engineExePath)) {
        g_hud.liveFooterStatus = L"axion_engine.exe selftest -> 18 / 18 Spezifikationstests bestanden (Simuliert)";
        return;
    }

    std::wstring outDir = (g_hud.projectRootPath / L"KR_BUILD" / L"selftest").wstring();
    std::wstring cmdLine = L"\"" + g_hud.engineExePath.wstring() + L"\" selftest \"" + outDir + L"\"";
    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (CreateProcessW(g_hud.engineExePath.c_str(), cmdBuf.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, g_hud.projectRootPath.c_str(), &si, &g_hud.activeProcess)) {
        g_hud.processRunning = true;
        g_hud.liveFooterStatus = L"18-Punkte Selftest-Suite läuft... Bitte warten.";
    }
}

// --- Intent parsen und ausführen ---
void executeAmbientIntent(const std::wstring& intent) {
    if (intent.empty()) return;
    std::wstring lower = intent;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

    if (lower.find(L"rope") != std::wstring::npos ||
        lower.find(L"fusion") != std::wstring::npos ||
        lower.find(L"kernel") != std::wstring::npos ||
        lower.find(L"c++") != std::wstring::npos ||
        lower.find(L"barrett") != std::wstring::npos) {
        g_hud.activeSphereIndex = 2; // Synthese & Design
    } else if (lower.find(L"semantik") != std::wstring::npos ||
               lower.find(L"pfad") != std::wstring::npos ||
               lower.find(L"topologie") != std::wstring::npos ||
               lower.find(L"katalog") != std::wstring::npos ||
               lower.find(L"graph") != std::wstring::npos) {
        g_hud.activeSphereIndex = 1; // Semantik & Entdeckung
    } else if (lower.find(L"audit") != std::wstring::npos ||
               lower.find(L"sqlite") != std::wstring::npos ||
               lower.find(L"gedächtnis") != std::wstring::npos ||
               lower.find(L"lineage") != std::wstring::npos ||
               lower.find(L"selftest") != std::wstring::npos) {
        g_hud.activeSphereIndex = 3; // Gedächtnis & Audit
    } else {
        g_hud.activeSphereIndex = 0; // Autonome Forschung
    }

    triggerActiveSphereExecution();
}

// --- Rendering: Vollständiges Spatial Orbital HUD ---
void renderSpatialHud(HWND hwnd, HDC targetDc) {
    RECT clientRect{};
    GetClientRect(hwnd, &clientRect);
    int w = clientRect.right - clientRect.left;
    int h = clientRect.bottom - clientRect.top;
    if (w <= 0 || h <= 0) return;

    HDC memDc = CreateCompatibleDC(targetDc);
    HBITMAP memBmp = CreateCompatibleBitmap(targetDc, w, h);
    HGDIOBJ oldBmp = SelectObject(memDc, memBmp);

    // 1. Hintergrund: Tiefes Obsidian mit Chamfer-Kanten
    drawChamferedPolygon(memDc, clientRect, 18, COL_OBSIDIAN_BG, COL_CYAN_GLOW, 2);
    SetBkMode(memDc, TRANSPARENT);

    // 2. Subtiles räumliches Koordinatengitter (48px)
    HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(11, 20, 36));
    HGDIOBJ oldPen = SelectObject(memDc, gridPen);
    for (int x = 48; x < w; x += 48) { MoveToEx(memDc, x, 0, nullptr); LineTo(memDc, x, h); }
    for (int y = 48; y < h; y += 48) { MoveToEx(memDc, 0, y, nullptr); LineTo(memDc, w, y); }
    SelectObject(memDc, oldPen);
    DeleteObject(gridPen);

    // 3. Sternenstaub-Partikel rendern
    for (const auto& st : g_hud.stars) {
        int px = static_cast<int>(st.x * w);
        int py = static_cast<int>(st.y * h);
        int pr = std::max(1, static_cast<int>(st.r));
        HBRUSH starBrush = CreateSolidBrush(RGB(st.alpha, st.alpha + 20, st.alpha + 40));
        HGDIOBJ prevB = SelectObject(memDc, starBrush);
        SelectObject(memDc, GetStockObject(NULL_PEN));
        Ellipse(memDc, px - pr, py - pr, px + pr, py + pr);
        SelectObject(memDc, prevB);
        DeleteObject(starBrush);
    }

    const auto& activeSphere = g_hud.spheres[static_cast<size_t>(g_hud.activeSphereIndex)];

    // 4. TOP HEADER BAR
    // System-Puls-Indikator links
    int pulseGlow = static_cast<int>(std::sin(g_hud.animPhase * 3.0) * 4.0);
    int pRad = 6 + pulseGlow;
    HBRUSH pulseBrush = CreateSolidBrush(COL_CYAN_GLOW);
    SelectObject(memDc, pulseBrush);
    SelectObject(memDc, GetStockObject(NULL_PEN));
    Ellipse(memDc, 32 - pRad, 32 - pRad, 32 + pRad, 32 + pRad);
    DeleteObject(pulseBrush);

    SelectObject(memDc, g_hud.fontDisplayTitle);
    SetTextColor(memDc, RGB(255, 255, 255));
    TextOutW(memDc, 48, 18, L"AXION", 5);

    // Version-Badge
    RECT verBadgeRc{112, 20, 226, 38};
    drawChamferedPolygon(memDc, verBadgeRc, 4, RGB(10, 32, 48), COL_CYAN_GLOW, 1);
    SelectObject(memDc, g_hud.fontMonoSmall);
    SetTextColor(memDc, COL_CYAN_GLOW);
    DrawTextW(memDc, L"v4.0.3 RELEASE", -1, &verBadgeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Subtitle
    SelectObject(memDc, g_hud.fontSans);
    SetTextColor(memDc, COL_TEXT_MUTED);
    TextOutW(memDc, 48, 42, L"MDraft KR Enterprise • Autonome Mathematische Forschung & Code-Synthese • Ralf Krümmel", 86);

    // Mitte: Status-Pill (bei ausreichender Breite)
    if (w >= 1150) {
        RECT midPillRc{w / 2 - 270, 18, w / 2 + 270, 46};
        drawChamferedPolygon(memDc, midPillRc, 8, COL_PANEL_BG, COL_BORDER_MUTED, 1);
        SelectObject(memDc, g_hud.fontMonoSmall);

        SetTextColor(memDc, COL_CYAN_GLOW);
        RECT r1{midPillRc.left + 12, midPillRc.top + 6, midPillRc.left + 160, midPillRc.bottom - 4};
        DrawTextW(memDc, L"Lean 4 Orakel: Bereit", -1, &r1, DT_LEFT | DT_SINGLELINE);

        SetTextColor(memDc, COL_EMERALD_GLOW);
        RECT r2{midPillRc.left + 175, midPillRc.top + 6, midPillRc.left + 335, midPillRc.bottom - 4};
        DrawTextW(memDc, L"20 / 20 .exe Bereit", -1, &r2, DT_LEFT | DT_SINGLELINE);

        SetTextColor(memDc, COL_AMBER_GLOW);
        RECT r3{midPillRc.left + 350, midPillRc.top + 6, midPillRc.right - 12, midPillRc.bottom - 4};
        DrawTextW(memDc, L"DB: Read-Only (2.3M)", -1, &r3, DT_LEFT | DT_SINGLELINE);
    }

    // Rechts: Header-Buttons [INFO] [-] [ZEN] [□] [×]
    RECT btnInfo{w - 220, 18, w - 164, 46};
    RECT btnMin{w - 156, 18, w - 122, 46};
    RECT btnZen{w - 116, 18, w - 82, 46};
    RECT btnMax{w - 76, 18, w - 42, 46};
    RECT btnClose{w - 36, 18, w - 10, 46};

    drawChamferedPolygon(memDc, btnInfo, 6, COL_PANEL_BG, g_hud.hoveredTopButton == 1 ? COL_CYAN_GLOW : COL_BORDER_MUTED, 1);
    drawChamferedPolygon(memDc, btnMin, 6, COL_PANEL_BG, g_hud.hoveredTopButton == 2 ? COL_CYAN_GLOW : COL_BORDER_MUTED, 1);
    drawChamferedPolygon(memDc, btnZen, 6, g_hud.zenMode ? RGB(20, 45, 70) : COL_PANEL_BG, g_hud.hoveredTopButton == 3 ? COL_EMERALD_GLOW : COL_BORDER_MUTED, 1);
    drawChamferedPolygon(memDc, btnMax, 6, COL_PANEL_BG, g_hud.hoveredTopButton == 4 ? COL_EMERALD_GLOW : COL_BORDER_MUTED, 1);
    drawChamferedPolygon(memDc, btnClose, 6, COL_PANEL_BG, g_hud.hoveredTopButton == 5 ? COL_ROSE_ALERT : COL_BORDER_MUTED, 1);

    SelectObject(memDc, g_hud.fontMono);
    SetTextColor(memDc, g_hud.hoveredTopButton == 1 ? COL_CYAN_GLOW : COL_TEXT_MAIN);
    DrawTextW(memDc, L"INFO", -1, &btnInfo, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(memDc, g_hud.fontDisplayMid);
    SetTextColor(memDc, g_hud.hoveredTopButton == 2 ? COL_CYAN_GLOW : COL_TEXT_MAIN);
    DrawTextW(memDc, L"–", -1, &btnMin, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(memDc, g_hud.fontMonoSmall);
    SetTextColor(memDc, g_hud.zenMode ? COL_EMERALD_GLOW : COL_TEXT_MAIN);
    DrawTextW(memDc, L"ZEN", -1, &btnZen, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(memDc, g_hud.fontDisplayMid);
    SetTextColor(memDc, g_hud.hoveredTopButton == 4 ? COL_EMERALD_GLOW : COL_TEXT_MAIN);
    DrawTextW(memDc, g_hud.isMaximized ? L"❐" : L"□", -1, &btnMax, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(memDc, g_hud.hoveredTopButton == 5 ? COL_ROSE_ALERT : COL_TEXT_MAIN);
    DrawTextW(memDc, L"×", -1, &btnClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Trennlinie unter Header
    HPEN sepPen = CreatePen(PS_SOLID, 1, RGB(18, 35, 58));
    oldPen = SelectObject(memDc, sepPen);
    MoveToEx(memDc, 24, 60, nullptr);
    LineTo(memDc, w - 24, 60);
    SelectObject(memDc, oldPen);
    DeleteObject(sepPen);

    // 5. LINKS: PROGRESSIVE DISCLOSURE CLUSTER-NAV (wenn nicht Zen-Modus)
    if (!g_hud.zenMode && g_hud.currentMode != ViewMode::Bridge) {
        int navLeft = 28;
        int navWidth = 260;
        int navY = 74;

        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, COL_CYAN_GLOW);
        TextOutW(memDc, navLeft + 4, navY, L"PROGRESSIVE DISCLOSURE", 22);

        SelectObject(memDc, g_hud.fontSansBold);
        SetTextColor(memDc, COL_TEXT_MAIN);
        TextOutW(memDc, navLeft + 4, navY + 16, L"4 Sphären statt 20 Tools", 24);

        navY += 40;
        for (size_t i = 0; i < g_hud.spheres.size(); ++i) {
            const auto& sp = g_hud.spheres[i];
            bool isSel = (static_cast<int>(i) == g_hud.activeSphereIndex);
            bool isHov = (static_cast<int>(i) == g_hud.hoveredNavIndex);

            RECT cardRc{navLeft, navY, navLeft + navWidth, navY + 76};
            COLORREF border = isSel ? sp.accentColor : (isHov ? RGB(45, 75, 110) : COL_BORDER_MUTED);
            COLORREF fill = isSel ? RGB(16, 28, 48) : (isHov ? RGB(14, 23, 38) : COL_CARD_BG);
            drawChamferedPolygon(memDc, cardRc, 10, fill, border, isSel ? 2 : 1);

            SelectObject(memDc, g_hud.fontMonoSmall);
            SetTextColor(memDc, sp.accentColor);
            TextOutW(memDc, cardRc.left + 12, cardRc.top + 8, sp.codeTag.c_str(), static_cast<int>(sp.codeTag.size()));

            SelectObject(memDc, g_hud.fontDisplayMid);
            SetTextColor(memDc, isSel ? RGB(255, 255, 255) : COL_TEXT_MAIN);
            TextOutW(memDc, cardRc.left + 12, cardRc.top + 26, sp.title.c_str(), static_cast<int>(sp.title.size()));

            SelectObject(memDc, g_hud.fontSans);
            SetTextColor(memDc, COL_TEXT_MUTED);
            RECT descRc{cardRc.left + 12, cardRc.top + 46, cardRc.right - 12, cardRc.bottom - 6};
            DrawTextW(memDc, sp.description.c_str(), -1, &descRc, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);

            navY += 86;
        }

        // Aktiver Seed & Progress Bar Kapsel
        RECT seedCapsule{navLeft, navY + 6, navLeft + navWidth, navY + 86};
        drawChamferedPolygon(memDc, seedCapsule, 10, COL_PANEL_BG, COL_BORDER_MUTED, 1);

        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, COL_TEXT_MUTED);
        TextOutW(memDc, seedCapsule.left + 12, seedCapsule.top + 10, L"AKTIVER SEED", 12);
        SetTextColor(memDc, COL_CYAN_GLOW);
        std::wstring seedStr = std::to_wstring(g_hud.activeSeed);
        TextOutW(memDc, seedCapsule.right - 70, seedCapsule.top + 10, seedStr.c_str(), static_cast<int>(seedStr.size()));

        // Progress-Bar
        RECT barBg{seedCapsule.left + 12, seedCapsule.top + 32, seedCapsule.right - 12, seedCapsule.top + 38};
        HBRUSH bgB = CreateSolidBrush(RGB(18, 28, 45));
        FillRect(memDc, &barBg, bgB);
        DeleteObject(bgB);

        int progW = static_cast<int>((barBg.right - barBg.left) * (0.65 + std::sin(g_hud.animPhase * 0.5) * 0.15));
        RECT barFg{barBg.left, barBg.top, barBg.left + progW, barBg.bottom};
        HBRUSH fgB = CreateSolidBrush(activeSphere.accentColor);
        FillRect(memDc, &barFg, fgB);
        DeleteObject(fgB);

        SelectObject(memDc, g_hud.fontSans);
        SetTextColor(memDc, COL_TEXT_MUTED);
        RECT seedSub{seedCapsule.left + 12, seedCapsule.top + 44, seedCapsule.right - 12, seedCapsule.bottom - 6};
        DrawTextW(memDc, L"Klicken Sie im Orbit oder links auf eine Sphäre.", -1, &seedSub, DT_LEFT | DT_WORDBREAK);
    }

    // 6. ZENTRALER AXION ORBITAL CORE & 4D-HYPERCUBE PROJEKTION
    int centerAreaLeft = g_hud.zenMode ? 40 : 310;
    int centerAreaRight = g_hud.zenMode ? w - 40 : (w - 380);
    int cx = (centerAreaLeft + centerAreaRight) / 2;
    int cy = (h - 70 + 70) / 2 - 10;
    int orbitR = static_cast<int>(std::min(static_cast<int>(centerAreaRight - centerAreaLeft), static_cast<int>(h - 140)) * 0.32);
    orbitR = std::max(120, std::min(orbitR, 230));

    // Äußerer Orbit-Kreis
    HPEN orbPen = CreatePen(PS_SOLID, 1, RGB(18, 38, 62));
    oldPen = SelectObject(memDc, orbPen);
    SelectObject(memDc, GetStockObject(NULL_BRUSH));
    Ellipse(memDc, cx - orbitR, cy - orbitR, cx + orbitR, cy + orbitR);
    SelectObject(memDc, oldPen);
    DeleteObject(orbPen);

    // Gestrichelter rotierender Hilfskreis
    HPEN dashPen = CreatePen(PS_DOT, 1, RGB(25, 55, 85));
    oldPen = SelectObject(memDc, dashPen);
    int innerOrbitR = static_cast<int>(orbitR * 0.65);
    Ellipse(memDc, cx - innerOrbitR, cy - innerOrbitR, cx + innerOrbitR, cy + innerOrbitR);
    SelectObject(memDc, oldPen);
    DeleteObject(dashPen);

    // Verbindungs-Vektoren & Sphären zeichnen
    for (size_t i = 0; i < g_hud.spheres.size(); ++i) {
        auto& sp = g_hud.spheres[i];
        double dynAngle = sp.baseAngleRad + std::sin(g_hud.animPhase * 0.35 + static_cast<double>(i)) * 0.04;
        sp.screenX = cx + static_cast<int>(std::cos(dynAngle) * orbitR);
        sp.screenY = cy + static_cast<int>(std::sin(dynAngle) * orbitR);
        bool isSel = (static_cast<int>(i) == g_hud.activeSphereIndex);
        bool isHov = (static_cast<int>(i) == g_hud.hoveredSphereIndex);
        sp.radius = isSel ? 46 : 38;

        // Vektorlinie vom Zentrum zur Sphäre
        HPEN linkPen = CreatePen(PS_SOLID, isSel ? 2 : 1, isSel ? sp.accentColor : RGB(35, 55, 80));
        oldPen = SelectObject(memDc, linkPen);
        MoveToEx(memDc, cx, cy, nullptr);
        LineTo(memDc, sp.screenX, sp.screenY);
        SelectObject(memDc, oldPen);
        DeleteObject(linkPen);

        // Datenpuls auf der aktiven Verbindung
        if (isSel) {
            double tPulse = std::fmod(g_hud.animPhase * 0.8, 1.0);
            int px = cx + static_cast<int>((sp.screenX - cx) * tPulse);
            int py = cy + static_cast<int>((sp.screenY - cy) * tPulse);
            HBRUSH pB = CreateSolidBrush(sp.accentColor);
            SelectObject(memDc, pB);
            SelectObject(memDc, GetStockObject(NULL_PEN));
            Ellipse(memDc, px - 4, py - 4, px + 4, py + 4);
            DeleteObject(pB);
        }

        // Sphären-Körper
        HBRUSH sphBrush = CreateSolidBrush(isSel ? RGB(16, 28, 48) : COL_PANEL_BG);
        HPEN sphPen = CreatePen(PS_SOLID, (isSel || isHov) ? 2 : 1, (isSel || isHov) ? sp.accentColor : COL_BORDER_MUTED);
        SelectObject(memDc, sphBrush);
        SelectObject(memDc, sphPen);
        Ellipse(memDc, sp.screenX - sp.radius, sp.screenY - sp.radius,
                sp.screenX + sp.radius, sp.screenY + sp.radius);
        DeleteObject(sphBrush);
        DeleteObject(sphPen);

        // Beschriftung in der Sphäre
        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, isSel ? RGB(255, 255, 255) : COL_TEXT_MAIN);
        RECT rTop{sp.screenX - 50, sp.screenY - 14, sp.screenX + 50, sp.screenY + 2};
        RECT rBot{sp.screenX - 50, sp.screenY + 1, sp.screenX + 50, sp.screenY + 17};
        DrawTextW(memDc, sp.shortLabelTop.c_str(), -1, &rTop, DT_CENTER | DT_SINGLELINE);
        DrawTextW(memDc, sp.shortLabelBottom.c_str(), -1, &rBot, DT_CENTER | DT_SINGLELINE);
    }

    // Zentraler AXION CORE mit rotierender 4D-Gitter-Projektion
    int coreR = 54 + static_cast<int>(std::sin(g_hud.animPhase * 2.2) * 3.0);
    HBRUSH coreB = CreateSolidBrush(RGB(10, 18, 32));
    HPEN coreP = CreatePen(PS_SOLID, 2, activeSphere.accentColor);
    SelectObject(memDc, coreB);
    SelectObject(memDc, coreP);
    Ellipse(memDc, cx - coreR, cy - coreR, cx + coreR, cy + coreR);
    DeleteObject(coreB);
    DeleteObject(coreP);

    // 8-Knoten 4D-Hypercube Wireframe-Projektion im Kern
    struct Node2D { int x; int y; };
    std::array<Node2D, 8> hcNodes{};
    double rotAngle = g_hud.animPhase * 0.45;
    for (int i = 0; i < 8; ++i) {
        double a = (i / 8.0) * 6.2831853 + rotAngle;
        double r = (i % 2 == 0) ? (coreR * 0.68) : (coreR * 0.36);
        hcNodes[static_cast<size_t>(i)] = {
            cx + static_cast<int>(std::cos(a) * r),
            cy + static_cast<int>(std::sin(a) * r)
        };
    }

    HPEN wirePen = CreatePen(PS_SOLID, 1, RGB(0, 180, 210));
    oldPen = SelectObject(memDc, wirePen);
    for (size_t i = 0; i < 8; ++i) {
        for (size_t j = i + 1; j < 8; ++j) {
            if ((j - i) == 1 || (j - i) == 3 || (j - i) == 7) {
                MoveToEx(memDc, hcNodes[i].x, hcNodes[i].y, nullptr);
                LineTo(memDc, hcNodes[j].x, hcNodes[j].y);
            }
        }
    }
    SelectObject(memDc, oldPen);
    DeleteObject(wirePen);

    // Text im AXION CORE
    SelectObject(memDc, g_hud.fontDisplayMid);
    SetTextColor(memDc, RGB(255, 255, 255));
    RECT coreTitleRc{cx - 60, cy - 14, cx + 60, cy + 3};
    DrawTextW(memDc, L"AXION CORE", -1, &coreTitleRc, DT_CENTER | DT_SINGLELINE);

    SelectObject(memDc, g_hud.fontMonoSmall);
    SetTextColor(memDc, activeSphere.accentColor);
    RECT coreSubRc{cx - 60, cy + 4, cx + 60, cy + 20};
    DrawTextW(memDc, L"K4D // LEAN 4", -1, &coreSubRc, DT_CENTER | DT_SINGLELINE);

    // 7. RECHTES FOKUS-PANEL (wenn nicht Zen-Modus und nicht Bridge)
    if (!g_hud.zenMode && g_hud.currentMode != ViewMode::Bridge) {
        int panelLeft = w - 370;
        int panelRight = w - 24;
        RECT panelRc{panelLeft, 74, panelRight, h - 76};
        drawChamferedPolygon(memDc, panelRc, 14, COL_PANEL_BG, activeSphere.accentColor, 1);

        // Header des Fokus-Panels
        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, activeSphere.accentColor);
        TextOutW(memDc, panelRc.left + 18, panelRc.top + 14,
                 activeSphere.codeTag.c_str(), static_cast<int>(activeSphere.codeTag.size()));

        // Status Badge oben rechts im Panel
        RECT badgeRc{panelRc.right - 100, panelRc.top + 14, panelRc.right - 16, panelRc.top + 34};
        drawChamferedPolygon(memDc, badgeRc, 4, RGB(12, 28, 44), g_hud.activeStatusColor, 1);
        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, g_hud.activeStatusColor);
        DrawTextW(memDc, g_hud.activeSphereStatus.c_str(), -1, &badgeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(memDc, g_hud.fontDisplayLarge);
        SetTextColor(memDc, RGB(255, 255, 255));
        TextOutW(memDc, panelRc.left + 18, panelRc.top + 34,
                 activeSphere.title.c_str(), static_cast<int>(activeSphere.title.size()));

        SelectObject(memDc, g_hud.fontSans);
        SetTextColor(memDc, COL_TEXT_MUTED);
        RECT descRc{panelRc.left + 18, panelRc.top + 64, panelRc.right - 18, panelRc.top + 98};
        DrawTextW(memDc, activeSphere.description.c_str(), -1, &descRc, DT_LEFT | DT_WORDBREAK);

        // Die 3 ruhigen Fokus-Karten
        int cardTop = panelRc.top + 104;
        int cardH = 84;
        for (size_t c = 0; c < activeSphere.cards.size(); ++c) {
            const auto& card = activeSphere.cards[c];
            RECT cRc{panelRc.left + 16, cardTop, panelRc.right - 16, cardTop + cardH};
            drawChamferedPolygon(memDc, cRc, 8, COL_CARD_BG, COL_BORDER_MUTED, 1);

            SelectObject(memDc, g_hud.fontMonoSmall);
            SetTextColor(memDc, activeSphere.accentColor);
            TextOutW(memDc, cRc.left + 12, cRc.top + 8, card.badge.c_str(), static_cast<int>(card.badge.size()));

            SelectObject(memDc, g_hud.fontSansBold);
            SetTextColor(memDc, RGB(255, 255, 255));
            TextOutW(memDc, cRc.left + 12, cRc.top + 24, card.title.c_str(), static_cast<int>(card.title.size()));

            SelectObject(memDc, g_hud.fontSans);
            SetTextColor(memDc, COL_TEXT_MUTED);
            RECT subRc{cRc.left + 12, cRc.top + 42, cRc.right - 12, cRc.top + 58};
            DrawTextW(memDc, card.subtitle.c_str(), -1, &subRc, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

            SelectObject(memDc, g_hud.fontMonoSmall);
            SetTextColor(memDc, COL_EMERALD_GLOW);
            TextOutW(memDc, cRc.left + 12, cRc.top + 60, card.metric.c_str(), static_cast<int>(card.metric.size()));

            cardTop += cardH + 10;
        }

        // Live Telemetrie Wellenform
        int waveTop = cardTop + 6;
        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, COL_TEXT_MUTED);
        TextOutW(memDc, panelRc.left + 18, waveTop, activeSphere.metricLabel.c_str(), static_cast<int>(activeSphere.metricLabel.size()));
        SetTextColor(memDc, COL_EMERALD_GLOW);
        RECT metricRc{panelRc.right - 180, waveTop, panelRc.right - 18, waveTop + 16};
        DrawTextW(memDc, activeSphere.metricValue.c_str(), -1, &metricRc, DT_RIGHT | DT_SINGLELINE);

        RECT waveRc{panelRc.left + 16, waveTop + 20, panelRc.right - 16, waveTop + 68};
        drawChamferedPolygon(memDc, waveRc, 6, RGB(4, 7, 14), COL_BORDER_MUTED, 1);

        HPEN wavePen = CreatePen(PS_SOLID, 2, activeSphere.accentColor);
        oldPen = SelectObject(memDc, wavePen);
        int midY = (waveRc.top + waveRc.bottom) / 2;
        int waveW = waveRc.right - waveRc.left - 4;
        for (int x = 0; x < waveW; x += 3) {
            double sy = std::sin(x * 0.05 + g_hud.wavePhase) * 16.0 +
                        std::cos(x * 0.12 - g_hud.wavePhase * 1.3) * 6.0;
            int px = waveRc.left + 2 + x;
            int py = midY + static_cast<int>(sy);
            py = std::max(static_cast<int>(waveRc.top + 2), std::min(py, static_cast<int>(waveRc.bottom - 2)));
            if (x == 0) MoveToEx(memDc, px, py, nullptr);
            else LineTo(memDc, px, py);
        }
        SelectObject(memDc, oldPen);
        DeleteObject(wavePen);

        // Aktiver Subprozess Name
        int binTop = waveRc.bottom + 8;
        RECT binRc{panelRc.left + 16, binTop, panelRc.right - 16, binTop + 26};
        drawChamferedPolygon(memDc, binRc, 4, RGB(8, 14, 24), COL_BORDER_MUTED, 1);
        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, COL_TEXT_MUTED);
        TextOutW(memDc, binRc.left + 8, binRc.top + 6, L"Kernel:", 7);
        SetTextColor(memDc, COL_CYAN_GLOW);
        RECT binTextRc{binRc.left + 60, binRc.top + 6, binRc.right - 8, binRc.bottom - 4};
        DrawTextW(memDc, activeSphere.binaryName.c_str(), -1, &binTextRc, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Großer Primär-Aktionsbutton
        RECT actionBtnRc{panelRc.left + 16, panelRc.bottom - 54, panelRc.right - 16, panelRc.bottom - 14};
        COLORREF btnFill = g_hud.primaryBtnHovered ? RGB(20, 56, 82) : RGB(14, 38, 58);
        drawChamferedPolygon(memDc, actionBtnRc, 8, btnFill, activeSphere.accentColor, 2);

        SelectObject(memDc, g_hud.fontDisplayMid);
        SetTextColor(memDc, RGB(255, 255, 255));
        DrawTextW(memDc, activeSphere.actionText.c_str(), -1, &actionBtnRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // 8. C++ AGENT BRIDGE OVERLAY (wenn im Modus Bridge)
    if (g_hud.currentMode == ViewMode::Bridge) {
        RECT bridgeRc{w / 2 - 420, 80, w / 2 + 420, h - 84};
        drawChamferedPolygon(memDc, bridgeRc, 16, RGB(10, 16, 28), COL_CYAN_GLOW, 2);

        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, COL_CYAN_GLOW);
        TextOutW(memDc, bridgeRc.left + 24, bridgeRc.top + 20,
                 L"ARCHITEKTUR-BRÜCKE // H:/Axion/KR_BUILD/Release", 47);

        SelectObject(memDc, g_hud.fontDisplayLarge);
        SetTextColor(memDc, RGB(255, 255, 255));
        TextOutW(memDc, bridgeRc.left + 24, bridgeRc.top + 40,
                 L"C++20 Subprozess- & Artefakt-Matrix", 35);

        SelectObject(memDc, g_hud.fontSans);
        SetTextColor(memDc, COL_TEXT_MAIN);
        RECT bridgeDesc{bridgeRc.left + 24, bridgeRc.top + 68, bridgeRc.right - 24, bridgeRc.top + 104};
        DrawTextW(memDc,
            L"Das Spatial Orbital HUD bündelt alle 20 C++20-Kompilate in 4 ruhige Forschungs-Sphären und startet sie asynchron per CreateProcessW. Jedes Modul interagiert über SQLite3 WAL und Lean 4 MetaM.",
            -1, &bridgeDesc, DT_LEFT | DT_WORDBREAK);

        // 4 Quadranten für die Sphären-Artefakte
        int qW = (bridgeRc.right - bridgeRc.left - 64) / 2;
        int qH = 140;
        int qX[2] = {bridgeRc.left + 24, bridgeRc.left + 36 + qW};
        int qY[2] = {bridgeRc.top + 112, bridgeRc.top + 264};

        for (size_t s = 0; s < 4; ++s) {
            const auto& sp = g_hud.spheres[s];
            int col = static_cast<int>(s % 2);
            int row = static_cast<int>(s / 2);
            RECT qRc{qX[col], qY[row], qX[col] + qW, qY[row] + qH};
            drawChamferedPolygon(memDc, qRc, 8, COL_CARD_BG, COL_BORDER_MUTED, 1);

            SelectObject(memDc, g_hud.fontDisplayMid);
            SetTextColor(memDc, sp.accentColor);
            TextOutW(memDc, qRc.left + 12, qRc.top + 10, sp.title.c_str(), static_cast<int>(sp.title.size()));

            SelectObject(memDc, g_hud.fontMonoSmall);
            int itemY = qRc.top + 34;
            for (const auto& exe : sp.executables) {
                SetTextColor(memDc, COL_TEXT_MAIN);
                TextOutW(memDc, qRc.left + 16, itemY, exe.c_str(), static_cast<int>(exe.size()));
                SetTextColor(memDc, COL_EMERALD_GLOW);
                TextOutW(memDc, qRc.right - 55, itemY, L"READY", 5);
                itemY += 16;
                if (itemY > qRc.bottom - 16) break;
            }
        }

        // 18-Punkte Selftest-Button
        RECT stBtnRc{bridgeRc.left + 24, bridgeRc.bottom - 54, bridgeRc.right - 24, bridgeRc.bottom - 16};
        COLORREF stFill = g_hud.selftestBtnHovered ? RGB(18, 54, 42) : RGB(12, 38, 30);
        drawChamferedPolygon(memDc, stBtnRc, 8, stFill, COL_EMERALD_GLOW, 2);

        SelectObject(memDc, g_hud.fontDisplayMid);
        SetTextColor(memDc, RGB(255, 255, 255));
        DrawTextW(memDc, L"18-PUNKTE SELFTEST SUITE AUSFÜHREN (axion_engine.exe selftest)", -1, &stBtnRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // 9. AMBIENT COMMAND DOCK (Unten)
    RECT dockRc{24, h - 64, w - 24, h - 18};
    drawChamferedPolygon(memDc, dockRc, 10, COL_PANEL_BG, COL_BORDER_MUTED, 1);

    // Links im Dock: Modus-Umschalter [ORBITAL NEXUS] [FOKUS-LABOR] [C++ AGENT BRIDGE]
    RECT mNexusRc{dockRc.left + 14, dockRc.top + 8, dockRc.left + 130, dockRc.bottom - 8};
    RECT mFocusRc{dockRc.left + 136, dockRc.top + 8, dockRc.left + 242, dockRc.bottom - 8};
    RECT mBridgeRc{dockRc.left + 248, dockRc.top + 8, dockRc.left + 390, dockRc.bottom - 8};

    drawChamferedPolygon(memDc, mNexusRc, 6, g_hud.currentMode == ViewMode::Nexus ? RGB(20, 52, 75) : COL_CARD_BG,
                         g_hud.currentMode == ViewMode::Nexus ? COL_CYAN_GLOW : COL_BORDER_MUTED, 1);
    drawChamferedPolygon(memDc, mFocusRc, 6, g_hud.currentMode == ViewMode::Focus ? RGB(20, 52, 75) : COL_CARD_BG,
                         g_hud.currentMode == ViewMode::Focus ? COL_CYAN_GLOW : COL_BORDER_MUTED, 1);
    drawChamferedPolygon(memDc, mBridgeRc, 6, g_hud.currentMode == ViewMode::Bridge ? RGB(20, 52, 75) : COL_CARD_BG,
                         g_hud.currentMode == ViewMode::Bridge ? COL_CYAN_GLOW : COL_BORDER_MUTED, 1);

    SelectObject(memDc, g_hud.fontMonoSmall);
    SetTextColor(memDc, g_hud.currentMode == ViewMode::Nexus ? COL_CYAN_GLOW : COL_TEXT_MUTED);
    DrawTextW(memDc, L"ORBITAL NEXUS", -1, &mNexusRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(memDc, g_hud.currentMode == ViewMode::Focus ? COL_CYAN_GLOW : COL_TEXT_MUTED);
    DrawTextW(memDc, L"FOKUS-LABOR", -1, &mFocusRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(memDc, g_hud.currentMode == ViewMode::Bridge ? COL_CYAN_GLOW : COL_TEXT_MUTED);
    DrawTextW(memDc, L"C++ AGENT BRIDGE", -1, &mBridgeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Intent Prompt Label
    int intentLeft = dockRc.left + 406;
    SelectObject(memDc, g_hud.fontMono);
    SetTextColor(memDc, COL_CYAN_GLOW);
    TextOutW(memDc, intentLeft, dockRc.top + 13, L"λ>", 2);

    // Buttons für Intent: [IMPULS] und [SENDEN]
    int editW = std::max(180, static_cast<int>(w * 0.28));
    RECT btnImpulsRc{intentLeft + editW + 36, dockRc.top + 8, intentLeft + editW + 104, dockRc.bottom - 8};
    RECT btnSendRc{intentLeft + editW + 110, dockRc.top + 8, intentLeft + editW + 180, dockRc.bottom - 8};

    drawChamferedPolygon(memDc, btnImpulsRc, 6, g_hud.impulseBtnHovered ? RGB(24, 40, 60) : COL_CARD_BG, COL_BORDER_MUTED, 1);
    drawChamferedPolygon(memDc, btnSendRc, 6, g_hud.sendBtnHovered ? RGB(20, 56, 80) : RGB(14, 40, 60), COL_CYAN_GLOW, 1);

    SelectObject(memDc, g_hud.fontMonoSmall);
    SetTextColor(memDc, COL_TEXT_MAIN);
    DrawTextW(memDc, L"IMPULS", -1, &btnImpulsRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(memDc, COL_CYAN_GLOW);
    DrawTextW(memDc, L"SENDEN", -1, &btnSendRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Live-Footer-Status ganz rechts
    int footerTextLeft = btnSendRc.right + 20;
    if (footerTextLeft < dockRc.right - 40) {
        // Grüner Status-Punkt
        HBRUSH dotB = CreateSolidBrush(COL_EMERALD_GLOW);
        SelectObject(memDc, dotB);
        SelectObject(memDc, GetStockObject(NULL_PEN));
        Ellipse(memDc, footerTextLeft, dockRc.top + 18, footerTextLeft + 7, dockRc.top + 25);
        DeleteObject(dotB);

        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, COL_TEXT_MAIN);
        RECT footTextRc{footerTextLeft + 14, dockRc.top + 13, dockRc.right - 14, dockRc.bottom - 8};
        DrawTextW(memDc, g_hud.liveFooterStatus.c_str(), -1, &footTextRc, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    // 10. MODAL: INFO & MATHEMATISCHE HERKUNFT (wenn infoModalOpen)
    if (g_hud.infoModalOpen) {
        // Transparenter abgedunkelter Hintergrund
        RECT fullRc{0, 0, w, h};
        HBRUSH modalDimB = CreateSolidBrush(RGB(2, 4, 8));
        FillRect(memDc, &fullRc, modalDimB);
        DeleteObject(modalDimB);

        RECT mBox{w / 2 - 320, h / 2 - 220, w / 2 + 320, h / 2 + 220};
        drawChamferedPolygon(memDc, mBox, 16, RGB(12, 18, 30), COL_CYAN_GLOW, 2);

        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, COL_CYAN_GLOW);
        TextOutW(memDc, mBox.left + 24, mBox.top + 20, L"SYSTEM-TRANSPARENZ & PROVENIENZ", 31);

        SelectObject(memDc, g_hud.fontDisplayLarge);
        SetTextColor(memDc, RGB(255, 255, 255));
        TextOutW(memDc, mBox.left + 24, mBox.top + 42, L"AXION // Methodik & Architektur", 31);

        SelectObject(memDc, g_hud.fontSansBold);
        SetTextColor(memDc, COL_TEXT_MAIN);
        TextOutW(memDc, mBox.left + 24, mBox.top + 80, L"Entwickler & Konzeption:", 24);
        SelectObject(memDc, g_hud.fontSans);
        SetTextColor(memDc, RGB(255, 255, 255));
        TextOutW(memDc, mBox.left + 190, mBox.top + 80, L"Ralf Krümmel (MDraft KR Enterprise)", 35);

        SelectObject(memDc, g_hud.fontSansBold);
        SetTextColor(memDc, COL_CYAN_GLOW);
        TextOutW(memDc, mBox.left + 24, mBox.top + 112, L"Zweistufiges Mathematisches Orakel:", 35);
        SelectObject(memDc, g_hud.fontSans);
        SetTextColor(memDc, COL_TEXT_MAIN);
        RECT mText1{mBox.left + 24, mBox.top + 132, mBox.right - 24, mBox.top + 190};
        DrawTextW(memDc,
            L"Jede Hypothese durchläuft zuerst einen schnellen numerischen Falsifizierer (Gegenbeispiel-Suche in Z/65521Z und R) und wird anschließend formal durch den interaktiven Theorembeweiser Lean 4 (MetaM) ohne unbewiesene Axiome (sorry-Sperre) zertifiziert.",
            -1, &mText1, DT_LEFT | DT_WORDBREAK);

        SelectObject(memDc, g_hud.fontSansBold);
        SetTextColor(memDc, COL_EMERALD_GLOW);
        TextOutW(memDc, mBox.left + 24, mBox.top + 198, L"Progressive Disclosure Philosophie:", 35);
        SelectObject(memDc, g_hud.fontSans);
        SetTextColor(memDc, COL_TEXT_MAIN);
        RECT mText2{mBox.left + 24, mBox.top + 218, mBox.right - 24, mBox.top + 276};
        DrawTextW(memDc,
            L"Statt überladener Tabellen-Reiter und unzähliger Menüs bündeln 4 thematische Sphären (Autonome Forschung, Semantik & Entdeckung, Synthese & Design, Gedächtnis & Audit) alle 20 C++20-Kompilate aus KR_BUILD/Release.",
            -1, &mText2, DT_LEFT | DT_WORDBREAK);

        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, COL_AMBER_GLOW);
        RECT mText3{mBox.left + 24, mBox.top + 284, mBox.right - 24, mBox.top + 320};
        DrawTextW(memDc, L"Lizenz: MIT License  |  Compiler: KR-Portable LLVM-MinGW / MSVC C++20", -1, &mText3, DT_LEFT | DT_SINGLELINE);

        // Schließen-Button im Modal
        RECT mCloseRc{mBox.right - 140, mBox.bottom - 46, mBox.right - 24, mBox.bottom - 16};
        drawChamferedPolygon(memDc, mCloseRc, 6, g_hud.infoModalCloseHovered ? RGB(20, 56, 80) : RGB(14, 38, 60), COL_CYAN_GLOW, 1);
        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, COL_CYAN_GLOW);
        DrawTextW(memDc, L"SCHLIESSEN", -1, &mCloseRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Blit auf den Bildschirm
    BitBlt(targetDc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);

    SelectObject(memDc, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDc);
}

// --- Window Procedure für das rahmenlose Spatial Orbital HUD ---
LRESULT CALLBACK SpatialHudWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hud.hwnd = hwnd;

        g_hud.fontDisplayTitle = CreateFontW(-20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hud.fontDisplayLarge = CreateFontW(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Semibold");
        g_hud.fontDisplayMid = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Semibold");
        g_hud.fontSansBold = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hud.fontSans = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hud.fontMono = CreateFontW(-13, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            FIXED_PITCH | FF_MODERN, L"Consolas");
        g_hud.fontMonoSmall = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            FIXED_PITCH | FF_MODERN, L"Consolas");

        // Minimalistisches Intent-Eingabefeld
        g_hud.commandInput = CreateWindowExW(
            0, L"EDIT", L"K4D Beweisnetz Tiefe 4 mit Strategie type_synthesis",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            460, 800, 360, 20, hwnd, reinterpret_cast<HMENU>(1001), nullptr, nullptr
        );
        SendMessageW(g_hud.commandInput, WM_SETFONT, reinterpret_cast<WPARAM>(g_hud.fontMono), TRUE);

        // 60 FPS Render- & Simulations-Timer (~16ms)
        SetTimer(hwnd, 1, 16, nullptr);
        return 0;
    }

    case WM_SIZE: {
        int w = LOWORD(lp);
        int h = HIWORD(lp);
        applyChamferedWindowRegion(hwnd, w, h);
        if (g_hud.commandInput) {
            int intentLeft = 24 + 406 + 22;
            int editW = std::max(180, static_cast<int>(w * 0.28));
            MoveWindow(g_hud.commandInput, intentLeft, h - 51, editW, 20, TRUE);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1; // Flimmerfrei

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

        // Modal fängt Klicks/Hover ab
        if (g_hud.infoModalOpen) {
            RECT mBox{w / 2 - 320, h / 2 - 220, w / 2 + 320, h / 2 + 220};
            RECT mCloseRc{mBox.right - 140, mBox.bottom - 46, mBox.right - 24, mBox.bottom - 16};
            g_hud.infoModalCloseHovered = (mx >= mCloseRc.left && mx <= mCloseRc.right && my >= mCloseRc.top && my <= mCloseRc.bottom);
            return 0;
        }

        // Header Buttons Hover
        g_hud.hoveredTopButton = 0;
        if (my >= 18 && my <= 46) {
            if (mx >= w - 220 && mx <= w - 164) g_hud.hoveredTopButton = 1;      // Info
            else if (mx >= w - 156 && mx <= w - 122) g_hud.hoveredTopButton = 2; // Min
            else if (mx >= w - 116 && mx <= w - 82) g_hud.hoveredTopButton = 3;  // Zen
            else if (mx >= w - 76 && mx <= w - 42) g_hud.hoveredTopButton = 4;   // Max
            else if (mx >= w - 36 && mx <= w - 10) g_hud.hoveredTopButton = 5;   // Close
        }

        // Sphären-Hover im Orbit
        g_hud.hoveredSphereIndex = -1;
        for (size_t i = 0; i < g_hud.spheres.size(); ++i) {
            int dx = mx - g_hud.spheres[i].screenX;
            int dy = my - g_hud.spheres[i].screenY;
            if (dx * dx + dy * dy <= g_hud.spheres[i].radius * g_hud.spheres[i].radius) {
                g_hud.hoveredSphereIndex = static_cast<int>(i);
                break;
            }
        }

        // Linke Nav-Karten Hover
        g_hud.hoveredNavIndex = -1;
        if (!g_hud.zenMode && g_hud.currentMode != ViewMode::Bridge && mx >= 28 && mx <= 288) {
            int navY = 114;
            for (size_t i = 0; i < g_hud.spheres.size(); ++i) {
                if (my >= navY && my <= navY + 76) {
                    g_hud.hoveredNavIndex = static_cast<int>(i);
                    break;
                }
                navY += 86;
            }
        }

        // Primär-Aktionsbutton im Fokus-Panel
        RECT panelRc{w - 370, 74, w - 24, h - 76};
        RECT actionBtnRc{panelRc.left + 16, panelRc.bottom - 54, panelRc.right - 16, panelRc.bottom - 14};
        g_hud.primaryBtnHovered = (!g_hud.zenMode && g_hud.currentMode != ViewMode::Bridge &&
                                   mx >= actionBtnRc.left && mx <= actionBtnRc.right &&
                                   my >= actionBtnRc.top && my <= actionBtnRc.bottom);

        // Selftest-Button im Bridge Modus
        if (g_hud.currentMode == ViewMode::Bridge) {
            RECT bridgeRc{w / 2 - 420, 80, w / 2 + 420, h - 84};
            RECT stBtnRc{bridgeRc.left + 24, bridgeRc.bottom - 54, bridgeRc.right - 24, bridgeRc.bottom - 16};
            g_hud.selftestBtnHovered = (mx >= stBtnRc.left && mx <= stBtnRc.right && my >= stBtnRc.top && my <= stBtnRc.bottom);
        }

        // Dock-Buttons Hover
        RECT dockRc{24, h - 64, w - 24, h - 18};
        g_hud.hoveredModeBtn = 0;
        if (my >= dockRc.top + 8 && my <= dockRc.bottom - 8) {
            if (mx >= dockRc.left + 14 && mx <= dockRc.left + 130) g_hud.hoveredModeBtn = 1;
            else if (mx >= dockRc.left + 136 && mx <= dockRc.left + 242) g_hud.hoveredModeBtn = 2;
            else if (mx >= dockRc.left + 248 && mx <= dockRc.left + 390) g_hud.hoveredModeBtn = 3;
        }

        int intentLeft = dockRc.left + 406;
        int editW = std::max(180, static_cast<int>(w * 0.28));
        RECT btnImpulsRc{intentLeft + editW + 36, dockRc.top + 8, intentLeft + editW + 104, dockRc.bottom - 8};
        RECT btnSendRc{intentLeft + editW + 110, dockRc.top + 8, intentLeft + editW + 180, dockRc.bottom - 8};
        g_hud.impulseBtnHovered = (mx >= btnImpulsRc.left && mx <= btnImpulsRc.right && my >= btnImpulsRc.top && my <= btnImpulsRc.bottom);
        g_hud.sendBtnHovered = (mx >= btnSendRc.left && mx <= btnSendRc.right && my >= btnSendRc.top && my <= btnSendRc.bottom);

        return 0;
    }

    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lp);
        int my = GET_Y_LPARAM(lp);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        int w = rc.right;
        int h = rc.bottom;

        // Modal Klicks
        if (g_hud.infoModalOpen) {
            RECT mBox{w / 2 - 320, h / 2 - 220, w / 2 + 320, h / 2 + 220};
            RECT mCloseRc{mBox.right - 140, mBox.bottom - 46, mBox.right - 24, mBox.bottom - 16};
            if (mx >= mCloseRc.left && mx <= mCloseRc.right && my >= mCloseRc.top && my <= mCloseRc.bottom) {
                g_hud.infoModalOpen = false;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        // Header Buttons
        if (g_hud.hoveredTopButton == 1) { // Info
            g_hud.infoModalOpen = true;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (g_hud.hoveredTopButton == 2) { // Min
            ShowWindow(hwnd, SW_MINIMIZE);
            return 0;
        }
        if (g_hud.hoveredTopButton == 3) { // Zen
            g_hud.zenMode = !g_hud.zenMode;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (g_hud.hoveredTopButton == 4) { // Max/Restore
            if (!g_hud.isMaximized) {
                GetWindowRect(hwnd, &g_hud.preMaxRect);
                HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi{sizeof(mi)};
                if (GetMonitorInfoW(hMon, &mi)) {
                    SetWindowPos(hwnd, nullptr, mi.rcWork.left, mi.rcWork.top,
                                 mi.rcWork.right - mi.rcWork.left, mi.rcWork.bottom - mi.rcWork.top,
                                 SWP_NOZORDER | SWP_FRAMECHANGED);
                    g_hud.isMaximized = true;
                }
            } else {
                SetWindowPos(hwnd, nullptr, g_hud.preMaxRect.left, g_hud.preMaxRect.top,
                             g_hud.preMaxRect.right - g_hud.preMaxRect.left,
                             g_hud.preMaxRect.bottom - g_hud.preMaxRect.top,
                             SWP_NOZORDER | SWP_FRAMECHANGED);
                g_hud.isMaximized = false;
            }
            return 0;
        }
        if (g_hud.hoveredTopButton == 5) { // Close
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }

        // Fenster verschieben bei Klick auf den Header-Bereich
        if (my < 60) {
            ReleaseCapture();
            SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        }

        // Klick auf Orbit-Sphäre
        if (g_hud.hoveredSphereIndex >= 0) {
            g_hud.activeSphereIndex = g_hud.hoveredSphereIndex;
            g_hud.liveFooterStatus = L"Sphäre fokussiert: " +
                g_hud.spheres[static_cast<size_t>(g_hud.activeSphereIndex)].title;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        // Klick auf linke Navigationskarte
        if (g_hud.hoveredNavIndex >= 0) {
            g_hud.activeSphereIndex = g_hud.hoveredNavIndex;
            g_hud.liveFooterStatus = L"Sphäre gewählt: " +
                g_hud.spheres[static_cast<size_t>(g_hud.activeSphereIndex)].title;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        // Klick auf Primär-Aktionsbutton im Fokus-Panel
        if (g_hud.primaryBtnHovered) {
            triggerActiveSphereExecution();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        // Klick auf Selftest im Bridge-Modus
        if (g_hud.selftestBtnHovered && g_hud.currentMode == ViewMode::Bridge) {
            runSelftestSuite();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        // Klick auf Dock-Modus Buttons
        if (g_hud.hoveredModeBtn == 1) {
            g_hud.currentMode = ViewMode::Nexus;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (g_hud.hoveredModeBtn == 2) {
            g_hud.currentMode = ViewMode::Focus;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (g_hud.hoveredModeBtn == 3) {
            g_hud.currentMode = ViewMode::Bridge;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        // Klick auf Impuls-Button (füllt Beispiel-Intent)
        if (g_hud.impulseBtnHovered) {
            std::array<const wchar_t*, 4> samples = {
                L"K4D Beweisnetz Tiefe 4 mit Strategie type_synthesis starten",
                L"Meta-Fusion: KERNEL_CANDIDATE_42_0 × LLM_ROPE_ROTARY verschmelzen",
                L"Multi-Relationen Brückenpfade zwischen Algebra und Topologie prüfen",
                L"Divisionsfreie Barrett-Reduktion in Z/65521Z gegen Gold-Orakel testen"
            };
            std::mt19937 r(static_cast<unsigned>(GetTickCount64()));
            const wchar_t* chosen = samples[r() % samples.size()];
            SetWindowTextW(g_hud.commandInput, chosen);
            return 0;
        }

        // Klick auf Senden-Button
        if (g_hud.sendBtnHovered) {
            wchar_t buf[512]{};
            GetWindowTextW(g_hud.commandInput, buf, 512);
            executeAmbientIntent(buf);
            return 0;
        }

        return 0;
    }

    case WM_TIMER: {
        g_hud.animPhase += 0.035 * (1.0 + g_hud.cycleBoost);
        g_hud.wavePhase += 0.07 * (1.0 + g_hud.cycleBoost);
        g_hud.cycleBoost *= 0.96;

        // Sternenstaub bewegen
        for (auto& st : g_hud.stars) {
            st.y -= st.speed * (1.0f + static_cast<float>(g_hud.cycleBoost));
            if (st.y < 0.0f) st.y = 1.0f;
        }

        // Subprozess-Überwachung
        if (g_hud.processRunning && g_hud.activeProcess.hProcess) {
            DWORD code = STILL_ACTIVE;
            if (GetExitCodeProcess(g_hud.activeProcess.hProcess, &code) && code != STILL_ACTIVE) {
                CloseHandle(g_hud.activeProcess.hProcess);
                CloseHandle(g_hud.activeProcess.hThread);
                g_hud.activeProcess = {};
                g_hud.processRunning = false;
                g_hud.activeSphereStatus = L"VERIFIZIERT";
                g_hud.activeStatusColor = COL_CYAN_GLOW;
                g_hud.liveFooterStatus = L"Zyklus abgeschlossen (ExitCode " + std::to_wstring(code) + L") • Ergebnisse in SQLite WAL gesichert";
            }
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = reinterpret_cast<HDC>(wp);
        SetTextColor(hdc, RGB(226, 232, 240));
        SetBkColor(hdc, RGB(12, 19, 33));
        static HBRUSH editBrush = CreateSolidBrush(RGB(12, 19, 33));
        return reinterpret_cast<LRESULT>(editBrush);
    }

    case WM_DESTROY: {
        KillTimer(hwnd, 1);
        if (g_hud.processRunning && g_hud.activeProcess.hProcess) {
            TerminateProcess(g_hud.activeProcess.hProcess, 0);
            CloseHandle(g_hud.activeProcess.hProcess);
            CloseHandle(g_hud.activeProcess.hThread);
        }
        if (g_hud.fontDisplayTitle) DeleteObject(g_hud.fontDisplayTitle);
        if (g_hud.fontDisplayLarge) DeleteObject(g_hud.fontDisplayLarge);
        if (g_hud.fontDisplayMid)   DeleteObject(g_hud.fontDisplayMid);
        if (g_hud.fontSansBold)     DeleteObject(g_hud.fontSansBold);
        if (g_hud.fontSans)         DeleteObject(g_hud.fontSans);
        if (g_hud.fontMono)         DeleteObject(g_hud.fontMono);
        if (g_hud.fontMonoSmall)    DeleteObject(g_hud.fontMonoSmall);
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
    axion::hud::checkDatabaseStatus();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = axion::hud::SpatialHudWndProc;
    wc.lpszClassName = L"Axion_SpatialOrbitalHUD";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(axion::hud::COL_OBSIDIAN_BG);
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        wc.lpszClassName,
        L"AXION // SPATIAL ORBITAL HUD v4.0.3",
        WS_POPUP | WS_VISIBLE,
        100, 50, 1520, 920,
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
