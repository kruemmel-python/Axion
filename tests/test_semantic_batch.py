"""Integration test for persisted, source-bound semantic probes.
A deterministic stub models Lean's output; this tests the C++ orchestration,
NOT Lean's theorem prover. The real Lean test is TEST_SEMANTIC_BATCH_WINDOWS.ps1.
"""
import os
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path

exe = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="mre_semantic_") as td:
    root = Path(td)
    src, out = root / "research.sqlite", root / "results.sqlite"
    with sqlite3.connect(src) as db:
        db.execute("CREATE TABLE declarations(file TEXT, line INTEGER, kind TEXT, name TEXT, statement TEXT)")
        db.executemany("INSERT INTO declarations VALUES (?,?,?,?,?)", [
            ("Init.lean", 1, "def", "Nat.succ", ""),
            ("Init.lean", 2, "def", "Nat.zero", ""),
            ("Init.lean", 3, "def", "Nat.pred", ""),
            ("OAI/Combinatorics/Ramsey/Construction/Certificates11.lean", 4, "lemma", "fake", ""),
        ])
    stub = root / "stub_lean"
    stub.write_text("#!/bin/sh\nprintf '%s\\n' '{\"type_correct\":true,\"formal_proof\":false}'\n", encoding="utf-8")
    os.chmod(stub, 0o700)
    lean_src = root / "LeanSynthesizer.lean"
    lean_src.write_text("-- only needed for path validation\n", encoding="utf-8")
    command = [str(exe), "semantic-batch", str(src), str(out), str(lean_src), str(stub), "2", "17", "Init", "5000"]
    first = subprocess.run(command, capture_output=True, text=True, check=False)
    assert first.returncode == 0, first.stdout + first.stderr
    assert "formal_proofs=0" in first.stdout, first.stdout
    with sqlite3.connect(out) as db:
        rows = db.execute("SELECT module_a,name_a,module_b,name_b,status,lean_output FROM semantic_probe_results ORDER BY id").fetchall()
    assert len(rows) == 2, rows
    with sqlite3.connect(out) as db:
        ranking = db.execute("SELECT strategy, score, run_tag FROM semantic_probe_ranking ORDER BY probe_id").fetchall()
    assert len(ranking) == 2, ranking
    assert all(v[0] == "affinity_v2" and v[1] >= 0 and v[2] for v in ranking), ranking
    assert all(row[4] == "INFRASTRUCTURE_ERROR" for row in rows), rows
    # Linux subprocess execution is not implemented in the pre-existing engine.
    # This test therefore covers selection, provenance, error classification and persistence only.
    assert all(row[0] == "Init" and row[2] == "Init" for row in rows), rows
    assert all("fake" not in str(row) for row in rows), rows
    print("SEMANTIC_BATCH_INTEGRATION_OK: source-derived pairs and infrastructure errors persisted; no proof claim")
