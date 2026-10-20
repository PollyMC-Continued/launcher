#!/usr/bin/env python3
"""Build index_v2.json for PollyMC-Continued translations.

Usage: make_translation_index.py <dir-with-mm-*.qm-and-ts>
Reads the .ts sources next to the .qm files, counts translated/fuzzy/
untranslated messages, computes sha1/size of the .qm files, and writes
index_v2.json in the same directory.
"""
import hashlib
import json
import os
import sys
import xml.etree.ElementTree as ET

# Message states that are not part of the live source and must not be counted.
IGNORED_TYPES = {"vanished", "obsolete"}


def parse_ts(path):
    root = ET.parse(path).getroot()
    translated = untranslated = fuzzy = 0
    for context in root.findall("context"):
        for msg in context.findall("message"):
            transl = msg.find("translation")
            if transl is None:
                continue
            ttype = transl.get("type")
            if ttype in IGNORED_TYPES:
                continue
            # Plural messages carry one <numerusform> per plural form; the
            # message is only finished if every form has text.
            forms = transl.findall("numerusform")
            if forms:
                texts = ["".join(f.itertext()) for f in forms]
            else:
                texts = ["".join(transl.itertext())]
            if ttype == "unfinished" or any(not t.strip() for t in texts):
                untranslated += 1
            elif ttype == "fuzzy":
                fuzzy += 1
            else:
                translated += 1
    return translated, untranslated, fuzzy


def main():
    if len(sys.argv) != 2 or not os.path.isdir(sys.argv[1]):
        print(f"usage: {os.path.basename(sys.argv[0])} <dir-with-mmc_*.qm-and-*.ts>", file=sys.stderr)
        return 2
    d = sys.argv[1]
    languages = {}
    for f in sorted(os.listdir(d)):
        if not (f.startswith("mmc_") and f.endswith(".qm")):
            continue
        key = f[4:-3]
        ts = os.path.join(d, f"{key}.ts")
        if not os.path.exists(ts):
            # index should only contain languages we have source stats for
            print(f"warning: no {key}.ts next to {f}; skipping", file=sys.stderr)
            continue
        qm_path = os.path.join(d, f)
        with open(qm_path, "rb") as fh:
            sha1 = hashlib.sha1(fh.read()).hexdigest()
        size = os.path.getsize(qm_path)
        translated, untranslated, fuzzy = parse_ts(ts)
        languages[key] = {
            "file": f,
            "sha1": sha1,
            "size": size,
            "translated": translated,
            "untranslated": untranslated,
            "fuzzy": fuzzy,
        }
    if not languages:
        # An empty index would silently tell every launcher there are no
        # translations; fail the deploy instead.
        print("error: no languages found; refusing to write an empty index", file=sys.stderr)
        return 1
    index = {
        "file_type": "MMC-TRANSLATION-INDEX",
        "version": 2,
        "languages": languages,
    }
    out = os.path.join(d, "index_v2.json")
    with open(out, "w", encoding="utf-8") as fh:
        json.dump(index, fh, ensure_ascii=False, sort_keys=True)
    print(f"wrote {out} with {len(languages)} languages")
    return 0


if __name__ == "__main__":
    sys.exit(main())
