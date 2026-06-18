# Makefile — CAGOULE v2.4.0 libcagoule.so
#
# Cibles principales :
#   make              → compile libcagoule.so (AVX2 si disponible)
#   make tests        → exécute tous les tests C (scalaires + AVX2)
#   make test-avx2    → tests AVX2 uniquement
#   make test-scalar  → force le chemin scalaire (CI sans AVX2)
#   make valgrind     → vérifie les fuites mémoire
#   make debug        → compile avec ASan + UBSan
#   make install      → copie dans le package Python
#   make clean        → nettoie
#
# Nouveautés v2.3.0 :
#   - cagoule_sbox_avx2.c compilé avec -mavx2 (S-box Feistel AVX2)
#   - cagoule_sbox_backend_is_avx2() exposé dans libcagoule.so
#   - test_sbox_avx2 : nouveau binaire de test parité S-box

CC       = gcc
# -O3 (vs -O2) est intentionnel pour une bibliothèque cryptographique :
#   - déroulage de boucles (-funroll-loops implicite) → critique pour les
#     boucles de 4 iterations dans les fonctions SIMD (sbox, rk_add, etc.)
#   - inlining plus agressif des helpers static inline
#   - meilleure allocation de registres pour les registres YMM
# -funroll-loops : déroulage explicite des boucles N=16 (4 iterations de 4)
CFLAGS   = -O3 -funroll-loops -std=c99 -Wall -Wextra -fPIC -Iinclude
CFLAGS  += -D_POSIX_C_SOURCE=200809L
LDFLAGS  = -shared
LDLIBS   = -lm -lcrypto

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
SRCS     = src/cagoule_matrix.c \
           src/cagoule_sbox.c   \
           src/cagoule_cipher.c \
           src/cagoule_omega.c

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

.PHONY: all tests test-avx2 test-scalar bench clean install \
        check-openssl debug valgrind help sysinfo

all: $(LIB)
	@echo "  AVX2 : $(AVX2_STATUS)"

$(LIB): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	@echo "✓ $(LIB) compilé (CAGOULE v2.4.0)"

%.o: %.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

-include $(DEPS)

tests: $(LIB) test_math test_matrix test_sbox test_cipher test_omega \
             test_math_avx2 test_matrix_avx2 test_sbox_avx2 test_cipher_pipeline4
	@echo ""
	@echo "══════════════════════════════════════════════════════"
	@echo "  Exécution des tests CAGOULE v2.4.0"
	@echo "══════════════════════════════════════════════════════"
	@status=0; \
	./test_math          && echo "✓ test_math          OK" || { echo "✗ test_math          FAIL"; status=1; }; \
	./test_matrix        && echo "✓ test_matrix        OK" || { echo "✗ test_matrix        FAIL"; status=1; }; \
	./test_sbox          && echo "✓ test_sbox          OK" || { echo "✗ test_sbox          FAIL"; status=1; }; \
	./test_cipher        && echo "✓ test_cipher        OK" || { echo "✗ test_cipher        FAIL"; status=1; }; \
	./test_omega         && echo "✓ test_omega         OK" || { echo "✗ test_omega         FAIL"; status=1; }; \
	./test_math_avx2     && echo "✓ test_math_avx2     OK" || { echo "✗ test_math_avx2     FAIL"; status=1; }; \
	./test_matrix_avx2   && echo "✓ test_matrix_avx2   OK" || { echo "✗ test_matrix_avx2   FAIL"; status=1; }; \
	./test_sbox_avx2     && echo "✓ test_sbox_avx2     OK" || { echo "✗ test_sbox_avx2     FAIL"; status=1; }; \
	./test_cipher_pipeline4 && echo "✓ test_cipher_pipeline4 OK" || { echo "✗ test_cipher_pipeline4 FAIL"; status=1; }; \
	echo "══════════════════════════════════════════════════════"; \
	[ $$status -eq 0 ] && echo "  ✅ Tous les tests passent (v2.4.0)" || echo "  ❌ Certains tests ont échoué"; \
	echo "══════════════════════════════════════════════════════"; \
	exit $$status

test-avx2: $(LIB) test_math_avx2 test_matrix_avx2 test_sbox_avx2
	./test_math_avx2 && ./test_matrix_avx2 && ./test_sbox_avx2
	@echo "✓ Tests AVX2 OK (matrice + S-box)"

test-scalar: $(LIB) test_matrix_avx2 test_sbox_avx2
	@echo "=== Mode scalaire forcé (CAGOULE_FORCE_SCALAR=1) ==="
	CAGOULE_FORCE_SCALAR=1 ./test_matrix_avx2
	CAGOULE_FORCE_SCALAR=1 ./test_sbox_avx2
	@echo "✓ Tests scalaires OK"

# ── Compilation des exécutables de test ─────────────────────────────────
test_math: tests/test_math.c include/cagoule_math.h
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

test_matrix: tests/test_matrix.c $(LIB) include/cagoule_math.h include/cagoule_matrix.h
	$(CC) $(CFLAGS) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS)

test_sbox: tests/test_sbox.c $(LIB) include/cagoule_math.h include/cagoule_sbox.h
	$(CC) $(CFLAGS) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS)

