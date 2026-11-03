# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Asset-free guards for AppStream/desktop/man-page packaging metadata."""

from pathlib import Path
import unittest
from xml.etree import ElementTree


ROOT = Path(__file__).resolve().parent.parent
APP_ID = 'io.github.grumbel.ef2000'


class PackagingMetadataTests(unittest.TestCase):
    def test_appstream_id_filename_developer_and_launchable(self):
        metainfo = ROOT / 'data' / f'{APP_ID}.metainfo.xml'
        root = ElementTree.parse(metainfo).getroot()
        self.assertEqual(root.attrib.get('type'), 'desktop-application')
        self.assertEqual(root.findtext('id'), APP_ID)
        self.assertEqual(root.findtext('launchable[@type="desktop-id"]'),
                         'ef2000.desktop')
        developer = root.find('developer')
        self.assertIsNotNone(developer)
        self.assertEqual(developer.attrib.get('id'), 'io.github.grumbel')
        self.assertTrue(developer.findtext('name'))

    def test_desktop_icon_files_exist(self):
        desktop = (ROOT / 'data/ef2000.desktop').read_text()
        self.assertIn('\nIcon=ef2000\n', desktop)
        self.assertTrue((ROOT / 'data/icons/ef2000.svg').is_file())
        for size in ('16x16', '32x32', '48x48', '64x64', '128x128', '256x256'):
            with self.subTest(size=size):
                icon = ROOT / 'data/icons/hicolor' / size / 'apps/ef2000.png'
                self.assertTrue(icon.is_file())

    def test_man_page_avoids_mandoc_line_length_warnings(self):
        manpage = ROOT / 'data/ef2000.6'
        lines = manpage.read_text().splitlines()
        too_long = [(number, len(line)) for number, line in enumerate(lines, 1)
                    if len(line.encode('utf-8')) > 80]
        self.assertEqual(too_long, [])


if __name__ == '__main__':
    unittest.main()
