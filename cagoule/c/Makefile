# Makefile — CAGOULE v3.1.0 libcagoule.so
#
# Cibles principales :
#   make              → compile libcagoule.so (AVX2 si disponible)
#   make tests        → exécute tous les tests C (scalaires + AVX2 + CTR)
#   make test-avx2    → tests AVX2 uniquement
#   make test-scalar  → force le chemin scalaire (CI sans AVX2)
#   make test-ctr     → tests CTR uniquement (v3.1.0)
#   make test-format  → tests format CGL1 (v3.1.0)
#   make test-ct      → test constant-time (dudect)
#   make valgrind     → vérifie les fuites mémoire (inclut CTR)
#   make debug        → compile avec ASan + UBSan
#   make fuzz         → libFuzzer 1M runs (CBC + CTR)
#   make install      → copie dans le package Python
#   make clean        → nettoie
#
# Nouveautés v3.0.0 :
#   - cagoule_ctr.c : mode CTR avec pipeline 4x
#   - test_ctr / test_format : nouveaux binaires de test
#   - fuzz_cipher : support CTR (mode bit 2)
#   - cagoule_cipher.h : constantes CGL1_VERSION_CBC/CTR
#   - Makefile : cibles test-ctr, test-format, test-ct

CC       = gcc
# -O3 (vs -O2) est intentionnel pour une bibliothèque cryptographique :
#   - déroulage de boucles (-funroll-loops implicite) → critique pour les
#     boucles de 4 iterations dans les fonctions SIMD (sbox, rk_add, etc.)
#   - inlining plus agressif des helpers static inline
#   - meilleure allocation de registres pour les registres YMM
# -funroll-loops : déroulage explicite des boucles N=16 (4 iterations de 4)
CFLAGS   = -O3 -funroll-loops -std=c99 -Wall -Wextra -fPIC -Iinclude
CFLAGS  += -D_POSIX_C_SOURCE=200809L
CFLAGS  += -Wno-aggressive-loop-optimizations
LDFLAGS  = -shared
LDLIBS   = -lm -lcrypto -largon2

PYTHON  ?= python3

ifeq ($(shell uname -m),x86_64) 
    CFLAGS += -march=native
endif

# Détection AVX2
HAS_AVX2 := $(shell $(PYTHON) $(dir $(firstword $(MAKEFILE_LIST)))check_avx2.py 2>/dev/null || echo no)

# Détection __uint128_t
HAS_UINT128 := $(shell echo 'int main() { __uint128_t x = 0; return 0; }' | \
               $(CC) -x c -o /dev/null - 2>/dev/null && echo yes || echo no)
ifeq ($(HAS_UINT128),no)
    $(error Ce compilateur ne supporte pas __uint128_t. GCC >= 4.6 requis)
endif

# Détection OpenSSL
HAS_OPENSSL := $(shell pkg-config --exists openssl 2>/dev/null && echo yes || echo no)
ifeq ($(HAS_OPENSSL),yes)
    CFLAGS  += $(shell pkg-config --cflags openssl)
    LDLIBS  += $(shell pkg-config --libs-only-l openssl)
else
    $(warning OpenSSL non trouvé via pkg-config, utilisation de -lcrypto direct)
endif

# ── Sources de base ─────────────────────────────────────────────────────
SRCS     = src/cagoule_math.c \
           src/cagoule_matrix.c \
           src/cagoule_sbox.c   \
           src/cagoule_cipher.c \
           src/cagoule_ctr.c \
           src/cagoule_omega.c \
           src/cagoule_api.c \
           src/cagoule_kdf.c \
           src/cagoule_params.c \
           src/cagoule_stream.c

# ── ARM NEON (v3.1.0 Feature 3) ─────────────────────────────────────
ifeq ($(shell uname -m),aarch64)
    SRCS += src/cagoule_matrix_neon.c
endif

# ── Modules AVX2 (compilés avec -mavx2 séparément) ─────────────────────
ifeq ($(HAS_AVX2),yes)
    SRCS       += src/cagoule_matrix_avx2.c
    SRCS       += src/cagoule_sbox_avx2.c
    CFLAGS_AVX2 = -mavx2
    AVX2_STATUS = actif (matrice + S-box)
else
    CFLAGS_AVX2 =
    AVX2_STATUS = absent (fallback scalaire)
endif

OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)
LIB  = libcagoule.so

