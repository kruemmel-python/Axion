// Generated from measured Algorithm Lab experiment.
// Seed 234, ordinal 13. No correctness or novelty claim.
#pragma once
#include <cstdint>
namespace mre_generated {
inline constexpr uint32_t modulus = 65521U;
inline uint32_t step(uint32_t state, uint32_t input) {
  const uint64_t x = state % modulus, y = input % modulus;
  return static_cast<uint32_t>((uint64_t{19121} * x + uint64_t{34513} * y + uint64_t{16581} * x * y + 46763U) % modulus);
}
}
