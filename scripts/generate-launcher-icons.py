#!/usr/bin/env python3
"""Render the fork's existing waffle mark as Release, Pre, DEV and CI app icons.

Requires Pillow 12.3.0 for its bundled Aileron font and consistent rasterization.
The committed icons are consumed directly; building the mod needs no Pillow.
"""

from __future__ import annotations

import argparse
import io
from pathlib import Path
import struct

from PIL import Image, ImageDraw, ImageFilter, ImageFont, __version__ as pillow_version


ROOT = Path(__file__).resolve().parents[1]
SIZE = 1024
SCALE = 3
CHANNELS = {
    "release": ("Release", "#5ddce5", "#164652"),
    "pre": ("Pre", "#ffbd50", "#4b351c"),
    "dev": ("DEV", "#bd9cff", "#35274c"),
    "ci": ("CI", "#a6c4dc", "#253b50"),
}
# Match iconutil's small ARGB entries and larger/Retina PNG representations.
# PNG data in icp4/icp5/icp6 renders as coloured noise in some macOS icon views.
ICNS_SIZES = (
    (b"ic04", 16), (b"ic05", 32),
    (b"ic07", 128), (b"ic08", 256), (b"ic09", 512), (b"ic10", 1024),
    (b"ic11", 32), (b"ic12", 64), (b"ic13", 256), (b"ic14", 512),
)


def rgb(color: str) -> tuple[int, int, int]:
    return tuple(int(color[index:index + 2], 16) for index in (1, 3, 5))


def rounded(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int], radius: int,
            fill: str | None, outline: str | None = None, width: int = 1) -> None:
    draw.rounded_rectangle(tuple(value * SCALE for value in box), radius * SCALE,
                           fill=fill, outline=outline, width=width * SCALE)


def render(channel: str) -> Image.Image:
    label, accent, badge = CHANNELS[channel]
    canvas = Image.new("RGBA", (SIZE * SCALE, SIZE * SCALE))

    # Same navy panel, cyan rounded waffle cells and gold center as the existing
    # guffawaffle-build-badge.png. The loading-screen wording is not part of an
    # app icon; the lower panel now carries only the build channel.
    glow = Image.new("RGBA", canvas.size)
    rounded(ImageDraw.Draw(glow), (70, 70, 954, 954), 186, accent)
    glow = glow.filter(ImageFilter.GaussianBlur(18 * SCALE))
    glow.putalpha(glow.getchannel("A").point(lambda value: value // 3))
    canvas.alpha_composite(glow)

    mask = Image.new("L", canvas.size)
    ImageDraw.Draw(mask).rounded_rectangle((64 * SCALE, 64 * SCALE, 960 * SCALE, 960 * SCALE),
                                          radius=184 * SCALE, fill=255)
    panel = Image.new("RGBA", canvas.size)
    draw = ImageDraw.Draw(panel)
    top, bottom = rgb("#103d4e"), rgb("#081f31")
    for y in range(64 * SCALE, 961 * SCALE):
        ratio = (y / SCALE - 64) / 896
        color = tuple(round(start + (end - start) * ratio) for start, end in zip(top, bottom))
        draw.line((64 * SCALE, y, 960 * SCALE, y), fill=(*color, 255))
    panel.putalpha(mask)
    canvas.alpha_composite(panel)
    draw = ImageDraw.Draw(canvas)
    rounded(draw, (64, 64, 960, 960), 184, None, accent, 14)
    rounded(draw, (85, 85, 939, 939), 165, None, "#d8eef4", 3)

    # The nine double-outline cells are the waffle branch's existing mark.
    for row in range(3):
        for column in range(3):
            x, y = 259 + column * 180, 170 + row * 180
            center = row == 1 and column == 1
            rounded(draw, (x, y, x + 146, y + 146), 31,
                    "#765017" if center else "#1d6071", "#ffbd50" if center else "#5ddce5", 13)
            rounded(draw, (x + 31, y + 31, x + 115, y + 115), 18, "#081f31", "#e5f3f7", 3)

    rounded(draw, (121, 738, 903, 922), 64, badge, accent, 7)
    font = ImageFont.load_default(size=170 * SCALE)
    box = draw.textbbox((0, 0), label, font=font, stroke_width=1 * SCALE)
    position = ((SIZE * SCALE - (box[2] - box[0])) / 2 - box[0],
                830 * SCALE - (box[3] - box[1]) / 2 - box[1])
    draw.text(position, label, font=font, fill="#f2f8fb", stroke_width=1 * SCALE)
    return canvas.resize((SIZE, SIZE), Image.Resampling.LANCZOS)


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
        draw.text((x + 30, 18), CHANNELS[channel][0], font=title, fill="#eef6fa")
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
    parser.add_argument("--output-dir", type=Path, default=ROOT / "assets/launcher-icons")
    parser.add_argument("--contact-sheet", type=Path)
    args = parser.parse_args()
    if pillow_version != "12.3.0":
        raise SystemExit("Regeneration requires Pillow 12.3.0: python -m pip install Pillow==12.3.0")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    images = {channel: render(channel) for channel in CHANNELS}
    for channel, image in images.items():
        (args.output_dir / f"launcher-{channel}.png").write_bytes(png_bytes(image))
        write_icns(image, args.output_dir / f"launcher-{channel}.icns")
    if args.contact_sheet:
        contact_sheet(images, args.contact_sheet)


if __name__ == "__main__":
    main()
