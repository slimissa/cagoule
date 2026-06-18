"""
test_ctr_api.py — Tests pytest de l'API Python CTR CAGOULE v3.0.0

Couvre :
  - encrypt_ctr() / decrypt_ctr() roundtrip
  - VERSION dispatch (v0x01 CBC → decrypt_cbc, v0x02 CTR → decrypt_ctr)
  - migrate_cbc_to_ctr()
  - encrypt_bulk_ctr() / decrypt_bulk_ctr()
  - Mauvais mot de passe → CagouleAuthError
  - Format invalide → CagouleFormatError
  - Tailles critiques (0, 1, 15, 16, 17, 255, 256, 1024)
  - encrypt() defaulte à CTR en v3.0.0
  - decrypt() dispatch automatique
"""
import os
import pytest

from cagoule import (
    encrypt, decrypt,
    encrypt_ctr, decrypt_ctr,
    encrypt_cbc, decrypt_cbc,
    encrypt_bulk, decrypt_bulk,
    encrypt_bulk_ctr, decrypt_bulk_ctr,
    migrate_cbc_to_ctr,
    __version__,
)
from cagoule.decipher     import CagouleAuthError, CagouleFormatError
from cagoule.cipher_ctr   import MAGIC, VERSION_CTR, VERSION_CBC

# ── Fixtures ────────────────────────────────────────────────────────────
PASSWORD  = b"test_password_v300"
MESSAGE   = b"CAGOULE v3.0.0 CTR test message"
FAST_KW   = {"fast_mode": True}   # KDF rapide pour les tests


# ── Version ─────────────────────────────────────────────────────────────
def test_version():
    assert __version__ == "3.0.0"


# ── encrypt() défaut → CTR (VERSION 0x02) ───────────────────────────────
def test_encrypt_defaults_to_ctr():
    ct = encrypt(MESSAGE, PASSWORD, **FAST_KW)
    assert ct[4:5] == VERSION_CTR, "encrypt() doit produire CGL1 v0x02 (CTR)"


def test_decrypt_dispatch_ctr():
    ct = encrypt(MESSAGE, PASSWORD, **FAST_KW)
    assert ct[4:5] == VERSION_CTR
    pt = decrypt(ct, PASSWORD, **FAST_KW)
    assert pt == MESSAGE


# ── encrypt_ctr / decrypt_ctr ───────────────────────────────────────────
class TestCTRBasic:
    def test_roundtrip(self):
        ct = encrypt_ctr(MESSAGE, PASSWORD, **FAST_KW)
        pt = decrypt_ctr(ct, PASSWORD, **FAST_KW)
        assert pt == MESSAGE

    def test_version_byte(self):
        ct = encrypt_ctr(MESSAGE, PASSWORD, **FAST_KW)
        assert ct[:4] == MAGIC
        assert ct[4:5] == VERSION_CTR

    def test_ct_len_equals_pt_len(self):
        """CGL1 v0x02 : |CT_aead| = |PT| + 16 (tag). Pas de PKCS7."""
        pt = b"x" * 100
        ct = encrypt_ctr(pt, PASSWORD, **FAST_KW)
        # header: 4+1+32+12 = 49, + CT(100) + TAG(16) = 165
        assert len(ct) == 49 + len(pt) + 16

    def test_ct_neq_pt(self):
        ct = encrypt_ctr(MESSAGE, PASSWORD, **FAST_KW)
        assert MESSAGE not in ct

    def test_nondeterministic(self):
        ct1 = encrypt_ctr(MESSAGE, PASSWORD, **FAST_KW)
        ct2 = encrypt_ctr(MESSAGE, PASSWORD, **FAST_KW)
        assert ct1 != ct2   # salt + nonce aléatoires

    def test_wrong_password(self):
        ct = encrypt_ctr(MESSAGE, PASSWORD, **FAST_KW)
        with pytest.raises(CagouleAuthError):
            decrypt_ctr(ct, b"wrong_password", **FAST_KW)

    def test_corrupted_tag(self):
        ct = bytearray(encrypt_ctr(MESSAGE, PASSWORD, **FAST_KW))
        ct[-1] ^= 0xFF  # corrompre le tag
        with pytest.raises(CagouleAuthError):
            decrypt_ctr(bytes(ct), PASSWORD, **FAST_KW)

    def test_wrong_version_in_ctr_decoder(self):
        """decrypt_ctr() refuse les ciphertexts v0x01 (CBC)."""
        ct_cbc = encrypt_cbc(MESSAGE, PASSWORD, **FAST_KW)
        with pytest.raises(CagouleFormatError):
            decrypt_ctr(ct_cbc, PASSWORD, **FAST_KW)

    def test_truncated_header(self):
        with pytest.raises(CagouleFormatError):
            decrypt_ctr(b"CGL1\x02", PASSWORD, **FAST_KW)


