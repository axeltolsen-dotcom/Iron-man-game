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

Audio uses Kokoro-82M (Apache-2.0, stock voices only - no cloning) via kokoro-onnx on CPU, light ffmpeg
"in-helmet" FX, and loudness normalisation to -20 LUFS / -3 dBTP. Voice blends (see VOICE_PROFILES):
    JARVIS      bm_george+bm_lewis, slower           FRIDAY  bf_emma+bf_isabella, brisker
    EDITH       af_heart+af_kore, crisp              ULTRON  am_onyx+bm_fable, pitched down, chorus/metallic
    WARMACHINE  am_michael+am_fenrir, clipped, compressed
Run with the venv that has kokoro-onnx:  /workspace/kokoro-venv/bin/python Tools/generate_suits_and_voices.py

Usage:
    python3 Tools/generate_suits_and_voices.py [--data DIR] [--voices-dir DIR] [--no-audio] [--force] [--only PERSONA] [--no-codegen]
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
# Natural neural TTS: Kokoro-82M (Apache-2.0) through kokoro-onnx (MIT) on CPU, stock voices only
# (no cloning, no reference clips). Light ffmpeg "in-helmet" FX, then every line is loudness-
# normalised to TARGET_LUFS with true peak <= TARGET_TP_DB. This module is also imported by
# Tools/generate_story_audio.py so both scripts share one engine.
#
#   python deps (venv):  pip install kokoro-onnx soundfile pyloudnorm scipy numpy
#   model files:         <voices-dir>/kokoro/kokoro-v1.0.onnx + voices-v1.0.bin
#                        (https://github.com/thewh1teagle/kokoro-onnx/releases/tag/model-files-v1.0)
#   system:              ffmpeg (with rubberband + chorus filters)
TARGET_LUFS = -20.0
TARGET_TP_DB = -3.0
LEAD_SILENCE = 0.12   # avoids clipped first syllables in some players
TAIL_SILENCE = 0.15   # lets the short reverb ring out

# voice: a Kokoro stock voice or a blend "a:0.7,b:0.3"; speed >1 = faster; fx: chain name below.
VOICE_PROFILES = {
    # suit AIs
    "JARVIS":     dict(voice="bm_george:0.75,bm_lewis:0.25", lang="en-gb", speed=0.92, fx="suit"),
    "FRIDAY":     dict(voice="bf_emma:0.8,bf_isabella:0.2", lang="en-gb", speed=1.06, fx="suit"),
    "EDITH":      dict(voice="af_heart:0.6,af_kore:0.4", lang="en-us", speed=1.04, fx="suit_crisp"),
    "ULTRON":     dict(voice="am_onyx:0.65,bm_fable:0.35", lang="en-us", speed=0.9, fx="ultron"),
    "WARMACHINE": dict(voice="am_michael:0.7,am_fenrir:0.3", lang="en-us", speed=1.1, fx="suit_military"),
    # story speakers
    "TONY":       dict(voice="am_fenrir:0.6,am_eric:0.4", lang="en-us", speed=1.03, fx="clean"),
    "PEPPER":     dict(voice="af_bella", lang="en-us", speed=1.0, fx="clean"),
    "MORGAN":     dict(voice="af_sky:0.7,af_nova:0.3", lang="en-us", speed=1.06, fx="radio", pitch=1.04),
    "PLAYER":     dict(voice="am_puck:0.75,am_echo:0.25", lang="en-us", speed=1.04, fx="clean", pitch=1.04),  # Harley, teen
    "VISON":      dict(voice="bm_daniel:0.6,bm_lewis:0.4", lang="en-gb", speed=0.88, fx="vision"),
    "OBSIDIAN_TITAN": dict(voice="am_onyx", lang="en-us", speed=0.85, fx="titan"),
    # Jocasta (contrib/jocasta/jocasta_voice.md): original stock-voice blend, mid-low, ~12% slower than
    # Morgan (1.06 -> 0.93), slight pitch-down for the register, signature sine under the first word.
    "JOCASTA":    dict(voice="af_nicole:0.55,af_sarah:0.45", lang="en-us", speed=0.93, fx="jocasta", pitch=0.97,
                       signature=True),
}
DEFAULT_PROFILE = "PLAYER"

