#!/usr/bin/env python3
"""
Generate the story-mode voice WAVs and SourceAudio/StoryAudio/story_lines.csv.

Reads Content/Story/story.json (prologue, chapter intro/outro shots, objective comms, finale)
and Content/Story/morgan.json (Morgan's "guy in the chair" comms chatter) and writes

    SourceAudio/StoryAudio/<SPEAKER>/<line id>.wav          default take ({playerName} = Harley)
    SourceAudio/StoryAudio/<SPEAKER>/<line id>_noname.wav   same line with the name tokens dropped
                                                        (used when the player renamed themself
                                                        and for {heroName} lines)
    SourceAudio/StoryAudio/MORGAN/MORGAN_<event>_<n>.wav    comms chatter
    SourceAudio/StoryAudio/story_lines.csv                  id, speaker, folder, file, text

The ids match UStoryLibrary (StoryLibrary.cpp): prologue_NN, <chapter>_intro_NN,
<chapter>_outro_NN, <chapter>_obj<N>_<trigger>, finale_NN, <FOLDER>_<event>_<n>.
Import them in the editor with Content/Python/import_jarvis_audio.py (it also imports
SourceAudio/StoryAudio into /Game/StoryAudio). Everything is synthetic placeholder audio:
offline piper TTS voices (https://huggingface.co/rhasspy/piper-voices), with simple pitch /
echo processing for the characters; U and DUM-E get procedural robot beeps.

    python3 Tools/generate_story_audio.py [--voices-dir DIR] [--force] [--only SPEAKER] [--csv-only]
"""
import argparse
import csv
import io
import json
import os
import re
import shutil
import subprocess
import sys
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STORY_DIR = os.path.join(ROOT, "Content", "Story")
OUT_DIR = os.path.join(ROOT, "SourceAudio", "StoryAudio")  # raw WAVs stay out of Content/; imported .uasset go to Content/StoryAudio

# speaker -> (piper voice, length scale, pitch factor (>1 higher), echo, espeak voice)
VOICES = {
    "TONY":    ("en_US-ryan-medium", 0.95, 1.00, False, "en-us+m3"),
    "PEPPER":  ("en_US-amy-medium", 1.02, 0.96, False, "en-us+f3"),
    "MORGAN":  ("en_US-amy-medium", 0.95, 1.12, False, "en-us+f4"),
    "PLAYER":  ("en_US-joe-medium", 0.95, 1.05, False, "en-us+m1"),
    "JARVIS":  ("en_GB-alan-medium", 1.05, 1.00, False, "en-gb"),
    "FRIDAY":  ("en_GB-alba-medium", 0.95, 1.00, False, "en-gb-scotland+f3"),
    "EDITH":   ("en_US-amy-medium", 0.92, 1.00, False, "en-us+f2"),
    "ULTRON":  ("en_US-ryan-medium", 0.90, 0.82, True, "en-us+m3"),
    "VISON":   ("en_GB-alan-medium", 1.08, 0.90, True, "en-gb"),
    "OBSIDIAN_TITAN": ("en_US-joe-medium", 1.10, 0.75, True, "en-us+m7"),
}
ROBOTS = {"U", "DUME"}
DEFAULT_NAME = "Harley"
HERO_DEFAULT = "Iron Lad"

warnings = []


def folder_for(speaker):
    """Mirror of UStoryLibrary::GetSpeakerFolder."""
    out = []
    for ch in speaker.upper():
        if ch.isalnum():
            out.append(ch)
        elif ch in " _":
            out.append("_")
    return "".join(out) or "UNKNOWN"


def ascii_text(s):
    rep = {"\u2019": "'", "\u2018": "'", "\u201c": '"', "\u201d": '"', "\u2014": ", ", "\u2013": "-", "\u2026": "..."}
    for k, v in rep.items():
        s = s.replace(k, v)
    return s


def resolve(text, player=DEFAULT_NAME, hero=HERO_DEFAULT):
    return text.replace("{playerName}", player).replace("{heroName}", hero)


