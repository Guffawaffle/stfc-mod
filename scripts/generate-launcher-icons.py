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
# PNG-backed native sizes, followed by Retina representations of native sizes.
ICNS_SIZES = (
    (b"icp4", 16), (b"icp5", 32), (b"icp6", 64),
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


def write_icns(image: Image.Image, path: Path) -> None:
    # ICNS is a big-endian container of typed image chunks. Retain explicit
    # 16/32/64 native entries as well as modern Retina PNG entries; Pillow's
    # standard ICNS writer omits the native 16px representation.
    pngs = {size: png_bytes(image.resize((size, size), Image.Resampling.LANCZOS))
            for _, size in ICNS_SIZES}
    entries = [(kind, 8 + len(pngs[size]), pngs[size]) for kind, size in ICNS_SIZES]
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