_COMMON_TAIL = "highpass=f=70,deesser=i=0.35,treble=g=-2.5:f=7000:t=s"  # soften sibilance / harsh highs
FX_CHAINS = {
    "clean": _COMMON_TAIL,
    # very light in-helmet AI: gentle low cut, small presence lift, tiny early reflections, faint chorus
    "suit": "highpass=f=110,equalizer=f=2800:t=o:w=1.4:g=1.2,"
            "aecho=0.92:0.9:13|27:0.07|0.045,chorus=0.97:0.97:24:0.06:0.25:1.2," + _COMMON_TAIL,
    "suit_crisp": "highpass=f=130,equalizer=f=3600:t=o:w=1.2:g=1.5,"
                  "aecho=0.93:0.9:11|23:0.06|0.04,chorus=0.97:0.97:20:0.05:0.3:1.0," + _COMMON_TAIL,
    "suit_military": "highpass=f=140,equalizer=f=1700:t=o:w=1.5:g=2,acompressor=threshold=0.12:ratio=3:attack=5:release=80,"
                     "aecho=0.92:0.9:9|19:0.07|0.05,chorus=0.97:0.97:22:0.05:0.3:1.0," + _COMMON_TAIL,
    # Ultron: deeper (pitch -1.3 semitone, formants kept human), metallic short comb + slap, wider chorus
    "ultron": "rubberband=pitch=0.93:formant=preserved,equalizer=f=160:t=o:w=1.2:g=2.5,"
              "chorus=0.9:0.9:28|41:0.22|0.16:0.22|0.31:1.8|2.4,"
              "aecho=0.9:0.85:6|47|90:0.2|0.1|0.06,asoftclip=type=tanh:threshold=0.9," + _COMMON_TAIL,
    # Morgan's comms from the workshop: light radio band-limit, still easy to understand
    "radio": "highpass=f=260,lowpass=f=4800,equalizer=f=1800:t=o:w=1.5:g=2,"
             "acompressor=threshold=0.1:ratio=3:attack=5:release=60," + _COMMON_TAIL,
    "vision": "aecho=0.9:0.88:35|62:0.1|0.07,chorus=0.97:0.97:30:0.06:0.2:1.0," + _COMMON_TAIL,
    # Jocasta: clean; faint 9 kHz shelf (+1.5 dB) on top of the common tail, short low-mix chorus (~12%)
    "jocasta": _COMMON_TAIL + ",treble=g=4:f=9000:t=s,chorus=0.9:0.9:18:0.12:0.35:1.1",
    # her Mirror Lock "old self": the same take 2 semitones down with a little warm distortion (layered low)
    "jocasta_echo": "rubberband=pitch=0.891:formant=preserved,lowpass=f=5200,asoftclip=type=tanh:threshold=0.5," + _COMMON_TAIL,
    "titan": "rubberband=pitch=0.82,chorus=0.9:0.9:30|45:0.25|0.2:0.2|0.3:2|2.5,"
             "aecho=0.85:0.8:8|80:0.25|0.12," + _COMMON_TAIL,
}

_SPOKEN = [
    (r"\bDUM-E\b", "Dummy"), (r"\bDum-E\b", "Dummy"), (r"\bWARMACHINE\b", "War Machine"),
    (r"\bMark III\b", "Mark three"), (r"\bMark VII\b", "Mark seven"), (r"\bMark 85\b", "Mark eighty-five"),
    (r"\bOS\b", "O.S."), (r"\bHUD\b", "hud"),
]


def spoken_text(text):
    """Caption text -> what the TTS should say (names in caps read as words, not letters)."""
    t = ascii_text(text)
    for pat, rep in _SPOKEN:
        t = re.sub(pat, rep, t)
    t = re.sub(r"\b(JARVIS|FRIDAY|EDITH|ULTRON|MORGAN|PEPPER|TONY|VISION)\b", lambda m: m.group(1).title(), t)
    return t


def run_fx(samples, rate, chain, pitch=1.0):
    import numpy as np
    af = chain
    if abs(pitch - 1.0) > 1e-3:
        af = "rubberband=pitch=%.3f:formant=preserved,%s" % (pitch, af)
    pad = "apad=pad_dur=%.2f" % TAIL_SILENCE
    cmd = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-f", "f32le", "-ar", str(rate), "-ac", "1", "-i", "-",
           "-af", pad + "," + af, "-f", "f32le", "-ar", str(rate), "-ac", "1", "-"]
    out = subprocess.run(cmd, input=np.asarray(samples, dtype=np.float32).tobytes(), capture_output=True, check=True).stdout
    return np.frombuffer(out, dtype=np.float32).copy()


def true_peak_db(x, rate):
    import numpy as np
    from scipy.signal import resample_poly
    if len(x) == 0:
        return -120.0
    up = resample_poly(x, 4, 1)
    return 20.0 * np.log10(max(1e-9, float(np.max(np.abs(up)))))


