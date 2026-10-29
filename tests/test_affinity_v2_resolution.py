"""Offline regression for distinct import module and fully-qualified declaration names.
No Lean runtime required: confirms persisted C++ resolver output only.
"""
import os
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path

exe = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='affinity_v2_') as td:
    root = Path(td)
    (root / 'OAI/Algebra/AffineCancellation').mkdir(parents=True)
    file = root / 'OAI/Algebra/AffineCancellation/LaurentEuler.lean'
    file.write_text('import Mathlib\nnamespace OAI\nnamespace ComplexCancellation.LaurentEuler\ndef linear := 1\nlemma linear_mul : True := by trivial\nend ComplexCancellation.LaurentEuler\nend OAI\n')
    source = root / 'source.sqlite'
    dest = root / 'results.sqlite'
    with sqlite3.connect(source) as db:
        db.execute('CREATE TABLE declarations(file TEXT, line INTEGER, kind TEXT, name TEXT, statement TEXT)')
        db.executemany('INSERT INTO declarations VALUES(?,?,?,?,?)', [
            ('OAI/Algebra/AffineCancellation/LaurentEuler.lean', 4, 'def', 'linear', ''),
            ('OAI/Algebra/AffineCancellation/LaurentEuler.lean', 5, 'lemma', 'linear_mul', ''),
        ])
    lean = root / 'lean_stub'
    lean.write_text('#!/bin/sh\nexit 0\n')
    os.chmod(lean, 0o700)
    script = root / 'Synth.lean'
    script.write_text('-- stub')
    env = dict(os.environ, MRE_OAI_SOURCE_ROOT=str(root))
    p = subprocess.run([str(exe), 'semantic-batch', str(source), str(dest), str(script), str(lean),
                        '2', '42', 'OAI.Algebra.AffineCancellation.', '5000'],
                       env=env, capture_output=True, text=True)
    assert p.returncode == 0, (p.stdout, p.stderr)
    with sqlite3.connect(dest) as db:
        rows = db.execute('SELECT qualified_a,qualified_b,resolution_a,resolution_b FROM semantic_probe_identity').fetchall()
    assert len(rows) == 2, rows
    assert all(r[0].startswith('OAI.ComplexCancellation.LaurentEuler.') and
               r[1].startswith('OAI.ComplexCancellation.LaurentEuler.') and
               r[2] == 'source_namespace_resolved' and r[3] == 'source_namespace_resolved' for r in rows), rows
    print('AFFINITY_V2_RESOLUTION_OK: import path separated from real namespace, recorded in SQLite')
