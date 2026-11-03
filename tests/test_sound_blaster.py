# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import unittest

from tools.read_sb16_probe import FIELDS, decode
from tools.sound_blaster import SB16Probe

REFERENCE = Path(__file__).resolve().parent.parent / 'docs/sb16-reference.json'


class SB16ReferenceTests(unittest.TestCase):
    def test_measured_reference_and_record_validation(self):
        reference = json.loads(REFERENCE.read_text())
        self.assertTrue(reference['source_unchanged'])
        samples = reference['record']['samples']
        record = b'SBDP' + struct.pack('<I', 1) + b'DONE' + bytes(4)
        record += struct.pack('<8I', *(samples[name] for name in FIELDS))
        self.assertEqual(hashlib.sha256(record).hexdigest(), reference['record_sha256'])
        self.assertEqual(decode(record), reference['record'])
        for invalid in (record[:-1], record + b'\0', record.replace(b'DONE', b'FAIL'),
                        record[:16] + struct.pack('<I', 256) + record[20:]):
            with self.assertRaises(ValueError):
                decode(invalid)

    def test_discovery_matches_reference_reset_version_and_resources(self):
        expected = json.loads(REFERENCE.read_text())['record']['samples']
        device = SB16Probe()
        device.write(0x226, 1)
        self.assertEqual(device.read(0x22e) & 128, 0)
        device.write(0x226, 0)
        self.assertEqual(device.read(0x22e) & 128, 128)
        samples = {'reset_response': device.read(0x22a),
                   'write_status': device.read(0x22c)}
        device.write(0x22c, 0xe1)
        samples['version_major'] = device.read(0x22a)
        samples['version_minor'] = device.read(0x22a)
        for index, name in ((0x80, 'irq_setup'), (0x81, 'dma_setup'), (0x82, 'interrupt_status')):
            device.write(0x224, index)
            samples[name] = device.read(0x225)
        samples['empty_read_status'] = device.read(0x22e)
        self.assertEqual(samples, expected)

    def test_reset_flushes_unread_responses_and_unimplemented_playback_stops(self):
        device = SB16Probe()
        with self.assertRaisesRegex(ValueError, 'before reset'):
            device.write(0x22c, 0xe1)
        device.write(0x226, 1)
        device.write(0x226, 0)
        device.read(0x22a)
        device.write(0x22c, 0xe1)
        self.assertEqual(device.read(0x22a), 4)
        device.write(0x226, 1)
        device.write(0x226, 0)
        self.assertEqual(device.read(0x22a), 0xaa)
        self.assertEqual(device.read(0x22e) & 128, 0)
        with self.assertRaisesRegex(ValueError, 'no audio/DMA'):
            device.write(0x22c, 0x41)
        with self.assertRaises(ValueError):
            device.read(0x225)  # Unmeasured mixer register 00h.
        with self.assertRaisesRegex(ValueError, 'port width'):
            device.read(0x22a, 2)


@unittest.skipUnless(os.environ.get('EF2000_EXE') and importlib.util.find_spec('unicorn'),
                     'requires original SB16.DIG and Unicorn')
class OriginalSoundDriverTests(unittest.TestCase):
    def test_original_driver_completes_dma_irq_self_test(self):
        from tools.real_mode import RealMode, MEMORY_SIZE, PACKET, MAGIC
        source = Path(os.environ['EF2000_EXE']).resolve().parent / 'SB16.DIG'
        image = source.read_bytes()
        engine = RealMode(sound_hardware='sb16')
        memory = bytearray(MEMORY_SIZE)
        struct.pack_into('<H', memory, 0x463, 0x3d4)
        memory[0x11000:0x11000 + len(image)] = image
        struct.pack_into('<4H', memory, 0x11016, 0x220, 0xffff, 0xffff, 0xffff)
        for function in (0x300, 0x304):
            packet = PACKET.pack(MAGIC, 0x1100016a, function, 0, 0, 0, 0, 0,
                                 0, 0, 0, 0, 0x202, 0x66)
            response, memory = engine.execute(packet, bytes(memory[:1024]), bytes(memory))
        self.assertEqual(PACKET.unpack(response)[2], 0x0405)
        self.assertEqual(engine.audio_interrupts, 1)
        self.assertEqual(struct.unpack_from('<H', memory, 0x1157c)[0], 1)
        self.assertEqual(engine.sound_device.completed[0]['pcm'], b'\x80\x80')

    def test_original_driver_parses_resources_before_explicit_pic_boundary(self):
        from tools.real_mode import RealMode, MEMORY_SIZE, PACKET, MAGIC
        source = Path(os.environ['EF2000_EXE']).resolve().parent / 'SB16.DIG'
        image = source.read_bytes()
        reference = json.loads(REFERENCE.read_text())['digital_driver']
        self.assertEqual(hashlib.sha256(image).hexdigest(), reference['sha256'])
        engine = RealMode(sound_hardware='sb16-probe')
        memory = bytearray(MEMORY_SIZE)
        memory[0x11000:0x11000 + len(image)] = image
        struct.pack_into('<4H', memory, 0x11016, 0x220, 0xffff, 0xffff, 0xffff)
        def call(function, data):
            packet = PACKET.pack(MAGIC, 0x1100016a, function, 0, 0, 0, 0, 0,
                                 0, 0, 0, 0, 0x202, 0x66)
            return engine.execute(packet, bytes(1024), data)
        _, memory = call(0x300, bytes(memory))
        with self.assertRaisesRegex(ValueError, 'IN port 0xa1'):
            call(0x304, memory)
        failed = engine.failed_memory
        self.assertEqual(struct.unpack_from('<4H', failed, 0x11016), (0x220, 7, 1, 5))
        self.assertEqual(struct.unpack_from('<H', failed, 0x1157a)[0], 0x0405)
        self.assertEqual(engine.failure_state['cs'], 0x1100)
        self.assertEqual(engine.failure_state['ip'], 0x374)  # Unicorn advances past IN at 0372h.
        self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(), reference['sha256'])
