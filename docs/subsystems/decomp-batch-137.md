<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception -->

# Decompilation batch 137: last byte in string

## `find_last_char` (`0x10d1da`, 26 bytes)

Original disassembly from the SHA-pinned payload:

```text
53                 push ebx
51                 push ecx
88 d1              mov cl,dl
31 db              xor ebx,ebx
3a 08              cmp cl,[eax]
75 02              jne next
89 c3              mov ebx,eax
next:
89 c2              mov edx,eax
8a 2a              mov ch,[edx]
40                 inc eax
84 ed              test ch,ch
75 f1              jne loop
89 d8              mov eax,ebx
59                 pop ecx
5b                 pop ebx
c3                 ret
```

EAX supplies the string address, DL the byte, and EAX returns the last match
or zero. The loop checks the terminating NUL before exiting, so a zero-byte
search returns the terminator address. EBX and ECX are preserved; EDX is
clobbered with the scan address. Three callsites were checked: two in
`0xe7fc0` pass `'\\'` and `':'`; `0xeb520@0xeb60a` passes `'.'`.

## Verification evidence

- The OpenWatcom fixture emits the exact original 26-byte instruction
  sequence and splices within the reviewed extent.
- Six isolated cases pass: empty string, last repeated match, matching NUL,
  no match, first/last duplicates, and a high-bit byte.
- Four captured original startup calls pass differential (4/4).
- Repeated-baseline main-menu, quick-fight setup, and cockpit replay pass
  3/3 with matching selected BSS, framebuffer and execution counters. The
  function is reached 20,889, 20,933 and 278,507 times respectively.
- `runtime_helpers.c` compiles with OpenWatcom 2.0 and zero warnings; focused
  differential/splicer tests pass 12/12. Tracker validation and
  `git diff --check` pass.

Original executable SHA-256:
`f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd`.
All local captures, objects, patches and replay output remain ignored.
