#!/usr/bin/env python3
"""Genera header C con font bitmap 4bpp per il display e-paper.

Renderizza un font TrueType (default: Segoe UI di Windows) nelle dimensioni
richieste e produce header C con:
  - bitmap 4bpp (2 pixel per byte, nibble alto = primo pixel)
  - tabella glifi ordinata per codepoint (ricerca binaria nel firmware)

Uso:
    python tools/gen_font.py

Gli header vengono scritti in lib/DisplayManager/fonts/.
"""

from __future__ import annotations

import os
from PIL import Image, ImageDraw, ImageFont

CHARSET = [chr(c) for c in range(32, 127)] + list("°àèéìíòóùúÀÈÉ")

OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "lib", "DisplayManager", "fonts")

FONTS = [
    # (nome_c, file_ttf, dimensione_px)
    ("FontSmall", "C:/Windows/Fonts/segoeui.ttf", 16),
    ("FontMed",   "C:/Windows/Fonts/segoeui.ttf", 22),
    ("FontBold",  "C:/Windows/Fonts/seguisb.ttf", 26),
    ("FontBig",   "C:/Windows/Fonts/seguisb.ttf", 64),
]


def render_glyph(font: ImageFont.FreeTypeFont, ch: str):
    """Rende un glifo; ritorna (w, h, left, top, advance, pixels_4bit)."""
    bbox = font.getbbox(ch)  # (x0, y0, x1, y1) rispetto al top della riga
    advance = int(round(font.getlength(ch)))
    if bbox is None or bbox[2] <= bbox[0] or bbox[3] <= bbox[1]:
        return 0, 0, 0, 0, advance, []
    x0, y0, x1, y1 = bbox
    w, h = x1 - x0, y1 - y0
    img = Image.new("L", (w, h), 0)
    draw = ImageDraw.Draw(img)
    draw.text((-x0, -y0), ch, font=font, fill=255)
    px = list(img.getdata())
    # 0 = sfondo (bianco su carta), 15 = inchiostro pieno
    quant = [min(15, (p + 8) // 17) for p in px]
    return w, h, x0, y0, advance, quant


def pack_4bpp(quant, w, h):
    """Impacchetta i valori 0..15 a 2 pixel per byte, riga per riga."""
    out = bytearray()
    for row in range(h):
        line = quant[row * w:(row + 1) * w]
        for i in range(0, w, 2):
            hi = line[i]
            lo = line[i + 1] if i + 1 < w else 0
            out.append((hi << 4) | lo)
    return bytes(out)


def gen_font(name: str, ttf: str, size: int) -> str:
    font = ImageFont.truetype(ttf, size)
    ascent, descent = font.getmetrics()

    glyphs = []  # (code, w, h, left, top, advance, offset)
    bitmap = bytearray()
    for ch in sorted(set(CHARSET), key=ord):
        w, h, left, top, adv, quant = render_glyph(font, ch)
        offset = len(bitmap)
        if w > 0:
            bitmap.extend(pack_4bpp(quant, w, h))
        glyphs.append((ord(ch), w, h, left, top, adv, offset))

    guard = name.upper()
    lines = []
    lines.append("// File generato automaticamente da tools/gen_font.py - NON MODIFICARE")
    lines.append(f"// Font: {os.path.basename(ttf)} @ {size}px, {len(glyphs)} glifi")
    lines.append("#pragma once")
    lines.append('#include "../Gfx/BitmapFont.h"')
    lines.append("")
    lines.append(f"static const uint8_t {name}Bitmap[] PROGMEM = {{")
    for i in range(0, len(bitmap), 20):
        chunk = ", ".join(f"0x{b:02x}" for b in bitmap[i:i + 20])
        lines.append(f"    {chunk},")
    lines.append("};")
    lines.append("")
    lines.append(f"static const Glyph {name}Glyphs[] PROGMEM = {{")
    for code, w, h, left, top, adv, off in glyphs:
        lines.append(
            f"    {{ {code}u, {w}, {h}, {left}, {top}, {adv}, {off}u }},")
    lines.append("};")
    lines.append("")
    lines.append(f"static const BitmapFont {name} = {{")
    lines.append(f"    {name}Bitmap, {name}Glyphs, {len(glyphs)}u,")
    lines.append(f"    {ascent}, {descent}, {ascent + descent}")
    lines.append("};")
    lines.append("")
    return "\n".join(lines)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, ttf, size in FONTS:
        content = gen_font(name, ttf, size)
        fname = {
            "FontSmall": "font_small.h",
            "FontMed": "font_med.h",
            "FontBold": "font_bold.h",
            "FontBig": "font_big.h",
        }[name]
        path = os.path.join(OUT_DIR, fname)
        with open(path, "w", encoding="utf-8") as f:
            f.write(content)
        print(f"{fname}: {os.path.getsize(path)} byte")


if __name__ == "__main__":
    main()
