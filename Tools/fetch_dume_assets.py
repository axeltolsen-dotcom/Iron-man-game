"""
Fetches Dume's 19 CC0 2K assets (Tools/data/dume_assets.json, from contrib/dume/assets.json):
Poly Haven texture sets + HDRIs and ambientCG texture zips + HDRI, all CC0 1.0, no login.

Output (same layout as Tools/fetch_cc0_assets.py, so Content/Python/import_cc0_textures.py imports it):
  SourceAssets/PolyHaven/<id>/<id>_{diff,nor_dx,arm}_2k.jpg      Poly Haven sets (API md5-checked)
  SourceAssets/PolyHaven/hdri/<id>_2k.hdr                         Poly Haven HDRIs
  SourceAssets/AmbientCG/<id>/<id>_{diff,nor_dx,arm}_2k.jpg       ambientCG sets; ARM packed here
                                                                  (R = AO or white, G = Roughness, B = Metalness or black)
  SourceAssets/AmbientCG/hdri/<id>_2k.hdr|exr                     ambientCG HDRI

    python3 Tools/fetch_dume_assets.py [--list]
Sanity checks: every map is 2048 px on its long side (or is resized to it), ARM is 3-channel,
normal maps are blue-dominant. Results -> Saved/Realism/dume_fetch_report.txt.
"""
import io, json, os, sys, zipfile, hashlib, urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "Tools", "data", "dume_assets.json")
PH = os.path.join(ROOT, "SourceAssets", "PolyHaven")
ACG = os.path.join(ROOT, "SourceAssets", "AmbientCG")
REPORT = os.path.join(ROOT, "Saved", "Realism", "dume_fetch_report.txt")
UA = {"User-Agent": "IronManArena-asset-fetch/1.0 (CC0 texture import script)"}
REPORT_LINES = []


def log(s):
    print(s)
    REPORT_LINES.append(s)


def fetch(url, timeout=300):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=timeout) as r:
        return r.read()


def md5b(b):
    return hashlib.md5(b).hexdigest()


def is_hdri(a):
    return "HDR" in a.get("format", "").upper() or "HDRI" in a["assetId"].upper() or a["url"].lower().endswith((".hdr", ".exr"))


def check_image(path, kind):
    from PIL import Image
    im = Image.open(path)
    w, h = im.size
    note = ""
    if max(w, h) > 2048:
        s = 2048.0 / max(w, h)
        im = im.convert("RGB").resize((int(w * s), int(h * s)), Image.LANCZOS)
        im.save(path, quality=92)
        note = " (resized from %dx%d)" % (w, h)
        w, h = im.size
    if kind == "nor_dx":
        r, g, b = [sum(c) / len(c) for c in zip(*list(im.convert("RGB").resize((32, 32)).getdata()))]
        if not (b > r and b > g):
            log("  WARN %s: normal map not blue-dominant (%.0f %.0f %.0f)" % (os.path.basename(path), r, g, b))
    log("  ok   %-48s %dx%d%s" % (os.path.relpath(path, ROOT), w, h, note))


def polyhaven_set(a):
    aid = a["assetId"]
    files = json.loads(fetch("https://api.polyhaven.com/files/" + aid, 60))
    for key, short in (("Diffuse", "diff"), ("nor_dx", "nor_dx"), ("arm", "arm")):
        f = files.get(key, {}).get("2k", {})
        f = f.get("jpg") or f.get("png")
        if not f:
            log("  MISSING %s %s" % (aid, key))
            continue
        path = os.path.join(PH, aid, "%s_%s_2k%s" % (aid, short, os.path.splitext(f["url"])[1]))
        if not (os.path.exists(path) and md5b(open(path, "rb").read()) == f["md5"]):
            os.makedirs(os.path.dirname(path), exist_ok=True)
            b = fetch(f["url"])
            if md5b(b) != f["md5"]:
                raise SystemExit("md5 mismatch " + f["url"])
            open(path, "wb").write(b)
        check_image(path, short)


