#!/usr/bin/env python3
"""Check committed ICNS pixels and encodings; native Finder presentation is separate."""

import importlib.util
import io
from pathlib import Path
import struct
import unittest

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("launcher_icons", ROOT / "scripts/generate-launcher-icons.py")
ICONS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ICONS)
SIZES = {
    b"ic04": 16, b"ic05": 32, b"ic07": 128, b"ic08": 256, b"ic09": 512,
    b"ic10": 1024, b"ic11": 32, b"ic12": 64, b"ic13": 256, b"ic14": 512,
}


def chunks(raw):
    if raw[:4] != b"icns" or struct.unpack(">I", raw[4:8])[0] != len(raw):
        raise ValueError("invalid ICNS header")
    result = {}
    table = bytearray()
    position = 8
    while position < len(raw):
        kind, length = struct.unpack(">4sI", raw[position:position + 8])
        if length < 8 or position + length > len(raw) or kind in result:
            raise ValueError("invalid ICNS chunk")
        result[kind] = raw[position + 8:position + length]
        if kind != b"TOC ":
            table.extend(raw[position:position + 8])
        position += length
    if position != len(raw) or result.pop(b"TOC ") != table:
        raise ValueError("ICNS table of contents disagrees with chunks")
    return result


def decode_argb(payload, size):
    if payload[:4] != b"ARGB":
        raise ValueError("small icon does not contain ARGB pixels")
    position = 4
    planes = []
    for _ in range(4):
        plane = bytearray()
        while len(plane) < size * size:
            control = payload[position]
            position += 1
            if control < 128:
                count = control + 1
                plane.extend(payload[position:position + count])
                position += count
            else:
                plane.extend([payload[position]] * (control - 125))
                position += 1
        if len(plane) != size * size:
            raise ValueError("ARGB run exceeds the channel boundary")
        planes.append(Image.frombytes("L", (size, size), bytes(plane)))
    if position != len(payload):
        raise ValueError("trailing ARGB data")
    alpha, red, green, blue = planes
    return Image.merge("RGBA", (red, green, blue, alpha))


class LauncherIconTests(unittest.TestCase):
    def test_committed_representations_preserve_source_pixels(self):
        for channel in ("release", "pre", "dev", "ci"):
            with self.subTest(channel=channel):
                directory = ROOT / "assets/launcher-icons"
                source = Image.open(directory / f"launcher-{channel}.png").convert("RGBA")
                entries = chunks((directory / f"launcher-{channel}.icns").read_bytes())
                self.assertEqual(set(entries), set(SIZES))
                for kind, size in SIZES.items():
                    with self.subTest(kind=kind):
                        payload = entries[kind]
                        if kind in (b"ic04", b"ic05"):
                            image = decode_argb(payload, size)
                        else:
                            self.assertTrue(payload.startswith(b"\x89PNG\r\n\x1a\n"))
                            image = Image.open(io.BytesIO(payload)).convert("RGBA")
                        self.assertEqual(image.size, (size, size))
                        expected = source.resize((size, size), Image.Resampling.LANCZOS)
                        self.assertEqual(image.tobytes(), expected.tobytes())

    def test_small_encoding_preserves_transparency_and_channel_order(self):
        # Channel lengths cross the 128-byte literal boundary. Distinct
        # RGB values beneath partial/zero alpha detect accidental premultiplication.
        image = Image.new("RGBA", (32, 32))
        image.putdata([(21, 127, 243, 0)] * 131 + [(11, 91, 221, 73)] * 131
                      + [(index % 256, index * 3 % 256, index * 7 % 256, index * 11 % 256)
                         for index in range(32 * 32 - 262)])
        self.assertEqual(decode_argb(ICONS.argb_bytes(image), 32).tobytes(), image.tobytes())


if __name__ == "__main__":
    unittest.main()
