"""Read-only acceptance check for the completed seed-314159 run."""
import sqlite3
from pathlib import Path

DB = Path(__file__).parents[1] / "research" / "database_first_seed314159.sqlite"

def main() -> int:
    con = sqlite3.connect(DB)
    coverage = con.execute("select source_count,indexed_count from coverage").fetchone()
    assert coverage == (2369933, 2369933), coverage
    assert con.execute("select count(*) from selections where run_id=1").fetchone()[0] == 2
    assert con.execute("select seed,status from research_runs where id=1").fetchone() == (314159, "NOT_ATTEMPTED")
    assert con.execute("select lean_status from hypotheses where run_id=1").fetchone()[0] == "NOT_ATTEMPTED"
    print("DATABASE_FIRST_ACCEPTANCE_OK")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
