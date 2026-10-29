#pragma once
#include "database.hpp"
namespace mre {
int runOpenResearch(const fs::path& resultDb, int count, uint64_t seed,
                    const fs::path& leanExe);
}
