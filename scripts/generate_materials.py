#!/usr/bin/env python3
"""Generate material tables by scraping the Bambu Lab store (upstream scraper).

This wraps queengooborg/Bambu-Lab-RFID-Library's scrape_filaments.py to pull the
latest colors/codes directly from the store instead of parsing the README table.
If a prior materials.json exists, we reuse any known variantId/materialId for a
filamentCode so lookups keep working; new colors will have blank variantId.
"""

import json
import re
import sys
from pathlib import Path

import requests


SCRAPER_URL = "https://raw.githubusercontent.com/queengooborg/Bambu-Lab-RFID-Library/main/scrape_filaments.py"
README_URL = "https://raw.githubusercontent.com/queengooborg/Bambu-Lab-RFID-Library/main/README.md"
ROW_RE = re.compile(r"^\|\s*(.*?)\s*\|\s*([0-9]{5})\s*\|\s*([A-Z0-9\-\/]+)\s*\|")
HEADER_RE = re.compile(r"^####\s+(.*)")


def load_existing_map(path: Path):
    if not path.exists():
        return {}
    try:
        data = json.loads(path.read_text())
        return {e["filamentCode"]: e for e in data if "filamentCode" in e}
    except Exception:
        return {}


def fetch_scraper():
    resp = requests.get(SCRAPER_URL, timeout=30)
    resp.raise_for_status()
    return resp.text


def fetch_readme():
    resp = requests.get(README_URL, timeout=30)
    resp.raise_for_status()
    return resp.text.splitlines()


def parse_readme_variants(lines):
    current_material = ""
    by_code = {}
    for line in lines:
        h = HEADER_RE.match(line)
        if h:
            current_material = h.group(1).strip()
            continue
        m = ROW_RE.match(line)
        if m:
            color, code, variant = m.groups()
            by_code[code] = {"variantId": variant, "material": current_material, "color": color}
    return by_code


def scrape_materials():
    code = fetch_scraper()
    ns = {}
    exec(compile(code, "scrape_filaments.py", "exec"), ns)
    return ns["get_materials"]()


def to_entries(materials, existing_by_code, readme_by_code):
    entries = []
    for _, mats in materials.items():
        for material, colors in mats.items():
            for color, code in colors.items():
                prev = existing_by_code.get(code, {})
                readme = readme_by_code.get(code, {})
                entries.append(
                    {
                        "material": material,
                        "color": color,
                        "filamentCode": code,
                        "variantId": prev.get("variantId") or readme.get("variantId", ""),
                        "materialId": prev.get("materialId", ""),
                    }
                )
    # Sort for stability
    entries.sort(key=lambda e: (e["material"], e["color"]))
    return entries


def write_files(entries, out_dir: Path):
    out_dir.mkdir(parents=True, exist_ok=True)
    json_path = out_dir / "materials.json"
    json_path.write_text(json.dumps(entries, indent=2, ensure_ascii=False))
    print(f"Wrote {len(entries)} entries to {json_path}")

    header_path = out_dir / "materials_snippet.h"
    with header_path.open("w", encoding="utf-8") as f:
        f.write("// Generated from Bambu-Lab-RFID-Library scrape_filaments.py (store scrape).\n")
        f.write("// materialId/variantId reused from prior data when available; otherwise blank.\n")
        for e in entries:
            f.write(
                f'    {{"{e["materialId"]}", "{e["variantId"]}", "{e["filamentCode"]}", "{e["material"]}", "{e["color"]}"}},\n'
            )
    print(f"Wrote header snippet to {header_path}")


def main():
    out_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("generated")
    existing_by_code = load_existing_map(out_dir / "materials.json")
    readme_by_code = parse_readme_variants(fetch_readme())
    materials = scrape_materials()
    entries = to_entries(materials, existing_by_code, readme_by_code)
    write_files(entries, out_dir)


if __name__ == "__main__":
    main()
