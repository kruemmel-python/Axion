#pragma once
#include "database.hpp"
namespace mre {
struct DatabaseFirstConfig {
    fs::path source_db, research_db, workspace, lean_exe, lean_script, lean_root;
    std::string gemini_url, model = "gemini-3.8-flash", provider = "gemini";
    uint64_t seed = 1; int iterations = 1; int sample_limit = 256;
    uint32_t timeout_ms = 320000; uint32_t llm_timeout_ms = 1800000; bool require_gemini = false; bool require_model = false;
};
int runDatabaseFirstSoftwareResearch(const DatabaseFirstConfig& config);
int completeDatabaseFirstSoftwareResearch(const DatabaseFirstConfig& config);
}
