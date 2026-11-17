#!/usr/bin/env python3
"""Package the approved Release, Pre, DEV and CI PNG masters as native app icons.

Requires Pillow 12.3.0 for consistent native-size resampling and preview fonts.
The committed icons are consumed directly; building the mod needs no Pillow.
"""

from __future__ import annotations

import argparse
import io
from pathlib import Path
import shutil
import struct

from PIL import Image, ImageDraw, ImageFont, __version__ as pillow_version


ROOT = Path(__file__).resolve().parents[1]
CHANNELS = {
    "release": "Release",
    "pre": "Pre",
    "dev": "DEV",
    "ci": "CI",
}
# Match iconutil's small ARGB entries and larger/Retina PNG representations.
# PNG data in icp4/icp5/icp6 renders as coloured noise in some macOS icon views.
ICNS_SIZES = (
    (b"ic04", 16), (b"ic05", 32),
    (b"ic07", 128), (b"ic08", 256), (b"ic09", 512), (b"ic10", 1024),
    (b"ic11", 32), (b"ic12", 64), (b"ic13", 256), (b"ic14", 512),
)


def load_masters(directory: Path) -> dict[str, Image.Image]:
    images = {}
    for channel in CHANNELS:
        path = directory / f"launcher-{channel}.png"
        with Image.open(path) as image:
            if image.format != "PNG" or image.mode != "RGBA":
                raise ValueError(f"Expected an RGBA PNG master: {path}")
            if image.width != image.height or image.width < 1024:
                raise ValueError(f"Expected a square master of at least 1024 pixels: {path}")
            image.load()
            images[channel] = image.copy()
    return images


def png_bytes(image: Image.Image) -> bytes:
    stream = io.BytesIO()
    image.save(stream, format="PNG", compress_level=9)
    return stream.getvalue()


def icns_packbits(data: bytes) -> bytes:
    # Literal packets are sufficient for these tiny planes: a control byte
    # 0..127 precedes 1..128 unchanged bytes. No repeat-run optimizer is needed.
    packets = (data[offset:offset + 128] for offset in range(0, len(data), 128))
    return b"".join(bytes((len(packet) - 1,)) + packet for packet in packets)


def argb_bytes(image: Image.Image) -> bytes:
    # Apple's small-icon payload is an ARGB marker followed by independently
    # compressed, straight-alpha planes. Premultiplication would darken edges.
    rgba = image.convert("RGBA")
    return b"ARGB" + b"".join(icns_packbits(rgba.getchannel(channel).tobytes()) for channel in "ARGB")


def write_icns(image: Image.Image, path: Path) -> None:
    # ICNS is a big-endian container of typed image chunks. Small native sizes
    # use ARGB; Retina and larger sizes use PNG, including 16@2x and 32@2x.
    images = {size: image.resize((size, size), Image.Resampling.LANCZOS) for _, size in ICNS_SIZES}
    entries = []
    for kind, size in ICNS_SIZES:
        payload = argb_bytes(images[size]) if kind in (b"ic04", b"ic05") else png_bytes(images[size])
        entries.append((kind, 8 + len(payload), payload))
    table = b"".join(kind + struct.pack(">I", length) for kind, length, _ in entries)
    chunks = b"TOC " + struct.pack(">I", 8 + len(table)) + table
    chunks += b"".join(kind + struct.pack(">I", length) + payload for kind, length, payload in entries)
    path.write_bytes(b"icns" + struct.pack(">I", 8 + len(chunks)) + chunks)


def contact_sheet(images: dict[str, Image.Image], path: Path) -> None:
    sheet = Image.new("RGB", (380 * len(images), 560), "#101923")
    draw = ImageDraw.Draw(sheet)
    title = ImageFont.load_default(size=27)
    small = ImageFont.load_default(size=19)
    for column, (channel, image) in enumerate(images.items()):
        x = column * 380
        draw.text((x + 30, 18), CHANNELS[channel], font=title, fill="#eef6fa")
        sheet.paste(image.resize((300, 300), Image.Resampling.LANCZOS), (x + 35, 60),
                    image.resize((300, 300), Image.Resampling.LANCZOS))
        for offset, size in ((40, 64), (180, 32), (285, 16)):
            thumb = image.resize((size, size), Image.Resampling.LANCZOS)
            sheet.paste(thumb, (x + offset, 410), thumb)
            draw.text((x + offset, 490), f"{size}px", font=small, fill="#acc1ce")
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path, format="PNG", compress_level=9)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, default=ROOT / "assets/launcher-icons")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "assets/launcher-icons")
    parser.add_argument("--contact-sheet", type=Path)
    args = parser.parse_args()
    if pillow_version != "12.3.0":
        raise SystemExit("Regeneration requires Pillow 12.3.0: python -m pip install Pillow==12.3.0")
    # PNG masters own the artwork and channel labels. Packing only resamples
    # native-size representations; it never redraws or recolors the mark.
    images = load_masters(args.source_dir)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for channel, image in images.items():
        source = args.source_dir / f"launcher-{channel}.png"
        destination = args.output_dir / source.name
        if source.resolve() != destination.resolve():
            shutil.copyfile(source, destination)
        write_icns(image, args.output_dir / f"launcher-{channel}.icns")
    if args.contact_sheet:
        contact_sheet(images, args.contact_sheet)


if __name__ == "__main__":
    main()
