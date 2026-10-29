// Generated from measured Algorithm Lab experiment.
// Seed 42, ordinal 22. No correctness or novelty claim.
#pragma once
#include <cstdint>
namespace mre_generated {
inline constexpr uint32_t modulus = 65521U;
inline uint32_t step(uint32_t state, uint32_t input) {
  const uint64_t x = state % modulus, y = input % modulus;
  return static_cast<uint32_t>((uint64_t{36729} * x + uint64_t{14481} * y + uint64_t{63521} * x * y + 19202U) % modulus);
}
}
