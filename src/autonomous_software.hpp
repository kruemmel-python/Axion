#pragma once

#include "database.hpp"

namespace mre {

struct SoftwareResearchConfig {
    fs::path memory_db;
    fs::path workspace;
    // Optional: Proven theorem / mathematical algorithm input from MRE
    fs::path candidates_db;
    int64_t candidate_id = 0;
    std::string lm_studio_url;
    std::string model = "local-coding-model";
    std::string application_type = "auto"; // "auto", "console", "win32_gui"
    int iterations = 1;
    uint64_t seed = 1;
    uint32_t build_timeout_ms = 120000;
    bool require_lm_studio = false;
};

int runAutonomousSoftwareResearch(const SoftwareResearchConfig& config);
int runAutonomousSoftwareSynthesis(const SoftwareResearchConfig& config);
int runSoftwareLabSelftest(const fs::path& temp_dir);

} // namespace mre
