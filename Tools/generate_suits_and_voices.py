#!/usr/bin/env python3
"""
Generate the C++ suit table, the AI voice-line table, the voice WAVs and SourceAudio/Audio/lines.csv
from the shared JSON data.

Inputs (defaults point at the team's shared data; override with --data):
    <data>/suits.json                       list of suits
    <data>/voices/{jarvis,friday,edith,ultron,warmachine}.json
        { "persona": "JARVIS", "events": {event: [lines]}, "handover": {OUTGOING_PERSONA: [lines]} }

Outputs (relative to the project root):
    Source/IronManArena/Private/Generated/SuitTable.inl      (included by SuitTypes.cpp)
    Source/IronManArena/Private/Generated/AIVoiceLines.inl   (included by JarvisComponent.cpp)
    SourceAudio/Audio/<PERSONA>/<LineId>.wav                 (unless --no-audio)
    SourceAudio/Audio/lines.csv                                  id,persona,event,context,file,text

Audio uses piper (offline neural TTS). Voices (download from https://huggingface.co/rhasspy/piper-voices):
    JARVIS      en_GB-alan-medium
    FRIDAY      en_GB-alba-medium                (closest offline Celtic-accented female voice)
    EDITH       en_US-amy-medium
    ULTRON      en_US-ryan-medium, pitched down ~18% + slight echo (numpy; sox not required)
    WARMACHINE  en_US-joe-medium
If piper is missing, espeak-ng is used as a fallback.

Usage:
    python3 Tools/generate_suits_and_voices.py [--data DIR] [--voices-dir DIR] [--no-audio] [--force]
Then import the WAVs into the editor with Content/Python/import_jarvis_audio.py.
"""
import argparse, csv, io, json, os, re, shutil, subprocess, sys, unicodedata, wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GEN_DIR = os.path.join(ROOT, "Source", "IronManArena", "Private", "Generated")
AUDIO_DIR = os.path.join(ROOT, "SourceAudio", "Audio")  # raw WAVs stay out of Content/; imported .uasset go to Content/Audio

PERSONAS = {  # json name -> (C++ enum, folder, piper voice, espeak voice)
    "JARVIS": ("Jarvis", "JARVIS", "en_GB-alan-medium", "en-gb"),
    "FRIDAY": ("Friday", "FRIDAY", "en_GB-alba-medium", "en-gb-scotland+f3"),
    "EDITH": ("Edith", "EDITH", "en_US-amy-medium", "en-us+f2"),
    "ULTRON": ("Ultron", "ULTRON", "en_US-ryan-medium", "en-us+m3"),
    "WARMACHINE": ("WarMachine", "WARMACHINE", "en_US-joe-medium", "en-us+m1"),
}
PERSONA_ALIASES = {"WAR MACHINE": "WARMACHINE", "WAR_MACHINE": "WARMACHINE", "WAR MACHINE OS": "WARMACHINE"}
LENGTH_SCALE = {"JARVIS": 1.05, "FRIDAY": 0.95, "EDITH": 0.92, "ULTRON": 0.9, "WARMACHINE": 0.9}

EVENTS = ["start", "suitUp", "suitSwapIn", "lowEnergy", "lowHealth", "toFlight", "toGround",
          "hulkbusterReady", "hulkbusterDeploy", "waveStart", "droneSwarm", "tankSpotted",
          "bossIncoming", "bossPhase2", "bossPhase3", "bossDefeated", "combo",
          "pickupHealth", "pickupEnergy", "pickupHulkbuster", "death", "highScore", "idle"]

ENUMS = {
    "primary": ("ESuitPrimaryWeapon", {"repulsor": "Repulsor", "laser": "Laser", "sonic": "Sonic",
                "flamethrower": "Flamethrower", "frost": "Frost", "minigun": "Minigun", "redblast": "RedBlast"}, "Repulsor"),
    "special": ("ESuitSpecialWeapon", {"unibeam": "Unibeam", "missiles": "Missiles", "overdrive": "Overdrive",
                "shield": "Shield", "droneswarm": "DroneSwarm", "nanoblades": "NanoBlades", "cloak": "Cloak",
                "laserwhip": "LaserWhip"}, "Unibeam"),
    "cinematic": ("ECinematicStyle", {"assembly": "Assembly", "gantry": "Gantry", "drop": "Drop", "nano": "Nano",
                  "stealth": "Stealth", "suitcase": "Suitcase", "ultron": "Ultron", "glitch": "Ultron"}, "Drop"),
    "helmet": ("ESuitHelmet", {"classic": "Classic", "sleek": "Sleek", "angular": "Angular", "bulky": "Bulky",
               "crude": "Crude"}, "Classic"),
    "reactor": ("ESuitReactor", {"round": "Round", "triangle": "Triangle", "none": "None", "mounted": "Mounted", "makeshift": "Mounted", "backpack": "Mounted", "spine": "Mounted", "forearm": "Mounted"}, "Round"),
    "shoulders": ("ESuitShoulders", {"none": "None", "pads": "Pads", "cannon": "Cannon", "spiked": "Spiked",
                  "spikes": "Spiked"}, "None"),
    "backpack": ("ESuitBackpack", {"none": "None", "jets": "Jets", "missiles": "Missiles", "minigun": "Minigun"}, "None"),
}

