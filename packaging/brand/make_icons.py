#!/usr/bin/env python3
"""Every icon and logo file of Everplay, made from the logo in this folder (needs Pillow).

    python packaging/brand/make_icons.py                       the desktop app's files (this repository)
    python packaging/brand/make_icons.py --android <res dir>   the Android launcher icons
    python packaging/brand/make_icons.py --web <site root>     the website and the browser version

everplay-logo.png is the logo as drawn (1024 px, full-bleed). The raster icons come from it; the
SVGs are a vector redrawing of it (the "e" and the disc in the middle), used where it has to scale
or move: the Linux icon, the website's mark and its 3D scene, the app's own LogoMark.qml.
"""

import io
import math
import shutil
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
LOGO = Image.open(HERE / "everplay-logo.png").convert("RGB")
RADIUS = 0.225          # the rounded tile of the app icons, of the side
BG_TOP, BG_BOTTOM = (55, 55, 55), (23, 23, 23)

# ---- the vector drawing (1024 units, the same place as in the logo) ----
CX, CY, R, W = 518, 520, 275, 112          # the ring of the "e": centre line and width
A0, A1 = 33, 326                            # where it starts (the lower end) and turns into the bar, degrees
BAR_END = (672, 466)
DISC = 100                                  # the disc in the middle
INK = "#efefef"


def _pt(a, r):
    return CX + r * math.cos(math.radians(a)), CY + r * math.sin(math.radians(a))


def e_path():
    """The "e" as one stroke (width W, round ends and joins)."""
    s, e = _pt(A0, R), _pt(A1, R)
    return f"M{s[0]:.1f} {s[1]:.1f} A{R} {R} 0 1 1 {e[0]:.1f} {e[1]:.1f} L{BAR_END[0]} {BAR_END[1]}"


def ticks(count=72):
    """The short spectrum lines around the disc."""
    out = []
    for i in range(count):
        a = i * 360 / count
        length = 8 + 6 * (math.sin(i * 1.7) + 1)
        (x1, y1), (x2, y2) = _pt(a, DISC - 4), _pt(a, DISC + length)
        out.append(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}"/>')
    return "".join(out)


def mark_svg_body(ink=INK, with_ticks=True):
    t = f'<g stroke="#9a9a9a" stroke-width="3" stroke-linecap="round">{ticks()}</g>' if with_ticks else ""
    return (f'<path d="{e_path()}" fill="none" stroke="{ink}" stroke-width="{W}" stroke-linecap="round" stroke-linejoin="round"/>'
            f'{t}<circle cx="{CX}" cy="{CY}" r="{DISC}" fill="{ink}"/>')


def icon_svg(rounded=True):
    rx = round(1024 * RADIUS) if rounded else 0
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">'
            '<defs><linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">'
            '<stop offset="0" stop-color="#383838"/><stop offset="1" stop-color="#171717"/></linearGradient></defs>'
            f'<rect width="1024" height="1024" rx="{rx}" fill="url(#bg)"/>{mark_svg_body()}</svg>\n')


def mark_svg():
    """The mark alone, on nothing (for light text on the page)."""
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="160 160 704 704">{mark_svg_body()}</svg>\n'


# ---- raster ----
def rounded(img, radius=RADIUS):
    size = img.size[0]
    big = Image.new("L", (size * 4, size * 4), 0)
    ImageDraw.Draw(big).rounded_rectangle((0, 0, size * 4 - 1, size * 4 - 1), radius=size * 4 * radius, fill=255)
    out = img.convert("RGBA")
    out.putalpha(big.resize((size, size), Image.LANCZOS))
    return out


def icon(size, round_corners=True):
    img = LOGO.resize((size, size), Image.LANCZOS)
    return rounded(img) if round_corners else img.convert("RGBA")


def png_bytes(img):
    buf = io.BytesIO()
    img.save(buf, "PNG", optimize=True)
    return buf.getvalue()


def write_ico(path, sizes=(16, 20, 24, 32, 40, 48, 64, 128, 256)):
    pngs = [png_bytes(icon(s)) for s in sizes]
    offset = 6 + 16 * len(sizes)
    head = struct.pack("<HHH", 0, 1, len(sizes))
    entries = b""
    for s, data in zip(sizes, pngs):
        entries += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    path.write_bytes(head + entries + b"".join(pngs))


def gradient(size):
    img = Image.new("RGB", (size, size))
    d = ImageDraw.Draw(img)
    for y in range(size):
        t = y / max(1, size - 1)
        d.line([(0, y), (size, y)], fill=tuple(round(a + (b - a) * t) for a, b in zip(BG_TOP, BG_BOTTOM)))
    return img


