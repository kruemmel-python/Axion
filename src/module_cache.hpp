#pragma once
#include "database.hpp"
#include <unordered_set>
namespace mre {
struct ModuleInventory {
    std::unordered_set<std::string> available;
    uint64_t oleanFiles = 0;
};
ModuleInventory scanOlean(const fs::path& root);
int runModuleInventory(const fs::path& source, const fs::path& output,
                       const fs::path& oleanRoot, const std::string& prefix);
int runSemanticReport(const fs::path& output);
}
