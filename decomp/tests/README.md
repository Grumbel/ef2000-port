<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: LicenseRef-EF2000-Derived -->

Per-function differential specifications will be named `<guest-address>.json`.
No fixture is a verification result until its OpenWatcom replacement has been
spliced and compared against the original executable.

`0x000432c.json` covers ten signed 32-bit score boundaries (including ties,
negative values, and `INT32_MIN`/`INT32_MAX`), with both global score addresses
initialized independently. It is a test *plan*, not a passing differential.

`0x000263e0.json` covers five old count values and checks the original
four-byte enclosing word: clearing the high 16 bits must not disturb the low
16 bits. EDX preservation is part of its confirmed ABI.

`0x000263ec.json` tests the paired append routine at counts 0, 1, 28, 29,
and 30, including the last tag adjacent to the count word, signed AX/DX
coordinates, and BL tags. The original clobbers EDX on successful append;
its signature deliberately does not assert preservation of that register.

`0x0006a1b4.json` covers ten signed AX bounds, nonzero and zero guest-pointer
entries, and high EAX bits that must not affect AX indexing. None of these
specifications claim an executed OpenWatcom guest differential yet.

`0x0007489c.json` covers nine filtered-lookup conditions and observable
diagnostic-counter side effects; its function-body size is noncontiguous.
`0x0006a354.json`, `0x0006a378.json`, `0x0006a394.json` and
`0x0006a3d0.json` exercise four linked state wrappers, including object
record flag guards, `0xe5` type-stride lookups, rejection sentinels and
preserved global state. All remain execution *plans*, not verified results.
