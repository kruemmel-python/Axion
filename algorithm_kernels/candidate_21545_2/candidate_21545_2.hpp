// Generated from measured Algorithm Lab experiment.
// Seed 21545, ordinal 2. No correctness or novelty claim.
#pragma once
#include <cstdint>
namespace mre_generated {
inline constexpr uint32_t modulus = 65521U;
inline uint32_t step(uint32_t state, uint32_t input) {
  const uint64_t x = state % modulus, y = input % modulus;
  return static_cast<uint32_t>((uint64_t{29556} * x + uint64_t{30234} * y + uint64_t{10602} * x * y + 50457U) % modulus);
}
}