def polyhaven_hdri(a):
    aid = a["assetId"]
    files = json.loads(fetch("https://api.polyhaven.com/files/" + aid, 60))
    f = files.get("hdri", {}).get("2k", {}).get("hdr")
    path = os.path.join(PH, "hdri", "%s_2k.hdr" % aid)
    if not (os.path.exists(path) and md5b(open(path, "rb").read()) == f["md5"]):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        b = fetch(f["url"])
        if md5b(b) != f["md5"]:
            raise SystemExit("md5 mismatch " + f["url"])
        open(path, "wb").write(b)
    log("  ok   %-48s %.1f MB" % (os.path.relpath(path, ROOT), os.path.getsize(path) / 1e6))


def acg(a):
    from PIL import Image
    aid = a["assetId"]
    out = os.path.join(ACG, "hdri" if is_hdri(a) else aid)
    if is_hdri(a):
        done = [f for f in os.listdir(out)] if os.path.isdir(out) else []
        if any(f.startswith(aid) for f in done):
            log("  ok   (cached) %s" % aid)
            return
    elif os.path.exists(os.path.join(out, "%s_arm_2k.jpg" % aid)):
        for s in ("diff", "nor_dx", "arm"):
            check_image(os.path.join(out, "%s_%s_2k.jpg" % (aid, s)), s)
        return
    z = zipfile.ZipFile(io.BytesIO(fetch(a["url"])))
    names = z.namelist()
    os.makedirs(out, exist_ok=True)
    if is_hdri(a):
        pick = [n for n in names if n.lower().endswith(".hdr")] or [n for n in names if n.lower().endswith(".exr")]
        if not pick:
            log("  MISSING hdr/exr in %s: %s" % (aid, names))
            return
        ext = os.path.splitext(pick[0])[1].lower()
        path = os.path.join(out, "%s_2k%s" % (aid, ext))
        open(path, "wb").write(z.read(pick[0]))
        log("  ok   %-48s %.1f MB" % (os.path.relpath(path, ROOT), os.path.getsize(path) / 1e6))
        return

    def find(tag):
        for n in names:
            if n.lower().endswith((".jpg", ".png")) and ("_" + tag.lower() + ".") in n.lower():
                return Image.open(io.BytesIO(z.read(n)))
        return None
    color, nrm, rough = find("Color"), find("NormalDX"), find("Roughness")
    ao, metal = find("AmbientOcclusion"), find("Metalness")
    if color is None or nrm is None or rough is None:
        log("  MISSING maps in %s: %s" % (aid, names))
        return
    size = color.size
    color.convert("RGB").save(os.path.join(out, "%s_diff_2k.jpg" % aid), quality=92)
    nrm.convert("RGB").resize(size).save(os.path.join(out, "%s_nor_dx_2k.jpg" % aid), quality=95)
    r = ao.convert("L").resize(size) if ao is not None else Image.new("L", size, 255)
    g = rough.convert("L").resize(size)
    b = metal.convert("L").resize(size) if metal is not None else Image.new("L", size, 0)
    Image.merge("RGB", (r, g, b)).save(os.path.join(out, "%s_arm_2k.jpg" % aid), quality=95)
    for s in ("diff", "nor_dx", "arm"):
        check_image(os.path.join(out, "%s_%s_2k.jpg" % (aid, s)), s)


def main():
    assets = json.load(open(DATA))["assets"]
    bad = [a["assetId"] for a in assets if a.get("license") != "CC0"]
    if bad:
        raise SystemExit("non-CC0 assets in the list, refusing: %s" % bad)
    for a in assets:
        log("%s  [%s, %s]  %s" % (a["assetId"], a["source"], a["license"], a.get("use", "")))
        if "--list" in sys.argv:
            continue
        try:
            if a["source"] == "Poly Haven":
                (polyhaven_hdri if is_hdri(a) else polyhaven_set)(a)
            elif a["source"] == "ambientCG":
                acg(a)
            else:
                log("  SKIP unknown source " + a["source"])
        except Exception as e:  # keep going; report it
            log("  FAIL %s: %s" % (a["assetId"], e))
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    open(REPORT, "w").write("\n".join(REPORT_LINES) + "\n")


if __name__ == "__main__":
    main()