# ── Règles spéciales AVX2 ───────────────────────────────────────────────
src/cagoule_matrix_avx2.o: src/cagoule_matrix_avx2.c
	$(CC) $(CFLAGS) $(CFLAGS_AVX2) -MMD -c $< -o $@

src/cagoule_sbox_avx2.o: src/cagoule_sbox_avx2.c
	$(CC) $(CFLAGS) $(CFLAGS_AVX2) -MMD -c $< -o $@

src/cagoule_cipher.o: src/cagoule_cipher.c
	$(CC) $(CFLAGS) $(CFLAGS_AVX2) -MMD -c $< -o $@


.PHONY: all tests test-avx2 test-scalar test-ctr test-format test-ct \
        bench clean install check-openssl debug valgrind help sysinfo \
        fuzz fuzz-long

all: $(LIB)
	@echo "  AVX2 : $(AVX2_STATUS)"

$(LIB): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	@echo "✓ $(LIB) compilé (CAGOULE v3.1.0)"

%.o: %.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

-include $(DEPS)

tests: $(LIB) test_math test_matrix test_sbox test_cipher test_omega test_ctr test_format \
             test_math_avx2 test_matrix_avx2 test_sbox_avx2 test_cipher_pipeline4 test_mersenne
	@echo ""
	@echo "══════════════════════════════════════════════════════"
	@echo "  Exécution des tests CAGOULE v3.1.0 (scalaires + AVX2 + CTR)"
	@echo "══════════════════════════════════════════════════════"
	@status=0; \
	./test_mersenne        && echo "✓ test_mersenne        OK" || { echo "✗ test_mersenne        FAIL"; status=1; }; \
	./test_math            && echo "✓ test_math            OK" || { echo "✗ test_math            FAIL"; status=1; }; \
	./test_matrix          && echo "✓ test_matrix          OK" || { echo "✗ test_matrix          FAIL"; status=1; }; \
	./test_sbox            && echo "✓ test_sbox            OK" || { echo "✗ test_sbox            FAIL"; status=1; }; \
	./test_cipher          && echo "✓ test_cipher          OK" || { echo "✗ test_cipher          FAIL"; status=1; }; \
	./test_omega           && echo "✓ test_omega           OK" || { echo "✗ test_omega           FAIL"; status=1; }; \
	./test_ctr             && echo "✓ test_ctr             OK" || { echo "✗ test_ctr             FAIL"; status=1; }; \
	./test_format          && echo "✓ test_format          OK" || { echo "✗ test_format          FAIL"; status=1; }; \
	./test_math_avx2       && echo "✓ test_math_avx2       OK" || { echo "✗ test_math_avx2       FAIL"; status=1; }; \
	./test_matrix_avx2     && echo "✓ test_matrix_avx2     OK" || { echo "✗ test_matrix_avx2     FAIL"; status=1; }; \
	./test_sbox_avx2       && echo "✓ test_sbox_avx2       OK" || { echo "✗ test_sbox_avx2       FAIL"; status=1; }; \
	./test_cipher_pipeline4 && echo "✓ test_cipher_pipeline4 OK" || { echo "✗ test_cipher_pipeline4 FAIL"; status=1; }; \
	echo "══════════════════════════════════════════════════════"; \
	[ $$status -eq 0 ] && echo "  ✅ Tous les tests passent (v3.1.0)" || echo "  ❌ Certains tests ont échoué"; \
	echo "══════════════════════════════════════════════════════"; \
	exit $$status

test-avx2: $(LIB) test_math_avx2 test_matrix_avx2 test_sbox_avx2 test_mersenne
	./test_mersenne && ./test_math_avx2 && ./test_matrix_avx2 && ./test_sbox_avx2
	@echo "✓ Tests AVX2 OK (Mersenne + matrice + S-box)"

test-scalar: $(LIB) test_matrix_avx2 test_sbox_avx2
	@echo "=== Mode scalaire forcé (CAGOULE_FORCE_SCALAR=1) ==="
	CAGOULE_FORCE_SCALAR=1 ./test_matrix_avx2
	CAGOULE_FORCE_SCALAR=1 ./test_sbox_avx2
	@echo "✓ Tests scalaires OK"

test-ctr: $(LIB) test_ctr test_format
	./test_ctr && ./test_format
	@echo "✓ Tests CTR + format OK (v3.1.0)"

test-format: $(LIB) test_format
	./test_format && echo "✓ test_format OK"

