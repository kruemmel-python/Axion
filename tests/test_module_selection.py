"""Run via python tests/test_module_selection.py; protects real DB from writes."""
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path

script = Path(__file__).resolve().parents[1] / 'tools' / 'select_oai_modules.py'
with tempfile.TemporaryDirectory() as tmp:
    db = Path(tmp) / 'research.sqlite'
    with sqlite3.connect(db) as con:
        con.execute('CREATE TABLE declarations (file TEXT, name TEXT)')
        con.executemany('INSERT INTO declarations VALUES (?,?)', [
            (r'OAI\\Algebra\\Foo.lean', 'one'),
            (r'F:\\math-main\\lean\\OAI\\Topology\\Bar.lean', 'two'),
            ('OAI/Algebra/Foo.lean', 'dupe'),
            ('OAI/Combinatorics/Certificate/Part.lean', 'exclude'),
            ('Mathlib/Data/Nat/Basic.lean', 'no'),
        ])
    result = subprocess.run([sys.executable, str(script), '--db', str(db), '--limit', '5'], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    actual = set(result.stdout.splitlines())
    assert actual == {'OAI.Algebra.Foo', 'OAI.Topology.Bar'}, actual
    with sqlite3.connect(db) as con:
        assert con.execute('SELECT COUNT(*) FROM declarations').fetchone()[0] == 5
    print('MODULE_SELECTION_OK: Windows, absolute, relative, duplicates, certificate exclusion')
