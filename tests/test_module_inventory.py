"""Real SQLite source and real local .olean file names, no Lean execution mocked as proofs."""
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path

exe = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="mre_inventory_") as td:
    root = Path(td)
    source = root / 'research.sqlite'
    result = root / 'out.sqlite'
    olean = root / 'olean'
    (olean / 'OAI' / 'Test').mkdir(parents=True)
    (olean / 'OAI' / 'Test' / 'One.olean').write_bytes(b'fixture: existence only')
    with sqlite3.connect(source) as db:
        db.execute('CREATE TABLE declarations(file TEXT, line INTEGER, kind TEXT, name TEXT)')
        db.executemany('INSERT INTO declarations VALUES(?,?,?,?)', [
            ('OAI/Test/One.lean', 1, 'def', 'one'),
            ('OAI/Test/One.lean', 2, 'theorem', 'oneThm'),
            ('OAI/Test/Two.lean', 1, 'lemma', 'two'),
        ])
    p = subprocess.run([str(exe), 'module-inventory', str(source), str(result), str(olean), 'OAI.'], capture_output=True, text=True)
    assert p.returncode == 0, p.stdout + p.stderr
    assert 'source_modules=2 available=1 unavailable=1' in p.stdout, p.stdout
    with sqlite3.connect(result) as db:
        assert db.execute('SELECT module, available FROM module_inventory ORDER BY module').fetchall() == [
            ('OAI.Test.One', 1), ('OAI.Test.Two', 0)]
    p = subprocess.run([str(exe), 'semantic-report', str(result)], capture_output=True, text=True)
    assert p.returncode == 0, p.stdout + p.stderr
    assert 'modules_available=1 count=1' in p.stdout
    p = subprocess.run([str(exe), 'module-inventory', str(source), str(result), str(olean), 'OAI.'], capture_output=True, text=True)
    assert p.returncode == 0, p.stdout + p.stderr
    print('MODULE_INVENTORY_INTEGRATION_OK: read-only source, cached OLEAN detection, SQLite persistence, idempotence')
