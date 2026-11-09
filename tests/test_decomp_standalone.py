# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""The Linux decomp bootstrap must actually link and execute recovered C."""

from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class DecompStandaloneTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('gcc'), 'requires GCC')
    def test_gcc_executable_runs_recovered_highscore_code(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / 'ef2000-decomp'
            subprocess.run([
                'gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                '-DEF_DECOMP_NATIVE=1', '-fno-strict-aliasing', '-fwrapv',
                '-ffunction-sections', '-fdata-sections',
                '-Wno-error=int-to-pointer-cast', '-Wno-error=parentheses',
                '-I' + str(ROOT / 'decomp/include'),
                '-I' + str(ROOT / 'runtime'),
                str(ROOT / 'decomp/standalone/main.c'),
                str(ROOT / 'decomp/src/hiscore.c'),
                str(ROOT / 'decomp/src/runtime_helpers25.c'),
                str(ROOT / 'decomp/src/math_string.c'),
                str(ROOT / 'decomp/src/entity_helpers2.c'),
                str(ROOT / 'decomp/src/game_helpers28.c'),
                str(ROOT / 'decomp/src/game_helpers120.c'),
                str(ROOT / 'decomp/src/game_helpers129.c'),
                str(ROOT / 'decomp/src/runtime_helpers6.c'),
                str(ROOT / 'decomp/src/game_main.c'),
                str(ROOT / 'decomp/src/tactcom_prefix.c'),
                str(ROOT / 'decomp/src/ascii_stricmp.c'),
                str(ROOT / 'decomp/src/runtime_helpers19.c'),
                str(ROOT / 'decomp/src/runtime_helpers17.c'),
                str(ROOT / 'decomp/src/game_helpers81.c'),
                str(ROOT / 'decomp/src/engine_helpers3.c'),
                str(ROOT / 'decomp/src/entity_helpers.c'),
                str(ROOT / 'decomp/src/game_helpers57.c'),
                str(ROOT / 'decomp/src/game_helpers30.c'),
                str(ROOT / 'decomp/src/game_helpers33.c'),
                str(ROOT / 'decomp/src/game_helpers101.c'),
                str(ROOT / 'decomp/src/runtime_helpers21.c'),
                str(ROOT / 'decomp/src/runtime_helpers22.c'),
                str(ROOT / 'decomp/src/runtime_helpers34.c'),
                str(ROOT / 'decomp/src/runtime_helpers31.c'),
                str(ROOT / 'decomp/src/runtime_helpers4.c'),
                str(ROOT / 'decomp/src/init_tables1.c'),
                str(ROOT / 'decomp/src/leaf_helpers6.c'),
                str(ROOT / 'decomp/src/load_planes1.c'),
                str(ROOT / 'decomp/src/localized_file_handlers.c'),
                str(ROOT / 'decomp/src/script_helpers1.c'),
                str(ROOT / 'decomp/src/misc_drafts1.c'),
                str(ROOT / 'decomp/src/views_sequences.c'),
                str(ROOT / 'decomp/src/aid_data.c'),
                str(ROOT / 'decomp/src/ui_widget_helpers.c'),
                str(ROOT / 'decomp/src/leaf_helpers1.c'),
                str(ROOT / 'runtime/decomp_image.c'),
                str(ROOT / 'runtime/decomp_startup.c'),
                str(ROOT / 'runtime/decomp_files.c'),
                '-lcrypto', '-Wl,--gc-sections',
                '-o', str(executable),
            ], check=True, capture_output=True)
            result = subprocess.run([str(executable), '--self-test'],
                                    capture_output=True, text=True, check=True)
            self.assertIn('Linux / GCC', result.stdout)
            self.assertIn('high-score comparison: passed', result.stdout)

            default = subprocess.run([str(executable)], capture_output=True,
                                     text=True, check=True)
            self.assertIn('TACTCOM', default.stdout)

            invalid = subprocess.run([str(executable), '--data-dir', '/unused'],
                                     capture_output=True, text=True)
            self.assertEqual(invalid.returncode, 2)

            bad_image = Path(directory) / '_EF2000.EXE'
            bad_image.write_bytes(b'not original game data')
            rejected = subprocess.run([str(executable), '--image', str(bad_image)],
                                      capture_output=True, text=True)
            self.assertEqual(rejected.returncode, 1)
            self.assertIn('unsupported executable SHA-256', rejected.stderr)

            unsupported = subprocess.run([
                str(executable), '--image', str(bad_image), '--probe-arena',
            ], capture_output=True, text=True)
            self.assertEqual(unsupported.returncode, 1)

            # Opt-in only: proprietary game bytes are never a test fixture.
            # The loader itself checks the SHA-pinned German executable.
            original = os.environ.get('EF2000_EXE')
            if original:
                real = subprocess.run([
                    str(executable), '--image', original, '--probe-main',
                ], capture_output=True, text=True, check=True)
                self.assertIn('arena 0x004df000 (3000000 bytes)', real.stdout)
                self.assertIn('TACTCOM\n\nfound 57,618K bytes of memory',
                              real.stdout)
                self.assertIn('TACTCOM first resource payload size:', real.stdout)
                self.assertRegex(real.stdout,
                                 r'stopped at unimplemented call 0x(?:000eb25a|0009ccb8)')
                if 'stopped at unimplemented call 0x000eb25a' in real.stdout:
                    self.assertIn('Datenset nicht gefunden.', real.stderr)
                    self.assertIn('Prüfen Sie', real.stderr)
                check_file = Path(directory) / 'check.cd'
                check_file.write_bytes(b'RA\x00\x00\x07\x00\x00\x00')
                present = subprocess.run([
                    str(executable), '--image', original,
                    '--data-dir', directory, '--probe-main',
                ], capture_output=True, text=True, check=True)
                self.assertIn('TACTCOM first resource payload size: 7 bytes',
                              present.stdout)
                self.assertIn('stopped at unimplemented call 0x000ea50c',
                              present.stdout)
                self.assertIn('views\\views2.dat', present.stdout)
                self.assertFalse((Path(directory) / 'TACTCOM.$$$').exists())
                (Path(directory) / 'EF2000.CFG').write_bytes(b'CONTROL=1\r\n')
                configured = subprocess.run([
                    str(executable), '--image', original,
                    '--data-dir', directory, '--probe-main',
                ], capture_output=True, text=True)
                self.assertEqual(configured.returncode, 0,
                                 configured.stdout + '\n' + configured.stderr)
                self.assertIn('stopped at unimplemented call 0x000ea50c',
                              configured.stdout)
                # The original full settings sequence exercises all the
                # recovered type-3 and type-5 callbacks in order.
                (Path(directory) / 'EF2000.CFG').write_bytes(
                    b'DETAIL=2\r\nREZ=0\r\nCONTROL=1\r\nMUSIC=1,127\r\n'
                    b'STICK=15\r\nJOYSTICK=4000,63,63,63,63,0,126,0,126,63,0\r\n'
                    b'SOUNDFX=1,127\r\nSPEECH=1\r\nVRHELMET=0\r\nJOY_FACTOR=0\r\n')
                mixed = subprocess.run([
                    str(executable), '--image', original,
                    '--data-dir', directory, '--probe-main',
                ], capture_output=True, text=True, check=True)
                self.assertIn('stopped at unimplemented call 0x000ea50c',
                              mixed.stdout)
                (Path(directory) / 'EF2000.CFG').write_bytes(
                    b'MUSIC=1,127\r\nSOUNDFX=1,127\r\n')
                sound_only = subprocess.run([
                    str(executable), '--image', original,
                    '--data-dir', directory, '--probe-main',
                ], capture_output=True, text=True, check=True)
                self.assertIn('stopped at unimplemented call 0x000ea50c',
                              sound_only.stdout)
                (Path(directory) / 'EF2000.CFG').write_bytes(
                    b';ignored\r\n \t\r\n\r\n')
                blank = subprocess.run([
                    str(executable), '--image', original,
                    '--data-dir', directory, '--probe-main',
                ], capture_output=True, text=True, check=True)
                self.assertIn('stopped at unimplemented call 0x000ea50c',
                              blank.stdout)

                # An invented plain resource verifies the recovered rewind,
                # stream line/token reader, '#' record, camera selector,
                # text, scalar, enum, vector, look-at, clamp and HUD options against the
                # original image's embedded labels and float constants.
                views = Path(directory) / 'views' / 'views2.dat'
                views.parent.mkdir(parents=True)
                views.write_bytes(
                    b'# synthetic\r\nMODE REL_CAM\r\n'
                    b'Name "Overhead View"\r\nKey V1\r\nLocation INSIDE\r\n'
                    b'PosDamping 12.5\r\nAngDamping 80.125\r\n'
                    b'MinAlt 2.25\r\nRollLock BANKING_HORIZON\r\n'
                    b'OffSet 1.25 -2.5 0\r\nRandomOffset 3 0.5 -1.125\r\n'
                    b'InitialOffSet -1 2.5 3.125\r\n'
                    b'LookAtObj SOURCE\r\nLookAt 1.25 -2.5 0.5\r\n'
                    b'Dist 0.25 1.5 10 2\r\n'
                    b'XClamp 1.5 -2.25\r\nYClamp 0.5 3.25\r\n'
                    b'HUDCenter -17.9\r\nHUDHeight 240.9\r\n'
                    b'Insts FrontInternalCock\r\nCockpit ON\r\nEND\r\n')
                plain = subprocess.run([
                    str(executable), '--image', original,
                    '--data-dir', directory, '--probe-main',
                ], capture_output=True, text=True, check=True)
                self.assertIn('stopped at unimplemented call 0x000ea50c',
                              plain.stdout)
                # The next original caller loads \views\seq2.dat, resolves
                # camera names against views2.dat, then calls Aid.c's
                # ReadAidData(), which opens a distinct aid2.dat resource.
                seq = views.parent / 'seq2.dat'
                seq.write_bytes(
                    b'# synthetic_list\r\n'
                    b'Name primary\r\nName secondary\r\nON\r\n'
                    b'synthetic 1.5 2.25\r\nEND\r\n')
                continued = subprocess.run([
                    str(executable), '--image', original,
                    '--data-dir', directory, '--probe-main',
                ], capture_output=True, text=True, check=True)
                self.assertIn('stopped at unimplemented call 0x000ea50c',
                              continued.stdout)
                aid = views.parent / 'aid2.dat'
                aid.write_bytes(
                    b'AID_HEADER\n' +
                    b''.join(f'row{i} {i + 130}\n'.encode() for i in range(27)) +
                    b'AID_SECOND_HEADER\n' +
                    b''.join(f'row{i} {i + 240}\n'.encode() for i in range(8)))
                aid_continued = subprocess.run([
                    str(executable), '--image', original,
                    '--data-dir', directory, '--probe-main',
                ], capture_output=True, text=True, check=True)
                self.assertIn('stopped at unimplemented call 0x0000120e',
                              aid_continued.stdout)
                views.write_bytes(b'JR\0\0\x08\0\0\0')
                packed = subprocess.run([
                    str(executable), '--image', original,
                    '--data-dir', directory, '--probe-main',
                ], capture_output=True, text=True, check=True)
                self.assertIn('stopped at unimplemented call 0x0009cd53',
                              packed.stdout)

    @unittest.skipUnless(shutil.which('gcc'), 'requires GCC')
    def test_guest_memory_and_x32_envelope(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / 'decomp-image-test'
            subprocess.run([
                'gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                '-DEF_DECOMP_NATIVE=1', '-fno-strict-aliasing', '-fwrapv',
                '-I' + str(ROOT / 'decomp/include'),
                '-I' + str(ROOT / 'runtime'),
                str(ROOT / 'tests/decomp_image_test.c'),
                str(ROOT / 'decomp/src/game_helpers25.c'),
                str(ROOT / 'runtime/decomp_image.c'), '-lcrypto',
                '-o', str(executable),
            ], check=True, capture_output=True)
            subprocess.run([str(executable)], capture_output=True, check=True)

    @unittest.skipUnless(shutil.which('gcc'), 'requires GCC')
    def test_first_recovered_game_startup_stage(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / 'decomp-startup-test'
            subprocess.run([
                'gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                '-DEF_DECOMP_NATIVE=1', '-fno-strict-aliasing', '-fwrapv',
                '-ffunction-sections', '-fdata-sections',
                '-Wno-error=int-to-pointer-cast', '-Wno-error=parentheses',
                '-I' + str(ROOT / 'decomp/include'),
                '-I' + str(ROOT / 'runtime'),
                str(ROOT / 'tests/decomp_startup_test.c'),
                str(ROOT / 'decomp/src/runtime_helpers25.c'),
                str(ROOT / 'decomp/src/math_string.c'),
                str(ROOT / 'decomp/src/entity_helpers2.c'),
                str(ROOT / 'decomp/src/game_helpers28.c'),
                str(ROOT / 'decomp/src/game_helpers120.c'),
                str(ROOT / 'decomp/src/game_helpers129.c'),
                str(ROOT / 'decomp/src/runtime_helpers6.c'),
                str(ROOT / 'decomp/src/game_main.c'),
                str(ROOT / 'decomp/src/tactcom_prefix.c'),
                str(ROOT / 'decomp/src/ascii_stricmp.c'),
                str(ROOT / 'decomp/src/runtime_helpers19.c'),
                str(ROOT / 'decomp/src/runtime_helpers17.c'),
                str(ROOT / 'decomp/src/game_helpers81.c'),
                str(ROOT / 'decomp/src/engine_helpers3.c'),
                str(ROOT / 'decomp/src/entity_helpers.c'),
                str(ROOT / 'decomp/src/game_helpers57.c'),
                str(ROOT / 'decomp/src/game_helpers30.c'),
                str(ROOT / 'decomp/src/game_helpers33.c'),
                str(ROOT / 'decomp/src/game_helpers101.c'),
                str(ROOT / 'decomp/src/runtime_helpers21.c'),
                str(ROOT / 'decomp/src/runtime_helpers22.c'),
                str(ROOT / 'decomp/src/runtime_helpers34.c'),
                str(ROOT / 'decomp/src/runtime_helpers31.c'),
                str(ROOT / 'decomp/src/runtime_helpers4.c'),
                str(ROOT / 'decomp/src/init_tables1.c'),
                str(ROOT / 'decomp/src/leaf_helpers6.c'),
                str(ROOT / 'decomp/src/load_planes1.c'),
                str(ROOT / 'decomp/src/localized_file_handlers.c'),
                str(ROOT / 'decomp/src/script_helpers1.c'),
                str(ROOT / 'decomp/src/misc_drafts1.c'),
                str(ROOT / 'decomp/src/views_sequences.c'),
                str(ROOT / 'decomp/src/aid_data.c'),
                str(ROOT / 'decomp/src/ui_widget_helpers.c'),
                str(ROOT / 'decomp/src/leaf_helpers1.c'),
                str(ROOT / 'runtime/decomp_image.c'),
                str(ROOT / 'runtime/decomp_startup.c'),
                str(ROOT / 'runtime/decomp_files.c'),
                '-lcrypto', '-Wl,--gc-sections', '-o', str(executable),
            ], check=True, capture_output=True)
            result = subprocess.run([str(executable)], capture_output=True)
            self.assertEqual(result.returncode, 0,
                             result.stdout.decode(errors='replace') +
                             result.stderr.decode(errors='replace'))
            self.assertIn(b'Memory: 60,415K', result.stdout)


if __name__ == '__main__':
    unittest.main()
