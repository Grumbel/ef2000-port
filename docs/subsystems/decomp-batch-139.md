<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 139: strip carriage returns in place

## `strip_cr_inplace` (`0x1091e0`, 28 bytes)

Original disassembly from the SHA-pinned payload:

```text
53                 push ebx
52                 push edx
89 c2              mov edx,eax
31 db              xor ebx,ebx
loop:
8a 18              mov bl,[eax]
83 fb 0d           cmp ebx,0x0d
75 03              jne copy
40                 inc eax
eb f4              jmp loop
copy:
42                 inc edx
40                 inc eax
88 5a ff           mov [edx-1],bl
84 db              test bl,bl
75 eb              jne loop
5a                 pop edx
5b                 pop ebx
c3                 ret
```

EAX supplies a writable string and returns one byte past the first NUL. The
function removes CR bytes, copies all other bytes—including the terminating
NUL—in place, and leaves data after that NUL untouched. EBX and EDX are
preserved; the byte is zero-extended through EBX before the compare/copy.
The direct caller `0x1093d8@0x1093ea` is a text parser.

## Verification evidence

- The OpenWatcom fixture emits the original 28-byte sequence and splices in
  place.
- Six isolated cases pass: empty string, no CR, mixed/repeated CR, all CR,
  high-bit bytes, and trailing data after NUL.
- One captured startup call passes differential and compares 348 guest RAM
  bytes written by the CR-removal path.
- Repeated-baseline main-menu, quick-fight setup and cockpit replays pass
  3/3 with matching selected BSS, framebuffer and execution counters. The
  target is hit 1, 1 and 2 times respectively.
- `runtime_helpers.c` compiles with OpenWatcom 2.0 and zero warnings. Focused
  differential/splicer tests pass 12/12; tracker validation and
  `git diff --check` pass.

Original executable SHA-256:
`f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.
Captures, object, patch and replay output remain ignored under `local/`.
