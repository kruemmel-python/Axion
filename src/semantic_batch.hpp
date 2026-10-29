#pragma once
#include "database.hpp"
namespace mre {
int runSemanticBatch(const fs::path& source, const fs::path& output,
                     const fs::path& script, const fs::path& lean,
                     int limit, uint64_t seed, const std::string& modulePrefix,
                     uint32_t timeoutMs);
}