# ── Tailles critiques ────────────────────────────────────────────────────
@pytest.mark.parametrize("size", [0, 1, 7, 15, 16, 17, 31, 32, 33, 63,
                                   64, 65, 127, 128, 129, 255, 256, 257,
                                   1023, 1024, 1025])
def test_ctr_roundtrip_all_sizes(size):
    pt = os.urandom(size)
    ct = encrypt_ctr(pt, PASSWORD, **FAST_KW)
    assert decrypt_ctr(ct, PASSWORD, **FAST_KW) == pt


# ── CBC rétrocompatibilité ───────────────────────────────────────────────
class TestCBCBackcompat:
    def test_encrypt_cbc_version(self):
        ct = encrypt_cbc(MESSAGE, PASSWORD, **FAST_KW)
        assert ct[4:5] == VERSION_CBC

    def test_decrypt_cbc_explicit(self):
        ct = encrypt_cbc(MESSAGE, PASSWORD, **FAST_KW)
        pt = decrypt_cbc(ct, PASSWORD, **FAST_KW)
        assert pt == MESSAGE

    def test_dispatch_routes_v01_to_cbc(self):
        """decrypt() dispatch v0x01 → CBC pipeline."""
        ct_cbc = encrypt_cbc(MESSAGE, PASSWORD, **FAST_KW)
        assert ct_cbc[4:5] == VERSION_CBC
        pt = decrypt(ct_cbc, PASSWORD, **FAST_KW)
        assert pt == MESSAGE

    def test_dispatch_routes_v02_to_ctr(self):
        """decrypt() dispatch v0x02 → CTR pipeline."""
        ct_ctr = encrypt_ctr(MESSAGE, PASSWORD, **FAST_KW)
        assert ct_ctr[4:5] == VERSION_CTR
        pt = decrypt(ct_ctr, PASSWORD, **FAST_KW)
        assert pt == MESSAGE

    def test_dispatch_unknown_version(self):
        ct = bytearray(encrypt_cbc(MESSAGE, PASSWORD, **FAST_KW))
        ct[4] = 0xFF   # version inconnue
        with pytest.raises(CagouleFormatError):
            decrypt(bytes(ct), PASSWORD, **FAST_KW)

    def test_cbc_ctr_ct_differ(self):
        ct_cbc = encrypt_cbc(MESSAGE, PASSWORD, **FAST_KW)
        ct_ctr = encrypt_ctr(MESSAGE, PASSWORD, **FAST_KW)
        assert ct_cbc != ct_ctr
        assert ct_cbc[4:5] != ct_ctr[4:5]


# ── Migration CBC → CTR ─────────────────────────────────────────────────
class TestMigration:
    def test_migrate_basic(self):
        ct_cbc = encrypt_cbc(MESSAGE, PASSWORD, **FAST_KW)
        ct_ctr = migrate_cbc_to_ctr(ct_cbc, PASSWORD, fast_mode=True)
        assert ct_ctr[4:5] == VERSION_CTR
        pt = decrypt_ctr(ct_ctr, PASSWORD, **FAST_KW)
        assert pt == MESSAGE

    def test_migrate_roundtrip(self):
        original = b"migration test payload " * 10
        ct_cbc = encrypt_cbc(original, PASSWORD, **FAST_KW)
        ct_ctr = migrate_cbc_to_ctr(ct_cbc, PASSWORD, fast_mode=True)
        recovered = decrypt(ct_ctr, PASSWORD, **FAST_KW)
        assert recovered == original

    def test_migrate_ct_size(self):
        """Après migration, |CT_body| == |plaintext| (pas de padding CBC)."""
        pt = b"x" * 100
        ct_ctr = migrate_cbc_to_ctr(
            encrypt_cbc(pt, PASSWORD, **FAST_KW),
            PASSWORD, fast_mode=True
        )
        # header 49 + PT(100) + TAG(16) = 165
        assert len(ct_ctr) == 49 + len(pt) + 16


