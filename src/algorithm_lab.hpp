#pragma once
#include "database.hpp"
namespace mre {
int runAlgorithmLab(const fs::path& dbPath, const fs::path& artifactDir, int count, uint64_t seed);
}
