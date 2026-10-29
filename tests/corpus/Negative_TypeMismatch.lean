-- Negative Test Case: Incompatible types
def f_nat (x : Nat) : Nat := x + 1

-- Attempting to pass a String to Nat function must trigger elaboration error
def bad_apply : Nat := f_nat "not a nat"
