"""Deterministic regression of independent finite-structure operator exploration."""
import pathlib
import sqlite3
import subprocess
import sys
import tempfile

exe = pathlib.Path(sys.argv[1])
with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as temp:
    outputs = []
    for name in ('a.sqlite', 'b.sqlite'):
        db = pathlib.Path(temp) / name
        proc = subprocess.run([str(exe), 'open-structures', str(db), '30', '42'],
                              capture_output=True, text=True, timeout=90)
        assert proc.returncode == 0, (proc.stdout, proc.stderr)
        assert 'OPEN_STRUCTURES_DONE count=30' in proc.stdout
        with sqlite3.connect(db) as con:
            rows = con.execute('SELECT ordinal,domain,law,operator_rule,parameters,status,trials,matches,counterexample,novelty,proof_status FROM open_structures ORDER BY ordinal').fetchall()
        outputs.append(rows)
    assert outputs[0] == outputs[1], 'same seed must reproduce results'
    rows = outputs[0]
    assert len(rows) == 30
    assert {r[1] for r in rows} == {'ZMod', 'IntegerPair', 'BooleanPair'}
    assert {r[2] for r in rows} == {'associativity','commutativity','zero_is_identity'}
    assert all(r[6] > 0 and 0 <= r[7] <= r[6] for r in rows)
    assert all(r[9] == 'NOT_ASSESSED' and r[10] == 'NOT_ATTEMPTED' for r in rows)
    assert any(r[5] == 'NUMERIC_COUNTEREXAMPLE' and r[8] for r in rows)
    assert any(r[5] == 'FINITE_TESTS_SURVIVED_UNRESOLVED' for r in rows)
print('OPEN_STRUCTURES_TEST_OK: deterministic, three domains, three laws, counterexamples and unresolved statuses')
