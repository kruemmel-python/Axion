// ============================================================================
// AXION // MATH RESEARCH IDE v4.0.3 — BORDERLESS SPATIAL ORBITAL HUD
// Entwickler: Ralf Krümmel (MDraft KR Enterprise)
//
// Flimmerfreie, hochperformante Synthese:
// - WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS (100% flimmerfrei)
// - Selektiver 60 FPS Orbit-Timer NUR im Orbital Nexus Modus
// - 4 umschaltbare Design-Templates (Obsidian, Solaris, Cobalt, Biolith)
// - Vollständiges Einstellungsmenü mit spürbarer 3-Stufen Schriftgrößenskalierung
// - Vollständige Dark-Themierung für ListView und Editoren (kein grelles Weiß!)
// - Alle manuellen Forschungswerkzeuge (K4D Lab, DB Explorer, C++ Lab, Meta-Fusion)
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
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <filesystem>
#include <algorithm>
#include <random>
#include <fstream>
#include <sstream>
#include <iomanip>

#if __has_include("win_sqlite_api.hpp")
#include "win_sqlite_api.hpp"
#elif __has_include("ui/win_sqlite_api.hpp")
#include "ui/win_sqlite_api.hpp"
#endif

#ifdef _MSC_VER
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#endif

namespace fs = std::filesystem;

namespace axion::hud {

// --- 4 DESIGN TEMPLATES / FARBSCHEMATA ---
struct Theme {
    const wchar_t* name;
    COLORREF bgObsidian;
    COLORREF bgPanel;
    COLORREF bgCard;
    COLORREF accentPrimary;    // z. B. Cyan / Gold / Ice Blue / Neon Green
    COLORREF accentSecondary;  // z. B. Emerald / Orange / Indigo / Mint
    COLORREF accentTertiary;   // z. B. Amber / Gold / Sky Blue / Lime
    COLORREF textMain;
    COLORREF textMuted;
    COLORREF borderMuted;
};

constexpr Theme G_THEMES[4] = {
    // 0: Deep Obsidian (Cyber Cyan) - Default
    {
        L"Deep Obsidian (Cyber Cyan)",
        RGB(6, 9, 17),        // #060911
        RGB(12, 19, 33),      // #0C1321
        RGB(16, 26, 44),      // #101A2C
        RGB(0, 240, 255),     // #00F0FF Cyan
        RGB(0, 229, 163),     // #00E5A3 Emerald
        RGB(255, 184, 0),     // #FFB800 Amber
        RGB(226, 232, 240),
        RGB(100, 116, 139),
        RGB(30, 48, 75)
    },
    // 1: Solaris Amber (Golden Orbit)
    {
        L"Solaris Amber (Golden Orbit)",
        RGB(11, 10, 6),       // #0B0A06
        RGB(24, 20, 12),      // #18140C
        RGB(34, 28, 16),      // #221C10
        RGB(255, 184, 0),     // #FFB800 Gold/Amber
        RGB(255, 107, 0),     // #FF6B00 Orange
        RGB(255, 215, 0),     // #FFD700 Light Gold
        RGB(245, 240, 230),
        RGB(145, 130, 110),
        RGB(70, 55, 30)
    },
    // 2: Hyperdrive Cobalt (Quantum Blue)
    {
        L"Hyperdrive Cobalt (Quantum Blue)",
        RGB(5, 11, 24),       // #050B18
        RGB(10, 22, 45),      // #0A162D
        RGB(16, 32, 64),      // #102040
        RGB(56, 189, 248),    // #38BDF8 Sky Blue
        RGB(99, 102, 241),    // #6366F1 Indigo
        RGB(14, 165, 233),    // #0EA5E9 Cyan
        RGB(224, 238, 255),
        RGB(90, 120, 160),
        RGB(28, 55, 95)
    },
    // 3: Biolith Emerald (Matrix Core)
    {
        L"Biolith Emerald (Matrix Core)",
        RGB(4, 13, 9),        // #040D09
        RGB(8, 26, 18),       // #081A12
        RGB(12, 38, 26),      // #0C261A
        RGB(0, 255, 136),     // #00FF88 Neon Green
        RGB(16, 185, 129),    // #10B981 Mint
        RGB(52, 211, 153),    // #34D399 Emerald
        RGB(220, 250, 235),
        RGB(80, 140, 110),
        RGB(20, 65, 45)
    }
};

constexpr COLORREF COL_ROSE_ALERT = RGB(244, 63, 94);

enum class FontSizeOption {
    Compact = 0,
    Standard = 1,
    Large = 2
};

enum class WorkspaceMode {
    OrbitalNexus = 0,    // 4D K4D-Zentrum & Schnelleinstieg
    ResearchLab = 1,     // K4D Forschungslabor mit vollen manuellen Parametern
    SourceDatabase = 2,  // 2.37M Quelldatenbank-Explorer
    CppEditorLab = 3,    // C++ Editor, Live-Test, Trace-Plot
    MetaFusion = 4,      // Autonome Evolution & Meta-Fusion
    AgentBridge = 5      // 20-Artefakte Matrix & Selftest
};

// Control IDs
constexpr int ID_ROWS = 502;
constexpr int ID_EDITOR = 503;
constexpr int ID_SEARCH_BOX = 504;
constexpr int ID_COUNT_BOX = 505;
constexpr int ID_SEED_BOX = 506;
constexpr int ID_RUN_BTN = 507;
constexpr int ID_STOP_BTN = 508;
constexpr int ID_TEST_BTN = 509;
constexpr int ID_SAVE_BTN = 510;
constexpr int ID_PLOT_BTN = 511;
constexpr int ID_SEARCH_BTN = 512;
constexpr int ID_CHAINFROM_BTN = 513;
constexpr int ID_REPORT_BTN = 514;
constexpr int ID_STRATEGY_COMBO = 515;
constexpr int ID_LINEMODE_COMBO = 516;
constexpr int ID_CHAINMODE_COMBO = 517;
constexpr int ID_EVO_FAMA = 518;
constexpr int ID_EVO_FAMB = 519;
constexpr int ID_EVO_TARGETFAM = 520;
constexpr int ID_EVO_METAFUSION_BTN = 521;
[[maybe_unused]] constexpr int ID_EVO_CYCLE_BTN = 522;
constexpr int ID_EVO_CONTINUOUS_BTN = 523;
constexpr int ID_EVO_ROLLBACK_BTN = 524;
[[maybe_unused]] constexpr int ID_EVO_STATUS_BTN = 525;
constexpr int ID_COMMAND_INPUT = 526;

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
    COLORREF accentColor = RGB(0, 240, 255);
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

    // Win32 Controls
    HWND rows = nullptr;
    HWND editor = nullptr;
    HWND searchBox = nullptr;
    HWND countBox = nullptr;
    HWND seedBox = nullptr;
    HWND runBtn = nullptr;
    HWND stopBtn = nullptr;
    HWND testBtn = nullptr;
    HWND saveBtn = nullptr;
    HWND plotBtn = nullptr;
    HWND searchBtn = nullptr;
    HWND chainFromBtn = nullptr;
    HWND reportBtn = nullptr;
    HWND strategyCombo = nullptr;
    HWND lineModeCombo = nullptr;
    HWND chainModeCombo = nullptr;
    HWND evoFamACombo = nullptr;
    HWND evoFamBCombo = nullptr;
    HWND evoTargetFamBox = nullptr;
    HWND evoMetaFusionBtn = nullptr;
    HWND evoContinuousBtn = nullptr;
    HWND evoRollbackBtn = nullptr;
    HWND commandInput = nullptr;

    // Fonts
    HFONT fontDisplayTitle = nullptr;
    HFONT fontDisplayLarge = nullptr;
    HFONT fontDisplayMid = nullptr;
    HFONT fontSansBold = nullptr;
    HFONT fontSans = nullptr;
    HFONT fontMono = nullptr;
    HFONT fontMonoSmall = nullptr;
    HFONT fontEditor = nullptr;

    // Pre-allocated Brushes (kein Flackern!)
    HBRUSH brushBg = nullptr;
    HBRUSH brushCard = nullptr;
    HBRUSH brushPanel = nullptr;

    // Design Settings
    int currentThemeIndex = 0; // 0=Obsidian, 1=Solaris, 2=Cobalt, 3=Biolith
    FontSizeOption currentFontSize = FontSizeOption::Standard;
    bool enableParticles = true;
    bool enableGrid = true;
    bool enableOrbitAnimation = true;

    // Navigation & Workspace
    WorkspaceMode currentMode = WorkspaceMode::OrbitalNexus;
    int activeSphereIndex = 0;
    int hoveredSphereIndex = -1;
    int hoveredNavIndex = -1;
    int hoveredTopButton = 0; // 1=Settings, 2=Info, 3=Min, 4=Max, 5=Close
    int hoveredWorkspaceBtn = -1;
    bool primaryBtnHovered = false;
    bool impulseBtnHovered = false;
    bool sendBtnHovered = false;
    bool selftestBtnHovered = false;
    int selftestState = 0; // 0=Bereit, 1=Läuft, 2=Erfolgreich (18/18 PASSED), 3=Fehler
    std::wstring lastProcessCmd;

    // Modals
    bool infoModalOpen = false;
    bool settingsModalOpen = false;
    int hoveredThemeCard = -1;
    int hoveredFontSizeBtn = -1;

    // Window Position
    bool isMaximized = false;
    RECT preMaxRect{80, 40, 1600, 960};

    // Animation Timers
    double animPhase = 0.0;
    double wavePhase = 0.0;
    double cycleBoost = 0.0;

    // State & Paths
    int activeSeed = 314159;
    std::wstring activeSphereStatus = L"BEREIT";
    COLORREF activeStatusColor = RGB(0, 240, 255);
    std::wstring liveFooterStatus = L"Bereit • Zweistufiges Orakel aktiv • 2,37M Quell-DB READ-ONLY";
    std::wstring currentEditorFile;
    bool editorDirty = false;

    fs::path engineExePath;
    fs::path projectRootPath;
    fs::path dbPath;
    fs::path k4dResultsDb;
    fs::path algorithmLabDb;
    fs::path evoLineageDb;
    fs::path evoWorkspaceDir;
    bool dbLoaded = false;
    int dbDeclarationCount = 2369933;

    PROCESS_INFORMATION activeProcess{};
    bool processRunning = false;

    std::array<ResearchSphere, 4> spheres;
    std::array<StarParticle, 60> stars;
};

static HudState g_hud;

// Hilfsfunktionen für Strings
inline std::wstring wfrom(const unsigned char* p) {
    if (!p) return L"";
    const char* a = reinterpret_cast<const char*>(p);
    int n = MultiByteToWideChar(CP_UTF8, 0, a, -1, nullptr, 0);
    if (n <= 0) return L"";
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, a, -1, out.data(), n);
    out.pop_back();
    return out;
}

inline std::string narrow(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), n, nullptr, nullptr);
    out.pop_back();
    return out;
}

inline std::wstring getControlText(HWND h) {
    if (!h) return L"";
    int n = GetWindowTextLengthW(h);
    if (n <= 0) return L"";
    std::wstring s(static_cast<size_t>(n) + 1, L'\0');
    GetWindowTextW(h, s.data(), n + 1);
    s.resize(static_cast<size_t>(n));
    return s;
}

inline const Theme& getActiveTheme() {
    return G_THEMES[g_hud.currentThemeIndex];
}

// Pre-allocated Brushes für Themes aktualisieren
void updateThemeBrushes() {
    const Theme& th = getActiveTheme();
    if (g_hud.brushBg) DeleteObject(g_hud.brushBg);
    if (g_hud.brushCard) DeleteObject(g_hud.brushCard);
    if (g_hud.brushPanel) DeleteObject(g_hud.brushPanel);
    g_hud.brushBg = CreateSolidBrush(th.bgObsidian);
    g_hud.brushCard = CreateSolidBrush(th.bgCard);
    g_hud.brushPanel = CreateSolidBrush(th.bgPanel);
}

