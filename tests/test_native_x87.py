# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Only measured interruption may discard a native observation."""

from types import SimpleNamespace
import unittest
from unittest import mock

from tests.native_x87 import call_without_context_switch


def usage(voluntary, involuntary):
    return SimpleNamespace(ru_nvcsw=voluntary, ru_nivcsw=involuntary)


class NativeObservationTests(unittest.TestCase):
    def test_uninterrupted_result_is_not_filtered_or_retried(self):
        native = mock.Mock(return_value=0)
        with mock.patch('tests.native_x87.resource.getrusage',
                        side_effect=[usage(1, 2), usage(1, 2)]):
            self.assertEqual(call_without_context_switch(native, 'operand'), 0)
        native.assert_called_once_with('operand')

    def test_repeats_only_after_measured_switch(self):
        native = mock.Mock(side_effect=[0, 0, 0x1234])
        with mock.patch('tests.native_x87.resource.getrusage', side_effect=[
                usage(1, 2), usage(1, 3), usage(1, 3), usage(2, 3),
                usage(2, 3), usage(2, 3)]):
            self.assertEqual(call_without_context_switch(native), 0x1234)
        self.assertEqual(native.call_count, 3)

    def test_repeated_interruption_fails_closed(self):
        native = mock.Mock(return_value=0x1234)
        with mock.patch('tests.native_x87.resource.getrusage',
                        side_effect=[usage(0, i) for i in range(8)]):
            with self.assertRaisesRegex(RuntimeError, 'four times'):
                call_without_context_switch(native)
        self.assertEqual(native.call_count, 4)
