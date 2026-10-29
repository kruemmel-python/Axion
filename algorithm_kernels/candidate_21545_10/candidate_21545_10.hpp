// Generated from measured Algorithm Lab experiment.
// Seed 21545, ordinal 10. No correctness or novelty claim.
#pragma once
#include <cstdint>
namespace mre_generated {
inline constexpr uint32_t modulus = 65521U;
inline uint32_t step(uint32_t state, uint32_t input) {
  const uint64_t x = state % modulus, y = input % modulus;
  return static_cast<uint32_t>((uint64_t{19264} * x + uint64_t{30715} * y + uint64_t{28457} * x * y + 26714U) % modulus);
}
}
