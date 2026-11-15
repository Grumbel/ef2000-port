# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Take native x87 observations without a context switch during the call."""

import resource


def call_without_context_switch(function, *arguments):
    # A switch between the x87 instruction and FNSAVE can invalidate saved
    # instruction/data pointers. Discard only observations with a measured
    # switch; callers still apply every numerical and metadata assertion.
    scope = getattr(resource, 'RUSAGE_THREAD', resource.RUSAGE_SELF)
    for _ in range(4):
        before = resource.getrusage(scope)
        result = function(*arguments)
        after = resource.getrusage(scope)
        if (before.ru_nvcsw, before.ru_nivcsw) == (after.ru_nvcsw, after.ru_nivcsw):
            return result
    raise RuntimeError('native x87 observation interrupted by context switches four times')
