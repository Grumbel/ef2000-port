# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

import unittest

from tools.isa_audio import AudioPIC, DMA8237
from tools.sound_blaster import SoundBlaster16


class ISAAudioTests(unittest.TestCase):
    def test_pic_mask_pending_in_service_and_eoi(self):
        pic = AudioPIC()
        pic.request(7)
        self.assertIsNone(pic.next_irq())
        pic.write(0x21, 0x78)
        self.assertEqual(pic.next_irq(), 7)
        pic.begin(7)
        pic.write(0x20, 0x0b)
        self.assertEqual(pic.read(0x20), 0x80)
        pic.write(0x20, 0x20)
        self.assertEqual(pic.read(0x20), 0)

    def test_word_dma_address_count_and_auto_init_reload(self):
        dma = DMA8237()
        dma.write(0xd8, 0)
        dma.write(0xc4, 0x20)
        dma.write(0xc4, 0)
        dma.write(0xc6, 3)
        dma.write(0xc6, 0)
        dma.write(0x8b, 3)  # Low page bit is ignored on a word channel.
        dma.write(0xd6, 0x59)
        dma.write(0xd4, 1)
        reads = []
        def memory(address, length):
            reads.append((address, length))
            return bytes(range(length))
        self.assertEqual(dma.transfer(5, 2, memory), bytes(range(4)))
        self.assertEqual(dma.count[5], 1)
        self.assertEqual(dma.transfer(5, 2, memory), bytes(range(4)))
        self.assertEqual(dma.count[5], 3)
        self.assertEqual(dma.address[5], 0x20)
        self.assertEqual(reads, [(0x20040, 4), (0x20044, 4)])

    def test_overdue_auto_init_dma_keeps_pcm_but_coalesces_irq(self):
        device = SoundBlaster16()
        device.write(0x226, 1)
        device.write(0x226, 0)
        device.read(0x22a)
        for port, value in ((0xd8, 0), (0xc4, 0), (0xc4, 0x80),
                            (0xc6, 3), (0xc6, 0), (0x8b, 0),
                            (0xd6, 0x59), (0xd4, 1)):
            device.write(port, value)
        for value in (0xb4, 0x30, 1, 0):
            device.write(0x22c, value)
        now = device.active['deadline'] + 2.5 * device.active['duration']
        device.advance(now, lambda address, length: bytes(length))
        self.assertEqual(len(device.completed), 3)
        self.assertEqual(device.pic.pending[0], 0x80)
        self.assertGreater(device.active['deadline'], now)
        device.advance(now, lambda address, length: bytes(length))
        self.assertEqual(len(device.completed), 3)
        self.assertEqual(device.dma.count[5], 1)

    def test_timed_pcm_completion_and_interrupt_acknowledgement(self):
        device = SoundBlaster16()
        device.write(0x226, 1)
        device.write(0x226, 0)
        device.read(0x22a)
        for port, value in ((0xd8, 0), (0xc4, 0), (0xc4, 0x80),
                            (0xc6, 3), (0xc6, 0), (0x8b, 0),
                            (0xd6, 0x49), (0xd4, 1)):
            device.write(port, value)
        for value in (0x41, 0x56, 0x22, 0xb0, 0x30, 3, 0):
            device.write(0x22c, value)
        deadline = device.active['deadline']
        memory = lambda address, length: b'\x01\x02\x03\x04\x05\x06\x07\x08'[:length]
        device.advance(deadline - 0.00001, memory)
        self.assertFalse(device.completed)
        device.advance(deadline, memory)
        self.assertEqual(device.completed[0]['pcm'], bytes(range(1, 9)))
        self.assertEqual(device.resources[0x82] & 3, 2)
        device.read(0x22f)
        self.assertEqual(device.resources[0x82] & 3, 0)
        self.assertIsNone(device.active)
