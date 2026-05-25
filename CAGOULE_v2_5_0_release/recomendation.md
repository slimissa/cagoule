Update the header documentation (cagoule_cipher.h) to accurately reflect the byte-level implementation. The current header comment is misleading.

Add a roadmap item for v2.5.1 or v3.0.0: "Port Z-Domain Shifting to mod-p algebraic layer." This would use addmod64x4 on the 16 uint64_t elements after _load_plain and before the matrix multiplication. It would be cleaner and potentially faster.

Consider renaming the parameter from z_offset to something that distinguishes the two approaches, like z_byte_offset vs z_field_offset, to avoid confusion when the algebraic version is eventually implemented.