# ── Compilation des exécutables de test ─────────────────────────────────
test_mersenne: tests/test_mersenne.c $(LIB) include/cagoule_math.h include/cagoule_math_avx2.h
	$(CC) $(CFLAGS) $(CFLAGS_AVX2) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS)

test_math: tests/test_math.c $(LIB) include/cagoule_math.h
	$(CC) $(CFLAGS) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS)

test_matrix: tests/test_matrix.c $(LIB) include/cagoule_math.h include/cagoule_matrix.h
	$(CC) $(CFLAGS) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS)

test_sbox: tests/test_sbox.c $(LIB) include/cagoule_math.h include/cagoule_sbox.h
	$(CC) $(CFLAGS) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS)

test_cipher: tests/test_cipher.c $(LIB) include/cagoule_cipher.h
	$(CC) $(CFLAGS) $(CFLAGS_AVX2) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS)

test_omega: tests/test_omega.c $(LIB) include/cagoule_omega.h include/cagoule_math.h
	$(CC) $(CFLAGS) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS) -lpthread

test_math_avx2: tests/test_math_avx2.c $(LIB) include/cagoule_math.h include/cagoule_math_avx2.h
	$(CC) $(CFLAGS) $(CFLAGS_AVX2) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS)

test_matrix_avx2: tests/test_matrix_avx2.c $(LIB) include/cagoule_math.h include/cagoule_matrix.h include/cagoule_math_avx2.h
	$(CC) $(CFLAGS) $(CFLAGS_AVX2) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS)

test_sbox_avx2: tests/test_sbox_avx2.c $(LIB) \
                include/cagoule_math.h include/cagoule_sbox.h \
                include/cagoule_math_avx2.h include/cagoule_sbox_avx2.h
	$(CC) $(CFLAGS) $(CFLAGS_AVX2) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS)

test_cipher_pipeline4: tests/test_cipher_pipeline4.c $(LIB) \
                       include/cagoule_cipher.h include/cagoule_math_avx2.h \
                       include/cagoule_sbox_avx2.h
	$(CC) $(CFLAGS) $(CFLAGS_AVX2) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS)

test_ctr: tests/test_ctr.c $(LIB) include/cagoule_ctr.h
	$(CC) $(CFLAGS) $(CFLAGS_AVX2) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS)

test_format: tests/test_format.c $(LIB) include/cagoule_cipher.h include/cagoule_ctr.h
	$(CC) $(CFLAGS) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS)

test_constant_time: tests/test_constant_time.c $(LIB) include/cagoule_math.h include/cagoule_math_avx2.h
	$(CC) $(CFLAGS) $(CFLAGS_AVX2) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS) -lm

# ── Constant-time test ───────────────────────────────────────────────────
test-ct: $(LIB) test_constant_time
	@echo "══════════════════════════════════════════════════════"
	@echo "  dudect — constant-time validation"
	@echo "══════════════════════════════════════════════════════"
	./test_constant_time
	@echo "✓ Constant-time tests OK"

# ── Benchmarks ───────────────────────────────────────────────────────────
bench: $(LIB) test_math test_matrix test_sbox test_omega test_ctr \
             test_matrix_avx2 test_sbox_avx2
	@echo "══════════════════════════════════════════════════════"
	@echo "  Benchmarks CAGOULE v3.1.0 (scalaires + AVX2 + CTR)"
	@echo "══════════════════════════════════════════════════════"
	@./test_math && ./test_matrix && ./test_sbox && ./test_omega && ./test_ctr
	@./test_matrix_avx2 && ./test_sbox_avx2

# ── Debug (ASan + UBSan) ─────────────────────────────────────────────────
debug: CFLAGS  += -g -fsanitize=address,undefined -DDEBUG -O0
debug: LDFLAGS += -fsanitize=address,undefined
debug: clean $(LIB) tests
	@echo "✓ Build debug avec sanitizers"

