#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shellapi.h>
#include <objbase.h>
#include <iomanip>
#include "win_sqlite_api.hpp"
#include <dwmapi.h>
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#ifdef _MSC_VER
#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"winsqlite3.lib")
#pragma comment(lib,"shell32.lib")
#endif
namespace fs = std::filesystem;
namespace {
HWND windowHandle{}, tabs{}, rows{}, editor{}, searchBox{}, countBox{}, seedBox{}, status{}, label{};
HWND runButton{}, testButton{}, saveButton{}, plotButton{}, stopButton{}, reportButton{}, searchButton{};
HWND strategyCombo{}, lineModeCombo{}, chainModeCombo{}, chainFromButton{}, lmDevButton{}, metaSynthButton{};
HWND providerCombo{}, apiKeyLabel{}, apiKeyBox{}, modelLabel{}, modelBox{}, appTypeCombo{};
HWND evoDumpBox{}, evoBrowseButton{}, evoCycleButton{}, evoContinuousButton{}, evoRollbackButton{}, evoStatusButton{};
HWND evoSubModeCombo{}, evoFamACombo{}, evoFamBCombo{}, evoTargetFamBox{}, evoMetaFusionButton{};
HWND appTooltip{};
HFONT g_uiFont = nullptr;
HFONT g_editorFont = nullptr;

void initUiFonts() {
    if (!g_uiFont) {
        NONCLIENTMETRICSW ncm{sizeof(ncm)};
        if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) {
            g_uiFont = CreateFontIndirectW(&ncm.lfMessageFont);
        }
        if (!g_uiFont) {
            g_uiFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        }
        if (!g_uiFont) {
            g_uiFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        }
    }
    if (!g_editorFont) {
        g_editorFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            FIXED_PITCH | FF_MODERN, L"Consolas");
    }
}

void applyUiFont(HWND parent) {
    if (!g_uiFont) return;
    SendMessageW(parent, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
    EnumChildWindows(parent, [](HWND child, LPARAM lp) -> BOOL {
        SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(lp), TRUE);
        return TRUE;
    }, reinterpret_cast<LPARAM>(g_uiFont));
}

constexpr int ID_TABS = 501, ID_ROWS = 502, ID_SEARCH = 503, ID_RUN = 504, ID_COUNT = 505, ID_SEED = 506;
constexpr int ID_SAVE = 507, ID_TEST = 508, ID_PLOT = 509, ID_STOP = 510, ID_REPORT = 511;
constexpr int ID_STRATEGY = 512, ID_LINEMODE = 513, ID_CHAINMODE = 514, ID_CHAINFROM = 515, ID_LMDEV = 516;
constexpr int ID_PROVIDER = 517, ID_APIKEY = 518, ID_MODEL = 519, ID_METASYNTH = 520;
constexpr int ID_EVO_DUMP = 521, ID_EVO_BROWSE = 522, ID_EVO_CYCLE = 523, ID_EVO_CONTINUOUS = 524;
constexpr int ID_EVO_ROLLBACK = 525, ID_EVO_STATUS = 526, ID_APPTYPE = 527;
constexpr int ID_EVO_SUBMODE = 528, ID_EVO_FAMA = 529, ID_EVO_FAMB = 530, ID_EVO_TARGETFAM = 531, ID_EVO_METAFUSION = 532;
constexpr int ID_CTX_OPEN_DIR = 601, ID_CTX_DELETE = 602;
constexpr int ID_MENU_ABOUT = 701, ID_MENU_MATH_ORIGIN = 702;

HBRUSH g_bgBrush = nullptr;
HBRUSH g_editBrush = nullptr;
HBRUSH g_panelBrush = nullptr;

void showAboutDialog(HWND parent) {
    const wchar_t* title = L"Über Axion";
    const wchar_t* msg = 
        L"AXION – Autonomous Mathematical Research & Synthesis Engine\n\n"
        L"Entwickler: Ralf Krümmel\n"
        L"Architektur: C++20 High-Performance Core, Lean 4 Integration, Win32 IDE\n"
        L"Lizenz: MIT License\n\n"
        L"Axion verbindet formale mathematische Beweisführung, autonome Hypothesen-Evolution\n"
        L"und automatisierte C++ Code-Synthese in einer einheitlichen Forschungsumgebung.";
    MessageBoxW(parent, msg, title, MB_OK | MB_ICONINFORMATION);
}

void showMathOriginDialog(HWND parent) {
    const wchar_t* title = L"Axion – Herkunft der mathematischen Algorithmen";
    const wchar_t* msg =
        L"Mathematische Grundlagen und Algorithmen-Herkunft in Axion:\n\n"
        L"• Entwickler & Systemkonzept:\n"
        L"   Ralf Krümmel\n\n"
        L"• Formale Logik & Beweisführung:\n"
        L"   - Formale Beweise und Typ-Reflexion über Lean 4 (MetaM Environment)\n"
        L"   - Taktik- und Kontext-Discharge (arithmetic / refl) ohne unbewiesene Axiome\n\n"
        L"• Autonome Forschungs-Evolution (K4D):\n"
        L"   - Multi-Relationale Hypothesennetzwerke (Type Synthesis, Systematic, Random,\n"
        L"     Cross Domain, Mutation, Iterative)\n"
        L"   - Spektrale Graphentheorie & Laplace-Matrix-Zerlegung zur Konsistenzprüfung\n\n"
        L"• Mathematische Validierung:\n"
        L"   - Zweistufige Orakel-Verifikation: Numerische Falsifikation & formale Verifikation\n"
        L"   - Automatische Gegenbeispiel-Suche gegen unzulässige algebraische Behauptungen\n\n"
        L"• Code-Synthese:\n"
        L"   - Algorithmische C++20 Operatorgraphen und automatische Win32/CLI-Applikationssynthese";
    MessageBoxW(parent, msg, title, MB_OK | MB_ICONINFORMATION);
}

std::wstring getSelectedAppTypeArg(){
    if(!appTypeCombo) return L"";
    int idx = ComboBox_GetCurSel(appTypeCombo);
    if(idx == 1) return L" --app-type=console";
    if(idx == 2) return L" --app-type=win32_gui";
    return L" --app-type=auto";
}

std::wstring ramGeminiApiKey; // Exclusively in volatile RAM, never written to disk or logs
std::wstring sourceDb = L"F:\\MathResearchDB_Backup_20261009\\research.sqlite";
std::wstring k4dResultsDb = L"F:\\mre_new\\k4d_six_results.sqlite";
std::wstring k4dLeanDir = L"F:\\mre_new\\K4D_SIX_LEAN";
std::wstring resultsDb = L"F:\\MathResearchDB_v0_4\\algorithm_lab_results.sqlite";
std::wstring kernelDir = L"F:\\MathResearchDB_v0_4\\algorithm_kernels";
std::wstring leanBin = L"C:\\Users\\ralfk\\.elan\\bin\\lean.exe";
std::wstring evoLineageDb = L"F:\\mre_new\\evolution_ws\\evolution_lineage.sqlite";
std::wstring evoWorkspaceDir = L"F:\\mre_new\\evolution_ws";

std::wstring engine, projectRoot, currentFile, selectedSource;
int mode = 0; bool dirty = false; bool loadingEditor = false; bool kernelTestRunning = false;
PROCESS_INFORMATION job{};

std::wstring wfrom(const unsigned char* p){
    if(!p) return L"";
    auto a = reinterpret_cast<const char*>(p);
    int n = MultiByteToWideChar(CP_UTF8, 0, a, -1, nullptr, 0);
    if(n <= 0) return L"";
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, a, -1, out.data(), n);
    out.pop_back();
    return out;
}

std::string narrow(const std::wstring& w){
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if(n <= 0) return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), n, nullptr, nullptr);
    out.pop_back();
    return out;
}

std::wstring value(HWND h){
    int n = GetWindowTextLengthW(h);
    std::wstring s(static_cast<size_t>(n) + 1, L'\0');
    GetWindowTextW(h, s.data(), n + 1);
    s.resize(static_cast<size_t>(n));
    return s;
}

void report(const std::wstring& s){ SetWindowTextW(status, s.c_str()); }
HMENU ctl(int id){ return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)); }

void setColumns(const std::vector<std::wstring>& names){
    ListView_DeleteAllItems(rows);
    while(ListView_DeleteColumn(rows, 0)){}
    for(size_t i = 0; i < names.size(); ++i){
        LVCOLUMNW c{};
        c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        c.pszText = const_cast<wchar_t*>(names[i].c_str());
        if (mode == 0) {
            if (i == 0) c.cx = 55;        // ID
            else if (i == 1) c.cx = 100;  // Strategie
            else if (i == 2) c.cx = 110;  // Linie
            else if (i == 3) c.cx = 150;  // Status
            else if (i == 4) c.cx = 50;   // Tiefe
            else if (i == 5) c.cx = 65;   // Eltern-ID
            else if (i == 6) c.cx = 160;  // Quelle A
            else if (i == 7) c.cx = 160;  // Quelle B
            else if (i == 8) c.cx = 250;  // Dynamik / Kette
            else if (i == 9) c.cx = 75;   // Dauer
            else c.cx = 120;
        } else if (mode == 6) {
            if (i == 0) c.cx = 150;       // Forschungsfamilie
            else if (i == 1) c.cx = 75;   // Generation (MRE_0, MRE_1, etc.)
            else if (i == 2) c.cx = 150;  // Komplexitaet
            else if (i == 3) c.cx = 200;  // Fingerabdruck
            else if (i == 4) c.cx = 100;  // Validierungsstatus
            else if (i == 5) c.cx = 75;   // Speedup
            else if (i == 6) c.cx = 70;   // Oracles
            else if (i == 7) c.cx = 260;  // Mathematische_Hypothese
            else if (i == 8) c.cx = 150;  // Generation_ID
            else if (i == 9) c.cx = 135;  // Zeitstempel
            else c.cx = 120;
        } else {
            c.cx = (i == 0 ? 75 : i == 1 ? 240 : 190);
        }
        c.iSubItem = static_cast<int>(i);
        ListView_InsertColumn(rows, static_cast<int>(i), &c);
    }
}

void addRow(const std::vector<std::wstring>& data){
    int idx = ListView_GetItemCount(rows);
    if(data.empty()) return;
    LVITEMW v{};
    v.mask = LVIF_TEXT;
    v.iItem = idx;
    v.pszText = const_cast<wchar_t*>(data[0].c_str());
    ListView_InsertItem(rows, &v);
    for(size_t i = 1; i < data.size(); ++i){
        ListView_SetItemText(rows, idx, static_cast<int>(i), const_cast<wchar_t*>(data[i].c_str()));
    }
}

std::wstring pick(int col){
    int idx = ListView_GetNextItem(rows, -1, LVNI_SELECTED);
    if(idx < 0) return L"";
    wchar_t buf[8192]{};
    ListView_GetItemText(rows, idx, col, buf, 8192);
    return buf;
}