def normalise(x, rate, target=TARGET_LUFS, tp=TARGET_TP_DB):
    """Integrated loudness -> target LUFS, then pull down if the 4x-oversampled true peak exceeds tp."""
    import numpy as np
    import pyloudnorm as pyln
    if len(x) == 0 or float(np.max(np.abs(x))) < 1e-6:
        return x
    meter = pyln.Meter(rate)
    probe = x if len(x) >= int(0.45 * rate) else np.concatenate([x, x, x])
    try:
        lufs = meter.integrated_loudness(probe)
    except Exception:
        lufs = float("-inf")
    if not np.isfinite(lufs):  # extremely short / quiet: fall back to RMS
        lufs = 20.0 * np.log10(max(1e-9, float(np.sqrt(np.mean(x ** 2))))) - 0.7
    y = x * (10.0 ** ((target - lufs) / 20.0))
    peak = true_peak_db(y, rate)
    if peak > tp:  # tame the few transient peaks with a gentle limiter instead of turning the whole line down
        lim = 10.0 ** ((tp - 0.6) / 20.0)
        cmd = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-f", "f32le", "-ar", str(rate), "-ac", "1", "-i", "-",
               "-af", "alimiter=limit=%.4f:attack=3:release=50:level=disabled:asc=1" % lim,
               "-f", "f32le", "-ar", str(rate), "-ac", "1", "-"]
        y = np.frombuffer(subprocess.run(cmd, input=y.astype(np.float32).tobytes(), capture_output=True,
                                         check=True).stdout, dtype=np.float32).copy()
        peak = true_peak_db(y, rate)
    if peak > tp:
        y = y * (10.0 ** ((tp - peak - 0.1) / 20.0))
    return y


def write_wav(path, x, rate):
    import numpy as np
    import soundfile as sf
    x = np.asarray(x, dtype=np.float32)
    x = np.concatenate([np.zeros((int(rate * LEAD_SILENCE),) + x.shape[1:], dtype=np.float32), x])
    os.makedirs(os.path.dirname(path), exist_ok=True)
    sf.write(path, np.clip(x, -1.0, 1.0), rate, subtype="PCM_16")


class Synth:
    """Kokoro-82M (onnx) voice engine shared by both generator scripts."""

    def __init__(self, voices_dir):
        try:
            from kokoro_onnx import Kokoro
        except Exception as e:
            raise RuntimeError("kokoro-onnx missing: run with /workspace/kokoro-venv/bin/python "
                               "(pip install kokoro-onnx soundfile pyloudnorm scipy)") from e
        kdir = os.path.join(voices_dir, "kokoro")
        model, voices = os.path.join(kdir, "kokoro-v1.0.onnx"), os.path.join(kdir, "voices-v1.0.bin")
        for p in (model, voices):
            if not os.path.exists(p):
                raise RuntimeError("Kokoro model file missing: " + p)
        if not shutil.which("ffmpeg"):
            raise RuntimeError("ffmpeg is required for the voice FX")
        self.k = Kokoro(model, voices)
        self.styles = {}

    def style(self, spec):
        if spec not in self.styles:
            parts = [(p.split(":")[0], float(p.split(":")[1]) if ":" in p else 1.0) for p in spec.split(",")]
            tot = sum(w for _, w in parts)
            self.styles[spec] = sum(self.k.get_voice_style(n) * (w / tot) for n, w in parts) if len(parts) > 1 else parts[0][0]
        return self.styles[spec]

    def render(self, profile_name, text):
        prof = VOICE_PROFILES.get(profile_name)
        if prof is None:
            warnings.append("no voice profile for %s (using %s)" % (profile_name, DEFAULT_PROFILE))
            prof = VOICE_PROFILES[DEFAULT_PROFILE]
        samples, rate = self.k.create(spoken_text(text), voice=self.style(prof["voice"]), speed=prof["speed"], lang=prof["lang"])
        y = run_fx(samples, rate, FX_CHAINS[prof["fx"]], prof.get("pitch", 1.0))
        if prof.get("signature"):
            y = add_signature_tone(y, rate)
        return normalise(y, rate), rate

    def render_echo(self, profile_name, text):
        """Jocasta's Mirror Lock variant: stereo, her take centred plus a second take 2 semitones down,
        warm-distorted, hard-panned right, 14 dB under her (never overpowers the current voice)."""
        import numpy as np
        prof = VOICE_PROFILES[profile_name]
        y, rate = self.render(profile_name, text)
        samples, _ = self.k.create(spoken_text(text), voice=self.style(prof["voice"]), speed=prof["speed"], lang=prof["lang"])
        e = normalise(run_fx(samples, rate, FX_CHAINS["jocasta_echo"], prof.get("pitch", 1.0)), rate) * (10 ** (-14 / 20))
        lag = int(rate * 0.045)
        n = max(len(y), len(e) + lag)
        L = np.zeros(n, dtype=np.float32); R = np.zeros(n, dtype=np.float32)
        L[:len(y)] += y * 0.92; R[:len(y)] += y * 0.92
        R[lag:lag + len(e)] += e
        return np.stack([L, R], axis=1), rate


