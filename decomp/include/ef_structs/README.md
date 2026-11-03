<!-- SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> -->
<!-- SPDX-License-Identifier: LicenseRef-EF2000-Derived -->

Recovered struct families belong here. Add offset/size assertions for each
field relied upon; do not infer a layout solely from Ghidra's guessed types.
