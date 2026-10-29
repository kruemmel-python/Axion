import Lean

open Lean Meta

def binderToStr (b : BinderInfo) : String :=
  match b with
  | .default => "default"
  | .implicit => "implicit"
  | .strictImplicit => "strictImplicit"
  | .instImplicit => "instImplicit"

def extractDecl (nameStr : String) : MetaM (Option Json) := do
  let env ← getEnv
  let name := nameStr.toName
  let some ci := env.find? name | return none
  
  let kind := match ci with
    | .axiomInfo _ => "axiom"
    | .defnInfo _ => "def"
    | .thmInfo _ => "theorem"
    | .opaqueInfo _ => "opaque"
    | .quotInfo _ => "quot"
    | .inductInfo _ => "inductive"
    | .ctorInfo _ => "constructor"
    | .recInfo _ => "recursor"

  forallTelescope ci.type fun xs body => do
    let mut paramsArr : Array Json := #[]
    let mut premisesArr : Array Json := #[]
    for x in xs do
      let fvarId := x.fvarId!
      let decl ← fvarId.getDecl
      let typeFmt ← ppExpr decl.type
      let isPropVal ← isProp decl.type
      let pObj := Json.mkObj [
        ("name", Json.str decl.userName.toString),
        ("type", Json.str (toString typeFmt)),
        ("binder", Json.str (binderToStr decl.binderInfo)),
        ("is_prop", Json.bool isPropVal)
      ]
      paramsArr := paramsArr.push pObj
      if isPropVal && decl.binderInfo == .default then
        premisesArr := premisesArr.push (Json.str (toString typeFmt))

    let conclusionFmt ← ppExpr body
    let conclusionIsProp ← isProp body

    let uParams := ci.levelParams.map (fun l => Json.str l.toString)
    let usedConsts := ci.type.getUsedConstants.map (fun c => Json.str c.toString)

    let result := Json.mkObj [
      ("name", Json.str name.toString),
      ("kind", Json.str kind),
      ("type_raw", Json.str (toString (← ppExpr ci.type))),
      ("universe_params", Json.arr uParams.toArray),
      ("param_count", Json.num paramsArr.size),
      ("params", Json.arr paramsArr),
      ("premises", Json.arr premisesArr),
      ("conclusion", Json.str (toString conclusionFmt)),
      ("is_prop", Json.bool conclusionIsProp),
      ("constants", Json.arr usedConsts)
    ]
    return some result

def runExtraction (names : List String) : MetaM (Array Json) := do
  let mut results : Array Json := #[]
  for n in names do
    if let some j ← extractDecl n then
      results := results.push j
  return results

def main (args : List String) : IO UInt32 := do
  -- Usage: lean --run LeanExtractor.lean --module Foo.Bar Name.one Name.two
  -- An explicit import is essential: declarations outside Init do not exist
  -- in a fresh Lean environment until their defining modules are loaded.
  let (modules, names) := match args with
    | "--module" :: moduleName :: rest => ([moduleName.toName], rest)
    | _ => ([`Init], args)
  if names.isEmpty then
    IO.eprintln "Usage: LeanExtractor --module Module.Name Declaration.Name ..."
    return 2
  let env ← importModules (modules.toArray.map fun m => { module := m }) {}
  let coreContext : Core.Context := {
    fileName := "<extractor>",
    fileMap := FileMap.ofString ""
  }
  let coreState : Core.State := { env := env }
  
  let (results, _) ← (MetaM.run' (runExtraction names)).toIO coreContext coreState
  for r in results do
    IO.println r.compress
  return 0
