#!/usr/bin/env python3
"""Select OAI modules from existing SQLite index (read-only), handling Windows paths."""
import argparse
import pathlib
import sqlite3
import sys


def normalize_module(file_value: object) -> str | None:
    """Accept relative, drive-qualified, and backslash-separated Lean source paths."""
    if not isinstance(file_value, str):
        return None
    path = file_value.strip().replace('\\', '/')
    segments = [segment for segment in path.split('/') if segment]
    try:
        pos = segments.index('OAI')
    except ValueError:
        return None
    relative = segments[pos:]
    if len(relative) < 2 or not relative[-1].endswith('.lean'):
        return None
    if any(not part or part in ('.', '..') for part in relative):
        return None
    if any('Certificate' in part or 'Generated' in part for part in relative):
        return None
    relative[-1] = relative[-1][:-5]
    if not all(part.replace('_', '').isalnum() for part in relative):
        return None
    return '.'.join(relative)


def select(db_path: pathlib.Path, limit: int) -> list[str]:
    if limit < 1:
        raise ValueError('--limit must be positive')
    uri = db_path.resolve().as_uri() + '?mode=ro'
    with sqlite3.connect(uri, uri=True) as con:
        con.execute('PRAGMA query_only=ON')
        tables = {row[0] for row in con.execute("SELECT name FROM sqlite_master WHERE type='table'")}
        if 'declarations' not in tables:
            raise RuntimeError('research database has no declarations table')
        cols = {row[1] for row in con.execute('PRAGMA table_info(declarations)')}
        if 'file' not in cols:
            raise RuntimeError('declarations table has no file column')
        # Paths in the v0.2 index may use Windows backslashes and/or drive prefixes.
        # DISTINCT keeps processing bounded by file count, not declaration count.
        rows = con.execute("SELECT DISTINCT file FROM declarations WHERE file LIKE '%OAI%' ORDER BY file")
        found, seen = [], set()
        for (raw,) in rows:
            module = normalize_module(raw)
            if module is None or module in seen:
                continue
            found.append(module)
            seen.add(module)
            if len(found) >= limit:
                break
        return found


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--db', type=pathlib.Path, required=True)
    parser.add_argument('--limit', type=int, required=True)
    args = parser.parse_args()
    try:
        modules = select(args.db, args.limit)
    except (OSError, ValueError, RuntimeError, sqlite3.Error) as exc:
        print(f'Module selection failed: {exc}', file=sys.stderr)
        return 2
    if not modules:
        print('No OAI modules found; inspect declarations.file path format.', file=sys.stderr)
        return 3
    for module in modules:
        print(module)
    return 0


if __name__ == '__main__':
    sys.exit(main())
