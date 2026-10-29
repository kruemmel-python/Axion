// Generated from measured Algorithm Lab experiment.
// Seed 234, ordinal 7. No correctness or novelty claim.
#pragma once
#include <cstdint>
namespace mre_generated {
inline constexpr uint32_t modulus = 65521U;
inline uint32_t step(uint32_t state, uint32_t input) {
  const uint64_t x = state % modulus, y = input % modulus;
  return static_cast<uint32_t>((uint64_t{40647} * x + uint64_t{4701} * y + uint64_t{61202} * x * y + 29372U) % modulus);
}
}
