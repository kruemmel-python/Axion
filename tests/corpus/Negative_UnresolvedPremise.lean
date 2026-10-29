-- Negative Test Case: Unresolved premise that cannot be discharged
axiom P_unproven : Prop
axiom Q_target : Prop
axiom premise_rule : P_unproven → Q_target

-- Without having proven P_unproven, Q_target cannot be derived
theorem goal_requires_p : Q_target := by
  apply premise_rule
  -- Stuck here: P_unproven cannot be discharged!
