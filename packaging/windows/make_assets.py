# Generuje grafiki instalatora (PNG + ICO) z plików SVG przy użyciu rsvg-convert z MSYS2.
import struct, subprocess, sys
from pathlib import Path

here = Path(__file__).resolve().parent
icon_svg = here.parent / "linux" / "nokturn.svg"
out = here / "assets"
out.mkdir(exist_ok=True)
rsvg = sys.argv[1] if len(sys.argv) > 1 else "rsvg-convert"

def render(svg, dest, w, h):
    subprocess.run([rsvg, "-w", str(w), "-h", str(h), "-o", str(dest), str(svg)], check=True)
    return dest.read_bytes()

# Ikona: wiele rozmiarów PNG w jednym pliku .ico.
sizes = [16, 24, 32, 48, 64, 128, 256]
pngs = [render(icon_svg, out / f"icon-{s}.png", s, s) for s in sizes]
offset = 6 + 16 * len(sizes)
head = struct.pack("<HHH", 0, 1, len(sizes))
entries = b""
for s, data in zip(sizes, pngs):
    entries += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(data), offset)
    offset += len(data)
(out / "nokturn.ico").write_bytes(head + entries + b"".join(pngs))
for s in sizes:
    (out / f"icon-{s}.png").unlink()

# Grafika boczna kreatora i małe logo (rozmiar 100% i 200% dla ekranów HiDPI).
for scale in (1, 2):
    render(here / "wizard.svg", out / f"wizard-{scale * 100}.png", 164 * scale, 314 * scale)
    render(icon_svg, out / f"small-{scale * 100}.png", 55 * scale, 55 * scale)
print("Gotowe:", out)