def estimate_f0(x, rate, lo=110.0, hi=300.0):
    """Rough fundamental of the first voiced 400 ms (autocorrelation); 180 Hz if unsure."""
    import numpy as np
    x = np.asarray(x, dtype=np.float64)
    thr = 0.1 * (np.max(np.abs(x)) + 1e-9)
    idx = np.nonzero(np.abs(x) > thr)[0]
    if len(idx) == 0:
        return 180.0, 0
    start = int(idx[0])
    seg = x[start:start + int(rate * 0.4)]
    if len(seg) < int(rate / lo) * 2:
        return 180.0, start
    seg = seg - seg.mean()
    ac = np.correlate(seg, seg, mode="full")[len(seg) - 1:]
    a, b = int(rate / hi), int(rate / lo)
    lag = a + int(np.argmax(ac[a:b]))
    return (rate / lag if ac[lag] > 0.3 * ac[0] else 180.0), start


def add_signature_tone(x, rate, level_db=-24.0, dur=0.6, fade_in=0.15):
    """Jocasta's signal mark: a soft sine near the line's fundamental under the first word, ~-24 dB
    relative to the line's RMS, 150 ms fade-in and a gentle fade-out."""
    import numpy as np
    x = np.asarray(x, dtype=np.float32).copy()
    f0, start = estimate_f0(x, rate)
    n = min(int(rate * dur), len(x) - start)
    if n <= 0:
        return x
    voiced = x[np.abs(x) > 0.02 * (np.max(np.abs(x)) + 1e-9)]
    rms = float(np.sqrt(np.mean(voiced.astype(np.float64) ** 2))) if len(voiced) else 0.05
    amp = rms * np.sqrt(2.0) * 10 ** (level_db / 20.0)
    t = np.arange(n) / rate
    env = np.minimum(1.0, t / fade_in) * np.minimum(1.0, (dur - t) / 0.2)
    x[start:start + n] += (amp * env * np.sin(2 * np.pi * f0 * t)).astype(np.float32)
    return x

    def say(self, profile_name, text, path):
        y, rate = self.render(profile_name, text)
        write_wav(path, y, rate)


def write_if_changed(path, text):
    if os.path.exists(path) and open(path).read() == text:
        return False
    open(path, "w").write(text)
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", default=os.environ.get("IRONMAN_DATA", "/workspace/ironman-3d/contrib"))
    ap.add_argument("--voices-dir", default=os.environ.get("PIPER_VOICES", "/workspace/tts-voices"))
    ap.add_argument("--no-audio", action="store_true")
    ap.add_argument("--force", action="store_true", help="re-synthesise WAVs that already exist")
    ap.add_argument("--no-codegen", action="store_true", help="do not touch Source/.../Generated/*.inl")
    ap.add_argument("--only", default="", help="only synthesise this persona folder (e.g. ULTRON)")
    ap.add_argument("--shard", default="0/1", help="i/n: synthesise every n-th line starting at i (parallel runs)")
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
    if not a.no_codegen:  # only rewrites a .inl whose content actually changed
        for name, text in (("SuitTable.inl", suit_inl), ("AIVoiceLines.inl", line_inl)):
            print("%s: %s" % (name, "updated" if write_if_changed(os.path.join(GEN_DIR, name), text) else "unchanged"))
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
        si, sn = (int(v) for v in a.shard.split("/"))
        for n, l in enumerate(lines):
            if n % sn != si:
                continue
            path = os.path.join(AUDIO_DIR, PERSONAS[l["persona"]][1], l["id"] + ".wav")
            if a.only and PERSONAS[l["persona"]][1] != a.only.upper():
                continue
            if a.force or not os.path.exists(path):
                synth.say(l["persona"], l["text"], path)
            done += 1
            if done % 50 == 0:
                print("  audio %d / %d" % (done, len(lines)), flush=True)
        print("audio: %d WAVs in %s" % (done, AUDIO_DIR))
    for wmsg in sorted(set(warnings)):
        print("WARNING:", wmsg)


if __name__ == "__main__":
    main()
