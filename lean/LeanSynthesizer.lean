import Lean

open Lean Meta

/-!
Lean-native, source-bound synthesis probe.
Usage:
 lean --run lean/LeanSynthesizer.lean Module.A Declaration.a Module.B Declaration.b
The Lean search environment (LEAN_PATH / lake env) must include both modules.
Prints one JSON object per direction, with actual elaborated types.
No result is reported as a novel theorem or automatically a proof.
-/

private def resultJson (relation lhs rhs : String) (ok : Bool)
    (termType detail : String) : Json :=
  Json.mkObj [
    ("relation", Json.str relation),
    ("source_a", Json.str lhs),
    ("source_b", Json.str rhs),
    ("type_correct", Json.bool ok),
    ("result_type", Json.str termType),
    ("detail", Json.str detail),
    ("formal_proof", Json.bool false)
  ]

private def tryApply (lhs rhs : Name) : MetaM Json := do
  let env ← getEnv
  let some _ := env.find? lhs |
    return resultJson "application" lhs.toString rhs.toString false "" "source_a_not_found"
  let some _ := env.find? rhs |
    return resultJson "application" lhs.toString rhs.toString false "" "source_b_not_found"
  try
    let fn ← mkConstWithFreshMVarLevels lhs
    let arg ← mkConstWithFreshMVarLevels rhs
    let fnTy ← whnf (← inferType fn)
    match fnTy with
    | .forallE _ expected _ _ =>
      let argTy ← inferType arg
      if ← isDefEq expected argTy then
        let applied := mkApp fn arg
        let resultTy ← inferType applied
        let s ← ppExpr resultTy
        return resultJson "application" lhs.toString rhs.toString true (toString s) "checked_by_Lean_MetaM"
      else
        let e ← ppExpr expected
        let a ← ppExpr argTy
        return resultJson "application" lhs.toString rhs.toString false ""
          s!"argument_type_mismatch: expected {e}; actual {a}"
    | _ =>
      return resultJson "application" lhs.toString rhs.toString false "" "lhs_is_not_a_function"
  catch _ =>
    return resultJson "application" lhs.toString rhs.toString false "" "lean_meta_exception_during_application"

private def runPairs (a b : Name) : MetaM (Array Json) := do
  let ab ← tryApply a b
  let ba ← tryApply b a
  return #[ab, ba]

def main (args : List String) : IO UInt32 := do
  match args with
  | [ma, a, mb, b] =>
    try
      let modules := #[ma.toName, mb.toName]
      let env ← importModules (modules.map fun m => { module := m }) {}
      let ctx : Core.Context := { fileName := "<synthesis>", fileMap := FileMap.ofString "" }
      let state : Core.State := { env := env }
      let (results, _) ← (MetaM.run' (runPairs a.toName b.toName)).toIO ctx state
      for r in results do
        IO.println r.compress
      return 0
    catch err =>
      IO.eprintln s!"SYNTHESIS_INFRASTRUCTURE_ERROR: {err}"
      return 1
  | _ =>
    IO.eprintln "Usage: LeanSynthesizer Module.A Name.a Module.B Name.b"
    return 2
