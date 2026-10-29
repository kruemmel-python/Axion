#pragma once

#include "common.hpp"

namespace mre {

// Executes the comprehensive 18-point validation test suite
int runSelftest(const fs::path& temp_dir);

} // namespace mre
