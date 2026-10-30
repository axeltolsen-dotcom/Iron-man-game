#!/usr/bin/env python3
"""Download the helmet HUD fonts (all SIL OFL 1.1, from the google/fonts GitHub repo) into
SourceAssets/Fonts/<family>/ together with each family's OFL.txt. Content/Python/import_ui_fonts.py
then imports them as Font Face + Font assets under /Game/UI/Fonts (F_Rajdhani, F_ShareTechMono, ...).
"""
import os, sys, urllib.request

BASE = "https://raw.githubusercontent.com/google/fonts/main/ofl/"
FONTS = {
    "Rajdhani": ("rajdhani", ["Rajdhani-Regular.ttf", "Rajdhani-SemiBold.ttf", "Rajdhani-Bold.ttf"]),
    "ShareTechMono": ("sharetechmono", ["ShareTechMono-Regular.ttf"]),
    "Exo2": ("exo2", ["Exo2[wght].ttf"]),
    "Orbitron": ("orbitron", ["Orbitron[wght].ttf"]),
    "SairaStencilOne": ("sairastencilone", ["SairaStencilOne-Regular.ttf"]),
}
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(HERE, "SourceAssets", "Fonts")


def get(url, dst):
    if os.path.exists(dst) and os.path.getsize(dst) > 0:
        return "cached"
    req = urllib.request.Request(url.replace("[", "%5B").replace("]", "%5D"), headers={"User-Agent": "IronManArena-asset-fetch/1.0"})
    with urllib.request.urlopen(req, timeout=60) as r:
        data = r.read()
    with open(dst, "wb") as f:
        f.write(data)
    return "%d bytes" % len(data)


def main():
    ok = True
    for fam, (folder, files) in FONTS.items():
        d = os.path.join(OUT, fam)
        os.makedirs(d, exist_ok=True)
        for fn in files + ["OFL.txt"]:
            try:
                print(fam, fn, get(BASE + folder + "/" + fn, os.path.join(d, fn.replace("[wght]", "-Variable"))))
            except Exception as e:  # noqa
                ok = False
                print("FAILED", fam, fn, e)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
