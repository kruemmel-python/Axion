-- Positive Test Case: Function composition and transitivity
def inc (x : Nat) : Nat := x + 1
def dbl (x : Nat) : Nat := x * 2

theorem test_comp (x : Nat) : (dbl ∘ inc) x = (x + 1) * 2 := by
  rfl

#print axioms test_comp