void table(const std::wstring& dbPath, const char* sql, const std::string& argument = ""){
    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(narrow(dbPath).c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
    if(rc != SQLITE_OK){
        if(db) sqlite3_close(db);
        report(L"SQLite kann nicht gelesen werden: " + dbPath);
        return;
    }
    sqlite3_busy_timeout(db, 3000);
    sqlite3_stmt* st = nullptr;
    rc = sqlite3_prepare_v2(db, sql, -1, &st, nullptr);
    if(rc != SQLITE_OK){
        report(L"SQL-Schema: " + wfrom(reinterpret_cast<const unsigned char*>(sqlite3_errmsg(db))));
        sqlite3_close(db);
        return;
    }
    if(!argument.empty()){
        sqlite3_bind_text(st, 1, argument.c_str(), static_cast<int>(argument.size()), nullptr);
    }
    std::vector<std::wstring> names;
    for(int i = 0; i < sqlite3_column_count(st); ++i){
        names.push_back(wfrom(reinterpret_cast<const unsigned char*>(sqlite3_column_name(st, i))));
    }
    setColumns(names);
    int count = 0;
    while(count < 250 && sqlite3_step(st) == SQLITE_ROW){
        std::vector<std::wstring> entry;
        for(int i = 0; i < sqlite3_column_count(st); ++i){
            entry.push_back(wfrom(sqlite3_column_text(st, i)));
        }
        addRow(entry);
        ++count;
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
    report(L"Angezeigt: " + std::to_wstring(count) + L" Einträge (max. 250). Quell-DB nur lesend.");
}

std::wstring readText(const fs::path& path){
    std::ifstream stream(path, std::ios::binary);
    if(!stream) return L"";
    std::string bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    return wfrom(reinterpret_cast<const unsigned char*>(bytes.c_str()));
}

bool writeText(const fs::path& path, const std::wstring& data){
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if(!out) return false;
    auto utf = narrow(data);
    out.write(utf.data(), static_cast<std::streamsize>(utf.size()));
    return static_cast<bool>(out);
}

void showEditor(const std::wstring& text, bool editable){
    SendMessageW(editor, EM_SETREADONLY, editable ? FALSE : TRUE, 0);
    loadingEditor = true;
    SetWindowTextW(editor, text.c_str());
    loadingEditor = false;
    dirty = false;
}
void prepareLeanEnvironment() {
    SetEnvironmentVariableW(L"MRE_LEAN_TIMEOUT_MS", L"500000");
    SetEnvironmentVariableW(L"MRE_IMPORTABLE_ONLY", L"1");
    wchar_t envBuf[32768]{};
    if (GetEnvironmentVariableW(L"LEAN_PATH", envBuf, 32768) <= 0) {
        const wchar_t* fullLeanPath = 
            L"F:\\MathResearchDB_v0_4\\lean_cache\\oai_workspace;"
            L"F:\\MathResearchDB_v0_4\\lean_cache\\packages\\Cli\\.lake\\build\\lib\\lean;"
            L"F:\\MathResearchDB_v0_4\\lean_cache\\packages\\batteries\\.lake\\build\\lib\\lean;"
            L"F:\\MathResearchDB_v0_4\\lean_cache\\packages\\Qq\\.lake\\build\\lib\\lean;"
            L"F:\\MathResearchDB_v0_4\\lean_cache\\packages\\aesop\\.lake\\build\\lib\\lean;"
            L"F:\\MathResearchDB_v0_4\\lean_cache\\packages\\proofwidgets\\.lake\\build\\lib\\lean;"
            L"F:\\MathResearchDB_v0_4\\lean_cache\\packages\\importGraph\\.lake\\build\\lib\\lean;"
            L"F:\\MathResearchDB_v0_4\\lean_cache\\packages\\LeanSearchClient\\.lake\\build\\lib\\lean;"
            L"F:\\MathResearchDB_v0_4\\lean_cache\\packages\\plausible\\.lake\\build\\lib\\lean;"
            L"F:\\MathResearchDB_v0_4\\lean_cache\\mathlib\\.lake\\build\\lib\\lean;"
            L"C:\\Users\\ralfk\\.elan\\toolchains\\leanprover--lean4---v4.34.1\\lib\\lean";
        SetEnvironmentVariableW(L"LEAN_PATH", fullLeanPath);
    }
    if (fs::exists(L"C:\\Users\\ralfk\\.elan\\bin\\lean.exe")) {
        leanBin = L"C:\\Users\\ralfk\\.elan\\bin\\lean.exe";
    }
}

void showK4dSelection(){
    auto id = pick(0);
    if(id.empty()) return;
    sqlite3* db = nullptr;
    if(sqlite3_open_v2(narrow(k4dResultsDb).c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK){
        if(db) sqlite3_close(db);
        return;
    }
    sqlite3_stmt* st = nullptr;
    const char* sql = "SELECT id, strategy, seed, mode, source_a, source_b, file_a, file_b, "
                      "status, details, hypothesis_code, proof_code, error_log, "
                      "verification_exit_code, verification_duration_ms, parent_id, chain_depth "
                      "FROM candidates WHERE id = ?1";
    if(sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK){
        auto idNarrow = narrow(id);
        sqlite3_bind_text(st, 1, idNarrow.c_str(), static_cast<int>(idNarrow.size()), nullptr);
        if(sqlite3_step(st) == SQLITE_ROW){
            auto cid = wfrom(sqlite3_column_text(st, 0));
            auto strat = wfrom(sqlite3_column_text(st, 1));
            auto seed = wfrom(sqlite3_column_text(st, 2));
            auto cmode = wfrom(sqlite3_column_text(st, 3));
            auto srcA = wfrom(sqlite3_column_text(st, 4));
            auto srcB = wfrom(sqlite3_column_text(st, 5));
            auto fileA = wfrom(sqlite3_column_text(st, 6));
            auto fileB = wfrom(sqlite3_column_text(st, 7));
            auto statusVal = wfrom(sqlite3_column_text(st, 8));
            auto details = wfrom(sqlite3_column_text(st, 9));
            auto hypCode = wfrom(sqlite3_column_text(st, 10));
            auto proofCode = wfrom(sqlite3_column_text(st, 11));
            auto errLog = wfrom(sqlite3_column_text(st, 12));
            int exitCode = sqlite3_column_int(st, 13);
            int durationMs = sqlite3_column_int(st, 14);
            int64_t parentId = sqlite3_column_int(st, 15);
            int chainDepth = sqlite3_column_int(st, 16);

            std::wstring lineTag = (details.find(L"K4D") != std::wstring::npos || cmode == L"K4D_GUIDED") 
                                   ? L"K4D_GUIDED (4D-Attraktor-Dynamik & Perturbation)" 
                                   : L"CONTROL_NO_K (Gleichverteilte Kontroll-Baseline)";

            std::wostringstream ss;
            ss << L"================================================================================\r\n";
            ss << L"MRE K4D HYPOTHESEN-INSPEKTOR | KANDIDAT ID: " << cid << L"\r\n";
            ss << L"================================================================================\r\n";
            ss << L"Strategie:           " << strat << L"\r\n";
            ss << L"Forschungslinie:     " << lineTag << L"\r\n";
            ss << L"Lean-Beweisstatus:   " << statusVal << L"\r\n";
            ss << L"Lean Exit-Code:      " << exitCode << L"\r\n";
            ss << L"Verifikationsdauer:  " << durationMs << L" ms\r\n";
            ss << L"Generierungs-Seed:   " << seed << L"\r\n\r\n";

            ss << L"================================================================================\r\n";
            ss << L"BEWEIS-STAMMBAUM & NETZWERK-TOPOLOGIE:\r\n";
            ss << L"================================================================================\r\n";
            if (chainDepth > 0 || parentId > 0) {
                ss << L"Netzwerkstufe/Tiefe: Stufe " << (chainDepth > 0 ? chainDepth : 1) << L"\r\n";
                if (parentId > 0) {
                    ss << L"Eltern-Theorem:      Kandidat #" << parentId << L"\r\n";
                    ss << L"Beweiskette:         Dieser Satz baut direkt auf den bewiesenen Resultaten von\r\n";
                    ss << L"                     Kandidat #" << parentId << L" auf und nutzt sie als formale Lemmas.\r\n";
                } else {
                    ss << L"Eltern-Theorem:      Wurzel / Mathematische Basis (Depth 1 Foundation)\r\n";
                }
            } else {
                ss << L"Netzwerkstufe:       Basis / Einzelner Forschungszyklus (Tiefe 0)\r\n";
            }

            // Inspect children in the proof network (Verzweigte Nachfolger)
            sqlite3_stmt* cst = nullptr;
            std::string childSql = "SELECT id, status, chain_depth FROM candidates WHERE parent_id = " + narrow(cid) + " ORDER BY id ASC";
            if (sqlite3_prepare_v2(db, childSql.c_str(), -1, &cst, nullptr) == SQLITE_OK) {
                std::vector<std::wstring> childProven;
                std::vector<std::wstring> childOther;
                while (sqlite3_step(cst) == SQLITE_ROW) {
                    auto chId = wfrom(sqlite3_column_text(cst, 0));
                    auto chSt = wfrom(sqlite3_column_text(cst, 1));
                    if (chSt == L"FORMALLY_PROVEN") {
                        childProven.push_back(L"#" + chId);
                    } else {
                        childOther.push_back(L"#" + chId + L" (" + chSt + L")");
                    }
                }
                sqlite3_finalize(cst);

                ss << L"Verzweigte Kinder:   ";
                if (childProven.empty() && childOther.empty()) {
                    ss << L"Keine direkten Nachfolger erzeugt (Blattknoten)\r\n";
                } else {
                    ss << (childProven.size() + childOther.size()) << L" Zweig(e) aus diesem Satz hervorgegangen:\r\n";
                    if (!childProven.empty()) {
                        ss << L"                     - Erfolgreich bewiesen (Zweig lebt weiter): ";
                        for (size_t i = 0; i < childProven.size(); ++i) {
                            ss << (i > 0 ? L", " : L"") << childProven[i];
                        }
                        ss << L"\r\n";
                    }
                    if (!childOther.empty()) {
                        ss << L"                     - Beendete Zweige (nicht bewiesen): ";
                        for (size_t i = 0; i < childOther.size(); ++i) {
                            ss << (i > 0 ? L", " : L"") << childOther[i];
                        }
                        ss << L"\r\n";
                    }
                }
            }
            ss << L"\r\n";

            ss << L"--- MATHEMATISCHE QUELL-DEKLARATIONEN ---\r\n";
            ss << L"Quelle A:            " << srcA << (fileA.empty() ? L"" : (L"  [Modul: " + fileA + L"]")) << L"\r\n";
            ss << L"Quelle B:            " << srcB << (fileB.empty() ? L"" : (L"  [Modul: " + fileB + L"]")) << L"\r\n\r\n";

            ss << L"--- K4D ENTWICKLUNGS- UND DYNAMIK-MERKMALE ---\r\n";
            ss << L"Evolution/Trace:     " << details << L"\r\n\r\n";

            ss << L"================================================================================\r\n";
            ss << L"GENERIERTER FORMALER LEAN 4 QUELLTEXT (Candidate_" << cid << L".lean):\r\n";
            ss << L"================================================================================\r\n";
            if (!hypCode.empty()) {
                ss << hypCode << L"\r\n";
            }
            if (!proofCode.empty()) {
                ss << L"\r\n-- Beweistaktiken:\r\n" << proofCode << L"\r\n";
            }
            if (hypCode.empty() && proofCode.empty()) {
                ss << L"(Kein Lean-Quelltext im Datensatz hinterlegt)\r\n";
            }

            ss << L"\r\n================================================================================\r\n";
            ss << L"LEAN 4 VERIFIKATIONS- UND COMPILER-PROTOKOLL:\r\n";
            ss << L"================================================================================\r\n";
            if (!errLog.empty()) {
                ss << errLog << L"\r\n";
            } else {
                if (statusVal == L"FORMALLY_PROVEN") {
                    ss << L"ERFOLG: Keine Fehler oder Warnungen. Der formale Beweis wurde von Lean 4.34.1 vollständig akzeptiert.\r\n";
                } else {
                    ss << L"(Kein Fehlerprotokoll vorhanden; Status: " << statusVal << L")\r\n";
                }
            }

            showEditor(ss.str(), false);
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
}

void reportK4d(){
    sqlite3* db = nullptr;
    if(sqlite3_open_v2(narrow(k4dResultsDb).c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK){
        if(db) sqlite3_close(db);
        showEditor(L"Ergebnis-Datenbank konnte nicht geöffnet werden: " + k4dResultsDb, false);
        return;
    }
    sqlite3_busy_timeout(db, 3000);
    std::wostringstream ss;
    ss << L"================================================================================\r\n";
    ss << L"MRE K4D RESEARCH LAB | WISSENSCHAFTLICHER FORSCHUNGSBERICHT\r\n";
    ss << L"================================================================================\r\n";
    ss << L"Ergebnis-Datenbank: " << k4dResultsDb << L"\r\n";
    ss << L"Quell-Datenbank:    " << sourceDb << L" (READ-ONLY)\r\n";
    ss << L"Lean Toolchain:     v4.34.1 (OAI Workspace + Mathlib Precompiled Caches)\r\n\r\n";

    sqlite3_stmt* st = nullptr;
    int totalCount = 0;
    if(sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM candidates", -1, &st, nullptr) == SQLITE_OK){
        if(sqlite3_step(st) == SQLITE_ROW) totalCount = sqlite3_column_int(st, 0);
        sqlite3_finalize(st);
    }
    ss << L"Gesamtzahl generierter Forschungskandidaten: " << totalCount << L"\r\n";

    int maxChainDepth = 0;
    if(sqlite3_prepare_v2(db, "SELECT MAX(chain_depth) FROM candidates WHERE chain_depth > 0", -1, &st, nullptr) == SQLITE_OK){
        if(sqlite3_step(st) == SQLITE_ROW) maxChainDepth = sqlite3_column_int(st, 0);
        sqlite3_finalize(st);
    }
    ss << L"Maximal erreichte Kettentiefe: Stufe " << maxChainDepth << L"\r\n\r\n";

    ss << L"--------------------------------------------------------------------------------\r\n";
    ss << L"ÜBERSICHT NACH VERIFIKATIONSSTATUS:\r\n";
    ss << L"--------------------------------------------------------------------------------\r\n";
    if(sqlite3_prepare_v2(db, "SELECT status, COUNT(*) FROM candidates GROUP BY status ORDER BY COUNT(*) DESC", -1, &st, nullptr) == SQLITE_OK){
        while(sqlite3_step(st) == SQLITE_ROW){
            auto stName = wfrom(sqlite3_column_text(st, 0));
            int cnt = sqlite3_column_int(st, 1);
            ss << L"  - " << std::left << std::setw(28) << stName << L": " << cnt << L"\r\n";
        }
        sqlite3_finalize(st);
    }

    ss << L"\r\n--------------------------------------------------------------------------------\r\n";
    ss << L"VERGLEICH: K4D_GUIDED VS. CONTROL_NO_K (ÜBER ALLE 6 STRATEGIEN):\r\n";
    ss << L"--------------------------------------------------------------------------------\r\n";
    const char* lineSql = 
        "SELECT "
        "  CASE WHEN details LIKE '%K4D%' OR mode='K4D_GUIDED' THEN 'K4D_GUIDED' ELSE 'CONTROL_NO_K' END AS line, "
        "  COUNT(*) AS total, "
        "  SUM(CASE WHEN status='FORMALLY_PROVEN' THEN 1 ELSE 0 END) AS proven, "
        "  SUM(CASE WHEN status='PROOF_FAILED' THEN 1 ELSE 0 END) AS proof_failed, "
        "  SUM(CASE WHEN status='ELABORATION_FAILED' THEN 1 ELSE 0 END) AS elab_failed, "
        "  SUM(CASE WHEN status='TYPE_CORRECT' THEN 1 ELSE 0 END) AS type_correct, "
        "  AVG(verification_duration_ms) AS avg_duration "
        "FROM candidates "
        "GROUP BY 1 ORDER BY 1 DESC;";
    if(sqlite3_prepare_v2(db, lineSql, -1, &st, nullptr) == SQLITE_OK){
        while(sqlite3_step(st) == SQLITE_ROW){
            auto line = wfrom(sqlite3_column_text(st, 0));
            int total = sqlite3_column_int(st, 1);
            int proven = sqlite3_column_int(st, 2);
            int pf = sqlite3_column_int(st, 3);
            int ef = sqlite3_column_int(st, 4);
            int tc = sqlite3_column_int(st, 5);
            double avgMs = sqlite3_column_double(st, 6);
            ss << L"[" << line << L"] (Total: " << total << L")\r\n";
            ss << L"  Formally Proven (Lean): " << proven << L"\r\n";
            ss << L"  Proof Failed (Tactic):  " << pf << L"\r\n";
            ss << L"  Type Correct (Check):   " << tc << L"\r\n";
            ss << L"  Elaboration Failed:     " << ef << L"\r\n";
            ss << L"  Durchschnittl. Dauer:   " << static_cast<int>(avgMs) << L" ms\r\n\r\n";
        }
        sqlite3_finalize(st);
    }

    ss << L"--------------------------------------------------------------------------------\r\n";
    ss << L"AUFSCHLÜSSELUNG NACH STRATEGIE UND FORSCHUNGSLINIE:\r\n";
    ss << L"--------------------------------------------------------------------------------\r\n";
    const char* stratSql =
        "SELECT "
        "  strategy, "
        "  CASE WHEN details LIKE '%K4D%' OR mode='K4D_GUIDED' THEN 'K4D' ELSE 'CONTROL' END AS line, "
        "  status, "
        "  COUNT(*) "
        "FROM candidates "
        "GROUP BY strategy, 2, status "
        "ORDER BY strategy, 2 DESC, status;";
    if(sqlite3_prepare_v2(db, stratSql, -1, &st, nullptr) == SQLITE_OK){
        std::wstring lastStrat = L"";
        while(sqlite3_step(st) == SQLITE_ROW){
            auto strat = wfrom(sqlite3_column_text(st, 0));
            auto line = wfrom(sqlite3_column_text(st, 1));
            auto stat = wfrom(sqlite3_column_text(st, 2));
            int cnt = sqlite3_column_int(st, 3);
            if(strat != lastStrat){
                ss << L"\r\nStrategie: " << strat << L"\r\n";
                lastStrat = strat;
            }
            ss << L"  " << std::left << std::setw(10) << line << L" | " 
               << std::left << std::setw(22) << stat << L" : " << cnt << L"\r\n";
        }
        sqlite3_finalize(st);
    }

    ss << L"\r\n================================================================================\r\n";
    ss << L"BEMERKUNG ZUR METHODIK:\r\n";
    ss << L"Erfolgreich bewiesene Theoreme werden in der fortlaufenden Forschungskette\r\n";
    ss << L"automatisch als verifizierte Lemmas für die nächste Tiefenstufe übernommen.\r\n";
    ss << L"================================================================================\r\n";

    sqlite3_close(db);
    showEditor(ss.str(), false);
    report(L"Forschungsbericht erstellt.");
}

void updateControlVisibility(){
    if (mode == 0) {
        ShowWindow(chainModeCombo, SW_SHOW);
        ShowWindow(strategyCombo, SW_SHOW);
        ShowWindow(lineModeCombo, SW_SHOW);
        ShowWindow(chainFromButton, SW_SHOW);
        ShowWindow(stopButton, SW_SHOW);
        ShowWindow(reportButton, SW_SHOW);
        ShowWindow(lmDevButton, SW_SHOW);
        ShowWindow(countBox, SW_SHOW);
        ShowWindow(seedBox, SW_SHOW);
        ShowWindow(runButton, SW_SHOW);
        ShowWindow(searchBox, SW_HIDE);
        SetWindowTextW(searchButton, L"Aktualisieren");
        SetWindowTextW(label, L"K4D Research Lab | Fortlaufende Forschungskette (Beweis → Basis) | DB: k4d_six_results.sqlite");
        int cSel = ComboBox_GetCurSel(chainModeCombo);
        SetWindowTextW(runButton, cSel == 1 ? L"Forschungskette starten" : L"K4D Forschung starten");
    } else if (mode == 1) {
        ShowWindow(chainModeCombo, SW_HIDE);
        ShowWindow(strategyCombo, SW_HIDE);
        ShowWindow(lineModeCombo, SW_HIDE);
        ShowWindow(chainFromButton, SW_HIDE);
        ShowWindow(stopButton, SW_HIDE);
        ShowWindow(reportButton, SW_HIDE);
        ShowWindow(lmDevButton, SW_HIDE);
        ShowWindow(countBox, SW_SHOW);
        ShowWindow(seedBox, SW_SHOW);
        ShowWindow(runButton, SW_SHOW);
        ShowWindow(searchBox, SW_SHOW);
        SetWindowTextW(runButton, L"Generieren + testen");
        SetWindowTextW(searchButton, L"Suchen / Laden");
        SetWindowTextW(label, L"Quelle: F:\\MathResearchDB_Backup_20261009\\research.sqlite (READ ONLY) | Quelldatenbank-Explorer");
    } else if (mode == 6) {
        ShowWindow(chainModeCombo, SW_HIDE);
        ShowWindow(strategyCombo, SW_HIDE);
        ShowWindow(lineModeCombo, SW_HIDE);
        ShowWindow(chainFromButton, SW_HIDE);
        ShowWindow(stopButton, SW_SHOW);
        ShowWindow(reportButton, SW_HIDE);
        ShowWindow(lmDevButton, SW_HIDE);
        ShowWindow(countBox, SW_SHOW);
        ShowWindow(seedBox, SW_SHOW);
        ShowWindow(runButton, SW_HIDE);
        ShowWindow(searchBox, SW_HIDE);
        SetWindowTextW(searchButton, L"Aktualisieren");
        SetWindowTextW(label, L"Autonome Algorithmus-Evolutions-Engine (MRE_0 → MRE_1 → ...) | DB: evolution_lineage.sqlite");
    } else {
        ShowWindow(chainModeCombo, SW_HIDE);
        ShowWindow(strategyCombo, SW_HIDE);
        ShowWindow(lineModeCombo, SW_HIDE);
        ShowWindow(chainFromButton, SW_HIDE);
        ShowWindow(stopButton, SW_HIDE);
        ShowWindow(reportButton, SW_HIDE);
        ShowWindow(lmDevButton, (mode == 2) ? SW_SHOW : SW_HIDE);
        ShowWindow(countBox, SW_SHOW);
        ShowWindow(seedBox, SW_SHOW);
        ShowWindow(runButton, SW_SHOW);
        ShowWindow(searchBox, SW_HIDE);
        SetWindowTextW(runButton, L"Generieren + testen");
        SetWindowTextW(searchButton, L"Aktualisieren");
        if(mode == 2) SetWindowTextW(label, L"C++ Algorithmen & Autonome Software-Entwicklung | DB: algorithm_lab_results.sqlite & software_memory.sqlite");
        else if(mode == 3) SetWindowTextW(label, L"C++ Editor & Kernel Test");
        else if(mode == 4) SetWindowTextW(label, L"Forschungsläufe Zusammenfassung");
        else SetWindowTextW(label, L"Experimentanalyse & Merkmale");
    }
    bool showDevControls = (mode == 0 || mode == 2 || mode == 6);
    ShowWindow(providerCombo, showDevControls ? SW_SHOW : SW_HIDE);
    ShowWindow(apiKeyLabel, showDevControls ? SW_SHOW : SW_HIDE);
    ShowWindow(apiKeyBox, showDevControls ? SW_SHOW : SW_HIDE);
    ShowWindow(modelLabel, showDevControls ? SW_SHOW : SW_HIDE);
    ShowWindow(modelBox, showDevControls ? SW_SHOW : SW_HIDE);
    ShowWindow(appTypeCombo, showDevControls ? SW_SHOW : SW_HIDE);
    ShowWindow(lmDevButton, (mode == 0 || mode == 2) ? SW_SHOW : SW_HIDE);
    ShowWindow(metaSynthButton, (mode == 0 || mode == 2) ? SW_SHOW : SW_HIDE);
    ShowWindow(saveButton, mode == 3 ? SW_SHOW : SW_HIDE);
    ShowWindow(testButton, mode == 3 ? SW_SHOW : SW_HIDE);
    ShowWindow(plotButton, (mode == 3 || mode == 5) ? SW_SHOW : SW_HIDE);

    bool showEvoControls = (mode == 6);
    int evoSubMode = (showEvoControls && evoSubModeCombo) ? ComboBox_GetCurSel(evoSubModeCombo) : 0;
    if (evoSubMode < 0) evoSubMode = 0;

    ShowWindow(evoSubModeCombo, showEvoControls ? SW_SHOW : SW_HIDE);

    // Single evolution controls
    bool showSingleEvo = (showEvoControls && evoSubMode == 0);
    ShowWindow(evoDumpBox, showSingleEvo ? SW_SHOW : SW_HIDE);
    ShowWindow(evoBrowseButton, showSingleEvo ? SW_SHOW : SW_HIDE);
    ShowWindow(evoCycleButton, showSingleEvo ? SW_SHOW : SW_HIDE);
    ShowWindow(evoContinuousButton, showSingleEvo ? SW_SHOW : SW_HIDE);

    // Meta-Fusion controls
    bool showFusion = (showEvoControls && evoSubMode == 1);
    ShowWindow(evoFamACombo, showFusion ? SW_SHOW : SW_HIDE);
    ShowWindow(evoFamBCombo, showFusion ? SW_SHOW : SW_HIDE);
    ShowWindow(evoTargetFamBox, showFusion ? SW_SHOW : SW_HIDE);
    ShowWindow(evoMetaFusionButton, showFusion ? SW_SHOW : SW_HIDE);

    ShowWindow(evoRollbackButton, showEvoControls ? SW_SHOW : SW_HIDE);
    ShowWindow(evoStatusButton, showEvoControls ? SW_SHOW : SW_HIDE);
}

void refresh(){
    currentFile.clear();
    updateControlVisibility();
    if(mode == 0){
        table(k4dResultsDb,
            "SELECT id AS ID, strategy AS Strategie, "
            "CASE WHEN details LIKE '%K4D%' OR mode='K4D_GUIDED' THEN 'K4D_GUIDED' ELSE 'CONTROL_NO_K' END AS Linie, "
            "status AS Status, "
            "chain_depth AS Tiefe, "
            "parent_id AS Eltern_ID, "
            "source_a AS Quelle_A, source_b AS Quelle_B, "
            "details AS Dynamik_Evolution, verification_duration_ms AS Dauer_ms "
            "FROM candidates ORDER BY id DESC LIMIT 250");
        showEditor(
            L"MRE K4D RESEARCH LAB | FORTLAUFENDE FORSCHUNGSKETTE\r\n"
            L"================================================================================\r\n"
            L"ARCHITEKTUR DER FORSCHUNGSKETTE:\r\n"
            L"  Hypothese  →  Lean 4 Beweis  →  Neue mathematische Basis  →  Nächste Hypothese\r\n\r\n"
            L"BEDIENUNG:\r\n"
            L"1. Modus wählen: 'Fortlaufende Forschungskette' oder 'Einzelne Zyklen'.\r\n"
            L"2. 'Forschungskette starten': Beginnt an der Wurzel und vertieft die Sätze\r\n"
            L"   Schritt für Schritt (Tiefe 1 → 2 → 3 ...).\r\n"
            L"3. 'Kette ab Auswahl vertiefen': Wählen Sie einen bestehenden bewiesenen Satz\r\n"
            L"   in der Tabelle aus und vertiefen Sie direkt dessen Beweiszweig!\r\n"
            L"4. Im Hypothesen-Inspektor sehen Sie den vollständigen Stammbaum jedes Satzes.\r\n",
            false);
    }
    else if(mode == 1){
        std::string term = narrow(value(searchBox));
        table(sourceDb, "SELECT rowid AS ID, file AS Moduldatei, name AS Deklaration, kind AS Typ, line AS Zeile FROM declarations WHERE name LIKE ?1 ORDER BY rowid LIMIT 250", "%" + term + "%");
        showEditor(L"Deklaration auswählen: hier erscheint ihre originale Lean-Aussage.\r\nDie Quelldatenbank bleibt schreibgeschützt.", false);
    }
    else if(mode == 2 || mode == 3){
        table(resultsDb, "SELECT id AS ID, operator_formula AS Formel, candidate_ns AS Kandidat_ns, baseline_ns AS Referenz_ns, equivalence AS Resultat, artifact_path AS Header FROM algorithm_lab ORDER BY id DESC LIMIT 250");
        showEditor(mode == 3 ? L"Einen Algorithmus auswählen, um seinen C++-Code hier zu bearbeiten." : L"Algorithmus auswählen, um Formel, Messung und C++-Code anzuzeigen.", mode == 3);
    }
    else if(mode == 4){
        table(resultsDb, "SELECT run_seed AS Seed, COUNT(*) AS Anzahl, MIN(candidate_ns) AS Min_ns, MAX(candidate_ns) AS Max_ns, AVG(candidate_ns) AS Mittel_ns, AVG(baseline_ns) AS Referenz_ns FROM algorithm_lab GROUP BY run_seed ORDER BY MAX(id) DESC LIMIT 250");
        showEditor(L"Messungen sind Momentaufnahmen. Referenzabweichungen sind keine korrekten Ersatzalgorithmen.\r\nFür aussagekräftige Performancevergleiche sind wiederholte Messungen notwendig.", false);
    }
    else if(mode == 5){
        table(resultsDb, "SELECT id, operator_formula AS Formel, candidate_distinct AS Unterschiedliche_Zustaende, changed_from_baseline AS Geaendert, equivalence AS Bewertung, candidate_checksum AS Pruefsumme FROM algorithm_lab ORDER BY id DESC LIMIT 250");
        showEditor(L"Analyse: Ausgabevielfalt, Prüfsumme, Abweichung und Laufzeit sind getrennte Kriterien.\r\nEs wird keine Neuheit oder formale Korrektheit behauptet.", false);
    }
    else if(mode == 6){
        table(evoLineageDb,
            "SELECT family_id AS Forschungsfamilie, ('MRE_' || gen_index) AS Generation, "
            "COALESCE(complexity_class, 'O(N)') AS Komplexitaet, "
            "COALESCE(algo_fingerprint, 'ALGO_UNKNOWN') AS Fingerabdruck, "
            "status AS Validierungsstatus, printf('%.4fx', speedup_ratio) AS Speedup, "
            "printf('%d/%d', cases_passed, cases_passed + cases_failed) AS Oracles_OK, "
            "hypothesis AS Mathematische_Hypothese, gen_id AS Generation_ID, created_at AS Zeitstempel "
            "FROM generations ORDER BY created_at DESC, gen_index DESC LIMIT 250");

        // Populate family combos if empty or refresh
        if (evoFamACombo && evoFamBCombo) {
            std::wstring curA = value(evoFamACombo);
            std::wstring curB = value(evoFamBCombo);
            ComboBox_ResetContent(evoFamACombo);
            ComboBox_ResetContent(evoFamBCombo);

            sqlite3* db = nullptr;
            if (sqlite3_open_v2(narrow(evoLineageDb).c_str(), &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK) {
                sqlite3_stmt* st = nullptr;
                if (sqlite3_prepare_v2(db, "SELECT family_id FROM research_families ORDER BY family_id ASC", -1, &st, nullptr) == SQLITE_OK) {
                    while (sqlite3_step(st) == SQLITE_ROW) {
                        std::wstring fid = wfrom(sqlite3_column_text(st, 0));
                        ComboBox_AddString(evoFamACombo, fid.c_str());
                        ComboBox_AddString(evoFamBCombo, fid.c_str());
                    }
                    sqlite3_finalize(st);
                }
                sqlite3_close(db);
            }

            int selA = ComboBox_FindStringExact(evoFamACombo, -1, curA.empty() ? L"TENSOR_ATTENTION" : curA.c_str());
            ComboBox_SetCurSel(evoFamACombo, selA >= 0 ? selA : 0);
            int selB = ComboBox_FindStringExact(evoFamBCombo, -1, curB.empty() ? L"LLM_ROPE_ROTARY" : curB.c_str());
            ComboBox_SetCurSel(evoFamBCombo, selB >= 0 ? selB : (ComboBox_GetCount(evoFamBCombo) > 1 ? 1 : 0));
        }

        showEditor(
            L"# MRE – AUTONOME EVOLUTION & META-FUSION (MRE_0 → MRE_1 → ...)\r\n"
            L"================================================================================\r\n"
            L"MODI:\r\n"
            L"  1. Einzelevolution: Entwickelt eine gewählte Forschungsfamilie entlang ihrer eigenen Baseline.\r\n"
            L"  2. Meta-Fusion: Kombiniert zwei mathematische Forschungsfamilien (z.B. TENSOR_ATTENTION x LLM_ROPE_ROTARY)\r\n"
            L"     zu einer völlig neuen, eigenständigen Forschungsfamilie (FUSION_...) mit eigener Referenz,\r\n"
            L"     unabhängigen Oracles und neuem mathematischem Stammbaum.\r\n\r\n"
            L"ARCHITEKTUR & WISSENSCHAFTLICHE PRINZIPIEN:\r\n"
            L"  - Produktionssystem (Baseline): Unveränderte aktive Referenz.\r\n"
            L"  - Forschungsraum (Mutation Lab): Mathematische Invarianten-Extraktion & Meta-Fusion.\r\n"
            L"  - Unantastbare Validierungsinstanz (Independent Oracle Vault):\r\n"
            L"    Kein experimenteller Kandidat darf Testfälle, Regeln oder Oracles verändern.\r\n"
            L"  - Vollständiger Herkunftsnachweis (Mathematical Lineage Trace):\r\n"
            L"    Jede Generation dokumentiert die 7 Stufen von Originalcode bis Prüfbericht.\r\n\r\n"
            L"BEDIENUNG:\r\n"
            L"  - Rechtsklick auf eine Tabellenzeile: Ordner im Explorer öffnen oder Generation löschen.\r\n"
            L"  - Klick auf eine Zeile: Vollständigen mathematischen Herkunftsnachweis anzeigen!\r\n",
            false);
    }
}

void showSelection(){
    if(mode == 0){
        showK4dSelection();
    }else if(mode == 1){
        auto id = pick(0);
        if(id.empty()) return;
        sqlite3* db = nullptr;
        if(sqlite3_open_v2(narrow(sourceDb).c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK){
            if(db) sqlite3_close(db);
            return;
        }
        sqlite3_stmt* st = nullptr;
        std::wstring body = L"";
        if(sqlite3_prepare_v2(db, "SELECT statement FROM declarations WHERE rowid=?1", -1, &st, nullptr) == SQLITE_OK){
            const std::string idText = narrow(id);
            sqlite3_bind_text(st, 1, idText.c_str(), static_cast<int>(idText.size()), nullptr);
            if(sqlite3_step(st) == SQLITE_ROW) body = wfrom(sqlite3_column_text(st, 0));
            sqlite3_finalize(st);
        }
        sqlite3_close(db);
        selectedSource = pick(1) + L" | " + pick(2) + L" | " + pick(3) + L" | " + pick(4);
        showEditor(L"Quelle: " + selectedSource + L"\r\n\r\nLean-Aussage:\r\n"+ body + L"\r\n\r\nHinweis: Die Auswahl kann den Experiment-Seed ableiten. Eine semantische mathematische Ableitung aus dem Satz findet noch nicht statt.", false);
    }else if(mode == 2 || mode == 3){
        auto file = pick(5);
        if(file.empty()) return;
        currentFile = file;
        std::wstring body = readText(fs::path(file));
        if(body.empty()){
            showEditor(L"Header nicht lesbar: " + file, false);
            return;
        }
        if(mode == 3) showEditor(body, true);
        else showEditor(L"Formel: " + pick(1) + L"\r\nKandidat: " + pick(2) + L" ns/Element | Referenz: " + pick(3) + L" ns/Element\r\nVergleich: " + pick(4) + L"\r\n\r\n" + body, false);
    }else if(mode == 5){
        showEditor(L"Formel: " + pick(1) + L"\r\nVerschiedene Zustände: " + pick(2) + L" / 4096\r\nReferenz abweichend: " + pick(3) + L"\r\nStatus: " + pick(4) + L"\r\nPrüfsumme: " + pick(5) + L"\r\n\r\nDiese Werte sind Merkmale, kein mathematischer Neuheitsbeweis.", false);
    }else if(mode == 6){
        auto famId = pick(0);
        auto genIdxStr = pick(1);
        auto genComplexity = pick(2);
        auto genFingerprint = pick(3);
        auto genStatus = pick(4);
        auto genSpeedup = pick(5);
        auto genOracles = pick(6);
        auto genHypothesis = pick(7);
        auto genId = pick(8);
        auto genTimestamp = pick(9);
        if(genIdxStr.empty()) return;

        // Parse index from "MRE_14" or "14"
        int genIdx = 0;
        try {
            if (genIdxStr.rfind(L"MRE_", 0) == 0) {
                genIdx = std::stoi(genIdxStr.substr(4));
            } else {
                genIdx = std::stoi(genIdxStr);
            }
        } catch(...) { genIdx = 0; }

        fs::path ws = fs::path(evoWorkspaceDir);
        fs::path famDir = (famId == L"AFFINE_RECURRENCE" || famId.empty()) ? ws : (ws / L"families" / famId);
        fs::path provPath;
        fs::path foundGuiExe;

        // Search directory matching gen_<genIdx>_*
        std::string prefix = "gen_" + std::to_string(genIdx) + "_";
        if (fs::exists(famDir / L"generations")) {
            for (const auto& entry : fs::directory_iterator(famDir / L"generations")) {
                if (entry.is_directory() && entry.path().filename().string().rfind(prefix, 0) == 0) {
                    provPath = entry.path() / L"MATHEMATICAL_PROVENANCE.md";
                    for (const auto& cand : {
                        entry.path() / L"KR_BUILD" / L"Release" / L"research_gui.exe",
                        entry.path() / L"KR_BUILD" / L"research_gui.exe",
                        entry.path() / L"build" / L"Release" / L"research_gui.exe",
                        entry.path() / L"build" / L"research_gui.exe"
                    }) {
                        if (fs::exists(cand)) { foundGuiExe = cand; break; }
                    }
                    break;
                }
            }
        }
        if(provPath.empty() || !fs::exists(provPath)){
            provPath = famDir / L"generations" / (L"gen_" + std::to_wstring(genIdx) + L"_" + genId) / L"MATHEMATICAL_PROVENANCE.md";
        }
        if(!fs::exists(provPath) && genIdx == 0){
            provPath = famDir / L"generations" / L"gen_0_baseline" / L"MATHEMATICAL_PROVENANCE.md";
        }
        if(!fs::exists(provPath)){
            provPath = famDir / L"baseline" / L"MATHEMATICAL_PROVENANCE.md";
        }
        if (foundGuiExe.empty() && fs::exists(famDir / L"baseline")) {
            for (const auto& cand : {
                famDir / L"baseline" / L"KR_BUILD" / L"Release" / L"research_gui.exe",
                famDir / L"baseline" / L"build" / L"Release" / L"research_gui.exe"
            }) {
                if (fs::exists(cand)) { foundGuiExe = cand; break; }
            }
        }

        std::wstring guiNotice;
        if (!foundGuiExe.empty()) {
            guiNotice = L"\r\n\r\n---\r\n### 🖥️ Native Win32-GUI Anwendung\r\n"
                        L"**Executable:** `" + foundGuiExe.wstring() + L"`\r\n"
                        L"**Projekt-Vertrag:** `application_type = win32_gui` (wWinMain & Message-Loop verifiziert)\r\n";
        }

        std::wstring markdown = readText(provPath);
        if(!markdown.empty()){
            showEditor(markdown + guiNotice, false);
            report(L"Herkunftsnachweis geladen: [" + famId + L"] MRE_" + std::to_wstring(genIdx) + L" [" + genComplexity + L"] (" + genId + L")");
        } else {
            showEditor(
                L"# MRE Generation " + genIdxStr + L" [" + famId + L"]\r\n\r\n" +
                L"**Forschungsfamilie:** " + famId + L"\r\n" +
                L"**ID:** " + genId + L"\r\n" +
                L"**Komplexitätsklasse:** " + genComplexity + L"\r\n" +
                L"**Algorithmus-Fingerabdruck:** `" + genFingerprint + L"`\r\n" +
                L"**Validierungs-Status:** " + genStatus + L"\r\n" +
                L"**Speedup-Faktor:** " + genSpeedup + L"\r\n" +
                L"**Oracle-Äquivalenz:** " + genOracles + L"\r\n" +
                L"**Mathematische Hypothese:** " + genHypothesis + L"\r\n" +
                L"**Zeitstempel:** " + genTimestamp + L"\r\n\r\n" +
                L"Die detaillierte Markdown-Akte liegt noch im Generierungs-Cache." + guiNotice, false);
        }
    }
}

void openSelectedFolder(){
    if(mode == 6){
        auto famId = pick(0);
        auto genIdxStr = pick(1);
        auto genId = pick(8);
        if(famId.empty() && genIdxStr.empty()){
            report(L"Keine Generation ausgewählt.");
            return;
        }
        int genIdx = 0;
        try {
            if (genIdxStr.rfind(L"MRE_", 0) == 0) {
                genIdx = std::stoi(genIdxStr.substr(4));
            } else {
                genIdx = std::stoi(genIdxStr);
            }
        } catch(...) { genIdx = 0; }

        fs::path ws = fs::path(evoWorkspaceDir);
        fs::path famDir = (famId == L"AFFINE_RECURRENCE" || famId.empty()) ? ws : (ws / L"families" / famId);
        fs::path folderToOpen;

        std::string prefix = "gen_" + std::to_string(genIdx) + "_";
        std::error_code ec;
        if(fs::exists(famDir / L"generations")){
            for(const auto& entry : fs::directory_iterator(famDir / L"generations", ec)){
                if(entry.is_directory() && entry.path().filename().string().rfind(prefix, 0) == 0){
                    folderToOpen = entry.path();
                    break;
                }
            }
        }
        if(folderToOpen.empty() && !genId.empty()){
            fs::path cand = famDir / L"generations" / (L"gen_" + std::to_wstring(genIdx) + L"_" + genId);
            if(fs::exists(cand)) folderToOpen = cand;
        }
        if(folderToOpen.empty() && fs::exists(famDir / L"mutation_lab")){
            for(const auto& entry : fs::directory_iterator(famDir / L"mutation_lab", ec)){
                if(entry.is_directory() && (entry.path().filename().string().rfind(prefix, 0) == 0 ||
                    (!genId.empty() && entry.path().filename().wstring().find(genId) != std::wstring::npos))){
                    folderToOpen = entry.path();
                    break;
                }
            }
        }
        if(folderToOpen.empty() && genIdx == 0){
            if(fs::exists(famDir / L"generations" / L"gen_0_baseline")){
                folderToOpen = famDir / L"generations" / L"gen_0_baseline";
            } else if(fs::exists(famDir / L"baseline")){
                folderToOpen = famDir / L"baseline";
            }
        }
        if(folderToOpen.empty()){
            if(fs::exists(famDir / L"generations")) folderToOpen = famDir / L"generations";
            else if(fs::exists(famDir)) folderToOpen = famDir;
            else folderToOpen = ws;
        }

        if(fs::exists(folderToOpen)){
            ShellExecuteW(nullptr, L"open", folderToOpen.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
            report(L"Ordner geöffnet: " + folderToOpen.wstring());
        } else {
            report(L"Ordner existiert nicht: " + folderToOpen.wstring());
        }
    } else if(mode == 2 || mode == 3){
        auto file = pick(5);
        if(!file.empty() && fs::exists(fs::path(file))){
            fs::path p = fs::path(file).parent_path();
            ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
            report(L"Ordner geöffnet: " + p.wstring());
        } else if(fs::exists(fs::path(kernelDir))){
            ShellExecuteW(nullptr, L"open", kernelDir.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
            report(L"Ordner geöffnet: " + kernelDir);
        }
    } else if(mode == 0){
        if(fs::exists(fs::path(k4dLeanDir))){
            ShellExecuteW(nullptr, L"open", k4dLeanDir.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
            report(L"Ordner geöffnet: " + k4dLeanDir);
        }
    }
}

void deleteSelectedGeneration(){
    if(mode == 6){
        auto famId = pick(0);
        auto genIdxStr = pick(1);
        auto genId = pick(8);
        if(famId.empty() && genIdxStr.empty()){
            report(L"Keine Generation ausgewählt.");
            return;
        }

        int genIdx = 0;
        try {
            if (genIdxStr.rfind(L"MRE_", 0) == 0) {
                genIdx = std::stoi(genIdxStr.substr(4));
            } else {
                genIdx = std::stoi(genIdxStr);
            }
        } catch(...) { genIdx = 0; }

        std::wstring confirmMsg = L"Möchten Sie die Generation " + genIdxStr +
            L" der Forschungsfamilie [" + famId + L"] (" + genId + L") wirklich löschen?\r\n\r\n"
            L"• Zugehörige Ordner und Dateien werden von der Festplatte gelöscht.\r\n"
            L"• Die Generation wird aus der Datenbank (evolution_lineage.sqlite) entfernt.\r\n"
            L"• Der Stammbaum wird atomar aktualisiert.";

        if(MessageBoxW(windowHandle, confirmMsg.c_str(), L"Generation löschen", MB_YESNO | MB_ICONWARNING) != IDYES){
            return;
        }

        fs::path ws = fs::path(evoWorkspaceDir);
        fs::path famDir = (famId == L"AFFINE_RECURRENCE" || famId.empty()) ? ws : (ws / L"families" / famId);
        std::error_code ec;

        // 1. Delete generation folders from disk
        std::string prefix = "gen_" + std::to_string(genIdx) + "_";
        std::vector<fs::path> dirsToDelete;

        if(fs::exists(famDir / L"generations")){
            for(const auto& entry : fs::directory_iterator(famDir / L"generations", ec)){
                if(entry.is_directory() && entry.path().filename().string().rfind(prefix, 0) == 0){
                    dirsToDelete.push_back(entry.path());
                }
            }
        }
        if(fs::exists(famDir / L"mutation_lab")){
            for(const auto& entry : fs::directory_iterator(famDir / L"mutation_lab", ec)){
                if(entry.is_directory() && (entry.path().filename().string().rfind(prefix, 0) == 0 ||
                    (!genId.empty() && entry.path().filename().wstring().find(genId) != std::wstring::npos))){
                    dirsToDelete.push_back(entry.path());
                }
            }
        }
        if(genIdx == 0){
            if(fs::exists(famDir / L"generations" / L"gen_0_baseline")){
                dirsToDelete.push_back(famDir / L"generations" / L"gen_0_baseline");
            }
        }

        for(const auto& d : dirsToDelete){
            fs::remove_all(d, ec);
        }

        // 2. Clean SQLite database records
        sqlite3* db = nullptr;
        if(sqlite3_open_v2(narrow(evoLineageDb).c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK){
            sqlite3_busy_timeout(db, 3000);

            // Delete from generations
            {
                sqlite3_stmt* st = nullptr;
                const char* sql = "DELETE FROM generations WHERE family_id=?1 AND (gen_index=?2 OR gen_id=?3)";
                if(sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK){
                    std::string fStr = narrow(famId);
                    std::string gIdStr = narrow(genId);
                    sqlite3_bind_text(st, 1, fStr.c_str(), static_cast<int>(fStr.size()), nullptr);
                    sqlite3_bind_int(st, 2, genIdx);
                    sqlite3_bind_text(st, 3, gIdStr.c_str(), static_cast<int>(gIdStr.size()), nullptr);
                    sqlite3_step(st);
                    sqlite3_finalize(st);
                }
            }

            // Delete from mathematical_provenance
            {
                sqlite3_stmt* st = nullptr;
                const char* sql = "DELETE FROM mathematical_provenance WHERE family_id=?1 AND (gen_index=?2 OR gen_id=?3)";
                if(sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK){
                    std::string fStr = narrow(famId);
                    std::string gIdStr = narrow(genId);
                    sqlite3_bind_text(st, 1, fStr.c_str(), static_cast<int>(fStr.size()), nullptr);
                    sqlite3_bind_int(st, 2, genIdx);
                    sqlite3_bind_text(st, 3, gIdStr.c_str(), static_cast<int>(gIdStr.size()), nullptr);
                    sqlite3_step(st);
                    sqlite3_finalize(st);
                }
            }

            // Delete from failure_memory
            {
                sqlite3_stmt* st = nullptr;
                const char* sql = "DELETE FROM failure_memory WHERE family_id=?1 AND gen_index=?2";
                if(sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK){
                    std::string fStr = narrow(famId);
                    sqlite3_bind_text(st, 1, fStr.c_str(), static_cast<int>(fStr.size()), nullptr);
                    sqlite3_bind_int(st, 2, genIdx);
                    sqlite3_step(st);
                    sqlite3_finalize(st);
                }
            }

            // Check remaining generations for this family
            int maxGen = -1;
            {
                sqlite3_stmt* st = nullptr;
                const char* sql = "SELECT COALESCE(MAX(gen_index), -1) FROM generations WHERE family_id=?1";
                if(sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK){
                    std::string fStr = narrow(famId);
                    sqlite3_bind_text(st, 1, fStr.c_str(), static_cast<int>(fStr.size()), nullptr);
                    if(sqlite3_step(st) == SQLITE_ROW){
                        maxGen = sqlite3_column_int(st, 0);
                    }
                    sqlite3_finalize(st);
                }
            }

            if(maxGen >= 0){
                // Ensure highest remaining generation is marked PROMOTED
                {
                    sqlite3_stmt* st = nullptr;
                    const char* sql = "UPDATE generations SET status='PROMOTED' WHERE family_id=?1 AND gen_index=?2";
                    if(sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK){
                        std::string fStr = narrow(famId);
                        sqlite3_bind_text(st, 1, fStr.c_str(), static_cast<int>(fStr.size()), nullptr);
                        sqlite3_bind_int(st, 2, maxGen);
                        sqlite3_step(st);
                        sqlite3_finalize(st);
                    }
                }
                // Update research_families active_gen_index
                {
                    sqlite3_stmt* st = nullptr;
                    const char* sql = "UPDATE research_families SET active_gen_index=?1 WHERE family_id=?2";
                    if(sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK){
                        std::string fStr = narrow(famId);
                        sqlite3_bind_int(st, 1, maxGen);
                        sqlite3_bind_text(st, 2, fStr.c_str(), static_cast<int>(fStr.size()), nullptr);
                        sqlite3_step(st);
                        sqlite3_finalize(st);
                    }
                }
                // Sync surviving generation files to baseline dir
                fs::path survGenDir;
                std::string sPrefix = "gen_" + std::to_string(maxGen) + "_";
                if(fs::exists(famDir / L"generations")){
                    for(const auto& entry : fs::directory_iterator(famDir / L"generations", ec)){
                        if(entry.is_directory() && (entry.path().filename().string().rfind(sPrefix, 0) == 0 ||
                            (maxGen == 0 && entry.path().filename() == "gen_0_baseline"))){
                            survGenDir = entry.path();
                            break;
                        }
                    }
                }
                if(!survGenDir.empty() && fs::exists(famDir / L"baseline")){
                    for(const auto& f : {L"main.cpp", L"CMakeLists.txt", L"MATHEMATICAL_PROVENANCE.md", L"gui_main.cpp"}){
                        if(fs::exists(survGenDir / f)){
                            fs::copy_file(survGenDir / f, famDir / L"baseline" / f, fs::copy_options::overwrite_existing, ec);
                        }
                    }
                }
            } else {
                // No generations left for this family
                if(famId != L"AFFINE_RECURRENCE"){
                    sqlite3_stmt* st = nullptr;
                    const char* sql = "DELETE FROM research_families WHERE family_id=?1";
                    if(sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK){
                        std::string fStr = narrow(famId);
                        sqlite3_bind_text(st, 1, fStr.c_str(), static_cast<int>(fStr.size()), nullptr);
                        sqlite3_step(st);
                        sqlite3_finalize(st);
                    }
                    fs::remove_all(famDir, ec);
                } else {
                    sqlite3_stmt* st = nullptr;
                    const char* sql = "UPDATE research_families SET active_gen_index=0 WHERE family_id='AFFINE_RECURRENCE'";
                    if(sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK){
                        sqlite3_step(st);
                        sqlite3_finalize(st);
                    }
                }
            }

            sqlite3_close(db);
        }

        report(L"Generation " + genIdxStr + L" [" + famId + L"] gelöscht.");
        refresh();
    } else if(mode == 2 || mode == 3){
        auto id = pick(0);
        auto file = pick(5);
        if(id.empty()) return;
        if(MessageBoxW(windowHandle, (L"Möchten Sie den Algorithmus #" + id + L" wirklich löschen?").c_str(), L"Algorithmus löschen", MB_YESNO | MB_ICONWARNING) != IDYES){
            return;
        }
        std::error_code ec;
        if(!file.empty() && fs::exists(fs::path(file))){
            fs::remove(fs::path(file), ec);
        }
        sqlite3* db = nullptr;
        if(sqlite3_open_v2(narrow(resultsDb).c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK){
            sqlite3_stmt* st = nullptr;
            std::string sql = "DELETE FROM algorithm_lab WHERE id = " + narrow(id);
            if(sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) == SQLITE_OK){
                sqlite3_step(st);
                sqlite3_finalize(st);
            }
            sqlite3_close(db);
        }
        report(L"Algorithmus #" + id + L" gelöscht.");
        refresh();
    } else if(mode == 0){
        auto cid = pick(0);
        if(cid.empty()) return;
        if(MessageBoxW(windowHandle, (L"Möchten Sie den Satz #" + cid + L" wirklich löschen?").c_str(), L"Theorem löschen", MB_YESNO | MB_ICONWARNING) != IDYES){
            return;
        }
        std::error_code ec;
        fs::path lFile = fs::path(k4dLeanDir) / (L"Candidate_" + cid + L".lean");
        if(fs::exists(lFile)) fs::remove(lFile, ec);
        sqlite3* db = nullptr;
        if(sqlite3_open_v2(narrow(k4dResultsDb).c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK){
            sqlite3_stmt* st = nullptr;
            std::string sql = "DELETE FROM candidates WHERE id = " + narrow(cid);
            if(sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) == SQLITE_OK){
                sqlite3_step(st);
                sqlite3_finalize(st);
            }
            sqlite3_close(db);
        }
        report(L"Kandidat #" + cid + L" gelöscht.");
        refresh();
    }
}

void showContextMenuForSelection(HWND parent, POINT pt){
    int sel = ListView_GetNextItem(rows, -1, LVNI_SELECTED);
    if(sel < 0) return;

    HMENU hMenu = CreatePopupMenu();
    if(!hMenu) return;

    AppendMenuW(hMenu, MF_STRING, ID_CTX_OPEN_DIR, L"Ordner öffnen");
    AppendMenuW(hMenu, MF_STRING, ID_CTX_DELETE, L"Löschen");

    SetForegroundWindow(parent);
    int cmd = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, parent, nullptr);
    DestroyMenu(hMenu);

    if(cmd == ID_CTX_OPEN_DIR){
        openSelectedFolder();
    } else if(cmd == ID_CTX_DELETE){
        deleteSelectedGeneration();
    }
}

uint64_t seedFromSource(const std::wstring& source, uint64_t base){
    uint64_t h = 14695981039346656037ULL;
    auto s = narrow(source);
    for(unsigned char c : s){
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h ^ base;
}

void launch(const std::wstring& command, const std::wstring& program){
    if(job.hProcess){
        report(L"Experiment läuft bereits.");
        return;
    }
    std::vector<wchar_t> cmd(command.begin(), command.end());
    cmd.push_back(L'\0');

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    fs::path logPath = fs::path(evoWorkspaceDir) / L"last_run.log";
    HANDLE hLog = CreateFileW(logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    if(hLog != INVALID_HANDLE_VALUE){
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdOutput = hLog;
        si.hStdError = hLog;
    }

    if(!CreateProcessW(program.c_str(), cmd.data(), nullptr, nullptr, (hLog != INVALID_HANDLE_VALUE ? TRUE : FALSE), CREATE_NO_WINDOW, nullptr, nullptr, &si, &job)){
        if(hLog != INVALID_HANDLE_VALUE) CloseHandle(hLog);
        kernelTestRunning = false;
        report(L"Prozessstart fehlgeschlagen: " + std::to_wstring(GetLastError()));
        return;
    }
    if(hLog != INVALID_HANDLE_VALUE) CloseHandle(hLog);
    EnableWindow(runButton, FALSE);
    EnableWindow(chainFromButton, FALSE);
    EnableWindow(lmDevButton, FALSE);
    EnableWindow(metaSynthButton, FALSE);
    EnableWindow(testButton, FALSE);
    EnableWindow(evoCycleButton, FALSE);
    EnableWindow(evoContinuousButton, FALSE);
    EnableWindow(evoRollbackButton, FALSE);
    EnableWindow(evoStatusButton, FALSE);
    if(mode == 0 || mode == 6) EnableWindow(stopButton, TRUE);
    report(L"Experiment läuft im Hintergrund ...");
    SetTimer(windowHandle, 1, 300, nullptr);
}

void stopJob(){
    if(job.hProcess){
        TerminateProcess(job.hProcess, 1);
        CloseHandle(job.hProcess);
        CloseHandle(job.hThread);
        job = {};
        KillTimer(windowHandle, 1);
        EnableWindow(runButton, TRUE);
        EnableWindow(chainFromButton, TRUE);
        EnableWindow(lmDevButton, TRUE);
        EnableWindow(metaSynthButton, TRUE);
        EnableWindow(testButton, TRUE);
        EnableWindow(evoCycleButton, TRUE);
        EnableWindow(evoContinuousButton, TRUE);
        EnableWindow(evoRollbackButton, TRUE);
        EnableWindow(evoStatusButton, TRUE);
        EnableWindow(stopButton, FALSE);
        report(L"Forschungslauf manuell angehalten.");
    }
}

void chainFromSelection(){
    auto idStr = pick(0);
    if(idStr.empty()){
        report(L"Bitte zuerst einen Satz in der Tabelle auswählen.");
        return;
    }
    int64_t candId = 0;
    try { candId = std::stoll(idStr); } catch(...) { return; }

    int maxDepth = 4;
    uint64_t seed = 42;
    try {
        maxDepth = std::stoi(value(countBox));
        seed = std::stoull(value(seedBox));
    } catch (...) {}
    if (maxDepth < 1) maxDepth = 4;

    int stratIdx = ComboBox_GetCurSel(strategyCombo);
    if (stratIdx < 0) stratIdx = 0;
    const wchar_t* strats[] = {
        L"all", L"type_synthesis", L"systematic", L"random",
        L"cross_domain", L"mutation", L"iterative"
    };
    std::wstring stratName = strats[stratIdx];
    if (stratName == L"all") stratName = L"random";

    int modeIdx = ComboBox_GetCurSel(lineModeCombo);
    bool k4dGuided = (modeIdx != 2);

    prepareLeanEnvironment();

    std::wstring cmd = L"\"" + engine + L"\" chain \"" +
                       sourceDb + L"\" \"" +
                       k4dResultsDb + L"\" \"" +
                       k4dLeanDir + L"\" " +
                       std::to_wstring(maxDepth) + L" " +
                       std::to_wstring(seed) + L" " +
                       stratName + L" " +
                       std::to_wstring(candId) + L" \"" +
                       leanBin + L"\" " +
                       (k4dGuided ? L"1" : L"0");

    launch(cmd, engine);
    report(L"Forschungskette gestartet: Vertiefe ab Satz #" + std::to_wstring(candId) + L" (Zieltiefe: " + std::to_wstring(maxDepth) + L") ...");
}

void developSoftwareFromSelection(){
    auto idStr = pick(0);
    int64_t candId = 0;
    if(!idStr.empty()){
        try { candId = std::stoll(idStr); } catch(...) { candId = 0; }
    }

    uint64_t seed = 42;
    try {
        seed = std::stoull(value(seedBox));
    } catch (...) {}

    // Check selected provider: 0 = LM Studio (Lokal), 1 = Google Gemini (Cloud API)
    int provIdx = ComboBox_GetCurSel(providerCombo);
    if (provIdx < 0) provIdx = 0;
    bool isGemini = (provIdx == 1);

    std::wstring chosenModel = value(modelBox);
    while(!chosenModel.empty() && chosenModel.front() == L' ') chosenModel.erase(chosenModel.begin());
    while(!chosenModel.empty() && chosenModel.back() == L' ') chosenModel.pop_back();

    std::wstring endpointUrl;
    if (isGemini) {
        // Read API key from RAM box into volatile memory
        ramGeminiApiKey = value(apiKeyBox);
        while(!ramGeminiApiKey.empty() && ramGeminiApiKey.front() == L' ') ramGeminiApiKey.erase(ramGeminiApiKey.begin());
        while(!ramGeminiApiKey.empty() && ramGeminiApiKey.back() == L' ') ramGeminiApiKey.pop_back();

        if (ramGeminiApiKey.empty()) {
            MessageBoxW(windowHandle, 
                L"Gemini API-Key fehlt!\r\n\r\n"
                L"Bitte geben Sie Ihren Google Gemini API-Key in das obere Feld ein.\r\n"
                L"Der Key verbleibt ausschließlich im Arbeitsspeicher (RAM) der IDE und wird niemals auf die Festplatte gespeichert.",
                L"Gemini API-Key erforderlich", MB_ICONWARNING);
            report(L"Abgebrochen: Gemini API-Key im RAM fehlt.");
            return;
        }

        // Pass API key via process environment block in volatile memory for the duration of the child process
        SetEnvironmentVariableW(L"GEMINI_API_KEY", ramGeminiApiKey.c_str());

        endpointUrl = L"https://generativelanguage.googleapis.com/v1beta/openai/chat/completions";
        if (chosenModel.empty() || chosenModel == L"auto") {
            chosenModel = L"gemini-2.5-flash";
        }
    } else {
        // LM Studio Local
        endpointUrl = L"http://127.0.0.1:1234/v1/chat/completions";
        if (chosenModel.empty()) {
            chosenModel = L"auto";
        }
    }

    fs::path memoryDb = fs::path(projectRoot) / L"software_memory.sqlite";
    fs::path workspace = fs::path(projectRoot) / L"software_experiments";

    std::wstring cmd = L"\"" + engine + L"\" software-research \"" +
                       memoryDb.wstring() + L"\" \"" +
                       workspace.wstring() + L"\" 1 " +
                       std::to_wstring(seed) + L" \"" +
                       endpointUrl + L"\" \"" +
                       chosenModel + L"\" --require-lm --candidates-db \"" +
                       k4dResultsDb + L"\"";

    if (candId > 0) {
        cmd += L" --candidate-id " + std::to_wstring(candId);
    }
    cmd += getSelectedAppTypeArg();

    launch(cmd, engine);

    if (isGemini) {
        if (candId > 0) {
            report(L"Google Gemini (" + chosenModel + L") beauftragt: C++20 Implementierung für Theorem #" + std::to_wstring(candId) + L" ...");
        } else {
            report(L"Google Gemini (" + chosenModel + L") beauftragt: C++20 Implementierung für Theorem ...");
        }
    } else {
        if (candId > 0) {
            report(L"LM Studio (" + chosenModel + L") beauftragt: C++20 Implementierung für Theorem #" + std::to_wstring(candId) + L" ...");
        } else {
            report(L"LM Studio (" + chosenModel + L") beauftragt: C++20 Implementierung für Theorem ...");
        }
    }
}

void synthesizeAllSoftware(){
    uint64_t seed = 42;
    try {
        seed = std::stoull(value(seedBox));
    } catch (...) {}

    int provIdx = ComboBox_GetCurSel(providerCombo);
    if (provIdx < 0) provIdx = 0;
    bool isGemini = (provIdx == 1);

    std::wstring chosenModel = value(modelBox);
    while(!chosenModel.empty() && chosenModel.front() == L' ') chosenModel.erase(chosenModel.begin());
    while(!chosenModel.empty() && chosenModel.back() == L' ') chosenModel.pop_back();

    std::wstring endpointUrl;
    if (isGemini) {
        ramGeminiApiKey = value(apiKeyBox);
        while(!ramGeminiApiKey.empty() && ramGeminiApiKey.front() == L' ') ramGeminiApiKey.erase(ramGeminiApiKey.begin());
        while(!ramGeminiApiKey.empty() && ramGeminiApiKey.back() == L' ') ramGeminiApiKey.pop_back();

        if (ramGeminiApiKey.empty()) {
            MessageBoxW(windowHandle, 
                L"Gemini API-Key fehlt!\r\n\r\n"
                L"Bitte geben Sie Ihren Google Gemini API-Key in das obere Feld ein.\r\n"
                L"Der Key verbleibt ausschließlich im Arbeitsspeicher (RAM) der IDE und wird niemals auf die Festplatte gespeichert.",
                L"Gemini API-Key erforderlich", MB_ICONWARNING);
            report(L"Abgebrochen: Gemini API-Key im RAM fehlt.");
            return;
        }

        SetEnvironmentVariableW(L"GEMINI_API_KEY", ramGeminiApiKey.c_str());

        endpointUrl = L"https://generativelanguage.googleapis.com/v1beta/openai/chat/completions";
        if (chosenModel.empty() || chosenModel == L"auto") {
            chosenModel = L"gemini-2.5-flash";
        }
    } else {
        endpointUrl = L"http://127.0.0.1:1234/v1/chat/completions";
        if (chosenModel.empty()) {
            chosenModel = L"auto";
        }
    }

    fs::path memoryDb = fs::path(projectRoot) / L"software_memory.sqlite";
    fs::path workspace = fs::path(projectRoot) / L"software_experiments";

    std::wstring cmd = L"\"" + engine + L"\" software-meta-synthesis \"" +
                       memoryDb.wstring() + L"\" \"" +
                       workspace.wstring() + L"\" 1 " +
                       std::to_wstring(seed) + L" \"" +
                       endpointUrl + L"\" \"" +
                       chosenModel + L"\" --require-lm" +
                       getSelectedAppTypeArg();

    launch(cmd, engine);

    if (isGemini) {
        report(L"Google Gemini (" + chosenModel + L") beauftragt: Meta-Synthese aller Algorithmen & Speicherung des Voll-Prompts ...");
    } else {
        report(L"LM Studio (" + chosenModel + L") beauftragt: Meta-Synthese aller Algorithmen & Speicherung des Voll-Prompts ...");
    }
}

void browseDumpFolder(){
    BROWSEINFOW bi{};
    bi.hwndOwner = windowHandle;
    bi.lpszTitle = L"Projekt-Ordner oder Code-Dump für Autonome Evolution auswählen:";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if(pidl){
        wchar_t path[MAX_PATH]{};
        if(SHGetPathFromIDListW(pidl, path)){
            SetWindowTextW(evoDumpBox, path);
            report(L"Code-Dump Ordner gewählt: " + std::wstring(path));
        }
        CoTaskMemFree(pidl);
    }
}

void runEvolutionCycleUI(bool continuous){
    std::wstring dumpPath = value(evoDumpBox);
    while(!dumpPath.empty() && dumpPath.front() == L' ') dumpPath.erase(dumpPath.begin());
    while(!dumpPath.empty() && dumpPath.back() == L' ') dumpPath.pop_back();

    if(dumpPath.empty()){
        MessageBoxW(windowHandle,
            L"Bitte geben Sie einen Projekt-Ordner oder Code-Dump an.\r\n\r\n"
            L"Die Autonome Evolution Engine analysiert den fremden Code, extrahiert die mathematischen Invarianten "
            L"und entwickelt daraus eine neue Generation (MRE_n).",
            L"Code-Dump erforderlich", MB_ICONINFORMATION);
        report(L"Abgebrochen: Kein Code-Dump angegeben.");
        return;
    }

    uint64_t seed = 42;
    int rounds = 1;
    try {
        rounds = std::stoi(value(countBox));
        seed = std::stoull(value(seedBox));
    } catch (...) {}
    if (!continuous) rounds = 1;
    if (rounds < 1) rounds = 1;

    int provIdx = ComboBox_GetCurSel(providerCombo);
    if (provIdx < 0) provIdx = 0;
    bool isGemini = (provIdx == 1);

    std::wstring chosenModel = value(modelBox);
    while(!chosenModel.empty() && chosenModel.front() == L' ') chosenModel.erase(chosenModel.begin());
    while(!chosenModel.empty() && chosenModel.back() == L' ') chosenModel.pop_back();

    std::wstring endpointUrl;
    if (isGemini) {
        ramGeminiApiKey = value(apiKeyBox);
        while(!ramGeminiApiKey.empty() && ramGeminiApiKey.front() == L' ') ramGeminiApiKey.erase(ramGeminiApiKey.begin());
        while(!ramGeminiApiKey.empty() && ramGeminiApiKey.back() == L' ') ramGeminiApiKey.pop_back();

        if (ramGeminiApiKey.empty()) {
            MessageBoxW(windowHandle, 
                L"Gemini API-Key fehlt!\r\n\r\n"
                L"Bitte geben Sie Ihren Google Gemini API-Key in das obere Feld ein.\r\n"
                L"Der Key verbleibt ausschließlich im Arbeitsspeicher (RAM) der IDE und wird niemals auf die Festplatte gespeichert.",
                L"Gemini API-Key erforderlich", MB_ICONWARNING);
            report(L"Abgebrochen: Gemini API-Key im RAM fehlt.");
            return;
        }

        SetEnvironmentVariableW(L"GEMINI_API_KEY", ramGeminiApiKey.c_str());
        endpointUrl = L"https://generativelanguage.googleapis.com/v1beta/openai/chat/completions";
        if (chosenModel.empty() || chosenModel == L"auto") {
            chosenModel = L"gemini-2.5-flash";
        }
    } else {
        endpointUrl = L"http://127.0.0.1:1234/v1/chat/completions";
        if (chosenModel.empty()) {
            chosenModel = L"auto";
        }
    }

    std::wstring cmd = L"\"" + engine + L"\" evolution-run \"" +
                       dumpPath + L"\" \"" +
                       evoWorkspaceDir + L"\" " +
                       std::to_wstring(seed) + L" \"" +
                       endpointUrl + L"\" \"" +
                       chosenModel + L"\" --depth=" +
                       std::to_wstring(rounds) +
                       (continuous ? L" --max-cycles=10" : L" --max-cycles=1") +
                       getSelectedAppTypeArg();

    launch(cmd, engine);
    report(L"Autonome Evolution gestartet (Evolutionstiefe " + std::to_wstring(rounds) + L" pro Zyklus) für: " + dumpPath);
}

void runMetaFusionUI(){
    std::wstring famA = value(evoFamACombo);
    std::wstring famB = value(evoFamBCombo);
    std::wstring targetFam = value(evoTargetFamBox);
    while(!targetFam.empty() && targetFam.front() == L' ') targetFam.erase(targetFam.begin());
    while(!targetFam.empty() && targetFam.back() == L' ') targetFam.pop_back();

    if (famA.empty() || famB.empty()) {
        MessageBoxW(windowHandle, L"Bitte wählen Sie zwei Forschungsfamilien für die Meta-Fusion aus.", L"Familienauswahl erforderlich", MB_ICONINFORMATION);
        return;
    }
    if (famA == famB) {
        MessageBoxW(windowHandle, L"Für eine Meta-Fusion müssen zwei unterschiedliche Forschungsfamilien ausgewählt werden.", L"Ungültige Kombination", MB_ICONWARNING);
        return;
    }
    if (targetFam.empty()) {
        targetFam = L"FUSION_" + famA + L"_" + famB;
    }

    uint64_t seed = 42;
    int depth = 10;
    try {
        depth = std::stoi(value(countBox));
        seed = std::stoull(value(seedBox));
    } catch (...) {}
    if (depth < 1) depth = 10;

    std::wstring confirmMsg = L"Autonomen Meta-Fusions-Forschungsauftrag starten?\r\n\r\n"
                              L"Familie A: [" + famA + L"]\r\n"
                              L"Familie B: [" + famB + L"]\r\n\r\n"
                              L"Neue Ziel-Forschungsfamilie: [" + targetFam + L"]\r\n"
                              L"Start-Seed: " + std::to_wstring(seed) + L"\r\n"
                              L"Evolutionstiefe: " + std::to_wstring(depth) + L" Generationen pro Zyklus\r\n\r\n"
                              L"MRE forscht zyklisch in " + std::to_wstring(depth) + L"er-Blöcken mit deterministisch abgeleiteten Teil-Seeds, "
                              L"bis eine mathematisch gültige Fusion beider Elternfamilien nachgewiesen ist.";
    if (MessageBoxW(windowHandle, confirmMsg.c_str(), L"Meta-Fusion bestätigen", MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return;
    }

    std::wstring cmd = L"\"" + engine + L"\" evolution-fusion \"" +
                       famA + L"\" \"" +
                       famB + L"\" \"" +
                       evoWorkspaceDir + L"\" \"" +
                       targetFam + L"\" --depth=" +
                       std::to_wstring(depth) + L" --seed=" +
                       std::to_wstring(seed) + L" --max-cycles=10" +
                       getSelectedAppTypeArg();

    launch(cmd, engine);
    report(L"Meta-Fusionsauftrag gestartet: [" + famA + L"] x [" + famB + L"] -> [" + targetFam + L"] (Seed: " + std::to_wstring(seed) + L", Tiefe: " + std::to_wstring(depth) + L") ...");
}

void rollbackEvolutionUI(){
    auto famId = pick(0);
    auto idStr = pick(1);
    int targetGen = -1;
    if(!idStr.empty()){
        try {
            if (idStr.rfind(L"MRE_", 0) == 0) idStr = idStr.substr(4);
            targetGen = std::stoi(idStr);
        } catch(...) { targetGen = -1; }
    }

    std::wstring confirmMsg = (targetGen >= 0) 
        ? (L"Möchten Sie Familie [" + famId + L"] wirklich auf Generation MRE_" + std::to_wstring(targetGen) + L" zurücksetzen?")
        : L"Möchten Sie das System auf die vorherige Generation zurücksetzen?";

    if(MessageBoxW(windowHandle, confirmMsg.c_str(), L"Rollback bestätigen", MB_YESNO | MB_ICONQUESTION) != IDYES){
        return;
    }

    std::wstring famArg = famId.empty() ? L"" : (L" --family=" + famId);
    std::wstring cmd = L"\"" + engine + L"\" evolution-rollback \"" +
                       evoWorkspaceDir + L"\"" +
                       (targetGen >= 0 ? (L" " + std::to_wstring(targetGen)) : L"") +
                       famArg;

    launch(cmd, engine);
    report(L"Rollback wird durchgeführt ...");
}

void reportEvolutionStatusUI(){
    std::wstring cmd = L"\"" + engine + L"\" evolution-status \"" + evoWorkspaceDir + L"\"";
    launch(cmd, engine);
    report(L"Evolutionsstatus wird ermittelt ...");
}

void run(){
    int count = 0;
    uint64_t seed = 0;
    try {
        count = std::stoi(value(countBox));
        seed = std::stoull(value(seedBox));
    } catch (...) {
        report(L"Ungültige Anzahl oder Seed.");
        return;
    }
    if (count < 1 || count > 10000) {
        report(L"Anzahl muss 1..10000 sein.");
        return;
    }

    if (mode == 0) {
        int chainMode = ComboBox_GetCurSel(chainModeCombo);
        int stratIdx = ComboBox_GetCurSel(strategyCombo);
        if (stratIdx < 0) stratIdx = 0;
        const wchar_t* strats[] = {
            L"all", L"type_synthesis", L"systematic", L"random",
            L"cross_domain", L"mutation", L"iterative"
        };
        std::wstring stratName = strats[stratIdx];

        int modeIdx = ComboBox_GetCurSel(lineModeCombo);
        if (modeIdx < 0) modeIdx = 0;
        const wchar_t* modes[] = { L"both", L"k4d", L"control" };
        std::wstring lineMode = modes[modeIdx];
        bool k4dGuided = (modeIdx != 2);

        prepareLeanEnvironment();

        if (chainMode == 1) {
            // Continuous Research Chain Mode!
            if (stratName == L"all") stratName = L"random";
            std::wstring cmd = L"\"" + engine + L"\" chain \"" +
                               sourceDb + L"\" \"" +
                               k4dResultsDb + L"\" \"" +
                               k4dLeanDir + L"\" " +
                               std::to_wstring(count) + L" " +
                               std::to_wstring(seed) + L" " +
                               stratName + L" 0 \"" +
                               leanBin + L"\" " +
                               (k4dGuided ? L"1" : L"0");

            launch(cmd, engine);
            report(L"Fortlaufende Forschungskette gestartet (Zieltiefe: " + std::to_wstring(count) + L") ...");
            return;
        }

        if (chainMode == 2) {
            // Branching Research Network Mode (Verzweigtes Beweisnetz)
            if (stratName == L"all") stratName = L"random";
            int branchingFactor = 2; // Default binary branching
            std::wstring cmd = L"\"" + engine + L"\" network \"" +
                               sourceDb + L"\" \"" +
                               k4dResultsDb + L"\" \"" +
                               k4dLeanDir + L"\" " +
                               std::to_wstring(count) + L" " +
                               std::to_wstring(seed) + L" " +
                               stratName + L" " +
                               std::to_wstring(branchingFactor) + L" 0 \"" +
                               leanBin + L"\" " +
                               (k4dGuided ? L"1" : L"0");

            launch(cmd, engine);
            report(L"Verzweigtes Beweisnetz gestartet (Zieltiefe: " + std::to_wstring(count) + L", Verzweigung: " + std::to_wstring(branchingFactor) + L") ...");
            return;
        }

        // Single Cycle Mode across 6 strategies
        std::wstring cmd = L"\"" + engine + L"\" cycle-k4d-six \"" +
                           sourceDb + L"\" \"" +
                           k4dResultsDb + L"\" \"" +
                           k4dLeanDir + L"\" " +
                           std::to_wstring(count) + L" " +
                           std::to_wstring(seed) + L" \"" +
                           leanBin + L"\" " +
                           lineMode + L" " +
                           stratName;

        launch(cmd, engine);
        report(L"K4D Forschung gestartet (" + stratName + L", " + lineMode + L") ...");
        return;
    }

    if (mode == 1 && !selectedSource.empty()) {
        seed = seedFromSource(selectedSource, seed);
        report(L"Auswahl beeinflusst Seed; keine semantische Theorem-Ableitung.");
    }
    std::wstring cmd = L"\"" + engine + L"\" algorithm-lab \"" + resultsDb + L"\" \"" + kernelDir + L"\" " + std::to_wstring(count) + L" " + std::to_wstring(seed);
    launch(cmd, engine);
}

void save(){
    if(mode != 3 || currentFile.empty()){
        report(L"Erst einen C++-Kernel auswählen.");
        return;
    }
    fs::path original(currentFile);
    fs::path target = original.parent_path() / (original.stem().wstring() + L"_edited.hpp");
    if(!writeText(target, value(editor))){
        report(L"Datei konnte nicht gespeichert werden.");
        return;
    }
    currentFile = target.wstring();
    dirty = false;
    report(L"Bearbeitete Version separat gespeichert: " + currentFile);
}

void test(){
    if(mode != 3 || currentFile.empty()){
        report(L"Erst einen C++-Kernel auswählen.");
        return;
    }
    if(dirty){
        report(L"Vor dem Test zuerst 'Als Kopie speichern' klicken.");
        return;
    }
    fs::path script = fs::path(projectRoot) / L"TEST_IDE_KERNEL_WINDOWS.ps1";
    if(!fs::exists(script)){
        report(L"Testskript fehlt.");
        return;
    }
    fs::path program = L"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
    std::wstring cmd = L"\"" + program.wstring() + L"\" -NoProfile -ExecutionPolicy Bypass -File \"" + script.wstring() + L"\" -Header \"" + currentFile + L"\"";
    kernelTestRunning = true;
    launch(cmd, program.wstring());
}

void poll(){
    if(!job.hProcess) return;
    DWORD code = STILL_ACTIVE;
    if(!GetExitCodeProcess(job.hProcess, &code) || code == STILL_ACTIVE) return;
    CloseHandle(job.hProcess);
    CloseHandle(job.hThread);
    job = {};
    KillTimer(windowHandle, 1);
    EnableWindow(runButton, TRUE);
    EnableWindow(chainFromButton, TRUE);
    EnableWindow(lmDevButton, TRUE);
    EnableWindow(metaSynthButton, TRUE);
    EnableWindow(testButton, TRUE);
    EnableWindow(stopButton, FALSE);
    EnableWindow(evoCycleButton, TRUE);
    EnableWindow(evoContinuousButton, TRUE);
    EnableWindow(evoRollbackButton, TRUE);
    EnableWindow(evoStatusButton, TRUE);
    if(kernelTestRunning){
        kernelTestRunning = false;
        const fs::path result = fs::path(projectRoot) / L"ide_kernel_test" / L"result.txt";
        if(code == 0){
            const auto outcome = readText(result);
            showEditor(L"C++-Kernel-Test abgeschlossen\r\n" + outcome + L"\r\nDatei: " + result.wstring(), false);
        }
        report(L"C++-Kernel-Test beendet, ExitCode " + std::to_wstring(code));
    } else if (mode == 0) {
        refresh();
        report(L"K4D Forschung abgeschlossen, ExitCode " + std::to_wstring(code) + L". Tabelle aktualisiert.");
    } else if (mode == 6) {
        refresh();
        if (code == 0) {
            report(L"Autonome Evolution erfolgreich abgeschlossen (ExitCode 0). Stammbaum aktualisiert.");
        } else {
            std::wstring logContent = readText(fs::path(evoWorkspaceDir) / L"last_run.log");
            if (!logContent.empty()) {
                showEditor(L"# MRE PROTOKOLL & FEHLERANALYSE (ExitCode " + std::to_wstring(code) + L")\r\n"
                           L"================================================================================\r\n\r\n" + logContent, false);
            }
            report(L"Evolution fehlgeschlagen / abgelehnt (ExitCode " + std::to_wstring(code) + L"). Protokoll im Editor.");
        }
    } else {
        mode = 2;
        TabCtrl_SetCurSel(tabs, 2);
        refresh();
        report(L"Algorithmus-Experiment beendet, ExitCode " + std::to_wstring(code));
    }
}

struct TracePoint { unsigned input = 0, candidate = 0, reference = 0, delta = 0; };
std::vector<TracePoint> tracePoints;
std::vector<TracePoint> comparePoints;
std::wstring traceFile, compareFile, plotKernel;
size_t viewStart = 0, viewLength = 4096, selectedIndex = 0;
bool histogramMode = false;
HWND plotWindow = nullptr;
constexpr int PLOT_HEADER = 95;

bool readTrace(const fs::path& path, std::vector<TracePoint>& destination, std::wstring& error){
    std::ifstream in(path, std::ios::binary);
    if(!in){ error = L"CSV-Datei nicht lesbar."; return false; }
    std::string line;
    if(!std::getline(in, line)){ error = L"CSV-Datei leer."; return false; }
    if(!line.empty() && line.back() == '\r') line.pop_back();
    if(line != "index,input,candidate,reference,abs_delta"){
        error = L"CSV-Kopfzeile unbekannt: index,input,candidate,reference,abs_delta erwartet.";
        return false;
    }
    std::vector<TracePoint> points;
    while(std::getline(in, line)){
        if(!line.empty() && line.back() == '\r') line.pop_back();
        if(line.empty()) continue;
        std::istringstream stream(line);
        unsigned index = 0; char a = 0, b = 0, c = 0, d = 0; TracePoint point{};
        if(!(stream >> index >> a >> point.input >> b >> point.candidate >> c >> point.reference >> d >> point.delta)
           || a != ',' || b != ',' || c != ',' || d != ',' || index != points.size() ||
           point.candidate > 65520U || point.reference > 65520U || point.delta > 65520U){
            error = L"Trace-Zeile fehlerhaft bei Index " + std::to_wstring(points.size());
            return false;
        }
        char unexpected = 0;
        if(stream >> unexpected){ error = L"Unerwartete CSV-Felder."; return false; }
        points.push_back(point);
        if(points.size() > 1000000U){ error = L"Trace-Datei zu gross."; return false; }
    }
    if(points.size() < 2){ error = L"Mindestens zwei Messpunkte erforderlich."; return false; }
    destination = std::move(points);
    return true;
}

void plotText(HDC dc, int x, int y, const std::wstring& text){
    TextOutW(dc, x, y, text.c_str(), static_cast<int>(text.size()));
}

RECT graphArea(const RECT& client, bool bottom){
    const int width = static_cast<int>(client.right - client.left);
    const int height = static_cast<int>(client.bottom - client.top);
    const int available = std::max(120, height - PLOT_HEADER - 28);
    const int half = available / 2;
    if(bottom) return RECT{56, PLOT_HEADER + half + 22, width - 28, height - 35};
    return RECT{56, PLOT_HEADER + 16, width - 28, PLOT_HEADER + half - 15};
}

size_t visibleEnd(){ return std::min(tracePoints.size(), viewStart + viewLength); }

void drawSeries(HDC dc, const RECT& graph, const std::vector<TracePoint>& data, int field, COLORREF color, unsigned maximum){
    if(data.size() < 2 || viewStart >= data.size()) return;
    const size_t end = std::min(visibleEnd(), data.size());
    if(end <= viewStart) return;
    const int width = std::max(1, static_cast<int>(graph.right - graph.left));
    const int height = std::max(1, static_cast<int>(graph.bottom - graph.top));
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    const HGDIOBJ old = SelectObject(dc, pen);
    const size_t samples = end - viewStart;
    const size_t stride = std::max<size_t>(1, samples / static_cast<size_t>(width));
    bool first = true;
    for(size_t i = viewStart; i < end; i += stride){
        const unsigned y = field == 0 ? data[i].candidate : field == 1 ? data[i].reference : data[i].delta;
        const int px = graph.left + static_cast<int>((static_cast<uint64_t>(i - viewStart) * static_cast<unsigned>(width)) / std::max<size_t>(1, viewLength - 1));
        const int py = graph.bottom - static_cast<int>((static_cast<uint64_t>(y) * static_cast<unsigned>(height)) / std::max(1U, maximum));
        if(first){ MoveToEx(dc, px, py, nullptr); first = false; }
        else LineTo(dc, px, py);
    }
    const size_t i = end - 1;
    if(i > viewStart && (i - viewStart) % stride != 0){
        const unsigned y = field == 0 ? data[i].candidate : field == 1 ? data[i].reference : data[i].delta;
        const int px = graph.left + static_cast<int>((static_cast<uint64_t>(i - viewStart) * static_cast<unsigned>(width)) / std::max<size_t>(1, viewLength - 1));
        const int py = graph.bottom - static_cast<int>((static_cast<uint64_t>(y) * static_cast<unsigned>(height)) / std::max(1U, maximum));
        LineTo(dc, px, py);
    }
    SelectObject(dc, old);
    DeleteObject(pen);
}

void axes(HDC dc, const RECT& r){
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(155, 155, 155));
    auto old = SelectObject(dc, pen);
    MoveToEx(dc, r.left, r.top, nullptr); LineTo(dc, r.left, r.bottom); LineTo(dc, r.right, r.bottom);
    SelectObject(dc, old);
    DeleteObject(pen);
    plotText(dc, r.left, r.bottom + 3, std::to_wstring(viewStart));
    plotText(dc, r.right - 80, r.bottom + 3, std::to_wstring(visibleEnd() - 1));
}

void marker(HDC dc, const RECT& r){
    if(selectedIndex < viewStart || selectedIndex >= visibleEnd()) return;
    const int width = std::max(1, static_cast<int>(r.right - r.left));
    const int x = r.left + static_cast<int>((static_cast<uint64_t>(selectedIndex - viewStart) * static_cast<unsigned>(width)) / std::max<size_t>(1, viewLength - 1));
    HPEN pen = CreatePen(PS_DOT, 1, RGB(170, 40, 160));
    auto old = SelectObject(dc, pen);
    MoveToEx(dc, x, r.top, nullptr); LineTo(dc, x, r.bottom);
    SelectObject(dc, old);
    DeleteObject(pen);
}

void histogram(HDC dc, const RECT& area){
    const int bins = 32;
    std::array<unsigned, bins> counts{};
    for(size_t i = viewStart; i < visibleEnd(); ++i){
        counts[std::min(31U, tracePoints[i].candidate * bins / 65521U)]++;
    }
    const unsigned maxCount = *std::max_element(counts.begin(), counts.end());
    const int width = std::max(1, static_cast<int>(area.right - area.left));
    const int height = std::max(1, static_cast<int>(area.bottom - area.top));
    HBRUSH brush = CreateSolidBrush(RGB(45, 96, 210));
    auto old = SelectObject(dc, brush);
    for(int b = 0; b < bins; ++b){
        const int left = area.left + b * width / bins;
        const int right = area.left + (b + 1) * width / bins - 1;
        const int top = area.bottom - static_cast<int>(static_cast<uint64_t>(counts[static_cast<size_t>(b)]) * static_cast<unsigned>(height) / std::max(1U, maxCount));
        RECT bar{left, top, right, area.bottom};
        FillRect(dc, &bar, brush);
    }
    SelectObject(dc, old);
    DeleteObject(brush);
}

void zoomPlot(int direction){
    if(tracePoints.empty()) return;
    const size_t total = tracePoints.size();
    const size_t next = direction > 0 ? std::max<size_t>(16, viewLength / 2) : std::min(total, viewLength * 2);
    const size_t center = selectedIndex;
    viewLength = std::min(total, next);
    viewStart = center > viewLength / 2 ? center - viewLength / 2 : 0;
    if(viewStart + viewLength > total) viewStart = total - viewLength;
    InvalidateRect(plotWindow, nullptr, FALSE);
}

void openCompare(HWND parent){
    wchar_t chosen[32768]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = parent;
    dialog.lpstrFile = chosen;
    dialog.nMaxFile = 32768;
    dialog.lpstrFilter = L"Trace CSV (*.csv)\0*.csv\0Alle Dateien (*.*)\0*.*\0\0";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if(!GetOpenFileNameW(&dialog)) return;
    std::vector<TracePoint> loaded;
    std::wstring error;
    if(!readTrace(fs::path(chosen), loaded, error)){
        MessageBoxW(parent, error.c_str(), L"Vergleich", MB_ICONERROR);
        return;
    }
    if(loaded.size() != tracePoints.size()){
        MessageBoxW(parent, L"Der Vergleich benötigt dieselbe Anzahl Messpunkte.", L"Vergleich", MB_ICONERROR);
        return;
    }
    for(size_t i = 0; i < loaded.size(); ++i){
        if(loaded[i].input != tracePoints[i].input){
            MessageBoxW(parent, L"Eingaben weichen ab. Vergleich abgelehnt: identische Testeingaben erforderlich.", L"Vergleich", MB_ICONERROR);
            return;
        }
    }
    comparePoints = std::move(loaded);
    compareFile = chosen;
    InvalidateRect(parent, nullptr, FALSE);
}

LRESULT CALLBACK plotProc(HWND w, UINT msg, WPARAM wp, LPARAM lp){
    switch(msg){
    case WM_PAINT:{
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(w, &ps);
        RECT client{};
        GetClientRect(w, &client);
        FillRect(dc, &client, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
        SetBkMode(dc, TRANSPARENT);
        plotText(dc, 16, 8, L"Mausrad: Zoom | Klick: Messpunkt | Pfeile: Punkt wechseln | H: Histogramm | V: Vergleich | O: Ausreisser | E: C++ | R: Reset");
        if(tracePoints.empty()){ EndPaint(w, &ps); return 0; }
        const auto& t = tracePoints[selectedIndex];
        std::wstring detail = L"Index=" + std::to_wstring(selectedIndex) + L"  Input=" + std::to_wstring(t.input) + L"  Kandidat=" + std::to_wstring(t.candidate) + L"  Referenz=" + std::to_wstring(t.reference) + L"  |Delta|=" + std::to_wstring(t.delta);
        plotText(dc, 16, 29, detail);
        std::wstring kernel = L"Kernel: " + plotKernel;
        if(kernel.size() > 145) kernel = kernel.substr(0, 145) + L"...";
        plotText(dc, 16, 49, kernel);
        plotText(dc, 16, 69, L"Bereich: " + std::to_wstring(viewStart) + L" - " + std::to_wstring(visibleEnd() - 1) + (comparePoints.empty() ? L"" : L" | Vergleich geladen: " + fs::path(compareFile).filename().wstring()));
        const RECT upper = graphArea(client, false), lower = graphArea(client, true);
        axes(dc, upper);
        axes(dc, lower);
        if(histogramMode){
            plotText(dc, upper.left, upper.top - 16, L"Histogramm Kandidat (32 Klassen, sichtbarer Bereich)");
            histogram(dc, upper);
        }else{
            plotText(dc, upper.left, upper.top - 16, L"Zustaende: Kandidat blau, Referenz gruen, zweiter Kandidat violett");
            drawSeries(dc, upper, tracePoints, 0, RGB(30, 90, 215), 65520U);
            drawSeries(dc, upper, tracePoints, 1, RGB(0, 150, 85), 65520U);
            if(!comparePoints.empty()) drawSeries(dc, upper, comparePoints, 0, RGB(150, 40, 175), 65520U);
        }
        plotText(dc, lower.left, lower.top - 16, L"Absolute Abweichung zur Referenz (rot)");
        drawSeries(dc, lower, tracePoints, 2, RGB(210, 55, 45), 65520U);
        marker(dc, upper);
        marker(dc, lower);
        EndPaint(w, &ps);
        return 0;
    }
    case WM_MOUSEWHEEL: zoomPlot(GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1); return 0;
    case WM_LBUTTONDOWN:{
        SetFocus(w);
        if(tracePoints.empty()) return 0;
        RECT c{};
        GetClientRect(w, &c);
        const RECT r = graphArea(c, GET_Y_LPARAM(lp) > (c.bottom / 2));
        const int x = GET_X_LPARAM(lp), width = std::max(1, static_cast<int>(r.right - r.left));
        const int relative = std::max(0, std::min(width, x - static_cast<int>(r.left)));
        selectedIndex = std::min(tracePoints.size() - 1, viewStart + static_cast<size_t>(static_cast<uint64_t>(relative) * std::max<size_t>(1, viewLength - 1) / static_cast<unsigned>(width)));
        InvalidateRect(w, nullptr, FALSE);
        return 0;
    }
    case WM_KEYDOWN:
        if(wp == 'H') histogramMode = !histogramMode;
        else if(wp == 'V') openCompare(w);
        else if(wp == 'O' && !tracePoints.empty()){
            size_t peak = 0;
            for(size_t i = 1; i < tracePoints.size(); ++i) if(tracePoints[i].delta > tracePoints[peak].delta) peak = i;
            selectedIndex = peak;
            viewLength = std::min<size_t>(tracePoints.size(), 128);
            viewStart = peak > viewLength / 2 ? peak - viewLength / 2 : 0;
            if(viewStart + viewLength > tracePoints.size()) viewStart = tracePoints.size() - viewLength;
        }
        else if(wp == 'E' && !plotKernel.empty() && fs::exists(fs::path(plotKernel))){
            mode = 3;
            TabCtrl_SetCurSel(tabs, 3);
            refresh();
            currentFile = plotKernel;
            showEditor(readText(fs::path(plotKernel)), true);
            SetForegroundWindow(windowHandle);
            report(L"C++-Kernel aus Diagramm geöffnet: " + plotKernel);
        }
        else if(wp == 'R'){ viewStart = 0; viewLength = tracePoints.size(); selectedIndex = 0; comparePoints.clear(); compareFile.clear(); }
        else if(wp == VK_LEFT && selectedIndex > 0) --selectedIndex;
        else if(wp == VK_RIGHT && selectedIndex + 1 < tracePoints.size()) ++selectedIndex;
        else if(wp == VK_PRIOR) zoomPlot(1);
        else if(wp == VK_NEXT) zoomPlot(-1);
        if(selectedIndex < viewStart) viewStart = selectedIndex;
        if(selectedIndex >= viewStart + viewLength) viewStart = selectedIndex - viewLength + 1;
        InvalidateRect(w, nullptr, FALSE);
        return 0;
    case WM_CLOSE: plotWindow = nullptr; DestroyWindow(w); return 0;
    case WM_ERASEBKGND: return 1;
    default: return DefWindowProcW(w, msg, wp, lp);
    }
}

void plot(){
    const fs::path path = fs::path(projectRoot) / L"ide_kernel_test" / L"trace.csv";
    std::vector<TracePoint> points;
    std::wstring error;
    if(!readTrace(path, points, error)){ report(L"Diagramm: " + error); return; }
    tracePoints = std::move(points);
    traceFile = path.wstring();
    plotKernel = currentFile;
    viewStart = 0; viewLength = tracePoints.size(); selectedIndex = 0; histogramMode = false;
    comparePoints.clear(); compareFile.clear();
    if(plotWindow && IsWindow(plotWindow)){
        SetForegroundWindow(plotWindow);
        InvalidateRect(plotWindow, nullptr, FALSE);
    }else{
        plotWindow = CreateWindowExW(0, L"MRE_TracePlot", L"Algorithmus-Analyse | Interaktiv", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT, 1250, 850, windowHandle, nullptr, GetModuleHandleW(nullptr), nullptr);
        if(plotWindow){ ShowWindow(plotWindow, SW_SHOW); UpdateWindow(plotWindow); SetFocus(plotWindow); }
    }
    report(plotWindow ? L"Interaktives Diagramm geladen: " + traceFile : L"Diagrammfenster konnte nicht geöffnet werden.");
}

void layout(HWND w){
    RECT r{};
    GetClientRect(w, &r);
    int width = r.right, height = r.bottom;
    MoveWindow(tabs, 12, 8, width - 24, 34, TRUE);
    MoveWindow(label, 14, 44, width - 28, 20, TRUE);

    int topListY = 104;

    if (mode == 0) {
        // Row 1 (y = 66): K4D Experiment Controls
        MoveWindow(chainModeCombo, 14, 66, 175, 200, TRUE);
        MoveWindow(strategyCombo, 193, 66, 115, 200, TRUE);
        MoveWindow(lineModeCombo, 312, 66, 135, 200, TRUE);
        MoveWindow(countBox, 451, 66, 40, 26, TRUE);
        MoveWindow(seedBox, 495, 66, 50, 26, TRUE);
        MoveWindow(runButton, 549, 66, 145, 26, TRUE);
        MoveWindow(chainFromButton, 698, 66, 150, 26, TRUE);
        MoveWindow(stopButton, 852, 66, 60, 26, TRUE);
        MoveWindow(reportButton, 916, 66, 115, 26, TRUE);
        MoveWindow(searchButton, 1035, 66, 95, 26, TRUE);

        // Row 2 (y = 96): Autonomous C++ Software Developer (LM Studio / Gemini)
        MoveWindow(providerCombo, 14, 96, 145, 200, TRUE);
        MoveWindow(apiKeyLabel, 163, 99, 155, 20, TRUE);
        MoveWindow(apiKeyBox, 322, 96, 150, 26, TRUE);
        MoveWindow(modelLabel, 478, 99, 45, 20, TRUE);
        MoveWindow(modelBox, 527, 96, 85, 26, TRUE);
        MoveWindow(appTypeCombo, 618, 96, 165, 200, TRUE);
        MoveWindow(lmDevButton, 789, 96, 160, 26, TRUE);
        MoveWindow(metaSynthButton, 955, 96, 215, 26, TRUE);

        topListY = 132;
    } else if (mode == 1) {
        int half = width / 2;
        MoveWindow(searchBox, 14, 69, half - 33, 26, TRUE);
        MoveWindow(searchButton, half - 8, 69, 105, 26, TRUE);
        MoveWindow(countBox, half + 104, 69, 55, 26, TRUE);
        MoveWindow(seedBox, half + 165, 69, 90, 26, TRUE);
        MoveWindow(runButton, width - 160, 69, 145, 26, TRUE);
        topListY = 104;
    } else if (mode == 2) {
        // Row 1 (y = 66):
        MoveWindow(searchButton, 14, 66, 110, 26, TRUE);
        MoveWindow(countBox, 130, 66, 45, 26, TRUE);
        MoveWindow(seedBox, 181, 66, 55, 26, TRUE);
        MoveWindow(runButton, 242, 66, 150, 26, TRUE);

        // Row 2 (y = 96): Autonomous C++ Developer Controls
        MoveWindow(providerCombo, 14, 96, 145, 200, TRUE);
        MoveWindow(apiKeyLabel, 163, 99, 155, 20, TRUE);
        MoveWindow(apiKeyBox, 322, 96, 150, 26, TRUE);
        MoveWindow(modelLabel, 478, 99, 45, 20, TRUE);
        MoveWindow(modelBox, 527, 96, 85, 26, TRUE);
        MoveWindow(appTypeCombo, 618, 96, 165, 200, TRUE);
        MoveWindow(lmDevButton, 789, 96, 160, 26, TRUE);
        MoveWindow(metaSynthButton, 955, 96, 215, 26, TRUE);

        topListY = 132;
    } else if (mode == 6) {
        int evoSubMode = (evoSubModeCombo) ? ComboBox_GetCurSel(evoSubModeCombo) : 0;
        if (evoSubMode < 0) evoSubMode = 0;

        // Submode selector (Einzelevolution vs. Meta-Fusion)
        MoveWindow(evoSubModeCombo, 14, 66, 135, 200, TRUE);

        if (evoSubMode == 0) {
            // Row 1 (y = 66): Einzelevolution controls
            MoveWindow(evoDumpBox, 155, 66, 340, 26, TRUE);
            MoveWindow(evoBrowseButton, 501, 66, 110, 26, TRUE);
            MoveWindow(countBox, 617, 66, 45, 26, TRUE);
            MoveWindow(seedBox, 668, 66, 55, 26, TRUE);
            MoveWindow(evoCycleButton, 729, 66, 160, 26, TRUE);
            MoveWindow(evoContinuousButton, 895, 66, 140, 26, TRUE);
        } else {
            // Row 1 (y = 66): Meta-Fusion controls
            MoveWindow(evoFamACombo, 155, 66, 185, 200, TRUE);
            MoveWindow(evoFamBCombo, 346, 66, 185, 200, TRUE);
            MoveWindow(evoTargetFamBox, 537, 66, 230, 26, TRUE);
            MoveWindow(countBox, 773, 66, 45, 26, TRUE);
            MoveWindow(seedBox, 824, 66, 55, 26, TRUE);
            MoveWindow(evoMetaFusionButton, 885, 66, 155, 26, TRUE);
        }

        // Row 2 (y = 96): LLM Configuration on the left
        MoveWindow(providerCombo, 14, 96, 145, 200, TRUE);
        MoveWindow(apiKeyLabel, 165, 99, 150, 20, TRUE);
        MoveWindow(apiKeyBox, 321, 96, 130, 26, TRUE);
        MoveWindow(modelLabel, 457, 99, 45, 20, TRUE);
        MoveWindow(modelBox, 508, 96, 80, 26, TRUE);
        MoveWindow(appTypeCombo, 594, 96, 160, 200, TRUE);

        // Row 2 (y = 96): Global Evolution & Engine Actions on the right
        int actionStartX = std::max(760, width - 14 - 430);
        MoveWindow(evoRollbackButton, actionStartX, 96, 135, 26, TRUE);
        MoveWindow(evoStatusButton, actionStartX + 141, 96, 110, 26, TRUE);
        MoveWindow(stopButton, actionStartX + 257, 96, 65, 26, TRUE);
        MoveWindow(searchButton, actionStartX + 328, 96, 102, 26, TRUE);

        topListY = 132;
    } else {
        MoveWindow(searchButton, 14, 69, 110, 26, TRUE);
        MoveWindow(countBox, 130, 69, 55, 26, TRUE);
        MoveWindow(seedBox, 191, 69, 90, 26, TRUE);
        MoveWindow(runButton, width - 160, 69, 145, 26, TRUE);
        topListY = 104;
    }

    int listHeight = std::max(110, (height - topListY - 78) * 5 / 11);
    MoveWindow(rows, 14, topListY, width - 28, listHeight, TRUE);
    int y = topListY + listHeight + 6;
    int edHeight = std::max(70, height - y - 42);
    MoveWindow(editor, 14, y, width - 28, edHeight, TRUE);

    MoveWindow(saveButton, 14, height - 34, 150, 26, TRUE);
    MoveWindow(testButton, 168, height - 34, 165, 26, TRUE);
    MoveWindow(plotButton, 337, height - 34, 155, 26, TRUE);
    int stX = (mode == 3 ? 500 : 14);
    MoveWindow(status, stX, height - 34, std::max(60, width - stX - 14), 26, TRUE);
}

LRESULT CALLBACK proc(HWND w, UINT msg, WPARAM wp, LPARAM lp){
    switch(msg){
    case WM_CREATE:{
        g_bgBrush = CreateSolidBrush(RGB(24, 28, 36));
        g_editBrush = CreateSolidBrush(RGB(18, 21, 28));
        g_panelBrush = CreateSolidBrush(RGB(32, 38, 48));

        HMENU hMenuBar = CreateMenu();
        HMENU hInfoMenu = CreatePopupMenu();
        AppendMenuW(hInfoMenu, MF_STRING, ID_MENU_ABOUT, L"&Über Axion...");
        AppendMenuW(hInfoMenu, MF_STRING, ID_MENU_MATH_ORIGIN, L"&Mathematische Algorithmen && Herkunft...");
        AppendMenuW(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hInfoMenu), L"&Info");
        SetMenu(w, hMenuBar);

        INITCOMMONCONTROLSEX init{sizeof(init), ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES};
        InitCommonControlsEx(&init);
        tabs = CreateWindowW(WC_TABCONTROLW, L"", WS_VISIBLE | WS_CHILD, 0, 0, 1, 1, w, ctl(ID_TABS), nullptr, nullptr);
        for(auto t : {L"K4D Research Lab", L"Quelldatenbank", L"C++ Algorithmen", L"C++ Editor + Test", L"Forschungsläufe", L"Experimentanalyse", L"Autonome Evolution (Axion)"}){
            TCITEMW i{}; i.mask = TCIF_TEXT; i.pszText = const_cast<wchar_t*>(t);
            TabCtrl_InsertItem(tabs, TabCtrl_GetItemCount(tabs), &i);
        }
        label = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, w, nullptr, nullptr, nullptr);

        chainModeCombo = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 1, 1, w, ctl(ID_CHAINMODE), nullptr, nullptr);
        ComboBox_AddString(chainModeCombo, L"1. Einzelne Zyklen (Basis)");
        ComboBox_AddString(chainModeCombo, L"2. Lineare Forschungskette");
        ComboBox_AddString(chainModeCombo, L"3. Verzweigtes Beweisnetz (Baum)");
        ComboBox_SetCurSel(chainModeCombo, 2); // Default to branching research network!

        strategyCombo = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 1, 1, w, ctl(ID_STRATEGY), nullptr, nullptr);
        for(auto s : {L"Alle 6 Strategien", L"type_synthesis", L"systematic", L"random", L"cross_domain", L"mutation", L"iterative"}){
            ComboBox_AddString(strategyCombo, s);
        }
        ComboBox_SetCurSel(strategyCombo, 0);

        lineModeCombo = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 1, 1, w, ctl(ID_LINEMODE), nullptr, nullptr);
        for(auto m : {L"K4D & CONTROL Vergleich", L"Nur K4D", L"Nur CONTROL"}){
            ComboBox_AddString(lineModeCombo, m);
        }
        ComboBox_SetCurSel(lineModeCombo, 0);

        searchBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 1, 1, w, nullptr, nullptr, nullptr);
        searchButton = CreateWindowW(L"BUTTON", L"Aktualisieren", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, w, ctl(ID_SEARCH), nullptr, nullptr);

        countBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"4", WS_CHILD | WS_VISIBLE | ES_NUMBER, 0, 0, 1, 1, w, ctl(ID_COUNT), nullptr, nullptr);
        seedBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"42", WS_CHILD | WS_VISIBLE | ES_NUMBER, 0, 0, 1, 1, w, ctl(ID_SEED), nullptr, nullptr);
        runButton = CreateWindowW(L"BUTTON", L"Forschungskette starten", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, w, ctl(ID_RUN), nullptr, nullptr);
        chainFromButton = CreateWindowW(L"BUTTON", L"Kette ab Auswahl vertiefen", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, w, ctl(ID_CHAINFROM), nullptr, nullptr);
        
        providerCombo = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 1, 1, w, ctl(ID_PROVIDER), nullptr, nullptr);
        ComboBox_AddString(providerCombo, L"LM Studio (Lokal)");
        ComboBox_AddString(providerCombo, L"Google Gemini (Cloud API)");
        ComboBox_SetCurSel(providerCombo, 0);

        apiKeyLabel = CreateWindowW(L"STATIC", L"Gemini API-Key (nur im RAM):", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, w, nullptr, nullptr, nullptr);
        apiKeyBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_AUTOHSCROLL, 0, 0, 1, 1, w, ctl(ID_APIKEY), nullptr, nullptr);

        modelLabel = CreateWindowW(L"STATIC", L"Modell:", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, w, nullptr, nullptr, nullptr);
        modelBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"auto", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 1, 1, w, ctl(ID_MODEL), nullptr, nullptr);

        appTypeCombo = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 1, 1, w, ctl(ID_APPTYPE), nullptr, nullptr);
        ComboBox_AddString(appTypeCombo, L"0: Automatisch");
        ComboBox_AddString(appTypeCombo, L"1: Konsolenprogramm");
        ComboBox_AddString(appTypeCombo, L"2: Win32-GUI Anwendung");
        ComboBox_SetCurSel(appTypeCombo, 0);

        lmDevButton = CreateWindowW(L"BUTTON", L"C++ Software entwickeln", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, w, ctl(ID_LMDEV), nullptr, nullptr);
        metaSynthButton = CreateWindowW(L"BUTTON", L"Meta-Synthese aller Algorithmen", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, w, ctl(ID_METASYNTH), nullptr, nullptr);
        stopButton = CreateWindowW(L"BUTTON", L"Stopp", WS_CHILD | WS_VISIBLE | WS_DISABLED, 0, 0, 1, 1, w, ctl(ID_STOP), nullptr, nullptr);
        reportButton = CreateWindowW(L"BUTTON", L"Forschungsbericht", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, w, ctl(ID_REPORT), nullptr, nullptr);

        rows = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL, 0, 0, 1, 1, w, ctl(ID_ROWS), nullptr, nullptr);
        ListView_SetExtendedListViewStyle(rows, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        ListView_SetBkColor(rows, RGB(24, 28, 36));
        ListView_SetTextBkColor(rows, RGB(24, 28, 36));
        ListView_SetTextColor(rows, RGB(226, 232, 240));

        editor = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL | ES_AUTOVSCROLL | ES_WANTRETURN, 0, 0, 1, 1, w, nullptr, nullptr, nullptr);
        saveButton = CreateWindowW(L"BUTTON", L"Als Kopie speichern", WS_CHILD, 0, 0, 1, 1, w, ctl(ID_SAVE), nullptr, nullptr);
        testButton = CreateWindowW(L"BUTTON", L"C++ kompilieren + testen", WS_CHILD, 0, 0, 1, 1, w, ctl(ID_TEST), nullptr, nullptr);
        plotButton = CreateWindowW(L"BUTTON", L"Zustandsdiagramm", WS_CHILD, 0, 0, 1, 1, w, ctl(ID_PLOT), nullptr, nullptr);
        status = CreateWindowW(L"STATIC", L"Bereit", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, w, nullptr, nullptr, nullptr);

        // Evolution Engine Tab Controls
        evoSubModeCombo = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 1, 1, w, ctl(ID_EVO_SUBMODE), nullptr, nullptr);
        ComboBox_AddString(evoSubModeCombo, L"Einzelevolution");
        ComboBox_AddString(evoSubModeCombo, L"Meta-Fusion");
        ComboBox_SetCurSel(evoSubModeCombo, 0);

        evoDumpBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"F:\\mre_new\\software_experiments", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 1, 1, w, ctl(ID_EVO_DUMP), nullptr, nullptr);
        evoBrowseButton = CreateWindowW(L"BUTTON", L"Dump wählen...", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, w, ctl(ID_EVO_BROWSE), nullptr, nullptr);
        evoCycleButton = CreateWindowW(L"BUTTON", L"Evolutionszyklus starten", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, w, ctl(ID_EVO_CYCLE), nullptr, nullptr);
        evoContinuousButton = CreateWindowW(L"BUTTON", L"Dauerlauf starten", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, w, ctl(ID_EVO_CONTINUOUS), nullptr, nullptr);

        evoFamACombo = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 1, 1, w, ctl(ID_EVO_FAMA), nullptr, nullptr);
        evoFamBCombo = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 1, 1, w, ctl(ID_EVO_FAMB), nullptr, nullptr);
        evoTargetFamBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"FUSION_TENSOR_ATTENTION_ROPE", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 1, 1, w, ctl(ID_EVO_TARGETFAM), nullptr, nullptr);
        evoMetaFusionButton = CreateWindowW(L"BUTTON", L"Meta-Fusion starten", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, w, ctl(ID_EVO_METAFUSION), nullptr, nullptr);

        evoRollbackButton = CreateWindowW(L"BUTTON", L"Rollback (Baseline)", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, w, ctl(ID_EVO_ROLLBACK), nullptr, nullptr);
        evoStatusButton = CreateWindowW(L"BUTTON", L"Engine Status", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, w, ctl(ID_EVO_STATUS), nullptr, nullptr);

        initUiFonts();
        applyUiFont(w);
        if (g_editorFont) {
            SendMessageW(editor, WM_SETFONT, reinterpret_cast<WPARAM>(g_editorFont), TRUE);
        }

        SendMessageW(countBox, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"Tiefe"));
        SendMessageW(seedBox, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"Seed"));

        appTooltip = CreateWindowExW(0, TOOLTIPS_CLASSW, nullptr,
            WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            w, nullptr, nullptr, nullptr);
        if (appTooltip) {
            auto addTip = [](HWND target, const wchar_t* txt) {
                if (!target || !txt || !appTooltip) return;
                TOOLINFOW ti{sizeof(ti)};
                ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
                ti.hwnd = target;
                ti.uId = reinterpret_cast<UINT_PTR>(target);
                ti.lpszText = const_cast<wchar_t*>(txt);
                SendMessageW(appTooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
            };
            addTip(evoSubModeCombo, L"Modus wählen: Einzelevolution oder Meta-Fusion zweier Familien");
            addTip(evoDumpBox, L"Pfad zum Dump oder Workspace der Forschungsfamilie");
            addTip(evoBrowseButton, L"Ordner mit Algorithmus-Dump auswählen");
            addTip(countBox, L"Tiefe: Anzahl Generationen je Zyklus (z. B. 4 oder 10)");
            addTip(seedBox, L"Seed: Deterministischer Startzustand für die Suche");
            addTip(evoCycleButton, L"Evolutionszyklus mit der eingestellten Tiefe starten");
            addTip(evoContinuousButton, L"Autonomen Dauerlauf starten (mehrere Zyklen)");
            addTip(evoFamACombo, L"Eltern-Forschungsfamilie A für Meta-Fusion");
            addTip(evoFamBCombo, L"Eltern-Forschungsfamilie B für Meta-Fusion");
            addTip(evoTargetFamBox, L"Name der neuen Fusionsfamilie");
            addTip(evoMetaFusionButton, L"Meta-Fusion beider Forschungsfamilien starten");
            addTip(evoRollbackButton, L"Rollback: Familie auf Baseline (MRE_0) zurücksetzen");
            addTip(evoStatusButton, L"Engine Status: Analyse aller Forschungsfamilien anzeigen");
            addTip(stopButton, L"Laufenden Hintergrundprozess sauber abbrechen");
            addTip(searchButton, L"Tabelle und Statusansicht aktualisieren");
            addTip(providerCombo, L"LLM-Engine (LM Studio lokal oder Google Gemini API)");
            addTip(appTypeCombo, L"Anwendungstyp: Automatisch, Konsole oder Win32-GUI");
        }

        updateControlVisibility();
        refresh();
        return 0;
    }
    case WM_SIZE: layout(w); return 0;
    case WM_TIMER: poll(); return 0;
    case WM_CONTEXTMENU:{
        HWND target = reinterpret_cast<HWND>(wp);
        if(target == rows){
            POINT pt;
            pt.x = GET_X_LPARAM(lp);
            pt.y = GET_Y_LPARAM(lp);
            if(pt.x == -1 && pt.y == -1){
                int sel = ListView_GetNextItem(rows, -1, LVNI_SELECTED);
                if(sel >= 0){
                    RECT rc{};
                    ListView_GetItemRect(rows, sel, &rc, LVIR_BOUNDS);
                    pt.x = rc.left + 20;
                    pt.y = rc.top + 10;
                    ClientToScreen(rows, &pt);
                } else {
                    GetCursorPos(&pt);
                }
            } else {
                POINT clientPt = pt;
                ScreenToClient(rows, &clientPt);
                LVHITTESTINFO hti{};
                hti.pt = clientPt;
                int itemIdx = ListView_HitTest(rows, &hti);
                if(itemIdx >= 0){
                    ListView_SetItemState(rows, itemIdx, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                    showSelection();
                }
            }
            showContextMenuForSelection(w, pt);
            return 0;
        }
        return DefWindowProcW(w, msg, wp, lp);
    }
    case WM_NOTIFY:{
        auto* h = reinterpret_cast<NMHDR*>(lp);
        if(h->hwndFrom == tabs && h->code == TCN_SELCHANGE){
            mode = TabCtrl_GetCurSel(tabs);
            refresh();
            layout(w);
        } else if(h->hwndFrom == rows && h->code == LVN_ITEMCHANGED){
            showSelection();
        } else if(h->hwndFrom == rows && h->code == NM_RCLICK){
            auto* nmia = reinterpret_cast<NMITEMACTIVATE*>(lp);
            if(nmia && nmia->iItem >= 0){
                ListView_SetItemState(rows, nmia->iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                showSelection();
            }
        }
        return 0;
    }
    case WM_COMMAND:{
        int id = LOWORD(wp);
        if(id == ID_RUN) run();
        else if(id == ID_CHAINFROM) chainFromSelection();
        else if(id == ID_LMDEV) developSoftwareFromSelection();
        else if(id == ID_METASYNTH) synthesizeAllSoftware();
        else if(id == ID_EVO_BROWSE) browseDumpFolder();
        else if(id == ID_EVO_CYCLE) runEvolutionCycleUI(false);
        else if(id == ID_EVO_CONTINUOUS) runEvolutionCycleUI(true);
        else if(id == ID_EVO_METAFUSION) runMetaFusionUI();
        else if(id == ID_EVO_ROLLBACK) rollbackEvolutionUI();
        else if(id == ID_EVO_STATUS) reportEvolutionStatusUI();
        else if(id == ID_SEARCH) refresh();
        else if(id == ID_STOP) stopJob();
        else if(id == ID_REPORT) reportK4d();
        else if(id == ID_SAVE) save();
        else if(id == ID_TEST) test();
        else if(id == ID_PLOT) plot();
        else if(id == ID_EVO_SUBMODE && HIWORD(wp) == CBN_SELCHANGE) {
            updateControlVisibility();
            layout(w);
        }
        else if(id == ID_CHAINMODE && HIWORD(wp) == CBN_SELCHANGE) updateControlVisibility();
        else if(id == ID_PROVIDER && HIWORD(wp) == CBN_SELCHANGE) {
            int pSel = ComboBox_GetCurSel(providerCombo);
            if (pSel == 1) { // Gemini
                std::wstring currentM = value(modelBox);
                if (currentM.empty() || currentM == L"auto") {
                    SetWindowTextW(modelBox, L"gemini-2.5-flash");
                }
            } else { // LM Studio
                std::wstring currentM = value(modelBox);
                if (currentM == L"gemini-2.5-flash" || currentM == L"gemini-1.5-flash") {
                    SetWindowTextW(modelBox, L"auto");
                }
            }
        }
        else if(reinterpret_cast<HWND>(lp) == editor && HIWORD(wp) == EN_CHANGE && mode == 3 && !loadingEditor) dirty = true;
        else if(id == ID_MENU_ABOUT) showAboutDialog(w);
        else if(id == ID_MENU_MATH_ORIGIN) showMathOriginDialog(w);
        return 0;
    }
    case WM_ERASEBKGND: {
        HDC hdc = reinterpret_cast<HDC>(wp);
        RECT rc;
        GetClientRect(w, &rc);
        FillRect(hdc, &rc, g_bgBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wp);
        SetTextColor(hdc, RGB(226, 232, 240));
        SetBkColor(hdc, RGB(24, 28, 36));
        return reinterpret_cast<LRESULT>(g_bgBrush);
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = reinterpret_cast<HDC>(wp);
        SetTextColor(hdc, RGB(245, 248, 255));
        SetBkColor(hdc, RGB(18, 21, 28));
        return reinterpret_cast<LRESULT>(g_editBrush);
    }
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = reinterpret_cast<HDC>(wp);
        SetTextColor(hdc, RGB(245, 248, 255));
        SetBkColor(hdc, RGB(24, 28, 36));
        return reinterpret_cast<LRESULT>(g_bgBrush);
    }
    case WM_DESTROY:
        if(g_uiFont) DeleteObject(g_uiFont);
        if(g_editorFont) DeleteObject(g_editorFont);
        if(g_bgBrush) DeleteObject(g_bgBrush);
        if(g_editBrush) DeleteObject(g_editBrush);
        if(g_panelBrush) DeleteObject(g_panelBrush);
        if(job.hProcess){ CloseHandle(job.hProcess); CloseHandle(job.hThread); }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(w, msg, wp, lp);
    }
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show){
    SetProcessDPIAware();
    wchar_t executable[MAX_PATH]{};
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    fs::path p(executable);
    engine = (p.parent_path() / L"axion_engine.exe").wstring();
    if (!fs::exists(engine)) {
        engine = (p.parent_path() / L"math_research_v03.exe").wstring();
    }
    projectRoot = p.parent_path().parent_path().parent_path().wstring();

    if (fs::exists(fs::path(projectRoot) / L"k4d_six_results.sqlite")) {
        k4dResultsDb = (fs::path(projectRoot) / L"k4d_six_results.sqlite").wstring();
    }
    if (fs::exists(fs::path(projectRoot) / L"K4D_SIX_LEAN")) {
        k4dLeanDir = (fs::path(projectRoot) / L"K4D_SIX_LEAN").wstring();
    }
    if (fs::exists(fs::path(projectRoot) / L"algorithm_kernels")) {
        kernelDir = (fs::path(projectRoot) / L"algorithm_kernels").wstring();
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = proc;
    wc.lpszClassName = L"Axion_NativeResearchIDE";
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = CreateSolidBrush(RGB(24, 28, 36));
    RegisterClassExW(&wc);

    WNDCLASSEXW plotClass{};
    plotClass.cbSize = sizeof(plotClass);
    plotClass.hInstance = instance;
    plotClass.lpfnWndProc = plotProc;
    plotClass.lpszClassName = L"Axion_TracePlot";
    plotClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    plotClass.hbrBackground = CreateSolidBrush(RGB(20, 23, 29));
    RegisterClassExW(&plotClass);

    windowHandle = CreateWindowExW(0, wc.lpszClassName, L"Axion | Autonomous Mathematical Research & Synthesis IDE", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 1430, 930, nullptr, nullptr, instance, nullptr);
    if(!windowHandle) return 1;

    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(windowHandle, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
    ShowWindow(windowHandle, show);
    MSG message{};
    while(GetMessageW(&message, nullptr, 0, 0) > 0){
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
