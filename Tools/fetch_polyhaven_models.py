#!/usr/bin/env python3
"""Download extra CC0 Poly Haven street props (glTF, 1k textures) into SourceAssets/Env/Models/<id>/.

  python3 Tools/fetch_polyhaven_models.py

All Poly Haven assets are CC0 (https://polyhaven.com/license). No login needed. Idempotent.
Imported by Content/Python/import_env_assets.py into /Game/Env/Props/<id>/.
"""
import json, os, sys, urllib.request

# Street props Dum-E's pack (contrib/dume/assets, Tools/prepare_env_assets.py) does not include.
MODELS = [
    "utility_box_01", "utility_box_02", "water_manhole_cover", "barrel_03",
    "wooden_crate_02", "modular_street_seating", "old_tyre", "trashbag",
]
RES = "1k"
ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "SourceAssets", "Env", "Models")
UA = {"User-Agent": "IronManArena-asset-fetch/1.0"}


def get(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=120) as r:
        return r.read()


def main():
    manifest = []
    for mid in MODELS:
        try:
            files = json.loads(get("https://api.polyhaven.com/files/" + mid))
            info = json.loads(get("https://api.polyhaven.com/info/" + mid))
            g = files["gltf"][RES]["gltf"]
            out = os.path.join(ROOT, mid)
            os.makedirs(out, exist_ok=True)
            items = [(os.path.basename(g["url"]), g["url"])] + [(k, v["url"]) for k, v in g.get("include", {}).items()]
            for rel, url in items:
                dst = os.path.join(out, rel)
                if os.path.exists(dst) and os.path.getsize(dst) > 0:
                    continue
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                with open(dst, "wb") as f:
                    f.write(get(url))
            manifest.append({"id": mid, "name": info.get("name", mid), "license": "CC0",
                             "page": "https://polyhaven.com/a/" + mid, "authors": list(info.get("authors", {}).keys()),
                             "dimensions_mm": info.get("dimensions"), "gltf": os.path.basename(g["url"])})
            print("OK  ", mid)
        except Exception as e:  # keep going; the game falls back to primitives per prop
            print("FAIL", mid, e, file=sys.stderr)
    with open(os.path.join(ROOT, "polyhaven_extras.json"), "w") as f:
        json.dump({"license": "CC0 (Poly Haven)", "models": manifest}, f, indent=2)


if __name__ == "__main__":
    main()