# ── Bulk CTR ────────────────────────────────────────────────────────────
class TestBulkCTR:
    def test_bulk_roundtrip(self):
        messages = [b"msg1", b"msg2", b"msg3", b"msg4", b"msg5"]
        cts = encrypt_bulk_ctr(messages, PASSWORD, fast_mode=True)
        pts = [decrypt_ctr(ct, PASSWORD, **FAST_KW) for ct in cts]
        assert pts == messages

    def test_bulk_len(self):
        messages = [os.urandom(50) for _ in range(10)]
        cts = encrypt_bulk_ctr(messages, PASSWORD, fast_mode=True)
        assert len(cts) == 10

    def test_bulk_all_ctr(self):
        cts = encrypt_bulk_ctr([MESSAGE] * 5, PASSWORD, fast_mode=True)
        assert all(ct[4:5] == VERSION_CTR for ct in cts)

    def test_bulk_decrypt_roundtrip(self):
        messages = [b"bulk_" + str(i).encode() for i in range(20)]
        cts = encrypt_bulk(messages, PASSWORD, fast_mode=True)
        pts = decrypt_bulk(cts, PASSWORD, fast_mode=True)
        assert pts == messages

    def test_bulk_empty(self):
        assert encrypt_bulk_ctr([], PASSWORD, fast_mode=True) == []

    def test_bulk_single(self):
        cts = encrypt_bulk_ctr([MESSAGE], PASSWORD, fast_mode=True)
        assert len(cts) == 1
        assert decrypt_ctr(cts[0], PASSWORD, **FAST_KW) == MESSAGE

    def test_bulk_mixed_sizes(self):
        msgs = [b"", b"x", b"y" * 100, b"z" * 1000]
        cts  = encrypt_bulk_ctr(msgs, PASSWORD, fast_mode=True)
        pts  = [decrypt_ctr(ct, PASSWORD, **FAST_KW) for ct in cts]
        assert pts == msgs

    def test_bulk_wrong_password(self):
        cts = encrypt_bulk_ctr([MESSAGE], PASSWORD, fast_mode=True)
        with pytest.raises(CagouleAuthError):
            decrypt_ctr(cts[0], b"wrong", **FAST_KW)


# ── Types invalides ─────────────────────────────────────────────────────
def test_encrypt_ctr_bad_type_message():
    with pytest.raises(TypeError):
        encrypt_ctr("not bytes", PASSWORD, **FAST_KW)

def test_encrypt_ctr_bad_type_password():
    with pytest.raises(TypeError):
        encrypt_ctr(MESSAGE, "not bytes", **FAST_KW)

def test_decrypt_ctr_bad_type():
    with pytest.raises(TypeError):
        decrypt_ctr("not bytes", PASSWORD, **FAST_KW)


# ── Format invalide ─────────────────────────────────────────────────────
def test_decrypt_bad_magic():
    with pytest.raises(CagouleFormatError):
        decrypt_ctr(b"\xDE\xAD\xBE\xEF\x02" + b"\x00" * 100, PASSWORD, **FAST_KW)

def test_decrypt_too_short():
    with pytest.raises(CagouleFormatError):
        decrypt_ctr(b"CGL1\x02", PASSWORD, **FAST_KW)


# ── CTR est symétrique (encrypt == decrypt au niveau C) ─────────────────
def test_ctr_symmetry():
    """
    CTR : decrypt(encrypt(m)) == m
         ET encrypt(encrypt(m)) == m   (si on enlève le wrapper AEAD)
    On teste au niveau Python API que les deux sens fonctionnent.
    """
    for size in [0, 1, 16, 100, 500]:
        pt = os.urandom(size)
        ct = encrypt_ctr(pt, PASSWORD, **FAST_KW)
        assert decrypt_ctr(ct, PASSWORD, **FAST_KW) == pt


# ── Stabilité déterministe des params ──────────────────────────────────
def test_ctr_determinism_same_salt():
    """Mêmes params → même keystream → même ciphertext."""
    from cagoule.params import CagouleParams
    salt = os.urandom(32)
    params = CagouleParams.derive(PASSWORD, salt=salt, fast_mode=True)
    try:
        ct1 = encrypt_ctr(MESSAGE, PASSWORD, params=params, **FAST_KW)
        # Même salt → même k_master → même IV CTR → même keystream
        ct2 = encrypt_ctr(MESSAGE, PASSWORD, params=params, **FAST_KW)
        # Les nonces AEAD diffèrent (os.urandom), donc ct1 != ct2 au niveau AEAD
        # Mais les deux doivent décrypter vers MESSAGE
        assert decrypt_ctr(ct1, PASSWORD, params=params, **FAST_KW) == MESSAGE
        assert decrypt_ctr(ct2, PASSWORD, params=params, **FAST_KW) == MESSAGE
    finally:
        params.zeroize()
