#include "cli_monitor.hpp"

namespace mre {

void CliMonitor::printBanner() {
    std::cout << "=====================================================================\n";
    std::cout << "      MATH RESEARCH ENGINE v0.3 – AUTONOMOUS DISCOVERY ENGINE        \n";
    std::cout << "       Native C++20 / Windows 11 / MSVC 19.44 / Lean 4.34.1 / SQLite \n";
    std::cout << "=====================================================================\n";
}

void CliMonitor::printDbStats(Db& db, const std::string& label) {
    std::cout << "\n--- DATABASE STATISTICS: " << label << " ---\n";
    try {
        int64_t files = db.count("SELECT COUNT(*) FROM files");
        int64_t imports = db.count("SELECT COUNT(*) FROM imports");
        int64_t decls = db.count("SELECT COUNT(*) FROM declarations");
        std::cout << "Files: " << files << " | Imports: " << imports << " | Declarations: " << decls << "\n";

        Stmt s(db, "SELECT kind, COUNT(*) FROM declarations GROUP BY kind ORDER BY COUNT(*) DESC");
        std::cout << "Declaration Kinds:\n";
        while (s.step()) {
            std::cout << "  " << std::left << std::setw(12) << s.getText(0) << ": " << s.getInt64(1) << "\n";
        }
    } catch (const std::exception& e) {
        std::cout << "Notice: " << e.what() << "\n";
    }

    try {
        int64_t candidates = db.count("SELECT COUNT(*) FROM candidates");
        std::cout << "Candidates in DB: " << candidates << "\n";
        if (candidates > 0) {
            Stmt s(db, "SELECT status, COUNT(*) FROM candidates GROUP BY status ORDER BY COUNT(*) DESC");
            std::cout << "Candidate Statuses:\n";
            while (s.step()) {
                std::cout << "  " << std::left << std::setw(25) << s.getText(0) << ": " << s.getInt64(1) << "\n";
            }
        }
    } catch (...) {}
    std::cout << "---------------------------------------------------------------------\n\n";
}

void CliMonitor::renderDashboard(Db& discovery_db) {
    printBanner();
    int64_t total = discovery_db.count("SELECT COUNT(*) FROM candidates");
    std::cout << "\n[LIVE MONITOR DASHBOARD]\n";
    std::cout << "Total Research Candidates Generated: " << total << "\n\n";

    std::cout << "STATUS BREAKDOWN:\n";
    std::cout << "+------------------------------+------------+\n";
    std::cout << "| Status                       | Count      |\n";
    std::cout << "+------------------------------+------------+\n";

    Stmt st(discovery_db, "SELECT status, COUNT(*) FROM candidates GROUP BY status ORDER BY COUNT(*) DESC");
    while (st.step()) {
        std::cout << "| " << std::left << std::setw(28) << st.getText(0)
                  << " | " << std::right << std::setw(10) << st.getInt64(1) << " |\n";
    }
    std::cout << "+------------------------------+------------+\n\n";

    std::cout << "RECENT CANDIDATES:\n";
    std::cout << "+----+------------+--------------------------+--------------------------+-----------------------+\n";
    std::cout << "| ID | Strategy   | Source A                 | Source B                 | Status                |\n";
    std::cout << "+----+------------+--------------------------+--------------------------+-----------------------+\n";

    Stmt rec(discovery_db, "SELECT id, strategy, source_a, source_b, status FROM candidates ORDER BY id DESC LIMIT 10");
    while (rec.step()) {
        std::cout << "| " << std::right << std::setw(2) << rec.getInt64(0)
                  << " | " << std::left << std::setw(10) << rec.getText(1)
                  << " | " << std::left << std::setw(24) << rec.getText(2).substr(0, 24)
                  << " | " << std::left << std::setw(24) << rec.getText(3).substr(0, 24)
                  << " | " << std::left << std::setw(21) << rec.getText(4) << " |\n";
    }
    std::cout << "+----+------------+--------------------------+--------------------------+-----------------------+\n\n";
}

void CliMonitor::inspectCandidate(Db& discovery_db, int64_t candidate_id) {
    Stmt s(discovery_db, "SELECT id, seed, strategy, source_a, source_b, hypothesis_code, status, "
                         "details, counterexample, error_log, verification_exit_code, created_at "
                         "FROM candidates WHERE id = " + std::to_string(candidate_id));
    if (!s.step()) {
        std::cout << "Candidate ID " << candidate_id << " not found.\n";
        return;
    }

    std::cout << "\n================ CANDIDATE DETAILS ================\n";
    std::cout << "ID:               " << s.getInt64(0) << "\n";
    std::cout << "Seed:             " << s.getInt64(1) << "\n";
    std::cout << "Strategy:         " << s.getText(2) << "\n";
    std::cout << "Source A:         " << s.getText(3) << "\n";
    std::cout << "Source B:         " << s.getText(4) << "\n";
    std::cout << "Status:           " << s.getText(6) << "\n";
    std::cout << "Exit Code:        " << s.getInt64(10) << "\n";
    std::cout << "Details:          " << s.getText(7) << "\n";
    if (!s.getText(8).empty()) {
        std::cout << "Counterexample:   " << s.getText(8) << "\n";
    }
    if (!s.getText(9).empty()) {
        std::cout << "Error/Log:        " << s.getText(9) << "\n";
    }
    std::cout << "\n--- Lean Code ---\n" << s.getText(5) << "\n";
    std::cout << "===================================================\n\n";
}

void CliMonitor::exportCandidates(Db& discovery_db, const fs::path& out_path, const std::string& format) {
    std::ofstream out(out_path, std::ios::binary);
    check(bool(out), "Cannot open export file: " + utf8(out_path));

    Stmt s(discovery_db, "SELECT id, seed, strategy, status, source_a, source_b, hypothesis_code, counterexample FROM candidates ORDER BY id");

    if (format == "csv") {
        out << "id,seed,strategy,status,source_a,source_b,counterexample\n";
        while (s.step()) {
            out << s.getInt64(0) << ","
                << s.getInt64(1) << ",\""
                << s.getText(2) << "\",\""
                << s.getText(3) << "\",\""
                << s.getText(4) << "\",\""
                << s.getText(5) << "\",\""
                << s.getText(7) << "\"\n";
        }
    } else { // JSON format
        out << "[\n";
        bool first = true;
        while (s.step()) {
            if (!first) out << ",\n";
            first = false;
            out << "  {\n"
                << "    \"id\": " << s.getInt64(0) << ",\n"
                << "    \"seed\": " << s.getInt64(1) << ",\n"
                << "    \"strategy\": \"" << s.getText(2) << "\",\n"
                << "    \"status\": \"" << s.getText(3) << "\",\n"
                << "    \"source_a\": \"" << s.getText(4) << "\",\n"
                << "    \"source_b\": \"" << s.getText(5) << "\",\n"
                << "    \"counterexample\": \"" << s.getText(7) << "\"\n"
                << "  }";
        }
        out << "\n]\n";
    }
    std::cout << "Exported candidates to: " << utf8(out_path) << "\n";
}

} // namespace mre
