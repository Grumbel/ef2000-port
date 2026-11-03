<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: LicenseRef-EF2000-Derived -->

Per-function differential specifications will be named `<guest-address>.json`.
No fixture is a verification result until its OpenWatcom replacement has been
spliced and compared against the original executable.

`0x000432c.json` covers ten signed 32-bit score boundaries (including ties,
negative values, and `INT32_MIN`/`INT32_MAX`), with both global score addresses
initialized independently. It is a test *plan*, not a passing differential.
