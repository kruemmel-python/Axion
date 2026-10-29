"""Black-box tests for the independent mathematical exploration mode."""
import pathlib
import sqlite3
import subprocess
import sys
import tempfile

exe = pathlib.Path(sys.argv[1])
with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as d:
    a = pathlib.Path(d) / 'a.sqlite'
    b = pathlib.Path(d) / 'b.sqlite'
    for db in (a, b):
        p = subprocess.run([str(exe), 'open-explore', str(db), '25', '42'],
                           capture_output=True, text=True, timeout=90)
        assert p.returncode == 0, (p.stdout, p.stderr)
        assert 'OPEN_RESEARCH_DONE count=25' in p.stdout
    def read(db):
        with sqlite3.connect(db) as con:
            return con.execute('SELECT ordinal,lane,lhs,rhs,status,numeric_tests,agreement_points,counterexample,novelty FROM open_research ORDER BY ordinal').fetchall()
    x, y = read(a), read(b)
    assert x == y, 'Deterministic seed changed exploration'
    assert len(x) == 25
    assert len({r[1] for r in x}) == 5
    assert len({(r[2], r[3]) for r in x}) == 25
    assert all(r[5] == 343 and 0 <= r[6] <= 343 for r in x)
    assert all(r[8] == 'NOT_ASSESSED' for r in x)
    assert any(r[4] == 'NUMERIC_COUNTEREXAMPLE' and r[7] for r in x)
    assert any(r[4] == 'NUMERIC_SURVIVED_UNRESOLVED' and not r[7] for r in x)
print('OPEN_RESEARCH_TEST_OK: five lanes, deterministic output, numeric counterexamples, unresolved preservation, novelty not claimed')
