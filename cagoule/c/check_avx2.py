import subprocess, sys, tempfile, os

src = '#include <immintrin.h>\nint main(){__m256i x=_mm256_setzero_si256();(void)x;return 0;}\n'
with tempfile.NamedTemporaryFile(suffix='.c', mode='w', delete=False) as f:
    f.write(src)
    fname = f.name

try:
    r = subprocess.run(
        ['gcc', '-mavx2', fname, '-o', os.devnull],
        capture_output=True
    )
    print('yes' if r.returncode == 0 else 'no')
finally:
    os.unlink(fname)