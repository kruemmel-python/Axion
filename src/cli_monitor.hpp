#pragma once

#include "database.hpp"

namespace mre {

class CliMonitor {
public:
    static void printBanner();
    static void printDbStats(Db& db, const std::string& label);
    static void renderDashboard(Db& discovery_db);
    static void inspectCandidate(Db& discovery_db, int64_t candidate_id);
    static void exportCandidates(Db& discovery_db, const fs::path& out_path, const std::string& format = "json");
};

} // namespace mre
