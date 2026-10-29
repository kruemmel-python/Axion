#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace mre::k4d {
int run(const std::string& output, int side, int steps, std::uint64_t seed);
int selftest();
struct Observation { std::uint64_t signature; double activity; double intervention_effect; std::size_t unique; std::size_t revisits; };
Observation observe(int side, int steps, std::uint64_t seed, bool coupled);

struct DynamicsStep {
    std::uint64_t fingerprint = 0;
    double sector_activity[4] = {0.0, 0.0, 0.0, 0.0};
    double perturbation_distance = 0.0;
    bool is_revisit = false;
};

std::vector<DynamicsStep> traceDynamics(int side, int steps, std::uint64_t seed, bool coupled);
}