warnings = []


def ascii_text(s):
    """C++ TEXT() literals are kept pure ASCII so any compiler/charset setting is safe."""
    s = str(s)
    for a, b in {"\u2019": "'", "\u2018": "'", "\u201c": '"', "\u201d": '"', "\u2014": " - ", "\u2013": "-",
                 "\u2026": "...", "\u00a0": " "}.items():
        s = s.replace(a, b)
    s = unicodedata.normalize("NFKD", s).encode("ascii", "ignore").decode("ascii")
    return re.sub(r"\s+", " ", s).strip()


def cpp_str(s):
    return '"' + ascii_text(s).replace("\\", "\\\\").replace('"', '\\"') + '"'


def asset_name(s):
    return re.sub(r"[^A-Za-z0-9_]", "_", s)


def persona_key(p):
    p = str(p or "JARVIS").strip().upper()
    p = PERSONA_ALIASES.get(p, p)
    if p not in PERSONAS:
        warnings.append("unknown persona %r -> JARVIS" % p)
        p = "JARVIS"
    return p


def hex_to_linear(h, fallback=(1.0, 1.0, 1.0)):
    try:
        h = str(h).lstrip("#")
        if len(h) == 3:
            h = "".join(c * 2 for c in h)
        rgb = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    except Exception:
        warnings.append("bad colour %r" % h)
        return fallback
    lin = lambda c: c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4  # sRGB -> linear, like FLinearColor(FColor)
    return tuple(round(lin(c), 4) for c in rgb)


def enum_val(kind, raw):
    enum, table, default = ENUMS[kind]
    key = str(raw).strip().lower().replace("-", "").replace("_", "").replace(" ", "") if raw is not None else ""
    if key not in table:
        if raw not in (None, ""):
            warnings.append("unknown %s %r -> %s" % (kind, raw, default))
        return "%s::%s" % (enum, default)
    return "%s::%s" % (enum, table[key])


def rating(stats, key):
    try:
        return max(1, min(10, int(round(float(stats.get(key, 5))))))
    except Exception:
        return 5


def flt(v):
    return ("%.3f" % v).rstrip("0").rstrip(".") + "f" if "." in ("%.3f" % v) else "%sf" % v


