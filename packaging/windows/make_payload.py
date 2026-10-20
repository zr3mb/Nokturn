# Packs dist\ and the icon into the .tar.zst the setup downloads from the GitHub release, and writes
# what the setup is built with (version, sizes, SHA-256) to setup_config.h.
#   python make_payload.py <dist> <nokturn.ico> <version> <out.tar.zst> <setup_config.h> <zstd.exe>
import hashlib, subprocess, sys, tarfile
from pathlib import Path

dist, icon, version, out, config, zstd = sys.argv[1:7]
dist, out = Path(dist), Path(out)
tar_path = out.with_suffix("")   # Nokturn-X-win64.tar
installed = 0

def add(tar, path, name):
    global installed
    info = tar.gettarinfo(str(path), arcname=name)
    info.uid = info.gid = 0
    info.uname = info.gname = ""
    info.mode = 0o644
    with open(path, "rb") as f:
        tar.addfile(info, f)
    installed += info.size

# ustar: the setup reads plain headers only (names up to 255 bytes).
with tarfile.open(tar_path, "w", format=tarfile.USTAR_FORMAT, encoding="utf-8") as tar:
    for path in sorted(p for p in dist.rglob("*") if p.is_file()):
        add(tar, path, path.relative_to(dist).as_posix())
    add(tar, Path(icon), "nokturn.ico")

subprocess.run([zstd, "-19", "--long=27", "-T0", "-q", "-f", str(tar_path), "-o", str(out)], check=True)
tar_path.unlink()

sha = hashlib.sha256()
with open(out, "rb") as f:
    for chunk in iter(lambda: f.read(1 << 20), b""):
        sha.update(chunk)
parts = (version.split(".") + ["0", "0", "0"])[:3]
Path(config).parent.mkdir(parents=True, exist_ok=True)
Path(config).write_text(
    f'#define SETUP_VERSION "{version}"\n'
    f"#define SETUP_VERSION_COMMA {parts[0]},{parts[1]},{parts[2]},0\n"
    f'#define SETUP_PAYLOAD_SHA256 "{sha.hexdigest()}"\n'
    f"#define SETUP_PAYLOAD_SIZE {out.stat().st_size}ULL\n"
    f"#define SETUP_INSTALLED_SIZE {installed}ULL\n")
print(f"{out.name}: {out.stat().st_size / 2**20:.1f} MB (unpacked {installed / 2**20:.1f} MB)")