# ── Valgrind ─────────────────────────────────────────────────────────────
valgrind: $(LIB) test_math test_matrix test_sbox test_cipher test_omega \
               test_ctr test_matrix_avx2 test_sbox_avx2
	@echo "══════════════════════════════════════════════════════"
	@echo "  Valgrind — vérification des fuites mémoire"
	@echo "══════════════════════════════════════════════════════"
	@for t in test_math test_matrix test_sbox test_cipher test_omega \
	           test_ctr test_matrix_avx2 test_sbox_avx2; do \
	    echo "  → $$t"; \
	    RUNNING_ON_VALGRIND=1 valgrind --leak-check=full --error-exitcode=1 \
	        ./$$t > /tmp/valgrind_$$t.log 2>&1 \
	    && echo "    ✓ $$t : pas de fuite" \
	    || { echo "    ✗ $$t : fuite détectée (voir /tmp/valgrind_$$t.log)"; exit 1; }; \
	done
	@echo "✅ Aucune fuite mémoire détectée"

# ── Installation ─────────────────────────────────────────────────────────
install: $(LIB)
	@mkdir -p ..
	rm -f ../$(LIB)
	cp $(LIB) ../$(LIB)
	@echo "✓ $(LIB) → ../$(LIB)"

# ── Vérifications ────────────────────────────────────────────────────────
check-openssl:
	@echo -n "OpenSSL disponible : "
	@pkg-config --exists openssl && echo "✓ ($(shell pkg-config --modversion openssl))" || echo "✗ (installer libssl-dev)"

sysinfo:
	@echo "Compilateur     : $(CC) ($(shell $(CC) --version | head -1))"
	@echo "CFLAGS          : $(CFLAGS)"
	@echo "LDFLAGS         : $(LDFLAGS)"
	@echo "__uint128_t     : $(HAS_UINT128)"
	@echo "AVX2            : $(HAS_AVX2)"
	@echo "OpenSSL (pkg)   : $(HAS_OPENSSL)"
	@echo "Architecture    : $(shell uname -m)"

# ── Nettoyage ────────────────────────────────────────────────────────────
clean:
	rm -f $(OBJS) $(DEPS) $(LIB)
	rm -f test_math test_matrix test_sbox test_cipher test_omega 
	rm -f test_ctr test_format test_constant_time
	rm -f test_math_avx2 test_matrix_avx2 test_sbox_avx2 test_cipher_pipeline4 test_mersenne
	rm -f *.gcda *.gcno *.gcov
	rm -f /tmp/_chk_ossl /tmp/valgrind_*.log
	rm -f fuzz_cipher crash-* fuzz_cipher.log
	@echo "✓ clean terminé"

# ── Fuzz targets ─────────────────────────────────────────────────────────
# Requires clang with libFuzzer (-fsanitize=fuzzer)
fuzz_cipher: fuzz/fuzz_cipher.c $(LIB)
	clang -O1 -g -fsanitize=fuzzer,address,undefined \
	      -Iinclude $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS)

fuzz: fuzz_cipher
	@echo "══════════════════════════════════════════════════════"
	@echo "  libFuzzer — 1M runs (CBC + CTR)"
	@echo "══════════════════════════════════════════════════════"
	./fuzz_cipher -max_len=65536 -runs=1000000
	@echo "✓ Fuzzer 1M runs : 0 crashes"

fuzz-long: fuzz_cipher
	@echo "Starting fuzzer (10M iterations, may take hours)..."
	./fuzz_cipher -max_len=65536 -runs=10000000

# ── Aide ─────────────────────────────────────────────────────────────────
help:
	@echo "══════════════════════════════════════════════════════"
	@echo "  CAGOULE v3.1.0 — Cibles disponibles"
	@echo "══════════════════════════════════════════════════════"
	@echo "  make              → compile libcagoule.so"
	@echo "  make tests        → tous les tests C (12 binaires)"
	@echo "  make test-avx2    → tests AVX2 (Mersenne + matrice + S-box)"
	@echo "  make test-scalar  → force fallback scalaire"
	@echo "  make test-ctr     → tests CTR + format (v3.1.0)"
	@echo "  make test-ct      → test constant-time (dudect)"
	@echo "  make bench        → benchmarks de performance (inclut CTR)"
	@echo "  make debug        → build avec ASan + UBSan"
	@echo "  make valgrind     → vérification fuites mémoire (inclut CTR)"
	@echo "  make fuzz         → libFuzzer 1M runs (CBC + CTR)"
	@echo "  make fuzz-long    → libFuzzer 10M runs"
	@echo "  make install      → copie libcagoule.so dans le package Python"
	@echo "  make sysinfo      → affiche les infos système"
	@echo "  make check-openssl → vérifie la disponibilité d'OpenSSL"
	@echo "  make clean        → supprime les artefacts"
	@echo "══════════════════════════════════════════════════════"