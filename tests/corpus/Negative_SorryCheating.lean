-- Negative Test Case: Unproven assertion disguised with sorry
theorem test_sorry_cheating (n : Nat) : n + 100 = 0 := by
  sorry