def drop_names(text):
    """'Nice work, {playerName}.' -> 'Nice work.'  '{playerName}, listen.' -> 'Listen.'"""
    t = text
    for tok in ("{playerName}", "{heroName}"):
        t = re.sub(r"-and-" + re.escape(tok), "", t)  # "Morgan-and-{playerName} reactor"
        t = re.sub(r",\s*" + re.escape(tok), "", t)
        t = re.sub(re.escape(tok) + r"\s*,\s*", "", t)
        t = re.sub(r"\b(?:the\s+)?" + re.escape(tok), "", t)
        t = t.replace(tok, "")
    t = re.sub(r"\s+([.,!?])", r"\1", t)
    t = re.sub(r"\s{2,}", " ", t).strip()
    t = re.sub(r"(^|[.!?]\s+)([a-z])", lambda m: m.group(1) + m.group(2).upper(), t)
    return t


def is_meaningful(text):
    """A name-less take like 'Call me.' says nothing; those get a silent take instead."""
    return len(re.findall(r"[A-Za-z']+", text)) >= 3


def collect_lines(story, morgan):
    lines = []  # (id, speaker, text)

    def shots(holder, prefix):
        for i, s in enumerate((holder or {}).get("shots", []), 1):
            ln = s.get("line")
            if ln and ln.get("text"):
                lines.append(("%s_%02d" % (prefix, i), ln.get("speaker", ""), ln["text"]))

    shots(story.get("prologue"), "prologue")
    for ch in story.get("chapters", []):
        cid = ch.get("id", "")
        shots(ch.get("intro"), cid + "_intro")
        for oi, obj in enumerate(ch.get("objectives", []), 1):
            for cm in obj.get("comms", []):
                if cm.get("text"):
                    trig = (cm.get("trigger") or "start").lower()
                    lines.append(("%s_obj%d_%s" % (cid, oi, trig), cm.get("speaker", ""), cm["text"]))
        shots(ch.get("outro"), cid + "_outro")
    shots(story.get("finale"), "finale")

    if morgan:
        spk = morgan.get("persona", "MORGAN")
        fold = folder_for(spk)
        for event, items in morgan.get("events", {}).items():
            for i, text in enumerate(items, 1):
                if isinstance(text, str) and text:
                    lines.append(("%s_%s_%d" % (fold, event, i), spk, text))
    return lines


# ------------------------------------------------------------------------------------------ audio

def process(frames, rate, pitch, echo):
    import numpy as np
    x = np.frombuffer(frames, dtype=np.int16).astype(np.float32)
    if abs(pitch - 1.0) > 1e-3:
        n = int(len(x) / pitch)
        x = np.interp(np.arange(n) * pitch, np.arange(len(x)), x)
    if echo:
        for delay_s, gain in ((0.05, 0.3), (0.12, 0.15)):
            d = int(rate * delay_s)
            if d < len(x):
                x[d:] += gain * x[:-d].copy()
    peak = float(np.max(np.abs(x))) if len(x) else 1.0
    if peak > 1.0:
        x *= min(1.0, 30000.0 / peak) if peak > 30000.0 else 1.0
    return x.astype(np.int16).tobytes()


def robot_beeps(text, rate=22050):
    """Procedural chirps for U / DUM-E: one beep per word, pitch from the letters."""
    import numpy as np
    words = re.findall(r"[A-Za-z']+", text) or ["beep"]
    out = [np.zeros(int(rate * 0.12), dtype=np.float32)]
    for w in words[:14]:
        f0 = 500.0 + (sum(ord(c) for c in w) % 9) * 110.0
        dur = 0.07 + 0.025 * min(len(w), 6)
        t = np.arange(int(rate * dur)) / rate
        f = f0 * (1.0 + (0.35 if w.endswith("?") or len(w) % 2 else -0.2) * t / dur)
        tone = np.sin(2 * np.pi * np.cumsum(f) / rate) * 0.5 + 0.2 * np.sign(np.sin(2 * np.pi * f0 * 0.5 * t))
        env = np.minimum(1.0, np.minimum(t / 0.01, (dur - t) / 0.02))
        out.append((tone * env).astype(np.float32))
        out.append(np.zeros(int(rate * 0.05), dtype=np.float32))
    y = np.concatenate(out) * 12000.0
    return rate, y.astype(np.int16).tobytes()


def write_silence(path, seconds, rate=22050):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"\x00\x00" * int(rate * seconds))