test_cipher: tests/test_cipher.c $(LIB) include/cagoule_cipher.h
	$(CC) $(CFLAGS) $(CFLAGS_AVX2) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS)

test_omega: tests/test_omega.c $(LIB) include/cagoule_omega.h include/cagoule_math.h
	$(CC) $(CFLAGS) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS) -lpthread

test_math_avx2: tests/test_math_avx2.c include/cagoule_math.h include/cagoule_math_avx2.h
	$(CC) $(CFLAGS) $(CFLAGS_AVX2) $< -o $@ $(LDLIBS)

test_matrix_avx2: tests/test_matrix_avx2.c $(LIB) include/cagoule_math.h include/cagoule_matrix.h include/cagoule_math_avx2.h
	$(CC) $(CFLAGS) $(CFLAGS_AVX2) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS)

# ── v2.3.0 : test_sbox_avx2 ─────────────────────────────────────────────
test_sbox_avx2: tests/test_sbox_avx2.c $(LIB) \
                include/cagoule_math.h include/cagoule_sbox.h \
                include/cagoule_math_avx2.h include/cagoule_sbox_avx2.h
	$(CC) $(CFLAGS) $(CFLAGS_AVX2) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS)

# ── Benchmarks ───────────────────────────────────────────────────────────
bench: $(LIB) test_math test_matrix test_sbox test_omega test_matrix_avx2 test_sbox_avx2
	@echo "══════════════════════════════════════════════════════"
	@echo "  Benchmarks CAGOULE v2.4.0"
	@echo "══════════════════════════════════════════════════════"
	@./test_math && ./test_matrix && ./test_sbox && ./test_omega
	@./test_matrix_avx2 && ./test_sbox_avx2

# ── Debug (ASan + UBSan) ─────────────────────────────────────────────────
debug: CFLAGS  += -g -fsanitize=address,undefined -DDEBUG -O0
debug: LDFLAGS += -fsanitize=address,undefined
debug: clean $(LIB) tests
	@echo "✓ Build debug avec sanitizers"

# ── Valgrind ─────────────────────────────────────────────────────────────
valgrind: $(LIB) test_math test_matrix test_sbox test_cipher test_omega \
               test_matrix_avx2 test_sbox_avx2
	@echo "══════════════════════════════════════════════════════"
	@echo "  Valgrind — vérification des fuites mémoire"
	@echo "══════════════════════════════════════════════════════"
	@for t in test_math test_matrix test_sbox test_cipher test_omega \
	           test_matrix_avx2 test_sbox_avx2; do \
	    echo "  → $$t"; \
	    RUNNING_ON_VALGRIND=1 valgrind --leak-check=full --error-exitcode=1 \
	        ./$$t > /tmp/valgrind_$$t.log 2>&1 \
	    && echo "    ✓ $$t : pas de fuite" \
	    || { echo "    ✗ $$t : fuite détectée (voir /tmp/valgrind_$$t.log)"; exit 1; }; \
	done
	@echo "✅ Aucune fuite mémoire détectée"

# ── Installation ─────────────────────────────────────────────────────────
install: $(LIB)
	@mkdir -p ../cagoule
	rm -f ../cagoule/$(LIB)
	cp $(LIB) ../cagoule/$(LIB)
	@echo "✓ $(LIB) → ../cagoule/$(LIB)"

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
	rm -f test_math_avx2 test_matrix_avx2 test_sbox_avx2 test_cipher_pipeline4
	rm -f *.gcda *.gcno *.gcov
	rm -f /tmp/_chk_ossl /tmp/valgrind_*.log
	@echo "✓ clean terminé"

# ── Aide ─────────────────────────────────────────────────────────────────
help:
	@echo "══════════════════════════════════════════════════════"
	@echo "  CAGOULE v2.4.0 — Cibles disponibles"
	@echo "══════════════════════════════════════════════════════"
	@echo "  make              → compile libcagoule.so"
	@echo "  make tests        → tous les tests C (scalaire + AVX2)"
	@echo "  make test-avx2    → tests AVX2 (matrice + S-box)"
	@echo "  make test-scalar  → force fallback scalaire"
	@echo "  make bench        → benchmarks de performance"
	@echo "  make debug        → build avec ASan + UBSan"
	@echo "  make valgrind     → vérification fuites mémoire"
	@echo "  make install      → copie libcagoule.so dans le package Python"
	@echo "  make sysinfo      → affiche les infos système"
	@echo "  make check-openssl → vérifie la disponibilité d'OpenSSL"
	@echo "  make clean        → supprime les artefacts"
	@echo "══════════════════════════════════════════════════════"

# ── v2.4.0 : test_cipher_pipeline4 ──────────────────────────────────────
test_cipher_pipeline4: tests/test_cipher_pipeline4.c $(LIB) \
                       include/cagoule_cipher.h include/cagoule_math_avx2.h \
                       include/cagoule_sbox_avx2.h
	$(CC) $(CFLAGS) $(CFLAGS_AVX2) $< -L. -lcagoule -Wl,-rpath,. -o $@ $(LDLIBS)

test-cipher-pipeline: $(LIB) test_cipher_pipeline4
	./test_cipher_pipeline4 && echo "✓ test_cipher_pipeline4 OK"
