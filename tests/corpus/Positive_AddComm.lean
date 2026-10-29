-- Positive Test Case: Valid theorem with complete formal proof
theorem test_add_comm (n m : Nat) : n + m = m + n := by
  exact Nat.add_comm n m

#print axioms test_add_comm