class Synth:
    def __init__(self, voices_dir):
        self.voices_dir = voices_dir
        self.cache = {}
        try:
            from piper import PiperVoice, SynthesisConfig  # noqa: F401
            self.piper = True
        except Exception:
            self.piper = False
        self.espeak = shutil.which("espeak-ng") or shutil.which("espeak")

    def say(self, folder, text, path):
        if folder in ROBOTS:
            rate, frames = robot_beeps(text)
            width, ch = 2, 1
        else:
            voice, length, pitch, echo, esp = VOICES.get(folder, VOICES["PLAYER"])
            if folder not in VOICES:
                warnings.append("no voice mapping for %s (using PLAYER)" % folder)
            model = os.path.join(self.voices_dir, voice + ".onnx")
            if self.piper and os.path.exists(model):
                from piper import PiperVoice, SynthesisConfig
                if model not in self.cache:
                    self.cache[model] = PiperVoice.load(model)
                buf = io.BytesIO()
                with wave.open(buf, "wb") as w:
                    self.cache[model].synthesize_wav(text, w, syn_config=SynthesisConfig(length_scale=length))
                buf.seek(0)
                with wave.open(buf, "rb") as r:
                    rate, width, ch, frames = r.getframerate(), r.getsampwidth(), r.getnchannels(), r.readframes(r.getnframes())
            elif self.espeak:
                warnings.append("espeak fallback for %s" % folder)
                data = subprocess.run([self.espeak, "-v", esp, "-s", "165", "--stdout", text],
                                      check=True, capture_output=True).stdout
                with wave.open(io.BytesIO(data), "rb") as r:
                    rate, width, ch, frames = r.getframerate(), r.getsampwidth(), r.getnchannels(), r.readframes(r.getnframes())
            else:
                raise RuntimeError("no TTS available (pip install piper-tts, or install espeak-ng); missing " + model)
            if width == 2 and ch == 1:
                frames = process(frames, rate, pitch, echo)
        frames = b"\x00" * int(rate * 0.12) * width * ch + frames  # avoid clipped first syllables
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with wave.open(path, "wb") as w:
            w.setnchannels(ch)
            w.setsampwidth(width)
            w.setframerate(rate)
            w.writeframes(frames)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--voices-dir", default=os.environ.get("PIPER_VOICES", "/workspace/tts-voices"))
    ap.add_argument("--force", action="store_true", help="re-synthesise existing WAVs")
    ap.add_argument("--only", default="", help="only this speaker folder (e.g. MORGAN)")
    ap.add_argument("--csv-only", action="store_true", help="write story_lines.csv, no audio")
    a = ap.parse_args()

    with open(os.path.join(STORY_DIR, "story.json"), encoding="utf-8") as f:
        story = json.load(f)
    morgan = None
    mp = os.path.join(STORY_DIR, "morgan.json")
    if os.path.exists(mp):
        with open(mp, encoding="utf-8") as f:
            morgan = json.load(f)
    lines = collect_lines(story, morgan)

    ids = [l[0] for l in lines]
    dups = sorted({i for i in ids if ids.count(i) > 1})
    if dups:
        warnings.append("duplicate line ids (later ones win): " + ", ".join(dups))

    synth = None if a.csv_only else Synth(a.voices_dir)
    rows = []
    made = skipped = 0
    for lid, speaker, text in lines:
        folder = folder_for(speaker)
        takes = [(lid, resolve(text))]
        if "{playerName}" in text or "{heroName}" in text:
            takes.append((lid + "_noname", drop_names(text)))
        for take_id, take_text in takes:
            rel = "%s/%s.wav" % (folder, take_id)
            rows.append((take_id, speaker, folder, rel, take_text))
            if synth is None or (a.only and folder != a.only.upper()):
                continue
            path = os.path.join(OUT_DIR, folder, take_id + ".wav")
            if os.path.exists(path) and not a.force:
                skipped += 1
                continue
            if take_id.endswith("_noname") and not is_meaningful(take_text):
                write_silence(path, 1.0)  # the caption still shows the typed name
            else:
                synth.say(folder, ascii_text(take_text), path)
            made += 1

    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, "story_lines.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["id", "speaker", "folder", "file", "text"])
        w.writerows(rows)

    print("story lines: %d takes (%d synthesised, %d already present)" % (len(rows), made, skipped))
    for wmsg in sorted(set(warnings)):
        print("warning:", wmsg)
    return 0


if __name__ == "__main__":
    sys.exit(main())
