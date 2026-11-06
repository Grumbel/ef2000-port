; SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
; SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

; OpenWatcom double-precision sqrt helper. The input and result are ST(0).
; Current hybrid callers pass nonnegative squared-distance sums.
.386p
.model flat
_TEXT SEGMENT DWORD PUBLIC USE32 'CODE'
PUBLIC IF@DSQRT
IF@DSQRT:
    fsqrt
    ret
_TEXT ENDS
END
