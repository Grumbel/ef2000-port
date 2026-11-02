#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Export original EF2000 screen-anchored GUI textures and tactical sprites.

The DID1ST menu does not store each window title/interior as an isolated
bitmap. It composites rectangles from full-screen green/gray textures at
their original screen coordinates. Unit markers, flags, and HUD icons are
packed into a separate 640x480 sprite sheet with palette index zero as the
red transparency background. No original art is embedded in this module.
"""

import json
from pathlib import Path

try:
    from .did_archive import DIDArchive
    from .ilbm_image import Image, decode, encode_bmp, encode_png
except ImportError:
    from did_archive import DIDArchive
    from ilbm_image import Image, decode, encode_bmp, encode_png


# Hashes have been verified against the user's 640x400 German TACTCOM screen.
# The first entry also has a known archive pathname: iff/256/newgui03.lbm.
SKIN_IMAGES = {
    'background': (0xab1e4e53, (640, 400)),
    'title': (0xad224253, (640, 400)),
    'panel': (0xac204053, (640, 400)),
    'map': (0x7a08862b, (400, 400)),
    'units': (0xade5ec9a, (640, 480)),
}


def keyed_units(image):
    """Preserve foreground red while masking only source palette index 0."""
    if (image.width, image.height) != SKIN_IMAGES['units'][1]:
        raise ValueError('unexpected tactical unit sprite sheet dimensions')
    if len(image.indexed) != image.width * image.height or len(image.palette) != 768:
        raise ValueError('invalid unit sprite sheet pixels or palette')
    alpha = bytes(0 if pixel == 0 else 255 for pixel in image.indexed)
    keyed_palette = b'\xff\x00\xff' + image.palette[3:]
    return (Image(image.width, image.height, image.indexed,
                  image.palette, alpha),
            Image(image.width, image.height, image.indexed, keyed_palette))


def skin_exports(images):
    """Build PNG/BMP files and provenance manifest from decoded images."""
    files = {}
    manifest = {'source': 'DID1ST.DAT', 'textures': {}}
    for name, (image_hash, dimensions) in SKIN_IMAGES.items():
        try:
            image = images[image_hash]
        except KeyError as error:
            raise ValueError(f'missing original skin image {image_hash:08x}') from error
        if (image.width, image.height) != dimensions:
            raise ValueError(f'unexpected dimensions for {name} image {image_hash:08x}')
        manifest['textures'][name] = {
            'hash': f'{image_hash:08x}',
            'dimensions': list(dimensions),
            'usage': ('screen-coordinate texture' if name in ('background', 'title', 'panel')
                      else 'tactical map' if name == 'map' else 'indexed sprite sheet'),
        }
        if name == 'units':
            transparent, keyed = keyed_units(image)
            files['skin-units.png'] = encode_png(transparent)
            files['skin-units.bmp'] = encode_bmp(keyed)
        else:
            files[f'skin-{name}.png'] = encode_png(image)
            if name != 'map':
                files[f'skin-{name}.bmp'] = encode_bmp(image)
    files['skin.json'] = (json.dumps(manifest, indent=2) + '\n').encode('utf-8')
    return files, manifest


def rip_skin(game_dir, out_dir):
    archive = DIDArchive.from_path(Path(game_dir) / 'DID1ST.DAT')
    images = {}
    for image_hash, _ in SKIN_IMAGES.values():
        entry = archive.by_hash.get(image_hash)
        if not entry:
            raise ValueError(f'DID1ST.DAT is missing skin texture {image_hash:08x}')
        images[image_hash] = decode(archive.member(entry))
    files, manifest = skin_exports(images)
    output = Path(out_dir)
    output.mkdir(parents=True, exist_ok=True)
    for name in files:
        if (output / name).exists():
            raise FileExistsError(f'refusing to overwrite {output / name}')
    for name, content in files.items():
        with (output / name).open('xb') as stream:
            stream.write(content)
    return manifest