def load_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def build_suits(suits):
    out = ["// GENERATED by Tools/generate_suits_and_voices.py from suits.json - do not edit by hand.",
           "// %d suits. Included inside BuildSuitTable() in SuitTypes.cpp." % len(suits)]
    intros = []
    seen = set()
    for s in suits:
        sid = str(s.get("id") or asset_name(s.get("name", "suit")).lower())
        if sid in seen:
            warnings.append("duplicate suit id %s skipped" % sid)
            continue
        seen.add(sid)
        name = s.get("name") or sid
        mark = s.get("mark") or name
        cols = s.get("colors", {})
        parts = s.get("parts", {})
        st = s.get("stats", {})
        p = persona_key(s.get("ai"))
        sp, ar, en, rg, dm = (rating(st, k) for k in ("speed", "armor", "energy", "regen", "damage"))
        intro = s.get("intro") or ("%s online." % name)
        try:
            plating = max(0, min(2, int(parts.get("plating", 1))))
        except Exception:
            plating = 1
        col = lambda k, fb: "FLinearColor(%sf, %sf, %sf)" % hex_to_linear(cols.get(k, fb))
        L = ["{", "\tFSuitDef S;",
             "\tS.Id = FName(TEXT(%s));" % cpp_str(sid),
             "\tS.Name = INVTEXT(%s);" % cpp_str(name),
             "\tS.Mark = INVTEXT(%s);" % cpp_str(mark),
             "\tS.Description = INVTEXT(%s);" % cpp_str(intro),
             "\tS.PrimaryColor = %s;" % col("primary", "#b01010"),
             "\tS.SecondaryColor = %s;" % col("secondary", "#d4a017"),
             "\tS.AccentColor = %s;" % col("accent", cols.get("secondary", "#d4a017")),
             "\tS.EmissiveColor = %s;" % col("emissive", "#8fe8ff"),
             "\tS.Helmet = %s;" % enum_val("helmet", parts.get("helmet")),
             "\tS.Reactor = %s;" % enum_val("reactor", parts.get("reactor")),
             "\tS.Shoulders = %s;" % enum_val("shoulders", parts.get("shoulders")),
             "\tS.Backpack = %s;" % enum_val("backpack", parts.get("backpack")),
             "\tS.Plating = %d;" % plating,
             "\tS.bWings = %s;" % ("true" if parts.get("wings") else "false"),
             "\tS.SpeedRating = %d; S.ArmorRating = %d; S.EnergyRating = %d; S.RegenRating = %d; S.DamageRating = %d;" % (sp, ar, en, rg, dm),
             # 5 = baseline (x1.0 / 100 energy / 22 regen).
             "\tS.Speed = %.2ff;" % (0.70 + 0.06 * sp),
             "\tS.Armor = %.2ff;" % (0.60 + 0.08 * ar),
             "\tS.MaxEnergy = %.1ff;" % (60.0 + 8.0 * en),
             "\tS.Regen = %.1ff;" % (10.0 + 2.4 * rg),
             "\tS.WeaponDamage = %.2ff;" % (0.60 + 0.08 * dm),
             "\tS.PrimaryWeapon = %s;" % enum_val("primary", s.get("primary")),
             "\tS.SpecialWeapon = %s;" % enum_val("special", s.get("special")),
             "\tS.AI = EAIPersona::%s;" % PERSONAS[p][0],
             "\tS.JarvisIntroLine = INVTEXT(%s);" % cpp_str(intro),
             "\tS.CinematicStyle = %s;" % enum_val("cinematic", s.get("cinematic")),
             "\tOut.Add(S);", "}"]
        out.append("\n".join("\t\t" + l for l in L))
        intros.append(dict(id="%s_intro_%s" % (p, asset_name(sid)), persona=p, event="SuitIntro", context=sid, text=intro))
    return "\n".join(out) + "\n", intros


def build_lines(voices, intros):
    lines = []
    for p in PERSONAS:
        v = voices.get(p)
        if not v:
            warnings.append("no voice file for %s" % p)
            continue
        ev = v.get("events", {})
        for e in EVENTS:
            items = ev.get(e) or []
            if not items:
                warnings.append("%s: no lines for %s" % (p, e))
            for i, t in enumerate(items, 1):
                lines.append(dict(id="%s_%s_%d" % (p, e, i), persona=p, event=e[0].upper() + e[1:], context="", text=t))
        for e in ev:
            if e not in EVENTS:
                warnings.append("%s: unknown event %r ignored" % (p, e))
        ho = v.get("handover") or {}
        if isinstance(ho, list):  # tolerate an older flat list
            ho = {"": ho}
        for outgoing, items in ho.items():
            ok = persona_key(outgoing) if outgoing else ""
            ctx = PERSONAS[ok][1] if ok else ""
            for i, t in enumerate(items, 1):
                lines.append(dict(id="%s_handover_%s_%d" % (p, ctx or "ANY", i), persona=p, event="Handover", context=ctx, text=t))
    lines += intros
    out = ["// GENERATED by Tools/generate_suits_and_voices.py from voices/*.json and suits.json - do not edit by hand.",
           "// AI_LINE(Persona, Event, Id, Context, Text). %d lines." % len(lines)]
    for l in lines:
        out.append("AI_LINE(%s, %s, %s, %s, %s)" % (PERSONAS[l["persona"]][0], l["event"], cpp_str(l["id"]),
                                                    cpp_str(l["context"]), cpp_str(l["text"])))
    return "\n".join(out) + "\n", lines


# ---------------------------------------------------------------- audio
def ultron_fx(frames, rate):
    """Pitch down ~18% (resample, keeps the original rate) and add a short metallic echo."""
    import numpy as np
    x = np.frombuffer(frames, dtype=np.int16).astype(np.float32)
    factor = 0.82
    n = int(len(x) / factor)
    y = np.interp(np.arange(n) * factor, np.arange(len(x)), x)
    for delay_s, gain in ((0.045, 0.35), (0.11, 0.18)):
        d = int(rate * delay_s)
        y[d:] += gain * y[:-d].copy()
    y *= 30000.0 / max(1.0, float(np.max(np.abs(y))))
    return y.astype(np.int16).tobytes()


