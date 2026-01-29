#!/usr/bin/env python3
import json
import re
import sys
from pathlib import Path
from urllib.request import urlopen

RAW_URL = "https://raw.githubusercontent.com/queengooborg/Bambu-Lab-RFID-Library/main/README.md"

# Rough parser for the README tables. Not perfect, but captures most rows of the form:
# | Color | Filament Code | Variant ID | ... |
ROW_RE = re.compile(r"^\|\s*(.*?)\s*\|\s*([0-9]{5})\s*\|\s*([A-Z0-9\-\/]+)\s*\|")
HEADER_RE = re.compile(r"^####\s+(.*)")  # e.g., '#### PLA Basic'


def fetch_readme():
    with urlopen(RAW_URL) as resp:
        return resp.read().decode("utf-8", errors="replace").splitlines()


def parse(lines):
    current_material = ""
    entries = []
    for line in lines:
        h = HEADER_RE.match(line)
        if h:
            current_material = h.group(1).strip()
            continue
        m = ROW_RE.match(line)
        if m:
            color, code, variant = m.groups()
            entries.append(
                {
                    "material": current_material,
                    "color": color,
                    "filamentCode": code,
                    "variantId": variant,
                    "materialId": "",  # not present in README; leave empty
                }
            )
    return entries


def write_files(entries, out_dir: Path):
    out_dir.mkdir(parents=True, exist_ok=True)
    json_path = out_dir / "materials.json"
    with json_path.open("w", encoding="utf-8") as f:
        json.dump(entries, f, indent=2, ensure_ascii=False)
    print(f"Wrote {len(entries)} entries to {json_path}")

    # Also emit a header snippet for Arduino (MaterialInfo table rows)
    header_path = out_dir / "materials_snippet.h"
    with header_path.open("w", encoding="utf-8") as f:
        f.write("// Generated from Bambu-Lab-RFID-Library README. Partial; materialId is empty.\n")
        f.write("// Copy-paste into material_lookup.h MATERIALS array.\n")
        for e in entries:
            f.write(
                f'    {{"{e["materialId"]}", "{e["variantId"]}", "{e["filamentCode"]}", "{e["material"]}", "{e["color"]}"}},\n'
            )
    print(f"Wrote header snippet to {header_path}")


def main():
    out_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("generated")
    lines = fetch_readme()
    entries = parse(lines)
    write_files(entries, out_dir)


if __name__ == "__main__":
    main()