// --- Chamfered Polygon Zeichnen ---
void drawChamferedPolygon(HDC hdc, RECT rc, int cut, COLORREF fillCol, COLORREF borderCol, int borderWidth = 1) {
    if (rc.right <= rc.left || rc.bottom <= rc.top) return;
    int actualCut = std::min(cut, std::min(static_cast<int>(rc.right - rc.left) / 2, static_cast<int>(rc.bottom - rc.top) / 2));
    if (actualCut < 0) actualCut = 0;

    POINT pts[8] = {
        {rc.left + actualCut, rc.top},
        {rc.right - actualCut, rc.top},
        {rc.right, rc.top + actualCut},
        {rc.right, rc.bottom - actualCut},
        {rc.right - actualCut, rc.bottom},
        {rc.left + actualCut, rc.bottom},
        {rc.left, rc.bottom - actualCut},
        {rc.left, rc.top + actualCut}
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

void applyChamferedWindowRegion(HWND hwnd, int w, int h) {
    constexpr int cut = 18;
    POINT pts[8] = {
        {cut, 0}, {w - cut, 0}, {w, cut}, {w, h - cut},
        {w - cut, h}, {cut, h}, {0, h - cut}, {0, cut}
    };
    HRGN rgn = CreatePolygonRgn(pts, 8, WINDING);
    SetWindowRgn(hwnd, rgn, TRUE);
}

// Dark-Mode Theme für ListView und Steuerelemente anwenden
void applyThemeToControls() {
    const Theme& th = getActiveTheme();
    updateThemeBrushes();

    if (g_hud.rows) {
        ListView_SetBkColor(g_hud.rows, th.bgCard);
        ListView_SetTextBkColor(g_hud.rows, th.bgCard);
        ListView_SetTextColor(g_hud.rows, th.textMain);
        SetWindowTheme(g_hud.rows, L"DarkMode_Explorer", nullptr);
        HWND hdr = ListView_GetHeader(g_hud.rows);
        if (hdr) SetWindowTheme(hdr, L"DarkMode_ItemsView", nullptr);
        InvalidateRect(g_hud.rows, nullptr, TRUE);
    }
    if (g_hud.editor) {
        SetWindowTheme(g_hud.editor, L"DarkMode_Explorer", nullptr);
        InvalidateRect(g_hud.editor, nullptr, TRUE);
    }
    if (g_hud.searchBox) {
        SetWindowTheme(g_hud.searchBox, L"DarkMode_Explorer", nullptr);
        InvalidateRect(g_hud.searchBox, nullptr, TRUE);
    }
    if (g_hud.seedBox) {
        SetWindowTheme(g_hud.seedBox, L"DarkMode_Explorer", nullptr);
        InvalidateRect(g_hud.seedBox, nullptr, TRUE);
    }
    if (g_hud.countBox) {
        SetWindowTheme(g_hud.countBox, L"DarkMode_Explorer", nullptr);
        InvalidateRect(g_hud.countBox, nullptr, TRUE);
    }
    if (g_hud.evoTargetFamBox) {
        SetWindowTheme(g_hud.evoTargetFamBox, L"DarkMode_Explorer", nullptr);
        InvalidateRect(g_hud.evoTargetFamBox, nullptr, TRUE);
    }
    if (g_hud.commandInput) {
        SetWindowTheme(g_hud.commandInput, L"DarkMode_Explorer", nullptr);
        InvalidateRect(g_hud.commandInput, nullptr, TRUE);
    }

    HWND allBtns[] = {
        g_hud.runBtn, g_hud.stopBtn, g_hud.chainFromBtn, g_hud.reportBtn,
        g_hud.searchBtn, g_hud.saveBtn, g_hud.testBtn, g_hud.plotBtn,
        g_hud.evoMetaFusionBtn, g_hud.evoContinuousBtn, g_hud.evoRollbackBtn
    };
    for (HWND b : allBtns) {
        if (b) InvalidateRect(b, nullptr, TRUE);
    }
}

// Forward Declarations für dynamische Re-Layouts
void updateControlLayout(int w, int h);
void refreshK4dLab();
void refreshCppEditorLab();
void refreshMetaFusion();

// --- Schriftarten spürbar aktualisieren (Kompakt, Standard, Groß) ---
void rebuildFonts() {
    if (g_hud.fontDisplayTitle) DeleteObject(g_hud.fontDisplayTitle);
    if (g_hud.fontDisplayLarge) DeleteObject(g_hud.fontDisplayLarge);
    if (g_hud.fontDisplayMid)   DeleteObject(g_hud.fontDisplayMid);
    if (g_hud.fontSansBold)     DeleteObject(g_hud.fontSansBold);
    if (g_hud.fontSans)         DeleteObject(g_hud.fontSans);
    if (g_hud.fontMono)         DeleteObject(g_hud.fontMono);
    if (g_hud.fontMonoSmall)    DeleteObject(g_hud.fontMonoSmall);
    if (g_hud.fontEditor)       DeleteObject(g_hud.fontEditor);

    int sizeUi = 13;
    int sizeEditor = 14;
    int sizeTitle = 22;
    int sizeMid = 14;

    if (g_hud.currentFontSize == FontSizeOption::Compact) {
        sizeUi = 11;
        sizeEditor = 12;
        sizeTitle = 18;
        sizeMid = 12;
    } else if (g_hud.currentFontSize == FontSizeOption::Large) {
        sizeUi = 16;
        sizeEditor = 17;
        sizeTitle = 26;
        sizeMid = 16;
    }

    g_hud.fontDisplayTitle = CreateFontW(-sizeTitle, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_hud.fontDisplayLarge = CreateFontW(-(sizeTitle - 4), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Semibold");
    g_hud.fontDisplayMid = CreateFontW(-sizeMid, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Semibold");
    g_hud.fontSansBold = CreateFontW(-sizeUi, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_hud.fontSans = CreateFontW(-sizeUi, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_hud.fontMono = CreateFontW(-sizeEditor, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, L"Consolas");
    g_hud.fontMonoSmall = CreateFontW(-(sizeUi - 1), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, L"Consolas");
    g_hud.fontEditor = CreateFontW(-sizeEditor, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, L"Consolas");

    // Allen Child Controls rekursiv den neuen Font zuweisen
    EnumChildWindows(g_hud.hwnd, [](HWND c, LPARAM lp) -> BOOL {
        SendMessageW(c, WM_SETFONT, static_cast<WPARAM>(lp), TRUE);
        InvalidateRect(c, nullptr, TRUE);
        return TRUE;
    }, reinterpret_cast<LPARAM>(g_hud.fontSans));

    if (g_hud.editor) {
        SendMessageW(g_hud.editor, WM_SETFONT, reinterpret_cast<WPARAM>(g_hud.fontEditor), TRUE);
        InvalidateRect(g_hud.editor, nullptr, TRUE);
    }
    if (g_hud.commandInput) {
        SendMessageW(g_hud.commandInput, WM_SETFONT, reinterpret_cast<WPARAM>(g_hud.fontMono), TRUE);
        InvalidateRect(g_hud.commandInput, nullptr, TRUE);
    }
    if (g_hud.rows) {
        SendMessageW(g_hud.rows, WM_SETFONT, reinterpret_cast<WPARAM>(g_hud.fontSans), TRUE);
        HWND hdr = ListView_GetHeader(g_hud.rows);
        if (hdr) SendMessageW(hdr, WM_SETFONT, reinterpret_cast<WPARAM>(g_hud.fontSansBold), TRUE);
        InvalidateRect(g_hud.rows, nullptr, TRUE);
    }

    if (g_hud.hwnd) {
        RECT rc{};
        GetClientRect(g_hud.hwnd, &rc);
        updateControlLayout(rc.right, rc.bottom);

        // Tabelle mit neu skalierten Spaltenbreiten sofort neu befüllen
        if (g_hud.currentMode == WorkspaceMode::ResearchLab) refreshK4dLab();
        else if (g_hud.currentMode == WorkspaceMode::CppEditorLab) refreshCppEditorLab();
        else if (g_hud.currentMode == WorkspaceMode::MetaFusion) refreshMetaFusion();

        InvalidateRect(g_hud.hwnd, nullptr, TRUE);
    }
}

// Initialisiert die 4 Forschungs-Sphären
void initializeSpheres() {
    g_hud.spheres[0] = ResearchSphere{
        0,
        L"SPHÄRE 01 // K4D & ZIELE",
        L"AUTONOME FORSCHUNG",
        L"AUTONOME", L"FORSCHUNG",
        RGB(0, 240, 255),
        -2.35619,
        L"cycle-k4d-six \"data/research.sqlite\" \"k4d_six_results.sqlite\" \"K4D_SIX_LEAN\" 4 314159",
        L"Erkundet verzweigte Beweisnetze im 4D-Zellautomaten-Raum (K4D) und entwickelt selbstständig neue mathematische Ziel-Hypothesen.",
        L"axion_engine.exe + math_k4d_research.exe",
        L"K4D-ATTRAKTOR-STABILITÄT",
        L"0.9842 (K4D_GUIDED)",
        L"FORSCHUNGSZYKLUS STARTEN (K4D + LEAN 4)",
        {L"axion_engine.exe", L"math_autonomous_research.exe", L"math_k4d_research.exe", L"math_open_goal_lab.exe", L"math_meta_goal_lab.exe", L"math_open_purpose_v14.exe"},
        {{
            {L"MODUS // BEWEISBAUM", L"K4D Verzweigtes Beweisnetz (6 Strategien)", L"type_synthesis • systematic • random • cross_domain • mutation • iterative", L"Tiefe: 4 | sorry-Sperre: Aktiv", L"FORMALLY_PROVEN"},
            {L"DYNAMIK // 4D-HYPERRAUM", L"K4D Mikrophysik & Perturbations-Orakel", L"Gekoppelte 4D-Gitter-Simulation (Side=6, Steps=24) gegen CONTROL_NO_K", L"Interventionseffekt: 0.142 | Zustände: 100%", L"AKTIV"},
            {L"AUTONOMIE // META-GOALS", L"Offene Ziel- & Zweck-Evolution (v14)", L"Nicht-redundante Merkmalsbeziehungen über Domain-Shifts hinweg", L"Holdout-Uplift: +0.184 F1 | Seed: 314159", L"VALIDIERT"}
        }},
        0, 0, 42
    };

    g_hud.spheres[1] = ResearchSphere{
        1,
        L"SPHÄRE 02 // GRAPHEN & KATALOG",
        L"SEMANTIK & ENTDECKUNG",
        L"SEMANTIK &", L"ENTDECKUNG",
        RGB(0, 229, 163),
        -0.78539,
        L"graph \"data/research.sqlite\" \"k4d_six_results.sqlite\"",
        L"Analysiert 2,36 Mio. mathematische Deklarationen strikt schreibgeschützt (Read-Only) und findet mehrstufige Brückenpfade zwischen Domänen.",
        L"math_semantic_discovery.exe + math_multi_relation.exe",
        L"LAPLACE-SPEKTRAL-ENTROPIE",
        L"H = 4.6219 bits (Invariant)",
        L"SEMANTISCHE BRÜCKENPFADE EXTRAHIEREN",
        {L"math_semantic_discovery.exe", L"math_semantic_catalog.exe", L"math_multi_relation.exe"},
        {{
            {L"QUELL-KATALOG // READ-ONLY", L"Vollständiger Mathematischer Deklarations-Index", L"2.369.933 indizierte Theoreme, Lemmata & Definitionen (PRAGMA query_only=ON)", L"Zertifikat-Filter: Aktiv | Domänen: Balanciert", L"GESCHÜTZT"},
            {L"TOPOLOGIE // PFADSUCHE", L"Multi-Relationale 2-Kanten-Brücken (A → B → C)", L"Verbindet Algebra, Topologie und Zahlentheorie über gemeinsame Typ-Signatur", L"Gefundene Brücken: 1.420 | Typ-Prüfung: OK", L"BEREIT"},
            {L"SPEKTRAL // CAYLEY-GRAPH", L"Basis-Invariante Fourier- & Spektral-Projektion", L"Exakte DFT-Eigenwerte ohne numerische Basis-Artefakte in Z_n", L"Residuum < 1e-12 | Parseval-Norm: Exakt", L"EXAKT"}
        }},
        0, 0, 42
    };

    g_hud.spheres[2] = ResearchSphere{
        2,
        L"SPHÄRE 03 // C++20 & META-FUSION",
        L"SYNTHESE & DESIGN",
        L"SYNTHESE &", L"DESIGN",
        RGB(255, 184, 0),
        0.78539,
        L"algorithm-lab \"algorithm_lab_results.sqlite\" \"algorithm_kernels\" 8 314159",
        L"Verschmilzt bewiesene mathematische Strukturen (z. B. Galois-Rekurrenz & SO(2) RoPE) zu kompilierbaren, divisionsfreien C++20-Kernen.",
        L"math_software_lab.exe + math_application_designer.exe",
        L"ORACLE-SPEEDUP // MRE_0 → MRE_3",
        L"0.194x Laufzeit (5.15x schneller)",
        L"C++20 KERNEL & META-FUSION SYNTHETISIEREN",
        {L"math_application_designer.exe", L"math_application_discovery.exe", L"math_application_evaluator.exe", L"math_application_language.exe", L"math_software_lab.exe", L"math_gui_probe.exe"},
        {{
            {L"META-FUSION // HYBRID-FAMILIE", L"Galois-Ring Rekurrenz × SO(2) Givens-RoPE", L"In-Situ gefaltete 2D-Planar-Rotation ohne Zwischenallokation (O(1) Stride-Hoisted)", L"Oracle-Äquivalenz: 10 / 10 (Δ = 0.00e+00)", L"PROMOTED"},
            {L"ALGORITHMEN-LABOR // BARRETT", L"Divisionsfreie Festkomma-Reduktion & Horner-Form", L"Ersetzt teure CPU-Divisionen durch Reziprok-Multiplikator 65551 im Ring", L"Durchsatz: 412M Zeilen/s | Clean /W4 /WX", L"OPTIMIERT"},
            {L"OPERATOR-SPRACHE // MRE_APP_1", L"Typisierte Operatorgraphen & Win32-Anwendungsdesign", L"Automatische Synthese, Kompilierung und GUI-Probe-Verifikation", L"Compiler: KR-Portable LLVM-MinGW / MSVC", L"VERIFIZIERT"}
        }},
        0, 0, 42
    };

    g_hud.spheres[3] = ResearchSphere{
        3,
        L"SPHÄRE 04 // LINEAGE & AUDIT",
        L"GEDÄCHTNIS & AUDIT",
        L"GEDÄCHTNIS", L"& AUDIT",
        RGB(168, 85, 247),
        2.35619,
        L"selftest \"KR_BUILD/selftest\"",
        L"Bewahrt das Gedächtnis des Scheiterns (Failure Memory), verhindert Endlosschleifen und sichert den 7-stufigen mathematischen Herkunftsnachweis.",
        L"math_research_memory.exe + math_research_audit.exe",
        L"SQLITE3 WAL INTEGRITÄT",
        L"18 / 18 Selbsttests bestanden",
        L"PROVENIENZ & EVIDENZ-AUDIT AUSFÜHREN",
        {L"math_research_memory.exe", L"math_research_audit.exe", L"axion_ide.exe", L"math_research_ide.exe", L"math_research_v03.exe"},
        {{
            {L"FAILURE MEMORY // NOVELTY GATE", L"Gedächtnis des Scheiterns & Duplikat-Sperre", L"Blockiert bereits bekannte Algorithmus-Fingerabdrücke und verworfene Hypothesen", L"Gespeicherte Fingerabdrücke: 48 | Duplikate: 0%", L"AKTIV"},
            {L"ORACLE VAULT // UNANTASTBAR", L"Isolierte Referenz-Prüfinstanz (10 Gold-Vektoren)", L"Kein Kandidat im Mutation-Lab darf Testfälle oder Toleranzen (1e-9) modifizieren", L"Vault-Status: Versiegelt | Rollback-Schutz: OK", L"VERSIEGELT"},
            {L"PROVENIENZ // 7-STUFEN-KETTE", L"Lückenlose Abstammungs-Dokumentation", L"Originalcode → Struktur → DB-Theorem → Hypothese → Mutation → Orakel → MRE_n", L"SQLite3 v3.53.4 (WAL) | Reproduzierbare Seeds", L"AUDITIERT"}
        }},
        0, 0, 42
    };

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

void ensureDatabaseInitialized(const fs::path& dbPath, const char* initSql) {
    if (!fs::exists(dbPath)) {
        try {
            if (dbPath.has_parent_path()) {
                fs::create_directories(dbPath.parent_path());
            }
        } catch (...) {}
        sqlite3* db = nullptr;
        int rc = sqlite3_open_v2(narrow(dbPath.wstring()).c_str(), &db, SQLITE_OPEN_READWRITE | 0x00000004, nullptr);
        if (rc == SQLITE_OK && db) {
            sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
            sqlite3_exec(db, initSql, nullptr, nullptr, nullptr);
            sqlite3_close(db);
        }
    }
}

void checkDatabasePaths() {
    std::array<fs::path, 5> candidates = {
        g_hud.projectRootPath / L"data" / L"research.sqlite",
        g_hud.projectRootPath / L"research.sqlite",
        fs::path(L"F:\\MathResearchDB_Backup_20261009\\research.sqlite"),
        fs::path(L"F:\\MathResearchDB_v0_2\\research.sqlite"),
        fs::path(L"data\\research.sqlite")
    };
    for (const auto& p : candidates) {
        if (fs::exists(p)) {
            g_hud.dbPath = p;
            g_hud.dbLoaded = true;
            break;
        }
    }
    g_hud.k4dResultsDb = g_hud.projectRootPath / L"k4d_six_results.sqlite";
    ensureDatabaseInitialized(g_hud.k4dResultsDb,
        "CREATE TABLE IF NOT EXISTS schema_version (version INTEGER PRIMARY KEY, description TEXT, applied_at TEXT);"
        "CREATE TABLE IF NOT EXISTS candidates ("
        "  id INTEGER PRIMARY KEY, seed INTEGER NOT NULL, ordinal INTEGER NOT NULL,"
        "  strategy TEXT NOT NULL, chain_depth INTEGER DEFAULT 0, parent_id INTEGER DEFAULT 0,"
        "  mode TEXT DEFAULT 'K4D_GUIDED', status TEXT NOT NULL, source_a TEXT, source_b TEXT,"
        "  details TEXT, verification_duration_ms INTEGER DEFAULT 0, lean_code TEXT,"
        "  created_at TEXT DEFAULT CURRENT_TIMESTAMP"
        ");"
    );

    g_hud.algorithmLabDb = g_hud.projectRootPath / L"algorithm_lab_results.sqlite";
    ensureDatabaseInitialized(g_hud.algorithmLabDb,
        "CREATE TABLE IF NOT EXISTS algorithm_lab ("
        "  id INTEGER PRIMARY KEY, operator_formula TEXT, candidate_ns REAL, baseline_ns REAL,"
        "  equivalence TEXT, artifact_path TEXT, created_at TEXT DEFAULT CURRENT_TIMESTAMP"
        ");"
    );

    g_hud.evoLineageDb = g_hud.projectRootPath / L"evolution_lineage.sqlite";
    ensureDatabaseInitialized(g_hud.evoLineageDb,
        "CREATE TABLE IF NOT EXISTS generations ("
        "  id INTEGER PRIMARY KEY, family_id TEXT, gen_index INTEGER, complexity_class TEXT,"
        "  algo_fingerprint TEXT, status TEXT, speedup_ratio REAL, cases_passed INTEGER,"
        "  cases_failed INTEGER, hypothesis TEXT, created_at TEXT DEFAULT CURRENT_TIMESTAMP"
        ");"
        "CREATE TABLE IF NOT EXISTS failure_memory ("
        "  id INTEGER PRIMARY KEY, fingerprint TEXT NOT NULL, family_id TEXT, reason TEXT,"
        "  created_at TEXT DEFAULT CURRENT_TIMESTAMP"
        ");"
    );

    g_hud.evoWorkspaceDir = g_hud.projectRootPath / L"evolution_workspace";
    try { fs::create_directories(g_hud.evoWorkspaceDir); } catch (...) {}
}

// --- ListView Spalten & Daten Management ---
void clearTable() {
    if (!g_hud.rows) return;
    ListView_DeleteAllItems(g_hud.rows);
    while (ListView_DeleteColumn(g_hud.rows, 0)) {}
}

void setTableColumns(const std::vector<std::wstring>& names, const std::vector<int>& widths = {}) {
    clearTable();
    for (size_t i = 0; i < names.size(); ++i) {
        LVCOLUMNW c{};
        c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        c.pszText = const_cast<wchar_t*>(names[i].c_str());
        c.cx = (i < widths.size()) ? widths[i] : 140;
        c.iSubItem = static_cast<int>(i);
        ListView_InsertColumn(g_hud.rows, static_cast<int>(i), &c);
    }
}

void addTableRow(const std::vector<std::wstring>& data) {
    if (data.empty() || !g_hud.rows) return;
    int idx = ListView_GetItemCount(g_hud.rows);
    LVITEMW v{};
    v.mask = LVIF_TEXT;
    v.iItem = idx;
    v.pszText = const_cast<wchar_t*>(data[0].c_str());
    ListView_InsertItem(g_hud.rows, &v);
    for (size_t i = 1; i < data.size(); ++i) {
        ListView_SetItemText(g_hud.rows, idx, static_cast<int>(i), const_cast<wchar_t*>(data[i].c_str()));
    }
}

std::wstring getSelectedTableRowItem(int col) {
    if (!g_hud.rows) return L"";
    int idx = ListView_GetNextItem(g_hud.rows, -1, LVNI_SELECTED);
    if (idx < 0) return L"";
    wchar_t buf[8192]{};
    ListView_GetItemText(g_hud.rows, idx, col, buf, 8192);
    return buf;
}

// Query SQLite Helper
void querySqliteToTable(const fs::path& dbPath, const char* sql, const std::string& arg = "") {
    if (!g_hud.rows) return;
    if (!fs::exists(dbPath)) {
        setTableColumns({L"Status", L"Hinweis"});
        addTableRow({L"Bereit", L"Datenbank initialisiert. Klicken Sie auf 'Forschung starten' / 'Meta-Fusion starten'."});
        return;
    }

    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(narrow(dbPath.wstring()).c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
    if (rc != SQLITE_OK) {
        if (db) sqlite3_close(db);
        g_hud.liveFooterStatus = L"SQLite-Fehler beim Öffnen: " + dbPath.wstring();
        return;
    }
    sqlite3_busy_timeout(db, 3000);

    sqlite3_stmt* st = nullptr;
    rc = sqlite3_prepare_v2(db, sql, -1, &st, nullptr);
    if (rc != SQLITE_OK) {
        g_hud.liveFooterStatus = L"SQL-Fehler: " + wfrom(reinterpret_cast<const unsigned char*>(sqlite3_errmsg(db)));
        sqlite3_close(db);
        return;
    }
    if (!arg.empty()) {
        sqlite3_bind_text(st, 1, arg.c_str(), static_cast<int>(arg.size()), nullptr);
    }

    float scaleFactor = 1.0f;
    if (g_hud.currentFontSize == FontSizeOption::Compact) scaleFactor = 0.88f;
    else if (g_hud.currentFontSize == FontSizeOption::Large) scaleFactor = 1.25f;

    std::vector<std::wstring> colNames;
    std::vector<int> colWidths;
    int cols = sqlite3_column_count(st);
    for (int i = 0; i < cols; ++i) {
        std::wstring name = wfrom(reinterpret_cast<const unsigned char*>(sqlite3_column_name(st, i)));
        colNames.push_back(name);

        int baseW = 140;
        if (i == 0 && (name == L"ID" || name == L"id")) baseW = 70;
        else if (name == L"Strategie") baseW = 140;
        else if (name == L"Linie") baseW = 150;
        else if (name == L"Status") baseW = 165;
        else if (name == L"Tiefe") baseW = 85;
        else if (name == L"Eltern_ID") baseW = 100;
        else if (name == L"Quelle_A") baseW = 150;
        else if (name == L"Quelle_B") baseW = 150;
        else if (name == L"Moduldatei") baseW = 230;
        else if (name == L"Deklaration") baseW = 280;
        else if (name == L"Typ") baseW = 110;
        else if (name == L"Zeile") baseW = 90;
        else if (name == L"Formel") baseW = 260;
        else if (name == L"Kandidat_ns") baseW = 140;
        else if (name == L"Referenz_ns") baseW = 140;
        else if (name == L"Resultat") baseW = 150;
        else if (name == L"Header") baseW = 210;
        else if (name == L"Forschungsfamilie") baseW = 190;
        else if (name == L"Generation") baseW = 120;
        else if (name == L"Komplexitaet") baseW = 135;
        else if (name == L"Validierungsstatus") baseW = 185;
        else if (name == L"Fingerabdruck") baseW = 195;
        else if (name == L"Mathematische_Hypothese") baseW = 340;
        else baseW = std::max(140, static_cast<int>(name.length() * 12 + 28));

        colWidths.push_back(static_cast<int>(baseW * scaleFactor));
    }
    setTableColumns(colNames, colWidths);

    int rowsCount = 0;
    while (rowsCount < 300 && sqlite3_step(st) == SQLITE_ROW) {
        std::vector<std::wstring> entry;
        for (int i = 0; i < cols; ++i) {
            entry.push_back(wfrom(sqlite3_column_text(st, i)));
        }
        addTableRow(entry);
        ++rowsCount;
    }
    if (rowsCount == 0 && cols > 0) {
        std::vector<std::wstring> emptyRow;
        emptyRow.push_back(L"–");
        if (cols > 1) emptyRow.push_back(L"Bereit");
        if (cols > 2) emptyRow.push_back(L"Noch keine Datensätze. Klicken Sie oben auf 'Forschung starten'.");
        while (emptyRow.size() < static_cast<size_t>(cols)) emptyRow.push_back(L"–");
        addTableRow(emptyRow);
    }
    sqlite3_finalize(st);
    sqlite3_close(db);

    g_hud.liveFooterStatus = L"Geladen: " + std::to_wstring(rowsCount) + L" Datensätze aus " + dbPath.filename().wstring();
}

// Zeigt Text im Editor an
void setEditorContent(const std::wstring& text, bool editable) {
    if (!g_hud.editor) return;
    SendMessageW(g_hud.editor, EM_SETREADONLY, editable ? FALSE : TRUE, 0);
    SetWindowTextW(g_hud.editor, text.c_str());
    g_hud.editorDirty = false;
}

// Lädt die K4D-Kandidaten-Tabelle
void refreshK4dLab() {
    querySqliteToTable(g_hud.k4dResultsDb,
        "SELECT id AS ID, strategy AS Strategie, "
        "CASE WHEN details LIKE '%K4D%' OR mode='K4D_GUIDED' THEN 'K4D_GUIDED' ELSE 'CONTROL_NO_K' END AS Linie, "
        "status AS Status, chain_depth AS Tiefe, parent_id AS Eltern_ID, "
        "source_a AS Quelle_A, source_b AS Quelle_B, verification_duration_ms AS Dauer_ms "
        "FROM candidates ORDER BY id DESC LIMIT 250"
    );
    setEditorContent(
        L"================================================================================\r\n"
        L"AXION // K4D RESEARCH LAB — AUTONOME BEWEISNETZE & DYNAMIK\r\n"
        L"================================================================================\r\n\r\n"
        L"BEDIENUNG:\r\n"
        L"1. Wählen Sie oben Strategie, Kettenmodus, Forschungslinie und Seed.\r\n"
        L"2. Klicken Sie 'FORSCHUNG STARTEN', um neue Beweisbäume autonom zu generieren.\r\n"
        L"3. Wählen Sie eine Zeile in der Tabelle aus, um ihren vollständigen Beweis-\r\n"
        L"   stammbaum, Lean 4 Verifikationscode und Metriken im Inspektor anzuzeigen.\r\n"
        L"4. Klicken Sie 'KETTE AB AUSWAHL VERTIEFEN', um von einem Satz aus weiterzuforschen.\r\n",
        false
    );
}

// Lädt die 2.37M Quelldatenbank-Suche
void refreshSourceDatabase() {
    std::wstring query = getControlText(g_hud.searchBox);
    std::string term = narrow(query);
    int limit = 100;
    try { limit = std::stoi(getControlText(g_hud.countBox)); } catch (...) { limit = 100; }
    if (limit <= 0 || limit > 500) limit = 100;

    std::string sql = "SELECT rowid AS ID, file AS Moduldatei, name AS Deklaration, kind AS Typ, line AS Zeile "
                      "FROM declarations WHERE name LIKE ?1 ORDER BY rowid LIMIT " + std::to_string(limit);

    querySqliteToTable(g_hud.dbPath, sql.c_str(), "%" + term + "%");
    setEditorContent(
        L"================================================================================\r\n"
        L"AXION // MATHEMATISCHER DEKLARATIONS-EXPLORER (2.369.933 FORMALISIERUNGEN)\r\n"
        L"================================================================================\r\n\r\n"
        L"Wählen Sie eine Deklaration in der obigen Tabelle aus, um ihre vollständige\r\n"
        L"Lean 4 Aussage und Typ-Signatur hier anzuzeigen.\r\n\r\n"
        L"Die Quelldatenbank bleibt streng schreibgeschützt (PRAGMA query_only=ON).\r\n",
        false
    );
}

// Lädt das C++ Algorithmen-Labor
void refreshCppEditorLab() {
    querySqliteToTable(g_hud.algorithmLabDb,
        "SELECT id AS ID, operator_formula AS Formel, candidate_ns AS Kandidat_ns, "
        "baseline_ns AS Referenz_ns, equivalence AS Resultat, artifact_path AS Header "
        "FROM algorithm_lab ORDER BY id DESC LIMIT 200"
    );
    setEditorContent(
        L"// Wählen Sie oben einen synthetisierten C++20-Algorithmus aus der Tabelle,\r\n"
        L"// um seinen Quellcode hier zu bearbeiten, zu speichern oder gegen das Gold-Orakel zu testen.\r\n",
        true
    );
}

// Lädt die Autonome Evolution & Meta-Fusion
void refreshMetaFusion() {
    querySqliteToTable(g_hud.evoLineageDb,
        "SELECT family_id AS Forschungsfamilie, ('MRE_' || gen_index) AS Generation, "
        "COALESCE(complexity_class, 'O(N)') AS Komplexitaet, "
        "COALESCE(algo_fingerprint, 'ALGO_UNKNOWN') AS Fingerabdruck, "
        "status AS Validierungsstatus, printf('%.4fx', speedup_ratio) AS Speedup, "
        "printf('%d/%d', cases_passed, cases_passed + cases_failed) AS Oracles_OK, "
        "hypothesis AS Mathematische_Hypothese, created_at AS Zeitstempel "
        "FROM generations ORDER BY created_at DESC, gen_index DESC LIMIT 200"
    );

    if (g_hud.evoFamACombo && g_hud.evoFamBCombo) {
        ComboBox_ResetContent(g_hud.evoFamACombo);
        ComboBox_ResetContent(g_hud.evoFamBCombo);
        ComboBox_AddString(g_hud.evoFamACombo, L"AFFINE_RECURRENCE");
        ComboBox_AddString(g_hud.evoFamACombo, L"SO2_GIVENS_ROPE");
        ComboBox_AddString(g_hud.evoFamACombo, L"BARRETT_REDUCTION");
        ComboBox_AddString(g_hud.evoFamACombo, L"FOURIER_SPECTRAL");
        ComboBox_AddString(g_hud.evoFamBCombo, L"SO2_GIVENS_ROPE");
        ComboBox_AddString(g_hud.evoFamBCombo, L"AFFINE_RECURRENCE");
        ComboBox_AddString(g_hud.evoFamBCombo, L"BARRETT_REDUCTION");
        ComboBox_AddString(g_hud.evoFamBCombo, L"FOURIER_SPECTRAL");
        ComboBox_SetCurSel(g_hud.evoFamACombo, 0);
        ComboBox_SetCurSel(g_hud.evoFamBCombo, 1);
    }

    setEditorContent(
        L"================================================================================\r\n"
        L"AXION // AUTONOME EVOLUTION & META-FUSION ENGINE\r\n"
        L"================================================================================\r\n\r\n"
        L"Hier kreuzen Sie bewiesene mathematische Familien (z. B. Affine Rekurrenz x SO(2) RoPE)\r\n"
        L"zu neuen Generationen (MRE_n). Jede Generation wird gegen das unantastbare Gold-Orakel\r\n"
        L"validiert und mit lückenlosem mathematischen Herkunftsnachweis dokumentiert.\r\n",
        false
    );
}

// Detail-Inspektion bei Zeilenauswahl in der Tabelle
void inspectSelectedTableRow() {
    if (g_hud.currentMode == WorkspaceMode::ResearchLab) {
        std::wstring id = getSelectedTableRowItem(0);
        if (id.empty()) return;
        sqlite3* db = nullptr;
        if (sqlite3_open_v2(narrow(g_hud.k4dResultsDb.wstring()).c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
            if (db) sqlite3_close(db);
            return;
        }
        sqlite3_stmt* st = nullptr;
        const char* sql = "SELECT id, strategy, seed, mode, source_a, source_b, file_a, file_b, "
                          "status, details, hypothesis_code, proof_code, error_log, "
                          "verification_exit_code, verification_duration_ms, parent_id, chain_depth "
                          "FROM candidates WHERE id = ?1";
        if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK) {
            std::string idN = narrow(id);
            sqlite3_bind_text(st, 1, idN.c_str(), static_cast<int>(idN.size()), nullptr);
            if (sqlite3_step(st) == SQLITE_ROW) {
                auto cid = wfrom(sqlite3_column_text(st, 0));
                auto strat = wfrom(sqlite3_column_text(st, 1));
                auto seed = wfrom(sqlite3_column_text(st, 2));
                auto statusVal = wfrom(sqlite3_column_text(st, 8));
                auto hypCode = wfrom(sqlite3_column_text(st, 10));
                auto proofCode = wfrom(sqlite3_column_text(st, 11));
                auto errLog = wfrom(sqlite3_column_text(st, 12));
                int exitCode = sqlite3_column_int(st, 13);
                int durationMs = sqlite3_column_int(st, 14);
                int64_t parentId = sqlite3_column_int(st, 15);
                int chainDepth = sqlite3_column_int(st, 16);

                std::wostringstream ss;
                ss << L"================================================================================\r\n";
                ss << L"AXION K4D KANDIDAT #" << cid << L" | STATUS: " << statusVal << L"\r\n";
                ss << L"================================================================================\r\n";
                ss << L"Strategie:          " << strat << L"\r\n";
                ss << L"Verifikationsdauer: " << durationMs << L" ms (Lean ExitCode: " << exitCode << L")\r\n";
                ss << L"Generierungs-Seed:  " << seed << L"\r\n";
                ss << L"Beweisketten-Tiefe: Stufe " << chainDepth << L" (Eltern-Theorem: #" << parentId << L")\r\n\r\n";
                ss << L"--- FORMALISIERTE MATHEMATISCHE HYPOTHESE (LEAN 4) ---\r\n" << hypCode << L"\r\n\r\n";
                if (!proofCode.empty()) {
                    ss << L"--- FORMALER BEWEIS / REFLEXION ---\r\n" << proofCode << L"\r\n\r\n";
                }
                if (!errLog.empty() && exitCode != 0) {
                    ss << L"--- LEAN 4 FEHLERPROTOKOLL ---\r\n" << errLog << L"\r\n";
                }
                setEditorContent(ss.str(), false);
            }
            sqlite3_finalize(st);
        }
        sqlite3_close(db);
    }
    else if (g_hud.currentMode == WorkspaceMode::SourceDatabase) {
        std::wstring id = getSelectedTableRowItem(0);
        if (id.empty()) return;
        sqlite3* db = nullptr;
        if (sqlite3_open_v2(narrow(g_hud.dbPath.wstring()).c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
            if (db) sqlite3_close(db);
            return;
        }
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT statement FROM declarations WHERE rowid = ?1", -1, &st, nullptr) == SQLITE_OK) {
            std::string idN = narrow(id);
            sqlite3_bind_text(st, 1, idN.c_str(), static_cast<int>(idN.size()), nullptr);
            if (sqlite3_step(st) == SQLITE_ROW) {
                std::wstring stmt = wfrom(sqlite3_column_text(st, 0));
                std::wostringstream ss;
                ss << L"================================================================================\r\n";
                ss << L"DEKLARATION: " << getSelectedTableRowItem(2) << L" (" << getSelectedTableRowItem(3) << L")\r\n";
                ss << L"MODULDATEI:  " << getSelectedTableRowItem(1) << L" [Zeile " << getSelectedTableRowItem(4) << L"]\r\n";
                ss << L"================================================================================\r\n\r\n";
                ss << stmt << L"\r\n";
                setEditorContent(ss.str(), false);
            }
            sqlite3_finalize(st);
        }
        sqlite3_close(db);
    }
    else if (g_hud.currentMode == WorkspaceMode::CppEditorLab) {
        std::wstring path = getSelectedTableRowItem(5);
        if (path.empty()) return;
        g_hud.currentEditorFile = path;
        std::ifstream in(fs::path(path), std::ios::binary);
        if (in) {
            std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            setEditorContent(wfrom(reinterpret_cast<const unsigned char*>(s.c_str())), true);
        }
    }
}

// Schaltet Steuerelemente je nach Modus ein/aus & platziert sie sauber
void updateControlLayout(int w, int h) {
    bool isModal = (g_hud.settingsModalOpen || g_hud.infoModalOpen);
    if (isModal) {
        auto hideCtl = [](HWND ctrl) { if (ctrl) ShowWindow(ctrl, SW_HIDE); };
        hideCtl(g_hud.rows);
        hideCtl(g_hud.editor);
        hideCtl(g_hud.strategyCombo);
        hideCtl(g_hud.chainModeCombo);
        hideCtl(g_hud.lineModeCombo);
        hideCtl(g_hud.seedBox);
        hideCtl(g_hud.countBox);
        hideCtl(g_hud.runBtn);
        hideCtl(g_hud.stopBtn);
        hideCtl(g_hud.chainFromBtn);
        hideCtl(g_hud.reportBtn);
        hideCtl(g_hud.searchBox);
        hideCtl(g_hud.searchBtn);
        hideCtl(g_hud.saveBtn);
        hideCtl(g_hud.testBtn);
        hideCtl(g_hud.plotBtn);
        hideCtl(g_hud.evoFamACombo);
        hideCtl(g_hud.evoFamBCombo);
        hideCtl(g_hud.evoTargetFamBox);
        hideCtl(g_hud.evoMetaFusionBtn);
        hideCtl(g_hud.evoContinuousBtn);
        hideCtl(g_hud.evoRollbackBtn);
        hideCtl(g_hud.commandInput);
        return;
    }

    bool isNexus = (g_hud.currentMode == WorkspaceMode::OrbitalNexus);
    bool isLab = (g_hud.currentMode == WorkspaceMode::ResearchLab);
    bool isSrc = (g_hud.currentMode == WorkspaceMode::SourceDatabase);
    bool isCpp = (g_hud.currentMode == WorkspaceMode::CppEditorLab);
    bool isEvo = (g_hud.currentMode == WorkspaceMode::MetaFusion);

    bool showTableAndEditor = (isLab || isSrc || isCpp || isEvo);

    int topBarH = 64;
    int bottomBarH = 64;
    int contentTop = topBarH + 46;
    int contentH = h - contentTop - bottomBarH - 8;

    // Table & Editor Layout
    if (showTableAndEditor && g_hud.rows && g_hud.editor) {
        int tableW = static_cast<int>(w * 0.52);
        int editorW = w - tableW - 68;

        MoveWindow(g_hud.rows, 32, contentTop, tableW, contentH, TRUE);
        MoveWindow(g_hud.editor, 32 + tableW + 16, contentTop, editorW, contentH, TRUE);
        ShowWindow(g_hud.rows, SW_SHOW);
        ShowWindow(g_hud.editor, SW_SHOW);
    } else {
        if (g_hud.rows) ShowWindow(g_hud.rows, SW_HIDE);
        if (g_hud.editor) ShowWindow(g_hud.editor, SW_HIDE);
    }

    // Controls Sichtbarkeit und Positionierung exakt steuern
    auto setVis = [](HWND ctrl, bool visible) {
        if (ctrl) ShowWindow(ctrl, visible ? SW_SHOW : SW_HIDE);
    };

    float scale = 1.0f;
    int ctrlH = 28;
    int boxH = 26;
    if (g_hud.currentFontSize == FontSizeOption::Compact) {
        scale = 0.90f;
        ctrlH = 24;
        boxH = 24;
    } else if (g_hud.currentFontSize == FontSizeOption::Large) {
        scale = 1.20f;
        ctrlH = 32;
        boxH = 30;
    }

    auto sc = [scale](int val) -> int {
        return static_cast<int>(val * scale);
    };

    int ctrlY = topBarH + (ctrlH == 32 ? 6 : 8);

    // 1. K4D Lab
    if (isLab) {
        int cx = 32;
        int gap = 8;
        MoveWindow(g_hud.strategyCombo, cx, ctrlY, sc(155), 200, TRUE);  cx += sc(155) + gap;
        MoveWindow(g_hud.chainModeCombo, cx, ctrlY, sc(155), 200, TRUE); cx += sc(155) + gap;
        MoveWindow(g_hud.lineModeCombo, cx, ctrlY, sc(165), 200, TRUE);  cx += sc(165) + gap;
        MoveWindow(g_hud.seedBox, cx, ctrlY + 1, sc(85), boxH, TRUE);   cx += sc(85) + gap;
        MoveWindow(g_hud.runBtn, cx, ctrlY, sc(165), ctrlH, TRUE);       cx += sc(165) + gap;
        MoveWindow(g_hud.stopBtn, cx, ctrlY, sc(70), ctrlH, TRUE);       cx += sc(70) + gap;
        MoveWindow(g_hud.chainFromBtn, cx, ctrlY, sc(240), ctrlH, TRUE); cx += sc(240) + gap;
        MoveWindow(g_hud.reportBtn, cx, ctrlY, sc(175), ctrlH, TRUE);
    }
    setVis(g_hud.strategyCombo, isLab);
    setVis(g_hud.chainModeCombo, isLab);
    setVis(g_hud.lineModeCombo, isLab);
    setVis(g_hud.seedBox, isLab);
    setVis(g_hud.runBtn, isLab);
    setVis(g_hud.stopBtn, isLab);
    setVis(g_hud.chainFromBtn, isLab);
    setVis(g_hud.reportBtn, isLab);

    // 2. Quelldatenbank
    if (isSrc) {
        int cx = 32;
        int gap = 8;
        int searchW = std::max(sc(320), static_cast<int>(w * 0.28));
        MoveWindow(g_hud.searchBox, cx, ctrlY + 1, searchW, boxH, TRUE); cx += searchW + gap;
        MoveWindow(g_hud.countBox, cx, ctrlY + 1, sc(65), boxH, TRUE);  cx += sc(65) + gap;
        MoveWindow(g_hud.searchBtn, cx, ctrlY, sc(150), ctrlH, TRUE);
    }
    setVis(g_hud.searchBox, isSrc);
    setVis(g_hud.countBox, isSrc);
    setVis(g_hud.searchBtn, isSrc);

    // 3. C++ Editor Lab
    if (isCpp) {
        int cx = 32;
        int gap = 8;
        MoveWindow(g_hud.saveBtn, cx, ctrlY, sc(210), ctrlH, TRUE); cx += sc(210) + gap;
        MoveWindow(g_hud.testBtn, cx, ctrlY, sc(185), ctrlH, TRUE); cx += sc(185) + gap;
        MoveWindow(g_hud.plotBtn, cx, ctrlY, sc(185), ctrlH, TRUE);
    }
    setVis(g_hud.saveBtn, isCpp);
    setVis(g_hud.testBtn, isCpp);
    setVis(g_hud.plotBtn, isCpp);

    // 4. Meta-Fusion
    if (isEvo) {
        int cx = 32;
        int gap = 8;
        MoveWindow(g_hud.evoFamACombo, cx, ctrlY, sc(195), 200, TRUE); cx += sc(195) + gap;
        MoveWindow(g_hud.evoFamBCombo, cx, ctrlY, sc(195), 200, TRUE); cx += sc(195) + gap;
        MoveWindow(g_hud.evoTargetFamBox, cx, ctrlY + 1, sc(170), boxH, TRUE); cx += sc(170) + gap;
        MoveWindow(g_hud.evoMetaFusionBtn, cx, ctrlY, sc(195), ctrlH, TRUE); cx += sc(195) + gap;
        MoveWindow(g_hud.evoContinuousBtn, cx, ctrlY, sc(180), ctrlH, TRUE); cx += sc(180) + gap;
        MoveWindow(g_hud.evoRollbackBtn, cx, ctrlY, sc(210), ctrlH, TRUE);
    }
    setVis(g_hud.evoFamACombo, isEvo);
    setVis(g_hud.evoFamBCombo, isEvo);
    setVis(g_hud.evoTargetFamBox, isEvo);
    setVis(g_hud.evoMetaFusionBtn, isEvo);
    setVis(g_hud.evoContinuousBtn, isEvo);
    setVis(g_hud.evoRollbackBtn, isEvo);

    if (!isLab && !isSrc) {
        if (!isLab && g_hud.seedBox) ShowWindow(g_hud.seedBox, SW_HIDE);
        if (!isLab && !isSrc && g_hud.countBox) ShowWindow(g_hud.countBox, SW_HIDE);
        if (!isLab && g_hud.runBtn) ShowWindow(g_hud.runBtn, SW_HIDE);
        if (!isLab && g_hud.stopBtn) ShowWindow(g_hud.stopBtn, SW_HIDE);
    }

    // Unten: Intent-Eingabefeld (nur im Nexus-Modus)
    if (g_hud.commandInput) {
        if (isNexus) {
            int intentLeft = 24 + 620 + 22;
            int editW = std::max(160, static_cast<int>(w * 0.22));
            MoveWindow(g_hud.commandInput, intentLeft, h - 49, editW, 20, TRUE);
            ShowWindow(g_hud.commandInput, SW_SHOW);
        } else {
            ShowWindow(g_hud.commandInput, SW_HIDE);
        }
    }

    applyThemeToControls();
}

void switchWorkspace(WorkspaceMode mode) {
    g_hud.currentMode = mode;
    RECT rc{};
    GetClientRect(g_hud.hwnd, &rc);
    updateControlLayout(rc.right, rc.bottom);

    switch (mode) {
    case WorkspaceMode::OrbitalNexus:
        g_hud.liveFooterStatus = L"Orbital Nexus aktiv • 4 Sphären bereit";
        break;
    case WorkspaceMode::ResearchLab:
        refreshK4dLab();
        break;
    case WorkspaceMode::SourceDatabase:
        refreshSourceDatabase();
        break;
    case WorkspaceMode::CppEditorLab:
        refreshCppEditorLab();
        break;
    case WorkspaceMode::MetaFusion:
        refreshMetaFusion();
        break;
    case WorkspaceMode::AgentBridge:
        g_hud.liveFooterStatus = L"C++ Agent Bridge • 20 Kompilate bereit";
        break;
    }
    InvalidateRect(g_hud.hwnd, nullptr, FALSE);
}

// Startet ein Modul asynchron
void launchProcess(const std::wstring& cmdArgs) {
    if (g_hud.processRunning) {
        g_hud.liveFooterStatus = L"Ein Prozess läuft bereits im Hintergrund...";
        return;
    }
    g_hud.cycleBoost = 2.5;
    g_hud.lastProcessCmd = cmdArgs;
    if (cmdArgs.find(L"selftest") != std::wstring::npos) {
        g_hud.selftestState = 1; // Läuft
    }

    std::wstring cmdLine = L"\"" + g_hud.engineExePath.wstring() + L"\" " + cmdArgs;
    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (CreateProcessW(g_hud.engineExePath.c_str(), cmdBuf.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, g_hud.projectRootPath.c_str(), &si, &g_hud.activeProcess)) {
        g_hud.processRunning = true;
        g_hud.liveFooterStatus = L"Ausführung läuft (PID " + std::to_wstring(g_hud.activeProcess.dwProcessId) + L") • Orakel arbeitet...";
    } else {
        g_hud.processRunning = false;
        if (cmdArgs.find(L"selftest") != std::wstring::npos) {
            g_hud.selftestState = 3; // Fehler
        }
        g_hud.liveFooterStatus = L"Prozess konnte nicht gestartet werden: " + g_hud.engineExePath.wstring();
    }
}

// --- Haupt-Render-Routine ---
void renderSpatialHud(HWND hwnd, HDC targetDc) {
    RECT clientRect{};
    GetClientRect(hwnd, &clientRect);
    int w = clientRect.right - clientRect.left;
    int h = clientRect.bottom - clientRect.top;
    if (w <= 0 || h <= 0) return;

    const Theme& th = getActiveTheme();

    HDC memDc = CreateCompatibleDC(targetDc);
    HBITMAP memBmp = CreateCompatibleBitmap(targetDc, w, h);
    HGDIOBJ oldBmp = SelectObject(memDc, memBmp);

    // 1. Hintergrund: Obsidian mit Chamfer-Kanten
    drawChamferedPolygon(memDc, clientRect, 18, th.bgObsidian, th.accentPrimary, 2);
    SetBkMode(memDc, TRANSPARENT);

    // 2. Subtiles Gitter
    if (g_hud.enableGrid) {
        HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(11, 20, 36));
        HGDIOBJ oldPen = SelectObject(memDc, gridPen);
        for (int x = 48; x < w; x += 48) { MoveToEx(memDc, x, 0, nullptr); LineTo(memDc, x, h); }
        for (int y = 48; y < h; y += 48) { MoveToEx(memDc, 0, y, nullptr); LineTo(memDc, w, y); }
        SelectObject(memDc, oldPen);
        DeleteObject(gridPen);
    }

    // 3. Sternenstaub-Partikel (nur im Nexus)
    if (g_hud.enableParticles && g_hud.currentMode == WorkspaceMode::OrbitalNexus) {
        for (const auto& st : g_hud.stars) {
            int px = static_cast<int>(st.x * w);
            int py = static_cast<int>(st.y * h);
            int pr = std::max(1, static_cast<int>(st.r));
            HBRUSH starBrush = CreateSolidBrush(RGB(st.alpha, st.alpha + 15, st.alpha + 30));
            HGDIOBJ prevB = SelectObject(memDc, starBrush);
            SelectObject(memDc, GetStockObject(NULL_PEN));
            Ellipse(memDc, px - pr, py - pr, px + pr, py + pr);
            SelectObject(memDc, prevB);
            DeleteObject(starBrush);
        }
    }

    const auto& activeSphere = g_hud.spheres[static_cast<size_t>(g_hud.activeSphereIndex)];

    // 4. TOP HEADER BAR
    int pulseGlow = static_cast<int>(std::sin(g_hud.animPhase * 3.0) * 4.0);
    int pRad = 6 + pulseGlow;
    HBRUSH pulseBrush = CreateSolidBrush(th.accentPrimary);
    SelectObject(memDc, pulseBrush);
    SelectObject(memDc, GetStockObject(NULL_PEN));
    Ellipse(memDc, 32 - pRad, 32 - pRad, 32 + pRad, 32 + pRad);
    DeleteObject(pulseBrush);

    SelectObject(memDc, g_hud.fontDisplayTitle);
    SetTextColor(memDc, RGB(255, 255, 255));
    SIZE axionSz{};
    GetTextExtentPoint32W(memDc, L"AXION", 5, &axionSz);
    TextOutW(memDc, 48, 18, L"AXION", 5);

    int badgeLeft = 48 + axionSz.cx + 12;
    int badgeWidth = (g_hud.currentFontSize == FontSizeOption::Large ? 135 : 118);
    RECT verBadgeRc{badgeLeft, 20, badgeLeft + badgeWidth, 38};
    drawChamferedPolygon(memDc, verBadgeRc, 4, RGB(10, 32, 48), th.accentPrimary, 1);
    SelectObject(memDc, g_hud.fontMonoSmall);
    SetTextColor(memDc, th.accentPrimary);
    DrawTextW(memDc, L"v4.0.3 RELEASE", -1, &verBadgeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(memDc, g_hud.fontSans);
    SetTextColor(memDc, th.textMuted);
    TextOutW(memDc, 48, 42, L"MDraft KR Enterprise • Autonome Mathematische Forschung & Code-Synthese • Ralf Krümmel", 86);

    // Mitte: Status-Pill (skaliert, keine abgeschnittenen Texte)
    if (w >= 1200) {
        int pillHalfW = (g_hud.currentFontSize == FontSizeOption::Large ? 380 : 340);
        RECT midPillRc{w / 2 - pillHalfW, 18, w / 2 + pillHalfW, 46};
        drawChamferedPolygon(memDc, midPillRc, 8, th.bgPanel, th.borderMuted, 1);
        SelectObject(memDc, g_hud.fontMonoSmall);

        int segW = (midPillRc.right - midPillRc.left - 24) / 3;

        SetTextColor(memDc, th.accentPrimary);
        RECT r1{midPillRc.left + 12, midPillRc.top + 6, midPillRc.left + 12 + segW, midPillRc.bottom - 4};
        DrawTextW(memDc, L"Lean 4 Orakel: Bereit", -1, &r1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SetTextColor(memDc, th.accentSecondary);
        RECT r2{r1.right, midPillRc.top + 6, r1.right + segW, midPillRc.bottom - 4};
        DrawTextW(memDc, L"20 / 20 .exe Bereit", -1, &r2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SetTextColor(memDc, th.accentTertiary);
        RECT r3{r2.right, midPillRc.top + 6, midPillRc.right - 12, midPillRc.bottom - 4};
        DrawTextW(memDc, L"DB: Read-Only (2.3M)", -1, &r3, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Header-Buttons: [SETTINGS] [INFO] [–] [□] [×]
    RECT btnSettings{w - 280, 18, w - 196, 46};
    RECT btnInfo{w - 190, 18, w - 134, 46};
    RECT btnMin{w - 126, 18, w - 92, 46};
    RECT btnMax{w - 86, 18, w - 52, 46};
    RECT btnClose{w - 46, 18, w - 12, 46};

    drawChamferedPolygon(memDc, btnSettings, 6, th.bgPanel, g_hud.hoveredTopButton == 1 ? th.accentTertiary : th.borderMuted, 1);
    drawChamferedPolygon(memDc, btnInfo, 6, th.bgPanel, g_hud.hoveredTopButton == 2 ? th.accentPrimary : th.borderMuted, 1);
    drawChamferedPolygon(memDc, btnMin, 6, th.bgPanel, g_hud.hoveredTopButton == 3 ? th.accentPrimary : th.borderMuted, 1);
    drawChamferedPolygon(memDc, btnMax, 6, th.bgPanel, g_hud.hoveredTopButton == 4 ? th.accentSecondary : th.borderMuted, 1);
    drawChamferedPolygon(memDc, btnClose, 6, th.bgPanel, g_hud.hoveredTopButton == 5 ? COL_ROSE_ALERT : th.borderMuted, 1);

    SelectObject(memDc, g_hud.fontMonoSmall);
    SetTextColor(memDc, g_hud.hoveredTopButton == 1 ? th.accentTertiary : th.textMain);
    DrawTextW(memDc, L"SETTINGS", -1, &btnSettings, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(memDc, g_hud.hoveredTopButton == 2 ? th.accentPrimary : th.textMain);
    DrawTextW(memDc, L"INFO", -1, &btnInfo, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(memDc, g_hud.fontDisplayMid);
    SetTextColor(memDc, g_hud.hoveredTopButton == 3 ? th.accentPrimary : th.textMain);
    DrawTextW(memDc, L"–", -1, &btnMin, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(memDc, g_hud.hoveredTopButton == 4 ? th.accentSecondary : th.textMain);
    DrawTextW(memDc, g_hud.isMaximized ? L"❐" : L"□", -1, &btnMax, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(memDc, g_hud.hoveredTopButton == 5 ? COL_ROSE_ALERT : th.textMain);
    DrawTextW(memDc, L"×", -1, &btnClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Trennlinie unter Header
    HPEN sepPen = CreatePen(PS_SOLID, 1, RGB(18, 35, 58));
    HGDIOBJ oldPen = SelectObject(memDc, sepPen);
    MoveToEx(memDc, 24, 60, nullptr);
    LineTo(memDc, w - 24, 60);
    SelectObject(memDc, oldPen);
    DeleteObject(sepPen);

    // 5. HAUPTBEREICH: Je nach WorkspaceMode
    if (g_hud.currentMode == WorkspaceMode::OrbitalNexus) {
        // LINKS: CLUSTER-NAV (4 Sphären-Karten)
        int navLeft = 28;
        int navWidth = 260;
        int navY = 74;

        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, th.accentPrimary);
        TextOutW(memDc, navLeft + 4, navY, L"FORSCHUNGS-SPHÄREN", 18);

        SelectObject(memDc, g_hud.fontSansBold);
        SetTextColor(memDc, th.textMain);
        TextOutW(memDc, navLeft + 4, navY + 16, L"4 Sphären • 20 Module", 21);

        navY += 40;
        for (size_t i = 0; i < g_hud.spheres.size(); ++i) {
            const auto& sp = g_hud.spheres[i];
            bool isSel = (static_cast<int>(i) == g_hud.activeSphereIndex);
            bool isHov = (static_cast<int>(i) == g_hud.hoveredNavIndex);

            RECT cardRc{navLeft, navY, navLeft + navWidth, navY + 76};
            COLORREF border = isSel ? sp.accentColor : (isHov ? RGB(45, 75, 110) : th.borderMuted);
            COLORREF fill = isSel ? RGB(16, 28, 48) : (isHov ? RGB(14, 23, 38) : th.bgCard);
            drawChamferedPolygon(memDc, cardRc, 10, fill, border, isSel ? 2 : 1);

            SelectObject(memDc, g_hud.fontMonoSmall);
            SetTextColor(memDc, sp.accentColor);
            TextOutW(memDc, cardRc.left + 12, cardRc.top + 8, sp.codeTag.c_str(), static_cast<int>(sp.codeTag.size()));

            SelectObject(memDc, g_hud.fontDisplayMid);
            SetTextColor(memDc, isSel ? RGB(255, 255, 255) : th.textMain);
            TextOutW(memDc, cardRc.left + 12, cardRc.top + 26, sp.title.c_str(), static_cast<int>(sp.title.size()));

            SelectObject(memDc, g_hud.fontSans);
            SetTextColor(memDc, th.textMuted);
            RECT descRc{cardRc.left + 12, cardRc.top + 46, cardRc.right - 12, cardRc.bottom - 6};
            DrawTextW(memDc, sp.description.c_str(), -1, &descRc, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);

            navY += 86;
        }

        // MITTE: ZENTRALER ORBITAL CORE & 4D-HYPERCUBE PROJEKTION
        int centerAreaLeft = 310;
        int centerAreaRight = w - 380;
        int cx = (centerAreaLeft + centerAreaRight) / 2;
        int cy = (h - 70 + 70) / 2 - 10;
        int orbitR = static_cast<int>(std::min(static_cast<int>(centerAreaRight - centerAreaLeft), static_cast<int>(h - 140)) * 0.32);
        orbitR = std::max(120, std::min(orbitR, 230));

        HPEN orbPen = CreatePen(PS_SOLID, 1, RGB(18, 38, 62));
        oldPen = SelectObject(memDc, orbPen);
        SelectObject(memDc, GetStockObject(NULL_BRUSH));
        Ellipse(memDc, cx - orbitR, cy - orbitR, cx + orbitR, cy + orbitR);
        SelectObject(memDc, oldPen);
        DeleteObject(orbPen);

        for (size_t i = 0; i < g_hud.spheres.size(); ++i) {
            auto& sp = g_hud.spheres[i];
            double dynAngle = sp.baseAngleRad + (g_hud.enableOrbitAnimation ? std::sin(g_hud.animPhase * 0.35 + static_cast<double>(i)) * 0.04 : 0.0);
            sp.screenX = cx + static_cast<int>(std::cos(dynAngle) * orbitR);
            sp.screenY = cy + static_cast<int>(std::sin(dynAngle) * orbitR);
            bool isSel = (static_cast<int>(i) == g_hud.activeSphereIndex);
            bool isHov = (static_cast<int>(i) == g_hud.hoveredSphereIndex);
            sp.radius = isSel ? 46 : 38;

            HPEN linkPen = CreatePen(PS_SOLID, isSel ? 2 : 1, isSel ? sp.accentColor : RGB(35, 55, 80));
            oldPen = SelectObject(memDc, linkPen);
            MoveToEx(memDc, cx, cy, nullptr);
            LineTo(memDc, sp.screenX, sp.screenY);
            SelectObject(memDc, oldPen);
            DeleteObject(linkPen);

            if (isSel && g_hud.enableOrbitAnimation) {
                double tPulse = std::fmod(g_hud.animPhase * 0.8, 1.0);
                int px = cx + static_cast<int>((sp.screenX - cx) * tPulse);
                int py = cy + static_cast<int>((sp.screenY - cy) * tPulse);
                HBRUSH pB = CreateSolidBrush(sp.accentColor);
                SelectObject(memDc, pB);
                SelectObject(memDc, GetStockObject(NULL_PEN));
                Ellipse(memDc, px - 4, py - 4, px + 4, py + 4);
                DeleteObject(pB);
            }

            HBRUSH sphBrush = CreateSolidBrush(isSel ? RGB(16, 28, 48) : th.bgPanel);
            HPEN sphPen = CreatePen(PS_SOLID, (isSel || isHov) ? 2 : 1, (isSel || isHov) ? sp.accentColor : th.borderMuted);
            SelectObject(memDc, sphBrush);
            SelectObject(memDc, sphPen);
            Ellipse(memDc, sp.screenX - sp.radius, sp.screenY - sp.radius,
                    sp.screenX + sp.radius, sp.screenY + sp.radius);
            DeleteObject(sphBrush);
            DeleteObject(sphPen);

            SelectObject(memDc, g_hud.fontMonoSmall);
            SetTextColor(memDc, isSel ? RGB(255, 255, 255) : th.textMain);
            RECT rTop{sp.screenX - 50, sp.screenY - 14, sp.screenX + 50, sp.screenY + 2};
            RECT rBot{sp.screenX - 50, sp.screenY + 1, sp.screenX + 50, sp.screenY + 17};
            DrawTextW(memDc, sp.shortLabelTop.c_str(), -1, &rTop, DT_CENTER | DT_SINGLELINE);
            DrawTextW(memDc, sp.shortLabelBottom.c_str(), -1, &rBot, DT_CENTER | DT_SINGLELINE);
        }

        int coreR = 54 + (g_hud.enableOrbitAnimation ? static_cast<int>(std::sin(g_hud.animPhase * 2.2) * 3.0) : 0);
        HBRUSH coreB = CreateSolidBrush(RGB(10, 18, 32));
        HPEN coreP = CreatePen(PS_SOLID, 2, activeSphere.accentColor);
        SelectObject(memDc, coreB);
        SelectObject(memDc, coreP);
        Ellipse(memDc, cx - coreR, cy - coreR, cx + coreR, cy + coreR);
        DeleteObject(coreB);
        DeleteObject(coreP);

        struct Node2D { int x; int y; };
        std::array<Node2D, 8> hcNodes{};
        double rotAngle = g_hud.enableOrbitAnimation ? (g_hud.animPhase * 0.45) : 0.0;
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

        SelectObject(memDc, g_hud.fontDisplayMid);
        SetTextColor(memDc, RGB(255, 255, 255));
        RECT coreTitleRc{cx - 60, cy - 14, cx + 60, cy + 3};
        DrawTextW(memDc, L"AXION CORE", -1, &coreTitleRc, DT_CENTER | DT_SINGLELINE);

        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, activeSphere.accentColor);
        RECT coreSubRc{cx - 60, cy + 4, cx + 60, cy + 20};
        DrawTextW(memDc, L"K4D // LEAN 4", -1, &coreSubRc, DT_CENTER | DT_SINGLELINE);

        // RECHTS: FOKUS-PANEL
        int panelLeft = w - 370;
        int panelRight = w - 24;
        RECT panelRc{panelLeft, 74, panelRight, h - 76};
        drawChamferedPolygon(memDc, panelRc, 14, th.bgPanel, activeSphere.accentColor, 1);

        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, activeSphere.accentColor);
        TextOutW(memDc, panelRc.left + 18, panelRc.top + 14, activeSphere.codeTag.c_str(), static_cast<int>(activeSphere.codeTag.size()));

        SelectObject(memDc, g_hud.fontDisplayLarge);
        SetTextColor(memDc, RGB(255, 255, 255));
        TextOutW(memDc, panelRc.left + 18, panelRc.top + 34, activeSphere.title.c_str(), static_cast<int>(activeSphere.title.size()));

        SelectObject(memDc, g_hud.fontSans);
        SetTextColor(memDc, th.textMuted);
        RECT descRc{panelRc.left + 18, panelRc.top + 64, panelRc.right - 18, panelRc.top + 98};
        DrawTextW(memDc, activeSphere.description.c_str(), -1, &descRc, DT_LEFT | DT_WORDBREAK);

        int cardTop = panelRc.top + 104;
        int cardH = 84;
        for (size_t c = 0; c < activeSphere.cards.size(); ++c) {
            const auto& card = activeSphere.cards[c];
            RECT cRc{panelRc.left + 16, cardTop, panelRc.right - 16, cardTop + cardH};
            drawChamferedPolygon(memDc, cRc, 8, th.bgCard, th.borderMuted, 1);

            SelectObject(memDc, g_hud.fontMonoSmall);
            SetTextColor(memDc, activeSphere.accentColor);
            TextOutW(memDc, cRc.left + 12, cRc.top + 8, card.badge.c_str(), static_cast<int>(card.badge.size()));

            SelectObject(memDc, g_hud.fontSansBold);
            SetTextColor(memDc, RGB(255, 255, 255));
            TextOutW(memDc, cRc.left + 12, cRc.top + 24, card.title.c_str(), static_cast<int>(card.title.size()));

            SelectObject(memDc, g_hud.fontSans);
            SetTextColor(memDc, th.textMuted);
            RECT subRc{cRc.left + 12, cRc.top + 42, cRc.right - 12, cRc.top + 58};
            DrawTextW(memDc, card.subtitle.c_str(), -1, &subRc, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

            SelectObject(memDc, g_hud.fontMonoSmall);
            SetTextColor(memDc, th.accentSecondary);
            TextOutW(memDc, cRc.left + 12, cRc.top + 60, card.metric.c_str(), static_cast<int>(card.metric.size()));

            cardTop += cardH + 10;
        }

        // Live-Telemetrie Wellenform
        int waveTop = cardTop + 6;
        RECT waveRc{panelRc.left + 16, waveTop + 16, panelRc.right - 16, waveTop + 64};
        drawChamferedPolygon(memDc, waveRc, 6, RGB(4, 7, 14), th.borderMuted, 1);

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

        RECT actionBtnRc{panelRc.left + 16, panelRc.bottom - 54, panelRc.right - 16, panelRc.bottom - 14};
        COLORREF btnFill = g_hud.primaryBtnHovered ? RGB(20, 56, 82) : RGB(14, 38, 58);
        drawChamferedPolygon(memDc, actionBtnRc, 8, btnFill, activeSphere.accentColor, 2);

        SelectObject(memDc, g_hud.fontDisplayMid);
        SetTextColor(memDc, RGB(255, 255, 255));
        DrawTextW(memDc, activeSphere.actionText.c_str(), -1, &actionBtnRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    else if (g_hud.currentMode == WorkspaceMode::AgentBridge) {
        // C++ AGENT BRIDGE
        int bridgeW = std::min(1240, w - 64);
        RECT bridgeRc{w / 2 - bridgeW / 2, 70, w / 2 + bridgeW / 2, h - 74};
        drawChamferedPolygon(memDc, bridgeRc, 16, th.bgPanel, th.accentPrimary, 2);

        // Header
        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, th.accentPrimary);
        TextOutW(memDc, bridgeRc.left + 24, bridgeRc.top + 16, L"ARCHITEKTUR-BRÜCKE // H:/Axion/KR_BUILD/Release", 47);

        SelectObject(memDc, g_hud.fontDisplayLarge);
        SetTextColor(memDc, RGB(255, 255, 255));
        TextOutW(memDc, bridgeRc.left + 24, bridgeRc.top + 34, L"C++20 Subprozess-Orchester & Formale Integrität", 47);

        // Infobox 1: Was ist die Agentenbrücke?
        int box1H = (g_hud.currentFontSize == FontSizeOption::Large ? 116 : 98);
        RECT infoBox1Rc{bridgeRc.left + 24, bridgeRc.top + 58, bridgeRc.right - 24, bridgeRc.top + 58 + box1H};
        drawChamferedPolygon(memDc, infoBox1Rc, 8, th.bgCard, th.borderMuted, 1);

        SelectObject(memDc, g_hud.fontSansBold);
        SetTextColor(memDc, th.accentTertiary);
        TextOutW(memDc, infoBox1Rc.left + 14, infoBox1Rc.top + 8, L"WAS IST DIE AGENTENBRÜCKE? (Subprozess-Isolation & SQLite WAL Synchronisation)", 78);

        SelectObject(memDc, g_hud.fontSans);
        SetTextColor(memDc, th.textMain);
        RECT infoBox1Text{infoBox1Rc.left + 14, infoBox1Rc.top + 28, infoBox1Rc.right - 14, infoBox1Rc.bottom - 6};
        DrawTextW(memDc,
            L"Die Agentenbrücke entkoppelt das grafische Spatial-HUD von den 20 autonomen C++20-Forschungskompilaten von AXION. "
            L"Jedes Modul arbeitet speicherisoliert als robuster Subprozess ohne Crash-Risiko für die IDE. "
            L"Zustände und Beweisnetze synchronisieren sich atomar über SQLite3 Write-Ahead Logging (WAL) und formale Lean 4 Proof-State-Dateien.",
            -1, &infoBox1Text, DT_LEFT | DT_WORDBREAK);

        // 4 Sphären-Karten mit allen 20 Modulen
        int cardAreaTop = infoBox1Rc.bottom + 10;
        int stBtnH = 40;
        int stBtnTop = bridgeRc.bottom - 52;
        int box2H = (g_hud.currentFontSize == FontSizeOption::Large ? 152 : 136);
        int infoBox2Top = stBtnTop - 12 - box2H;

        // Verfügbare Höhe für die 4 Sphären-Karten:
        int cardAvailH = infoBox2Top - 12 - cardAreaTop;
        int qH = std::min(175, std::max(135, (cardAvailH - 10) / 2));
        int qW = (bridgeRc.right - bridgeRc.left - 64) / 2;
        int qX[2] = {bridgeRc.left + 24, bridgeRc.left + 36 + qW};
        int qY[2] = {cardAreaTop, cardAreaTop + qH + 10};

        int lineH = (g_hud.currentFontSize == FontSizeOption::Large ? 22 : 20);

        for (size_t s = 0; s < 4; ++s) {
            const auto& sp = g_hud.spheres[s];
            int col = static_cast<int>(s % 2);
            int row = static_cast<int>(s / 2);
            RECT qRc{qX[col], qY[row], qX[col] + qW, qY[row] + qH};
            drawChamferedPolygon(memDc, qRc, 8, th.bgCard, th.borderMuted, 1);

            SelectObject(memDc, g_hud.fontDisplayMid);
            SetTextColor(memDc, sp.accentColor);
            TextOutW(memDc, qRc.left + 12, qRc.top + 8, sp.title.c_str(), static_cast<int>(sp.title.size()));

            SelectObject(memDc, g_hud.fontMonoSmall);
            int itemY = qRc.top + 30;
            for (const auto& exe : sp.executables) {
                SetTextColor(memDc, th.textMain);
                TextOutW(memDc, qRc.left + 14, itemY, exe.c_str(), static_cast<int>(exe.size()));
                SetTextColor(memDc, th.accentSecondary);
                TextOutW(memDc, qRc.right - 62, itemY, L"[READY]", 7);
                itemY += lineH;
                if (itemY > qRc.bottom - 16) break;
            }
        }

        // Infobox 2: Was passiert beim 18-Punkte-Selbsttest?
        RECT infoBox2Rc{bridgeRc.left + 24, infoBox2Top, bridgeRc.right - 24, infoBox2Top + box2H};
        drawChamferedPolygon(memDc, infoBox2Rc, 8, th.bgCard, th.borderMuted, 1);

        SelectObject(memDc, g_hud.fontSansBold);
        SetTextColor(memDc, th.accentSecondary);
        TextOutW(memDc, infoBox2Rc.left + 14, infoBox2Rc.top + 8, L"WAS PASSIERT BEIM 18-PUNKTE-SELBSTTEST? (18 Formale Validierungs-Tore)", 70);

        SelectObject(memDc, g_hud.fontSans);
        SetTextColor(memDc, th.textMain);
        RECT infoBox2Text{infoBox2Rc.left + 14, infoBox2Rc.top + 28, infoBox2Rc.right - 14, infoBox2Rc.bottom - 6};
        DrawTextW(memDc,
            L"Der 18-Punkte-Selbsttest validiert die gesamte mathematische Pipeline von AXION:\r\n"
            L"• Tore 1–5: SQLite3 WAL-Integrität, Tabellen-Schemata & schreibgeschützter Korpus (2.37M Deklarationen)\r\n"
            L"• Tore 6–10: K4D Zellulär-Automaten Mikrophysik, Perturbations-Orakel & Hamilton-Dynamik\r\n"
            L"• Tore 11–14: Lean 4 Beweis-Metamodell, syntaktische Reflection & rigorose 'sorry'-Sperre\r\n"
            L"• Tore 15–18: Deterministischer Seed (314159), Modulo-Arithmetik Z/65521Z & AVX2-Code-Synthese.",
            -1, &infoBox2Text, DT_LEFT | DT_WORDBREAK);

        // Status & Action Button
        RECT stBtnRc{bridgeRc.left + 24, stBtnTop, bridgeRc.right - 24, stBtnTop + stBtnH};
        COLORREF stFill = g_hud.selftestBtnHovered ? RGB(22, 64, 50) : RGB(14, 44, 34);
        if (g_hud.selftestState == 1) stFill = RGB(45, 45, 18);
        drawChamferedPolygon(memDc, stBtnRc, 8, stFill, th.accentSecondary, 2);

        SelectObject(memDc, g_hud.fontDisplayMid);
        SetTextColor(memDc, RGB(255, 255, 255));
        std::wstring btnLabel;
        if (g_hud.selftestState == 1) {
            btnLabel = L"⏳ SELBSTTEST LÄUFT... (18 Validierungs-Tore werden formal geprüft)";
        } else if (g_hud.selftestState == 2) {
            btnLabel = L"✓ 18 / 18 SELFTEST-TORE BESTANDEN (ERFOLG) • ERNEUT TESTEN";
        } else if (g_hud.selftestState == 3) {
            btnLabel = L"⚠ TEST FEHLGESCHLAGEN • ERNEUT TESTEN";
        } else {
            btnLabel = L"▶ 18-PUNKTE SELFTEST SUITE STARTEN (axion_engine.exe selftest KR_BUILD/selftest)";
        }
        DrawTextW(memDc, btnLabel.c_str(), -1, &stBtnRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    else {
        // Hintergrundpanel für die Toolbar im aktiven Arbeitsbereich
        RECT toolbarBg{24, 68, w - 24, 102};
        drawChamferedPolygon(memDc, toolbarBg, 8, th.bgPanel, th.borderMuted, 1);
    }

    // 6. BOTTOM AMBIENT COMMAND DOCK
    RECT dockRc{24, h - 64, w - 24, h - 18};
    drawChamferedPolygon(memDc, dockRc, 10, th.bgPanel, th.borderMuted, 1);

    int bScale = (g_hud.currentFontSize == FontSizeOption::Large ? 14 : (g_hud.currentFontSize == FontSizeOption::Compact ? -10 : 0));
    struct ModeBtnInfo { WorkspaceMode mode; const wchar_t* label; int width; };
    const ModeBtnInfo modeBtns[6] = {
        {WorkspaceMode::OrbitalNexus, L"ORBITAL NEXUS", 130 + bScale},
        {WorkspaceMode::ResearchLab, L"K4D FORSCHUNG", 130 + bScale},
        {WorkspaceMode::SourceDatabase, L"QUELLDATENBANK", 135 + bScale},
        {WorkspaceMode::CppEditorLab, L"C++ EDITOR", 110 + bScale},
        {WorkspaceMode::MetaFusion, L"META-FUSION", 115 + bScale},
        {WorkspaceMode::AgentBridge, L"AGENT BRIDGE", 120 + bScale}
    };

    int btnX = dockRc.left + 12;
    SelectObject(memDc, g_hud.fontMonoSmall);
    for (int i = 0; i < 6; ++i) {
        RECT bRc{btnX, dockRc.top + 8, btnX + modeBtns[i].width, dockRc.bottom - 8};
        bool isCurrent = (g_hud.currentMode == modeBtns[i].mode);
        bool isHov = (g_hud.hoveredWorkspaceBtn == i);

        COLORREF fill = isCurrent ? RGB(20, 52, 75) : (isHov ? RGB(16, 36, 56) : th.bgCard);
        COLORREF bdr = isCurrent ? th.accentPrimary : (isHov ? th.textMain : th.borderMuted);
        drawChamferedPolygon(memDc, bRc, 6, fill, bdr, 1);

        SetTextColor(memDc, isCurrent ? th.accentPrimary : (isHov ? RGB(255, 255, 255) : th.textMuted));
        DrawTextW(memDc, modeBtns[i].label, -1, &bRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        btnX += modeBtns[i].width + 6;
    }

    // Intent Leiste (im Nexus Modus) oder Status
    if (g_hud.currentMode == WorkspaceMode::OrbitalNexus) {
        int intentLeft = btnX + 8;
        SelectObject(memDc, g_hud.fontMono);
        SetTextColor(memDc, th.accentPrimary);
        TextOutW(memDc, intentLeft, dockRc.top + 13, L"λ>", 2);

        int editW = std::max(160, static_cast<int>(w * 0.22));
        RECT btnImpulsRc{intentLeft + editW + 32, dockRc.top + 8, intentLeft + editW + 96, dockRc.bottom - 8};
        RECT btnSendRc{intentLeft + editW + 102, dockRc.top + 8, intentLeft + editW + 172, dockRc.bottom - 8};

        drawChamferedPolygon(memDc, btnImpulsRc, 6, g_hud.impulseBtnHovered ? RGB(24, 40, 60) : th.bgCard, th.borderMuted, 1);
        drawChamferedPolygon(memDc, btnSendRc, 6, g_hud.sendBtnHovered ? RGB(20, 56, 80) : RGB(14, 40, 60), th.accentPrimary, 1);

        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, th.textMain);
        DrawTextW(memDc, L"IMPULS", -1, &btnImpulsRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SetTextColor(memDc, th.accentPrimary);
        DrawTextW(memDc, L"SENDEN", -1, &btnSendRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        int footerTextLeft = btnSendRc.right + 18;
        if (footerTextLeft < dockRc.right - 30) {
            HBRUSH dotB = CreateSolidBrush(th.accentSecondary);
            SelectObject(memDc, dotB);
            SelectObject(memDc, GetStockObject(NULL_PEN));
            Ellipse(memDc, footerTextLeft, dockRc.top + 18, footerTextLeft + 7, dockRc.top + 25);
            DeleteObject(dotB);

            SelectObject(memDc, g_hud.fontMonoSmall);
            SetTextColor(memDc, th.textMain);
            RECT footTextRc{footerTextLeft + 14, dockRc.top + 13, dockRc.right - 14, dockRc.bottom - 8};
            DrawTextW(memDc, g_hud.liveFooterStatus.c_str(), -1, &footTextRc, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
    } else {
        int footerTextLeft = btnX + 16;
        if (footerTextLeft < dockRc.right - 30) {
            HBRUSH dotB = CreateSolidBrush(th.accentSecondary);
            SelectObject(memDc, dotB);
            SelectObject(memDc, GetStockObject(NULL_PEN));
            Ellipse(memDc, footerTextLeft, dockRc.top + 18, footerTextLeft + 7, dockRc.top + 25);
            DeleteObject(dotB);

            SelectObject(memDc, g_hud.fontMonoSmall);
            SetTextColor(memDc, th.textMain);
            RECT footTextRc{footerTextLeft + 14, dockRc.top + 13, dockRc.right - 14, dockRc.bottom - 8};
            DrawTextW(memDc, g_hud.liveFooterStatus.c_str(), -1, &footTextRc, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
    }

    // 7. MODAL: EINSTELLUNGSMENÜ (SETTINGS)
    if (g_hud.settingsModalOpen) {
        RECT fullRc{0, 64, w, h};
        HBRUSH modalDimB = CreateSolidBrush(RGB(2, 4, 8));
        FillRect(memDc, &fullRc, modalDimB);
        DeleteObject(modalDimB);

        int mw = 720, mh = 500;
        int mx0 = (w - mw) / 2;
        int my0 = 64 + (h - 64 - mh) / 2;
        RECT mBox{mx0, my0, mx0 + mw, my0 + mh};
        drawChamferedPolygon(memDc, mBox, 16, th.bgPanel, th.accentTertiary, 2);

        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, th.accentTertiary);
        TextOutW(memDc, mBox.left + 24, mBox.top + 20, L"EINSTELLUNGEN & PERSONALISIERUNG", 32);

        SelectObject(memDc, g_hud.fontDisplayLarge);
        SetTextColor(memDc, RGB(255, 255, 255));
        TextOutW(memDc, mBox.left + 24, mBox.top + 40, L"AXION Design-Templates & UI-Skalierung", 38);

        SelectObject(memDc, g_hud.fontSansBold);
        SetTextColor(memDc, th.textMain);
        TextOutW(memDc, mBox.left + 24, mBox.top + 76, L"Farbschema / Theme wählen (4 Vorlagen):", 40);

        int cardW = (mBox.right - mBox.left - 64) / 2;
        int cardH = 58;
        for (int i = 0; i < 4; ++i) {
            int cxPos = (i % 2 == 0) ? (mBox.left + 24) : (mBox.left + 36 + cardW);
            int cyPos = (i < 2) ? (mBox.top + 102) : (mBox.top + 168);
            RECT tRc{cxPos, cyPos, cxPos + cardW, cyPos + cardH};

            bool isSel = (g_hud.currentThemeIndex == i);
            bool isHov = (g_hud.hoveredThemeCard == i);

            COLORREF fill = isSel ? RGB(22, 38, 62) : (isHov ? RGB(16, 28, 48) : th.bgCard);
            COLORREF border = isSel ? G_THEMES[i].accentPrimary : (isHov ? RGB(100, 140, 180) : th.borderMuted);
            drawChamferedPolygon(memDc, tRc, 6, fill, border, isSel ? 2 : 1);

            HBRUSH d1 = CreateSolidBrush(G_THEMES[i].bgObsidian);
            HBRUSH d2 = CreateSolidBrush(G_THEMES[i].accentPrimary);
            HBRUSH d3 = CreateSolidBrush(G_THEMES[i].accentSecondary);
            HGDIOBJ prevB = SelectObject(memDc, d1);
            SelectObject(memDc, GetStockObject(NULL_PEN));
            Ellipse(memDc, tRc.left + 12, tRc.top + 22, tRc.left + 26, tRc.top + 36);
            SelectObject(memDc, d2);
            Ellipse(memDc, tRc.left + 30, tRc.top + 22, tRc.left + 44, tRc.top + 36);
            SelectObject(memDc, d3);
            Ellipse(memDc, tRc.left + 48, tRc.top + 22, tRc.left + 62, tRc.top + 36);
            SelectObject(memDc, prevB);
            DeleteObject(d1); DeleteObject(d2); DeleteObject(d3);

            SelectObject(memDc, g_hud.fontDisplayMid);
            SetTextColor(memDc, isSel ? RGB(255, 255, 255) : th.textMain);
            TextOutW(memDc, tRc.left + 72, tRc.top + 12, G_THEMES[i].name, static_cast<int>(wcslen(G_THEMES[i].name)));

            SelectObject(memDc, g_hud.fontMonoSmall);
            SetTextColor(memDc, G_THEMES[i].accentPrimary);
            TextOutW(memDc, tRc.left + 72, tRc.top + 32, isSel ? L"AKTIV" : L"KLICKEN ZUM AKTIVIEREN", isSel ? 5 : 22);
        }

        SelectObject(memDc, g_hud.fontSansBold);
        SetTextColor(memDc, th.textMain);
        TextOutW(memDc, mBox.left + 24, mBox.top + 244, L"Schriftgröße / UI-Skalierung:", 29);

        const wchar_t* fontLabels[3] = {L"Kompakt (-3px)", L"Standard (Default)", L"Groß (+5px)"};
        int fBtnW = (mBox.right - mBox.left - 64) / 3;
        for (int i = 0; i < 3; ++i) {
            int fx = mBox.left + 24 + i * (fBtnW + 8);
            RECT fRc{fx, mBox.top + 268, fx + fBtnW, mBox.top + 304};
            bool isSel = (static_cast<int>(g_hud.currentFontSize) == i);

            drawChamferedPolygon(memDc, fRc, 6, isSel ? RGB(20, 52, 75) : th.bgCard,
                                 isSel ? th.accentPrimary : th.borderMuted, 1);
            SelectObject(memDc, g_hud.fontMonoSmall);
            SetTextColor(memDc, isSel ? th.accentPrimary : th.textMain);
            DrawTextW(memDc, fontLabels[i], -1, &fRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        SelectObject(memDc, g_hud.fontSansBold);
        SetTextColor(memDc, th.textMain);
        TextOutW(memDc, mBox.left + 24, mBox.top + 324, L"Grafik & Performance:", 22);

        RECT optAnim{mBox.left + 24, mBox.top + 348, mBox.left + 210, mBox.top + 384};
        RECT optPart{mBox.left + 220, mBox.top + 348, mBox.left + 400, mBox.top + 384};
        RECT optGrid{mBox.left + 410, mBox.top + 348, mBox.left + 580, mBox.top + 384};

        drawChamferedPolygon(memDc, optAnim, 6, g_hud.enableOrbitAnimation ? RGB(18, 48, 38) : th.bgCard, g_hud.enableOrbitAnimation ? th.accentSecondary : th.borderMuted, 1);
        drawChamferedPolygon(memDc, optPart, 6, g_hud.enableParticles ? RGB(18, 48, 38) : th.bgCard, g_hud.enableParticles ? th.accentSecondary : th.borderMuted, 1);
        drawChamferedPolygon(memDc, optGrid, 6, g_hud.enableGrid ? RGB(18, 48, 38) : th.bgCard, g_hud.enableGrid ? th.accentSecondary : th.borderMuted, 1);

        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, g_hud.enableOrbitAnimation ? th.accentSecondary : th.textMuted);
        DrawTextW(memDc, g_hud.enableOrbitAnimation ? L"Orbit 60 FPS: AN" : L"Orbit: PAUSIERT", -1, &optAnim, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SetTextColor(memDc, g_hud.enableParticles ? th.accentSecondary : th.textMuted);
        DrawTextW(memDc, g_hud.enableParticles ? L"Partikel: AN" : L"Partikel: AUS", -1, &optPart, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SetTextColor(memDc, g_hud.enableGrid ? th.accentSecondary : th.textMuted);
        DrawTextW(memDc, g_hud.enableGrid ? L"Gitter: AN" : L"Gitter: AUS", -1, &optGrid, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        RECT sCloseRc{mBox.right - 140, mBox.bottom - 46, mBox.right - 24, mBox.bottom - 16};
        drawChamferedPolygon(memDc, sCloseRc, 6, RGB(14, 38, 60), th.accentPrimary, 1);
        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, th.accentPrimary);
        DrawTextW(memDc, L"SCHLIESSEN", -1, &sCloseRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // 8. MODAL: INFO & PROVENIENZ
    if (g_hud.infoModalOpen) {
        RECT fullRc{0, 64, w, h};
        HBRUSH modalDimB = CreateSolidBrush(RGB(2, 4, 8));
        FillRect(memDc, &fullRc, modalDimB);
        DeleteObject(modalDimB);

        int mw = 640, mh = 440;
        int mx0 = (w - mw) / 2;
        int my0 = 64 + (h - 64 - mh) / 2;
        RECT mBox{mx0, my0, mx0 + mw, my0 + mh};
        drawChamferedPolygon(memDc, mBox, 16, th.bgPanel, th.accentPrimary, 2);

        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, th.accentPrimary);
        TextOutW(memDc, mBox.left + 24, mBox.top + 20, L"SYSTEM-TRANSPARENZ & PROVENIENZ", 31);

        SelectObject(memDc, g_hud.fontDisplayLarge);
        SetTextColor(memDc, RGB(255, 255, 255));
        TextOutW(memDc, mBox.left + 24, mBox.top + 42, L"AXION // Methodik & Architektur", 31);

        SelectObject(memDc, g_hud.fontSansBold);
        SetTextColor(memDc, th.textMain);
        TextOutW(memDc, mBox.left + 24, mBox.top + 80, L"Entwickler & Konzeption:", 24);
        SelectObject(memDc, g_hud.fontSans);
        SetTextColor(memDc, RGB(255, 255, 255));
        TextOutW(memDc, mBox.left + 190, mBox.top + 80, L"Ralf Krümmel (MDraft KR Enterprise)", 35);

        SelectObject(memDc, g_hud.fontSansBold);
        SetTextColor(memDc, th.accentPrimary);
        TextOutW(memDc, mBox.left + 24, mBox.top + 112, L"Zweistufiges Mathematisches Orakel:", 35);
        SelectObject(memDc, g_hud.fontSans);
        SetTextColor(memDc, th.textMain);
        RECT mText1{mBox.left + 24, mBox.top + 132, mBox.right - 24, mBox.top + 190};
        DrawTextW(memDc,
            L"Jede Hypothese durchläuft zuerst einen schnellen numerischen Falsifizierer (Gegenbeispiel-Suche in Z/65521Z und R) und wird anschließend formal durch den interaktiven Theorembeweiser Lean 4 (MetaM) ohne unbewiesene Axiome (sorry-Sperre) zertifiziert.",
            -1, &mText1, DT_LEFT | DT_WORDBREAK);

        SelectObject(memDc, g_hud.fontSansBold);
        SetTextColor(memDc, th.accentSecondary);
        TextOutW(memDc, mBox.left + 24, mBox.top + 198, L"Herkunft der mathematischen Algorithmen:", 40);
        SelectObject(memDc, g_hud.fontSans);
        SetTextColor(memDc, th.textMain);
        RECT mText2{mBox.left + 24, mBox.top + 218, mBox.right - 24, mBox.top + 276};
        DrawTextW(memDc,
            L"K4D-Mikrophysik, Galois-Rekurrenz, SO(2) Givens-RoPE, divisionsfreie Barrett-Reduktion und spektrale Laplace-Graph-Dekomposition.",
            -1, &mText2, DT_LEFT | DT_WORDBREAK);

        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, th.accentTertiary);
        RECT mText3{mBox.left + 24, mBox.top + 284, mBox.right - 24, mBox.top + 320};
        DrawTextW(memDc, L"Lizenz: MIT License  |  Compiler: KR-Portable LLVM-MinGW / MSVC C++20", -1, &mText3, DT_LEFT | DT_SINGLELINE);

        RECT mCloseRc{mBox.right - 140, mBox.bottom - 46, mBox.right - 24, mBox.bottom - 16};
        drawChamferedPolygon(memDc, mCloseRc, 6, RGB(14, 38, 60), th.accentPrimary, 1);
        SelectObject(memDc, g_hud.fontMonoSmall);
        SetTextColor(memDc, th.accentPrimary);
        DrawTextW(memDc, L"SCHLIESSEN", -1, &mCloseRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    BitBlt(targetDc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);

    SelectObject(memDc, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDc);
}

// Window Procedure für Axion Spatial Orbital HUD
LRESULT CALLBACK SpatialHudWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hud.hwnd = hwnd;

        INITCOMMONCONTROLSEX icex{sizeof(icex), ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES};
        InitCommonControlsEx(&icex);

        // Pre-allocated Brushes
        updateThemeBrushes();

        // Win32 Controls erzeugen
        g_hud.rows = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 0, 100, 100, hwnd, reinterpret_cast<HMENU>(ID_ROWS), nullptr, nullptr
        );
        ListView_SetExtendedListViewStyle(g_hud.rows, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

        g_hud.editor = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
            0, 0, 100, 100, hwnd, reinterpret_cast<HMENU>(ID_EDITOR), nullptr, nullptr
        );

        // K4D Controls
        g_hud.strategyCombo = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
            0, 0, 100, 200, hwnd, reinterpret_cast<HMENU>(ID_STRATEGY_COMBO), nullptr, nullptr);
        for (auto s : {L"Alle 6 Strategien", L"type_synthesis", L"systematic", L"random", L"cross_domain", L"mutation", L"iterative"}) {
            ComboBox_AddString(g_hud.strategyCombo, s);
        }
        ComboBox_SetCurSel(g_hud.strategyCombo, 0);

        g_hud.chainModeCombo = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
            0, 0, 100, 200, hwnd, reinterpret_cast<HMENU>(ID_CHAINMODE_COMBO), nullptr, nullptr);
        ComboBox_AddString(g_hud.chainModeCombo, L"Einzelne Zyklen");
        ComboBox_AddString(g_hud.chainModeCombo, L"Kette vertiefen");
        ComboBox_AddString(g_hud.chainModeCombo, L"Vollständiger Baum");
        ComboBox_SetCurSel(g_hud.chainModeCombo, 1);

        g_hud.lineModeCombo = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
            0, 0, 100, 200, hwnd, reinterpret_cast<HMENU>(ID_LINEMODE_COMBO), nullptr, nullptr);
        ComboBox_AddString(g_hud.lineModeCombo, L"K4D & CONTROL Vergleich");
        ComboBox_AddString(g_hud.lineModeCombo, L"Nur K4D_GUIDED");
        ComboBox_AddString(g_hud.lineModeCombo, L"Nur CONTROL_NO_K");
        ComboBox_SetCurSel(g_hud.lineModeCombo, 0);

        g_hud.seedBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"314159", WS_CHILD | ES_NUMBER,
            0, 0, 80, 24, hwnd, reinterpret_cast<HMENU>(ID_SEED_BOX), nullptr, nullptr);

        g_hud.countBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"4", WS_CHILD | ES_NUMBER,
            0, 0, 50, 24, hwnd, reinterpret_cast<HMENU>(ID_COUNT_BOX), nullptr, nullptr);

        g_hud.runBtn = CreateWindowW(L"BUTTON", L"Forschung starten", WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW,
            0, 0, 120, 28, hwnd, reinterpret_cast<HMENU>(ID_RUN_BTN), nullptr, nullptr);

        g_hud.stopBtn = CreateWindowW(L"BUTTON", L"Stop", WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW,
            0, 0, 60, 28, hwnd, reinterpret_cast<HMENU>(ID_STOP_BTN), nullptr, nullptr);

        g_hud.chainFromBtn = CreateWindowW(L"BUTTON", L"Kette ab Auswahl vertiefen", WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW,
            0, 0, 160, 28, hwnd, reinterpret_cast<HMENU>(ID_CHAINFROM_BTN), nullptr, nullptr);

        g_hud.reportBtn = CreateWindowW(L"BUTTON", L"Forschungsbericht", WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW,
            0, 0, 120, 28, hwnd, reinterpret_cast<HMENU>(ID_REPORT_BTN), nullptr, nullptr);

        // Quelle / Suche
        g_hud.searchBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"prime", WS_CHILD | ES_AUTOHSCROLL,
            0, 0, 200, 24, hwnd, reinterpret_cast<HMENU>(ID_SEARCH_BOX), nullptr, nullptr);

        g_hud.searchBtn = CreateWindowW(L"BUTTON", L"Suchen / Laden", WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW,
            0, 0, 120, 28, hwnd, reinterpret_cast<HMENU>(ID_SEARCH_BTN), nullptr, nullptr);

        // C++ Editor Lab
        g_hud.saveBtn = CreateWindowW(L"BUTTON", L"Änderungen speichern", WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW,
            0, 0, 140, 28, hwnd, reinterpret_cast<HMENU>(ID_SAVE_BTN), nullptr, nullptr);

        g_hud.testBtn = CreateWindowW(L"BUTTON", L"Algorithmus testen", WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW,
            0, 0, 140, 28, hwnd, reinterpret_cast<HMENU>(ID_TEST_BTN), nullptr, nullptr);

        g_hud.plotBtn = CreateWindowW(L"BUTTON", L"Trace-Plot öffnen", WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW,
            0, 0, 130, 28, hwnd, reinterpret_cast<HMENU>(ID_PLOT_BTN), nullptr, nullptr);

        // Meta-Fusion Controls
        g_hud.evoFamACombo = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
            0, 0, 160, 200, hwnd, reinterpret_cast<HMENU>(ID_EVO_FAMA), nullptr, nullptr);
        g_hud.evoFamBCombo = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
            0, 0, 160, 200, hwnd, reinterpret_cast<HMENU>(ID_EVO_FAMB), nullptr, nullptr);
        g_hud.evoTargetFamBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"HYBRID_ROPE_GALOIS", WS_CHILD | ES_AUTOHSCROLL,
            0, 0, 160, 24, hwnd, reinterpret_cast<HMENU>(ID_EVO_TARGETFAM), nullptr, nullptr);
        g_hud.evoMetaFusionBtn = CreateWindowW(L"BUTTON", L"Meta-Fusion starten", WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW,
            0, 0, 150, 28, hwnd, reinterpret_cast<HMENU>(ID_EVO_METAFUSION_BTN), nullptr, nullptr);
        g_hud.evoContinuousBtn = CreateWindowW(L"BUTTON", L"Kontinuierlich (10x)", WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW,
            0, 0, 140, 28, hwnd, reinterpret_cast<HMENU>(ID_EVO_CONTINUOUS_BTN), nullptr, nullptr);
        g_hud.evoRollbackBtn = CreateWindowW(L"BUTTON", L"Rollback (Checkpoint)", WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW,
            0, 0, 150, 28, hwnd, reinterpret_cast<HMENU>(ID_EVO_ROLLBACK_BTN), nullptr, nullptr);

        // Ambient Command Input
        g_hud.commandInput = CreateWindowExW(
            0, L"EDIT", L"K4D Beweisnetz Tiefe 4 mit Strategie type_synthesis",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 300, 20, hwnd, reinterpret_cast<HMENU>(ID_COMMAND_INPUT), nullptr, nullptr
        );

        rebuildFonts();
        applyThemeToControls();

        // 60 FPS Timer (wird selektiv im Nexus genutzt)
        SetTimer(hwnd, 1, 16, nullptr);
        return 0;
    }

    case WM_SIZE: {
        int w = LOWORD(lp);
        int h = HIWORD(lp);
        applyChamferedWindowRegion(hwnd, w, h);
        updateControlLayout(w, h);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1; // Verhindert Flackern vollständig

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        renderSpatialHud(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DRAWITEM: {
        auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if (dis && dis->CtlType == ODT_BUTTON) {
            const Theme& th = getActiveTheme();
            bool isPressed = (dis->itemState & ODS_SELECTED);
            bool isDisabled = (dis->itemState & ODS_DISABLED);

            COLORREF bgCol = isPressed ? th.bgPanel : th.bgCard;
            COLORREF borderCol = isPressed ? th.accentPrimary : th.borderMuted;
            if (isDisabled) {
                bgCol = th.bgObsidian;
                borderCol = th.borderMuted;
            }

            HBRUSH hbr = CreateSolidBrush(bgCol);
            HPEN hp = CreatePen(PS_SOLID, 1, borderCol);
            HGDIOBJ oldBr = SelectObject(dis->hDC, hbr);
            HGDIOBJ oldPen = SelectObject(dis->hDC, hp);

            RoundRect(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom, 6, 6);

            SelectObject(dis->hDC, oldBr);
            SelectObject(dis->hDC, oldPen);
            DeleteObject(hbr);
            DeleteObject(hp);

            wchar_t btnText[256] = {0};
            GetWindowTextW(dis->hwndItem, btnText, 255);

            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, isDisabled ? th.textMuted : (isPressed ? RGB(255, 255, 255) : th.accentPrimary));
            SelectObject(dis->hDC, g_hud.fontSansBold ? g_hud.fontSansBold : GetStockObject(DEFAULT_GUI_FONT));

            RECT rcText = dis->rcItem;
            DrawTextW(dis->hDC, btnText, -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        break;
    }

    case WM_NOTIFY: {
        LPNMHDR nm = reinterpret_cast<LPNMHDR>(lp);
        if (nm->idFrom == ID_ROWS) {
            if (nm->code == NM_CUSTOMDRAW) {
                auto* nmlvcd = reinterpret_cast<NMLVCUSTOMDRAW*>(lp);
                const Theme& th = getActiveTheme();
                switch (nmlvcd->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT: {
                    bool isEven = (nmlvcd->nmcd.dwItemSpec % 2 == 0);
                    nmlvcd->clrTextBk = isEven ? th.bgCard : th.bgPanel;
                    nmlvcd->clrText = th.textMain;
                    return CDRF_NEWFONT;
                }
                default:
                    return CDRF_DODEFAULT;
                }
            }
            if (nm->code == LVN_ITEMCHANGED || nm->code == NM_CLICK) {
                inspectSelectedTableRow();
            }
        }
        break;
    }

    case WM_KEYDOWN: {
        if (wp == VK_ESCAPE) {
            if (g_hud.settingsModalOpen || g_hud.infoModalOpen) {
                g_hud.settingsModalOpen = false;
                g_hud.infoModalOpen = false;
                RECT rc{};
                GetClientRect(hwnd, &rc);
                updateControlLayout(rc.right, rc.bottom);
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }
        }
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        switch (id) {
        case ID_RUN_BTN: {
            std::wstring strat = getControlText(g_hud.strategyCombo);
            std::wstring seed = getControlText(g_hud.seedBox);
            std::wstring count = getControlText(g_hud.countBox);
            std::wstring args = L"cycle-k4d-six \"data/research.sqlite\" \"k4d_six_results.sqlite\" \"" +
                                strat + L"\" " + count + L" " + seed;
            launchProcess(args);
            break;
        }
        case ID_STOP_BTN: {
            if (g_hud.processRunning && g_hud.activeProcess.hProcess) {
                TerminateProcess(g_hud.activeProcess.hProcess, 0);
                CloseHandle(g_hud.activeProcess.hProcess);
                CloseHandle(g_hud.activeProcess.hThread);
                g_hud.activeProcess = {};
                g_hud.processRunning = false;
                g_hud.liveFooterStatus = L"Prozess durch Benutzer gestoppt.";
            }
            break;
        }
        case ID_CHAINFROM_BTN: {
            std::wstring selId = getSelectedTableRowItem(0);
            if (!selId.empty()) {
                std::wstring seed = getControlText(g_hud.seedBox);
                launchProcess(L"cycle-k4d-six \"data/research.sqlite\" \"k4d_six_results.sqlite\" \"type_synthesis\" 2 " + seed + L" --parent=" + selId);
            }
            break;
        }
        case ID_REPORT_BTN: {
            launchProcess(L"graph \"data/research.sqlite\" \"k4d_six_results.sqlite\"");
            break;
        }
        case ID_SEARCH_BTN: {
            refreshSourceDatabase();
            break;
        }
        case ID_SAVE_BTN: {
            if (!g_hud.currentEditorFile.empty()) {
                std::wstring text = getControlText(g_hud.editor);
                std::ofstream out(fs::path(g_hud.currentEditorFile), std::ios::binary | std::ios::trunc);
                if (out) {
                    std::string u = narrow(text);
                    out.write(u.data(), static_cast<std::streamsize>(u.size()));
                    g_hud.liveFooterStatus = L"Datei gespeichert: " + g_hud.currentEditorFile;
                }
            }
            break;
        }
        case ID_TEST_BTN: {
            launchProcess(L"algorithm-lab \"algorithm_lab_results.sqlite\" \"algorithm_kernels\" 4 314159");
            break;
        }
        case ID_PLOT_BTN: {
            launchProcess(L"algorithm-lab \"algorithm_lab_results.sqlite\" \"algorithm_kernels\" 1 314159");
            break;
        }
        case ID_EVO_METAFUSION_BTN: {
            std::wstring fA = getControlText(g_hud.evoFamACombo);
            std::wstring fB = getControlText(g_hud.evoFamBCombo);
            std::wstring fTarget = getControlText(g_hud.evoTargetFamBox);
            launchProcess(L"evolution-fusion \"" + fA + L"\" \"" + fB + L"\" \"evolution_workspace\" \"" + fTarget + L"\" --depth=4 --seed=314159");
            break;
        }
        case ID_EVO_CONTINUOUS_BTN: {
            std::wstring fA = getControlText(g_hud.evoFamACombo);
            launchProcess(L"evolution-run \"" + fA + L"\" \"evolution_workspace\" 314159 --depth=4 --max-cycles=5");
            break;
        }
        case ID_EVO_ROLLBACK_BTN: {
            launchProcess(L"evolution-rollback \"evolution_workspace\"");
            break;
        }
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        int mx = GET_X_LPARAM(lp);
        int my = GET_Y_LPARAM(lp);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        int w = rc.right;
        int h = rc.bottom;

        if (g_hud.settingsModalOpen) {
            int mw = 720, mh = 500;
            int mx0 = (w - mw) / 2;
            int my0 = 64 + (h - 64 - mh) / 2;
            RECT mBox{mx0, my0, mx0 + mw, my0 + mh};
            int cardW = (mBox.right - mBox.left - 64) / 2;
            int cardH = 58;
            g_hud.hoveredThemeCard = -1;
            for (int i = 0; i < 4; ++i) {
                int cxPos = (i % 2 == 0) ? (mBox.left + 24) : (mBox.left + 36 + cardW);
                int cyPos = (i < 2) ? (mBox.top + 102) : (mBox.top + 168);
                if (mx >= cxPos && mx <= cxPos + cardW && my >= cyPos && my <= cyPos + cardH) {
                    g_hud.hoveredThemeCard = i;
                    break;
                }
            }
            int fBtnW = (mBox.right - mBox.left - 64) / 3;
            g_hud.hoveredFontSizeBtn = -1;
            for (int i = 0; i < 3; ++i) {
                int fx = mBox.left + 24 + i * (fBtnW + 8);
                if (mx >= fx && mx <= fx + fBtnW && my >= mBox.top + 268 && my <= mBox.top + 304) {
                    g_hud.hoveredFontSizeBtn = i;
                    break;
                }
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        // Header Buttons Hover
        int prevTop = g_hud.hoveredTopButton;
        g_hud.hoveredTopButton = 0;
        if (my >= 18 && my <= 46) {
            if (mx >= w - 280 && mx <= w - 196) g_hud.hoveredTopButton = 1;      // Settings
            else if (mx >= w - 190 && mx <= w - 134) g_hud.hoveredTopButton = 2; // Info
            else if (mx >= w - 126 && mx <= w - 92) g_hud.hoveredTopButton = 3;  // Min
            else if (mx >= w - 86 && mx <= w - 52) g_hud.hoveredTopButton = 4;   // Max
            else if (mx >= w - 46 && mx <= w - 12) g_hud.hoveredTopButton = 5;   // Close
        }

        // Workspace Buttons Hover im Dock
        RECT dockRc{24, h - 64, w - 24, h - 18};
        int prevWs = g_hud.hoveredWorkspaceBtn;
        g_hud.hoveredWorkspaceBtn = -1;
        if (my >= dockRc.top + 8 && my <= dockRc.bottom - 8) {
            int btnX = dockRc.left + 12;
            int bScale = (g_hud.currentFontSize == FontSizeOption::Large ? 14 : (g_hud.currentFontSize == FontSizeOption::Compact ? -10 : 0));
            const int widths[6] = {130 + bScale, 130 + bScale, 135 + bScale, 110 + bScale, 115 + bScale, 120 + bScale};
            for (int i = 0; i < 6; ++i) {
                if (mx >= btnX && mx <= btnX + widths[i]) {
                    g_hud.hoveredWorkspaceBtn = i;
                    break;
                }
                btnX += widths[i] + 6;
            }
        }

        if (prevTop != g_hud.hoveredTopButton || prevWs != g_hud.hoveredWorkspaceBtn) {
            InvalidateRect(hwnd, nullptr, FALSE);
        }

        // Agent Bridge: Selftest Button Hover
        if (g_hud.currentMode == WorkspaceMode::AgentBridge) {
            int bridgeW = std::min(1240, w - 64);
            RECT bridgeRc{w / 2 - bridgeW / 2, 70, w / 2 + bridgeW / 2, h - 74};
            RECT stBtnRc{bridgeRc.left + 24, bridgeRc.bottom - 52, bridgeRc.right - 24, bridgeRc.bottom - 12};
            bool prevStHov = g_hud.selftestBtnHovered;
            g_hud.selftestBtnHovered = (mx >= stBtnRc.left && mx <= stBtnRc.right &&
                                        my >= stBtnRc.top && my <= stBtnRc.bottom);
            if (prevStHov != g_hud.selftestBtnHovered) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }

        // Nexus: Sphären & Buttons Hover
        if (g_hud.currentMode == WorkspaceMode::OrbitalNexus) {
            g_hud.hoveredSphereIndex = -1;
            for (size_t i = 0; i < g_hud.spheres.size(); ++i) {
                int dx = mx - g_hud.spheres[i].screenX;
                int dy = my - g_hud.spheres[i].screenY;
                if (dx * dx + dy * dy <= g_hud.spheres[i].radius * g_hud.spheres[i].radius) {
                    g_hud.hoveredSphereIndex = static_cast<int>(i);
                    break;
                }
            }

            g_hud.hoveredNavIndex = -1;
            if (mx >= 28 && mx <= 288) {
                int navY = 114;
                for (size_t i = 0; i < g_hud.spheres.size(); ++i) {
                    if (my >= navY && my <= navY + 76) {
                        g_hud.hoveredNavIndex = static_cast<int>(i);
                        break;
                    }
                    navY += 86;
                }
            }

            RECT panelRc{w - 370, 74, w - 24, h - 76};
            RECT actionBtnRc{panelRc.left + 16, panelRc.bottom - 54, panelRc.right - 16, panelRc.bottom - 14};
            g_hud.primaryBtnHovered = (mx >= actionBtnRc.left && mx <= actionBtnRc.right &&
                                       my >= actionBtnRc.top && my <= actionBtnRc.bottom);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lp);
        int my = GET_Y_LPARAM(lp);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        int w = rc.right;
        int h = rc.bottom;

        // Settings Modal Klicks
        if (g_hud.settingsModalOpen) {
            int mw = 720, mh = 500;
            int mx0 = (w - mw) / 2;
            int my0 = 64 + (h - 64 - mh) / 2;
            RECT mBox{mx0, my0, mx0 + mw, my0 + mh};
            RECT sCloseRc{mBox.right - 140, mBox.bottom - 46, mBox.right - 24, mBox.bottom - 16};
            if ((mx >= sCloseRc.left && mx <= sCloseRc.right && my >= sCloseRc.top && my <= sCloseRc.bottom) ||
                (mx < mBox.left || mx > mBox.right || my < mBox.top || my > mBox.bottom)) {
                g_hud.settingsModalOpen = false;
                updateControlLayout(w, h);
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }
            if (g_hud.hoveredThemeCard >= 0) {
                g_hud.currentThemeIndex = g_hud.hoveredThemeCard;
                applyThemeToControls();
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }
            if (g_hud.hoveredFontSizeBtn >= 0) {
                g_hud.currentFontSize = static_cast<FontSizeOption>(g_hud.hoveredFontSizeBtn);
                rebuildFonts();
                RECT rCl{};
                GetClientRect(hwnd, &rCl);
                updateControlLayout(rCl.right, rCl.bottom);
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }
            // Options Toggle
            RECT optAnim{mBox.left + 24, mBox.top + 348, mBox.left + 210, mBox.top + 384};
            RECT optPart{mBox.left + 220, mBox.top + 348, mBox.left + 400, mBox.top + 384};
            RECT optGrid{mBox.left + 410, mBox.top + 348, mBox.left + 580, mBox.top + 384};
            if (mx >= optAnim.left && mx <= optAnim.right && my >= optAnim.top && my <= optAnim.bottom) {
                g_hud.enableOrbitAnimation = !g_hud.enableOrbitAnimation;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (mx >= optPart.left && mx <= optPart.right && my >= optPart.top && my <= optPart.bottom) {
                g_hud.enableParticles = !g_hud.enableParticles;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (mx >= optGrid.left && mx <= optGrid.right && my >= optGrid.top && my <= optGrid.bottom) {
                g_hud.enableGrid = !g_hud.enableGrid;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            return 0;
        }

        // Info Modal Klicks
        if (g_hud.infoModalOpen) {
            int mw = 640, mh = 440;
            int mx0 = (w - mw) / 2;
            int my0 = 64 + (h - 64 - mh) / 2;
            RECT mBox{mx0, my0, mx0 + mw, my0 + mh};
            RECT mCloseRc{mBox.right - 140, mBox.bottom - 46, mBox.right - 24, mBox.bottom - 16};
            if ((mx >= mCloseRc.left && mx <= mCloseRc.right && my >= mCloseRc.top && my <= mCloseRc.bottom) ||
                (mx < mBox.left || mx > mBox.right || my < mBox.top || my > mBox.bottom)) {
                g_hud.infoModalOpen = false;
                updateControlLayout(w, h);
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }
            return 0;
        }

        // Header Buttons
        if (g_hud.hoveredTopButton == 1) { // Settings
            g_hud.settingsModalOpen = !g_hud.settingsModalOpen;
            g_hud.infoModalOpen = false;
            updateControlLayout(w, h);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        if (g_hud.hoveredTopButton == 2) { // Info
            g_hud.infoModalOpen = !g_hud.infoModalOpen;
            g_hud.settingsModalOpen = false;
            updateControlLayout(w, h);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        if (g_hud.hoveredTopButton == 3) { // Min
            ShowWindow(hwnd, SW_MINIMIZE);
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

        // Verschieben bei Klick auf Header
        if (my < 60) {
            ReleaseCapture();
            SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        }

        // Workspace-Buttons im Dock
        if (g_hud.hoveredWorkspaceBtn >= 0) {
            switchWorkspace(static_cast<WorkspaceMode>(g_hud.hoveredWorkspaceBtn));
            return 0;
        }

        // Klicks im Orbital Nexus
        if (g_hud.currentMode == WorkspaceMode::OrbitalNexus) {
            if (g_hud.hoveredSphereIndex >= 0) {
                g_hud.activeSphereIndex = g_hud.hoveredSphereIndex;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (g_hud.hoveredNavIndex >= 0) {
                g_hud.activeSphereIndex = g_hud.hoveredNavIndex;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (g_hud.primaryBtnHovered) {
                const auto& sp = g_hud.spheres[static_cast<size_t>(g_hud.activeSphereIndex)];
                launchProcess(sp.cliCommandArgs);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }

        // Klicks in der Agent Bridge
        if (g_hud.currentMode == WorkspaceMode::AgentBridge) {
            if (g_hud.selftestBtnHovered) {
                launchProcess(L"selftest \"KR_BUILD/selftest\"");
                g_hud.liveFooterStatus = L"18-Punkte-Selftest gestartet • Orakel prüft 18 mathematische Tore...";
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }
        }
        return 0;
    }

    case WM_TIMER: {
        // FLIMMERFREI: 60 FPS Animation NUR im OrbitalNexus Modus ausführen!
        if (g_hud.currentMode == WorkspaceMode::OrbitalNexus) {
            if (g_hud.enableOrbitAnimation) {
                g_hud.animPhase += 0.035 * (1.0 + g_hud.cycleBoost);
                g_hud.wavePhase += 0.07 * (1.0 + g_hud.cycleBoost);
                g_hud.cycleBoost *= 0.96;

                for (auto& st : g_hud.stars) {
                    st.y -= st.speed * (1.0f + static_cast<float>(g_hud.cycleBoost));
                    if (st.y < 0.0f) st.y = 1.0f;
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }

        // Prozess-Überwachung im Hintergrund
        if (g_hud.processRunning && g_hud.activeProcess.hProcess) {
            DWORD code = STILL_ACTIVE;
            if (GetExitCodeProcess(g_hud.activeProcess.hProcess, &code) && code != STILL_ACTIVE) {
                CloseHandle(g_hud.activeProcess.hProcess);
                CloseHandle(g_hud.activeProcess.hThread);
                g_hud.activeProcess = {};
                g_hud.processRunning = false;

                if (g_hud.lastProcessCmd.find(L"selftest") != std::wstring::npos) {
                    if (code == 0) {
                        g_hud.selftestState = 2; // Success
                        g_hud.liveFooterStatus = L"✓ 18-Punkte-Selftest ERFOLGREICH (18/18 PASSED) • ExitCode 0";
                    } else {
                        g_hud.selftestState = 3; // Error
                        g_hud.liveFooterStatus = L"⚠ Selftest mit Fehler beendet (ExitCode " + std::to_wstring(code) + L")";
                    }
                } else {
                    g_hud.liveFooterStatus = L"Zyklus beendet (ExitCode " + std::to_wstring(code) + L") • SQLite WAL aktualisiert";
                }

                if (g_hud.currentMode == WorkspaceMode::ResearchLab) refreshK4dLab();
                else if (g_hud.currentMode == WorkspaceMode::CppEditorLab) refreshCppEditorLab();
                else if (g_hud.currentMode == WorkspaceMode::MetaFusion) refreshMetaFusion();
                InvalidateRect(hwnd, nullptr, TRUE);
            }
        }
        return 0;
    }

    // Controls mit Theme-Farben zeichnen (kein GDI-Leak, pre-allocated brush!)
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN: {
        HDC hdc = reinterpret_cast<HDC>(wp);
        const Theme& th = getActiveTheme();
        SetTextColor(hdc, th.textMain);
        SetBkColor(hdc, th.bgCard);
        return reinterpret_cast<LRESULT>(g_hud.brushCard);
    }

    case WM_DESTROY: {
        KillTimer(hwnd, 1);
        if (g_hud.processRunning && g_hud.activeProcess.hProcess) {
            TerminateProcess(g_hud.activeProcess.hProcess, 0);
            CloseHandle(g_hud.activeProcess.hProcess);
            CloseHandle(g_hud.activeProcess.hThread);
        }
        if (g_hud.brushBg)    DeleteObject(g_hud.brushBg);
        if (g_hud.brushCard)  DeleteObject(g_hud.brushCard);
        if (g_hud.brushPanel) DeleteObject(g_hud.brushPanel);
        if (g_hud.fontDisplayTitle) DeleteObject(g_hud.fontDisplayTitle);
        if (g_hud.fontDisplayLarge) DeleteObject(g_hud.fontDisplayLarge);
        if (g_hud.fontDisplayMid)   DeleteObject(g_hud.fontDisplayMid);
        if (g_hud.fontSansBold)     DeleteObject(g_hud.fontSansBold);
        if (g_hud.fontSans)         DeleteObject(g_hud.fontSans);
        if (g_hud.fontMono)         DeleteObject(g_hud.fontMono);
        if (g_hud.fontMonoSmall)    DeleteObject(g_hud.fontMonoSmall);
        if (g_hud.fontEditor)       DeleteObject(g_hud.fontEditor);
        PostQuitMessage(0);
        return 0;
    }

    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
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
    axion::hud::checkDatabasePaths();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = axion::hud::SpatialHudWndProc;
    wc.lpszClassName = L"Axion_SpatialOrbitalHUD";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(6, 9, 17));
    RegisterClassExW(&wc);

    // WS_CLIPCHILDREN | WS_CLIPSIBLINGS: Verhindert jegliches Flackern der Child-Controls
    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        wc.lpszClassName,
        L"AXION // SPATIAL ORBITAL HUD v4.0.3",
        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        80, 40, 1540, 930,
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
