-- Positive Test Case: Theorem with resolved premise
theorem test_premise_resolved (n : Nat) (h : n ≤ n) : n ≤ n := by
  exact h

-- Instantiation where premise (n ≤ n) is automatically discharged via Nat.le_refl
theorem test_discharged (n : Nat) : n ≤ n := by
  apply test_premise_resolved
  exact Nat.le_refl n

#print axioms test_discharged
