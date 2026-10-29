import pathlib, sqlite3, subprocess, sys, tempfile

exe = pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as folder:
    root = pathlib.Path(folder)
    db = root / 'experiments.sqlite'
    kernels = root / 'generated'
    def run(seed):
        proc = subprocess.run([str(exe),'algorithm-lab',str(db),str(kernels),'8',str(seed)],
                              capture_output=True,text=True,timeout=60)
        assert proc.returncode == 0, (proc.stdout, proc.stderr)
        assert 'ALGORITHM_LAB_DONE count=8' in proc.stdout
    run(42)
    with sqlite3.connect(db) as conn:
        rows = conn.execute('select ordinal,a,b,c,d,operator_formula,equivalence,novelty from algorithm_lab order by ordinal').fetchall()
        assert len(rows) == 8
        assert all(row[6] in ('DIFFERENT_OUTPUTS','MATCHED_SAMPLED_TRACE') and row[7] == 'NOT_ASSESSED' for row in rows)
    run(42)
    with sqlite3.connect(db) as conn:
        duplicated = conn.execute('select ordinal,a,b,c,d,operator_formula,equivalence,novelty from algorithm_lab order by id desc limit 8').fetchall()[::-1]
        assert rows == duplicated
    assert len(list(kernels.glob('*.hpp'))) == 8
    header = kernels / 'candidate_42_0.hpp'
    source = root / 'compile_generated.cpp'
    source.write_text('#include "'+header.name+'"\nint main(){return mre_generated::step(3U,4U) < 65521U ? 0 : 1;}\n')
    compiler = sys.argv[2] if len(sys.argv) > 2 else 'c++'
    subprocess.run([compiler,'-std=c++20','-Wall','-Wextra','-Werror','-I',str(kernels),str(source),'-o',str(root/'generated_check')],check=True,timeout=60)
    subprocess.run([str(root/'generated_check')],check=True,timeout=10)
print('ALGORITHM_LAB_TESTS_OK deterministic=true sqlite=true generated_cpp_compiles=true')