def padded(size, scale):
    """The logo smaller on a larger canvas of its own background (adaptive and maskable icons)."""
    canvas = gradient(size)
    inner = round(size * scale)
    logo = LOGO.resize((inner, inner), Image.LANCZOS)
    feather = Image.new("L", (inner, inner), 0)
    edge = max(2, inner // 20)
    ImageDraw.Draw(feather).rounded_rectangle((edge, edge, inner - edge, inner - edge), radius=edge * 2, fill=255)
    from PIL import ImageFilter
    feather = feather.filter(ImageFilter.GaussianBlur(edge / 2))
    canvas.paste(logo, ((size - inner) // 2, (size - inner) // 2), feather)
    return canvas


def desktop():
    (HERE / "everplay.svg").write_text(icon_svg(rounded=False), encoding="utf-8")
    (HERE / "everplay-mark.svg").write_text(mark_svg(), encoding="utf-8")
    (REPO / "packaging" / "linux" / "everplay.svg").write_text(icon_svg(), encoding="utf-8")
    write_ico(REPO / "packaging" / "windows" / "everplay.ico")
    icon(256).save(REPO / "packaging" / "windows" / "setup" / "logo.png", optimize=True)
    icon(256).save(HERE / "icon-256.png", optimize=True)
    print("desktop icons written")


def android(res):
    res = Path(res)
    for density, px in (("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192)):
        d = res / f"mipmap-{density}"
        d.mkdir(parents=True, exist_ok=True)
        icon(px).save(d / "ic_launcher.png", optimize=True)
        # adaptive icon: the launcher masks a 108 dp square to its own shape and shows the middle 72 dp
        padded(round(px * 108 / 48), 0.74).save(d / "ic_launcher_background.png", optimize=True)
    anydpi = res / "mipmap-anydpi-v26"
    anydpi.mkdir(parents=True, exist_ok=True)
    (anydpi / "ic_launcher.xml").write_text(
        '<?xml version="1.0" encoding="utf-8"?>\n'
        '<!-- The logo is the background layer; the launcher cuts it to its own shape. -->\n'
        '<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
        '    <background android:drawable="@mipmap/ic_launcher_background" />\n'
        '    <foreground android:drawable="@android:color/transparent" />\n'
        '</adaptive-icon>\n', encoding="utf-8")
    print("android icons written")


def web(root):
    root = Path(root)
    static, icons = root / "site" / "static", root / "app" / "icons"
    icons.mkdir(parents=True, exist_ok=True)
    (static / "icon.svg").write_text(icon_svg(), encoding="utf-8")
    (static / "mark.svg").write_text(mark_svg(), encoding="utf-8")
    icon(512).save(static / "logo-512.png", optimize=True)          # also the app's Discord status picture
    icon(192).save(icons / "icon-192.png", optimize=True)
    icon(512).save(icons / "icon-512.png", optimize=True)
    padded(512, 0.8).save(icons / "icon-maskable-512.png", optimize=True)
    icon(180, round_corners=False).convert("RGB").save(icons / "apple-touch-icon.png", optimize=True)
    og(static / "og.jpg")
    print("web icons written")


def og(path):
    """The picture link previews show (1200 × 630)."""
    w, h = 1200, 630
    img = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / (h - 1)
        d.line([(0, y), (w, y)], fill=tuple(round(a + (b - a) * t) for a, b in zip((40, 40, 42), (14, 14, 16))))
    glow = Image.new("L", (w, h), 0)
    ImageDraw.Draw(glow).ellipse((w / 2 - 420, h / 2 - 330, w / 2 + 420, h / 2 + 330), fill=70)
    from PIL import ImageFilter
    img.paste((255, 255, 255), (0, 0), glow.filter(ImageFilter.GaussianBlur(120)))
    logo = icon(250)
    img.paste(logo, (w // 2 - 125, 70), logo)
    font = Path("C:/Windows/Fonts/bahnschrift.ttf")
    if not font.exists():
        font = Path("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf")
    title = ImageFont.truetype(str(font), 104)
    try:
        title.set_variation_by_name("Bold")
    except Exception:
        pass
    sub = ImageFont.truetype(str(font), 32)
    d.text((w / 2, 400), "Everplay", font=title, fill=(245, 245, 245), anchor="mt")
    d.text((w / 2, 530), "One player for all your music", font=sub, fill=(170, 170, 176), anchor="mt")
    img.save(path, quality=90, optimize=True)


if __name__ == "__main__":
    args = sys.argv[1:]
    if not args:
        desktop()
    elif args[0] == "--android":
        android(args[1])
    elif args[0] == "--web":
        web(args[1])
    else:
        sys.exit(__doc__)
