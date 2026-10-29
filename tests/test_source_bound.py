"""Regression: discoveries must depend on the selected source declarations."""
import pathlib
import sqlite3
import subprocess
import sys
import tempfile

binary = pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="mre-source-") as root:
    root = pathlib.Path(root)
    source = root / "source.sqlite"
    output = root / "discovery.sqlite"
    with sqlite3.connect(source) as db:
        db.execute("CREATE TABLE declarations(file TEXT, line INTEGER, kind TEXT, name TEXT, statement TEXT)")
        for n in range(100):
            db.execute("INSERT INTO declarations VALUES(?,?,?,?,?)", (
                f"OAI/Algebra/Example{n}.lean", n, "theorem", f"example_{n}",
                f"theorem example_{n} : True := by trivial"))
    result = subprocess.run([str(binary), "discover", str(source), str(output), "8", "12345", "random"],
                            text=True, capture_output=True)
    assert result.returncode == 0, result.stdout + result.stderr
    with sqlite3.connect(output) as db:
        rows = db.execute("SELECT source_a, source_b, hypothesis_code, status, premises_status FROM candidates").fetchall()
    assert len(rows) == 8, len(rows)
    for a, b, code, status, premises in rows:
        assert a in code and b in code, (a, b, code)
        assert "MRE_EXPLORATORY_ONLY" in code
        assert "Nat.add_comm" not in code and "Nat.zero_le" not in code
        assert status == "SOURCE_ONLY" and premises == "NOT_EXTRACTED"
    print("PASS: 8 source-bound candidates, no fabricated mathematical proofs")
