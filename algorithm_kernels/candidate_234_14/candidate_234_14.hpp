// Generated from measured Algorithm Lab experiment.
// Seed 234, ordinal 14. No correctness or novelty claim.
#pragma once
#include <cstdint>
namespace mre_generated {
inline constexpr uint32_t modulus = 65521U;
inline uint32_t step(uint32_t state, uint32_t input) {
  const uint64_t x = state % modulus, y = input % modulus;
  return static_cast<uint32_t>((uint64_t{51847} * x + uint64_t{10566} * y + uint64_t{16023} * x * y + 16602U) % modulus);
}
}