class Synth:
    def __init__(self, voices_dir):
        self.voices_dir = voices_dir
        self.cache = {}
        try:
            from piper import PiperVoice, SynthesisConfig  # noqa
            self.piper = True
        except Exception:
            self.piper = False
        self.espeak = shutil.which("espeak-ng") or shutil.which("espeak")
        if not self.piper and not self.espeak:
            raise RuntimeError("Neither piper (pip install piper-tts) nor espeak-ng is available")

    def say(self, persona, text, path):
        model = os.path.join(self.voices_dir, PERSONAS[persona][2] + ".onnx")
        if self.piper and os.path.exists(model):
            from piper import PiperVoice, SynthesisConfig
            if model not in self.cache:
                self.cache[model] = PiperVoice.load(model)
            buf = io.BytesIO()
            with wave.open(buf, "wb") as w:
                self.cache[model].synthesize_wav(text, w, syn_config=SynthesisConfig(length_scale=LENGTH_SCALE[persona]))
            buf.seek(0)
            with wave.open(buf, "rb") as r:
                rate, width, ch, frames = r.getframerate(), r.getsampwidth(), r.getnchannels(), r.readframes(r.getnframes())
        elif self.espeak:
            warnings.append("espeak fallback for %s" % persona)
            pitch = "20" if persona == "ULTRON" else "50"
            data = subprocess.run([self.espeak, "-v", PERSONAS[persona][3], "-s", "165", "-p", pitch, "--stdout", text],
                                  check=True, capture_output=True).stdout
            with wave.open(io.BytesIO(data), "rb") as r:
                rate, width, ch, frames = r.getframerate(), r.getsampwidth(), r.getnchannels(), r.readframes(r.getnframes())
        else:
            raise RuntimeError("voice model missing: " + model)
        if persona == "ULTRON" and width == 2 and ch == 1:
            frames = ultron_fx(frames, rate)
        # 120 ms of leading silence avoids clipped first syllables in some players.
        frames = b"\x00" * int(rate * 0.12) * width * ch + frames
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with wave.open(path, "wb") as w:
            w.setnchannels(ch); w.setsampwidth(width); w.setframerate(rate); w.writeframes(frames)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", default=os.environ.get("IRONMAN_DATA", "/workspace/ironman-3d/contrib"))
    ap.add_argument("--voices-dir", default=os.environ.get("PIPER_VOICES", "/workspace/tts-voices"))
    ap.add_argument("--no-audio", action="store_true")
    ap.add_argument("--force", action="store_true", help="re-synthesise WAVs that already exist")
    a = ap.parse_args()

    suits = load_json(os.path.join(a.data, "suits.json"))
    if isinstance(suits, dict):
        suits = suits.get("suits", [])
    voices = {}
    for f in ("jarvis", "friday", "edith", "ultron", "warmachine"):
        p = os.path.join(a.data, "voices", f + ".json")
        if os.path.exists(p):
            d = load_json(p)
            voices[persona_key(d.get("persona", f))] = d

    os.makedirs(GEN_DIR, exist_ok=True)
    suit_inl, intros = build_suits(suits)
    line_inl, lines = build_lines(voices, intros)
    open(os.path.join(GEN_DIR, "SuitTable.inl"), "w").write(suit_inl)
    open(os.path.join(GEN_DIR, "AIVoiceLines.inl"), "w").write(line_inl)
    print("suits: %d, lines: %d" % (suit_inl.count("Out.Add(S)"), len(lines)))

    os.makedirs(AUDIO_DIR, exist_ok=True)
    with open(os.path.join(AUDIO_DIR, "lines.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["id", "persona", "event", "context", "file", "text"])
        for l in lines:
            w.writerow([l["id"], l["persona"], l["event"], l["context"],
                        "%s/%s.wav" % (PERSONAS[l["persona"]][1], l["id"]), ascii_text(l["text"])])

    if not a.no_audio:
        synth = Synth(a.voices_dir)
        done = 0
        for l in lines:
            path = os.path.join(AUDIO_DIR, PERSONAS[l["persona"]][1], l["id"] + ".wav")
            if a.force or not os.path.exists(path):
                synth.say(l["persona"], ascii_text(l["text"]), path)
            done += 1
            if done % 50 == 0:
                print("  audio %d / %d" % (done, len(lines)), flush=True)
        print("audio: %d WAVs in %s" % (done, AUDIO_DIR))
    for wmsg in sorted(set(warnings)):
        print("WARNING:", wmsg)


if __name__ == "__main__":
    main()
