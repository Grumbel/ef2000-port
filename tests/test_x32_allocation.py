# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

import ctypes
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class Allocation(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint32) for name in (
        'heap_end', 'stack_end', 'free_bytes', 'minimum_stack', 'heap_ceiling',
        'arena_start', 'arena_end')]


class Result(ctypes.Structure):
    _fields_ = [('low_limit', ctypes.c_uint32), ('flags', ctypes.c_uint32)]


class HeapResult(ctypes.Structure):
    _fields_ = [('heap_end', ctypes.c_uint32), ('stack_low', ctypes.c_uint32), ('flags', ctypes.c_uint32)]


@unittest.skipUnless(shutil.which('gcc'), 'requires GCC for translated allocator checks')
class AllocationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix='ef2000-x32-test-')
        cls.addClassCleanup(cls.directory.cleanup)
        root = Path(__file__).resolve().parent.parent
        library = Path(cls.directory.name) / 'x32.so'
        subprocess.run(['gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-fPIC', '-shared', str(root / 'runtime/x32.c'), '-o', str(library)],
                       check=True, capture_output=True, timeout=30)
        cls.library = ctypes.CDLL(str(library))
        cls.reserve = cls.library.x32_stack_reserve
        cls.reserve.argtypes = [ctypes.POINTER(Allocation), ctypes.c_uint32, ctypes.c_uint32,
                                ctypes.c_uint32, ctypes.POINTER(Result)]
        cls.reserve.restype = ctypes.c_int
        cls.grow = cls.library.x32_heap_grow
        cls.grow.argtypes = [ctypes.POINTER(Allocation), ctypes.c_uint32, ctypes.c_uint32,
                            ctypes.POINTER(HeapResult)]
        cls.grow.restype = ctypes.c_int
        cls.reference = json.loads((root / 'docs/x32-reference.json').read_text())['record']

    def test_matches_original_extender_state_and_flags(self):
        before = self.reference['allocation_before']
        after = self.reference['allocation_after']
        request = self.reference['3509']['input']
        response = self.reference['3509']['output']
        state = Allocation(before['heap_end'], before['stack_end'], before['free'], before['minimum_stack'])
        result = Result()
        self.assertEqual(self.reserve(ctypes.byref(state), request['ebx'], request['ecx'],
                                      request['eflags'], ctypes.byref(result)), 1)
        self.assertEqual(state.heap_end, after['heap_end'])
        self.assertEqual(state.stack_end, after['stack_end'])
        self.assertEqual(state.free_bytes, after['free'])
        self.assertEqual(result.low_limit, response['edx'])
        self.assertEqual(result.flags, response['eflags'])

    def test_rejection_preserves_state(self):
        for sp, size in [(0x1000, 0x2000), (0x5000, 0x3000), (0x7000, 0x1000)]:
            state = Allocation(0x4000, 0x8000, 0x100, 0x4000)
            original = bytes(state)
            self.assertEqual(self.reserve(ctypes.byref(state), sp, size, 0x202, ctypes.byref(Result())), 0)
            self.assertEqual(bytes(state), original)

    def test_result_depends_on_allocation_model(self):
        state = Allocation(0x4000, 0x9000, 0x3000, 0x5000)
        result = Result()
        self.assertEqual(self.reserve(ctypes.byref(state), 0x9000, 0x2000, 0x202,
                                      ctypes.byref(result)), 1)
        self.assertEqual(state.stack_end, 0x7000)
        self.assertEqual(state.free_bytes, 0x1000)
        self.assertEqual(result.low_limit, 0x5fff)
        self.assertEqual(result.flags & 1, 0)

    def test_heap_growth_matches_original_state_result_and_flags(self):
        before = self.reference['allocation_after']
        after = self.reference['3508']
        state = Allocation(before['heap_end'], before['stack_end'], before['free'],
                           before['minimum_stack'], after['heap_ceiling'])
        result = HeapResult()
        self.assertEqual(self.grow(ctypes.byref(state), 0x4e0000, after['input_flags'],
                                   ctypes.byref(result)), 1)
        self.assertEqual(state.heap_end, after['heap_end'])
        self.assertEqual(state.stack_end, after['stack_end'])
        self.assertEqual(state.free_bytes, after['free'])
        self.assertEqual(result.heap_end, after['ecx'])
        self.assertEqual(result.stack_low, after['eax'])
        self.assertEqual(result.flags, after['eflags'])

    def test_heap_rejection_does_not_consume_space(self):
        for request in (0x7000, 0x6000):
            state = Allocation(0x4000, 0x8000, 0x1000, 0x5000, 0x7000)
            original = bytes(state)
            self.assertEqual(self.grow(ctypes.byref(state), request, 0x202,
                                       ctypes.byref(HeapResult())), 0)
            self.assertEqual(bytes(state), original)


if __name__ == '__main__':
    unittest.main()
