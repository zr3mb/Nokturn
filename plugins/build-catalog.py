#!/usr/bin/env python3
"""Packs every plugin folder next to this script into the catalog the app reads.

    python plugins/build-catalog.py [output folder, default dist/plugins]

The output holds <id>-<version>.zip per plugin, <id>.png (its preview, when it has one) and
index.json. Upload the folder's contents to /var/lib/nokturn/plugins on the server; the app reads
https://<server>/plugins/index.json (docs/plugins.md, "Publishing in the catalog").
"""

import hashlib
import json
import os
import re
import shutil
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ID = re.compile(r"^[a-z0-9][a-z0-9._-]{1,63}$")
SKIP = {"__pycache__", ".git", ".DS_Store", "Thumbs.db"}


def pack(folder, target):
    """A zip with the plugin's files at its root, the same bytes for the same files."""
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as z:
        for root, dirs, files in os.walk(folder):
            dirs[:] = sorted(d for d in dirs if d not in SKIP)
            for name in sorted(files):
                if name in SKIP:
                    continue
                path = os.path.join(root, name)
                arc = os.path.relpath(path, folder).replace(os.sep, "/")
                info = zipfile.ZipInfo(arc, date_time=(2020, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o644 << 16
                with open(path, "rb") as f:
                    z.writestr(info, f.read())


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "dist", "plugins")
    out = os.path.abspath(out)
    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(out)
    plugins = []
    for name in sorted(os.listdir(HERE)):
        folder = os.path.join(HERE, name)
        manifest_path = os.path.join(folder, "plugin.json")
        if not os.path.isfile(manifest_path):
            continue
        with open(manifest_path, encoding="utf-8") as f:
            m = json.load(f)
        pid, version = m.get("id", ""), m.get("version", "")
        if not ID.match(pid) or not version:
            sys.exit(f"{name}: plugin.json needs a valid id and a version")
        kinds = [k for k in ("wallpaper", "overlay", "main") if m.get(k)]
        if m.get("themes"):
            kinds.append("theme")
        archive = f"{pid}-{version}.zip"
        pack(folder, os.path.join(out, archive))
        with open(os.path.join(out, archive), "rb") as f:
            data = f.read()
        entry = {
            "id": pid, "name": m.get("name", pid), "version": version, "author": m.get("author", ""),
            "description": m.get("description", ""), "api": m.get("api", 1), "kinds": kinds,
            "file": archive, "size": len(data), "sha256": hashlib.sha256(data).hexdigest(),
        }
        preview = m.get("preview")
        if preview and os.path.isfile(os.path.join(folder, preview)):
            shutil.copyfile(os.path.join(folder, preview), os.path.join(out, f"{pid}.png"))
            entry["preview"] = f"{pid}.png"
        plugins.append(entry)
        print(f"{pid} {version}: {len(data)} bytes")
    with open(os.path.join(out, "index.json"), "w", encoding="utf-8") as f:
        json.dump({"api": 1, "plugins": plugins}, f, ensure_ascii=False, indent=1)
    print(f"{len(plugins)} plugins in {out}")


if __name__ == "__main__":
    main()
