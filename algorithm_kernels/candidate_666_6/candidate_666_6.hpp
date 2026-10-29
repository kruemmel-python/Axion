// Generated from measured Algorithm Lab experiment.
// Seed 666, ordinal 6. No correctness or novelty claim.
#pragma once
#include <cstdint>
namespace mre_generated {
inline constexpr uint32_t modulus = 65521U;
inline uint32_t step(uint32_t state, uint32_t input) {
  const uint64_t x = state % modulus, y = input % modulus;
  return static_cast<uint32_t>((uint64_t{64499} * x + uint64_t{4242} * y + uint64_t{51521} * x * y + 7599U) % modulus);
}
}
