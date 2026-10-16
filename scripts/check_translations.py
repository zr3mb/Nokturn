#!/usr/bin/env python3
"""Checks translations: every qsTr()/tr() string in qml/ and src/ needs an entry in i18n/pl.json,
and every plural string ("%n ...") also needs its English forms in i18n/en.json.

    python scripts/check_translations.py          # report missing / unused entries
    python scripts/check_translations.py --strict # exit code 1 when something is missing

Also flags string literals that still contain Polish letters outside i18n/ — UI text
belongs in English inside qsTr()/tr() (sources are English, Polish lives in pl.json).
Lines marked with an "i18n-ignore" comment are skipped (e.g. parsing Polish API responses).
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
STR = r'"((?:[^"\\]|\\.)*)"'
CALL = re.compile(r'(?:\b(?:qsTr|tr|QT_TR_NOOP)\s*\(|\btranslate\s*\(\s*"[^"]*"\s*,)\s*' + STR + r'(?:\s*,\s*' + STR + r')?')
POLISH = re.compile(r'"[^"\n]*[ąćęłńóśźżĄĆĘŁŃÓŚŹŻ][^"\n]*"')


def unescape(s):
    return s.replace('\\"', '"').replace("\\n", "\n").replace("\\\\", "\\")


def sources():
    for d in ("qml", "src"):
        for p in sorted((ROOT / d).rglob("*")):
            if p.suffix in (".qml", ".cpp", ".h", ".js"):
                yield p


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    pl = json.loads((ROOT / "i18n" / "pl.json").read_text(encoding="utf-8"))
    en = json.loads((ROOT / "i18n" / "en.json").read_text(encoding="utf-8"))
    used, missing, polish = set(), [], []
    for p in sources():
        text = p.read_text(encoding="utf-8")
        rel = p.relative_to(ROOT).as_posix()
        for m in CALL.finditer(text):
            key = unescape(m.group(1))
            if m.group(2):
                key += "|" + unescape(m.group(2))
            used.add(key)
            line = text.count("\n", 0, m.start()) + 1
            if key not in pl:
                missing.append(f"{rel}:{line}: {key}")
            if "%n" in key and key not in en:
                missing.append(f"{rel}:{line}: (en plural) {key}")
        for i, line in enumerate(text.splitlines(), 1):
            if POLISH.search(line) and "i18n-ignore" not in line:
                polish.append(f"{rel}:{i}: {line.strip()}")
    unused = sorted({k for k in list(pl) + list(en) if not k.startswith("_") and k not in used})

    for title, items in (("Missing translation", missing),
                         ("Polish text in sources", polish),
                         ("Unused entries in i18n/*.json", unused)):
        if items:
            print(f"== {title} ({len(items)})")
            for it in items:
                print("  " + it)
    print(f"{len(used)} translatable strings, {len(pl) - 1} Polish entries")
    if "--strict" in sys.argv and (missing or polish):
        sys.exit(1)


if __name__ == "__main__":
    main()
