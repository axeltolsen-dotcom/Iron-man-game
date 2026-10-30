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
Kokoro-82M stock voices (Apache-2.0, CPU via kokoro-onnx), light ffmpeg FX per character,
loudness-normalised to -20 LUFS; U and DUM-E get soft procedural robot beeps.
Run with /workspace/kokoro-venv/bin/python.

    python3 Tools/generate_story_audio.py [--voices-dir DIR] [--force] [--only SPEAKER] [--csv-only]

Side missions are unvoiced unless listed with --missions (ids from side_missions.json); their takes
are written to side_lines.csv instead of story_lines.csv. --missions sm_jocasta_second_signal also
writes Jocasta's in-game event takes (summon, Mirror Lock block/hints) to
SourceAudio/Audio/JOCASTA/JOCASTA_<event>_<n>.wav (-> /Game/Audio/JOCASTA, see MirrorLock.cpp).
Import just those with Content/Python/import_jocasta_audio.py.
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

# Voices: Kokoro-82M stock voices (no cloning), defined in generate_suits_and_voices.VOICE_PROFILES
# (TONY, PEPPER, MORGAN, PLAYER, VISON, OBSIDIAN_TITAN + the suit AIs). Shared engine, FX and
# -20 LUFS / -3 dBTP loudness normalisation.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
ROBOTS = {"U", "DUME"}
DEFAULT_NAME = "Harley"
HERO_DEFAULT = "Iron Lad"

warnings = []

AUDIO_DIR = os.path.join(ROOT, "SourceAudio", "Audio")
# Jocasta's event lines; the captions must match MirrorLock.cpp / StoryDirector.cpp (HandleSummonAlly).
_ORDINALS = ["first", "second", "third", "fourth", "fifth", "sixth"]
JOCASTA_EVENTS = [("JOCASTA_summon_1", "I'm on your flank. Don't thank me yet.", False),
                  ("JOCASTA_mirrorLockBlock_1", "Got it. That one doesn't reach you.", True)] + \
                 [("JOCASTA_mirrorLockHint_%d" % (i + 1), "The %s pillar. It sounds least like me." % o, True)
                  for i, o in enumerate(_ORDINALS)]
# Lines spoken while she argues with her old self at the Mirror Lock get the Ultron-echo variant.
ECHO_LINES = {"sm_jocasta_second_signal_obj4_start"}


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

def robot_beeps(text, rate=24000):
    """Soft procedural chirps for U / DUM-E. Onomatopoeia in *...* shapes them: beep = blip,
    bweep/bwoop = rising/falling glide, whirr = servo buzz, click/chunk = clunk; '?' rises."""
    import numpy as np
    m = re.findall(r"\*([^*]+)\*", text)
    words = re.findall(r"[A-Za-z']+[?!]?", " ".join(m) if m else text) or ["beep"]
    out = [np.zeros(int(rate * 0.05), dtype=np.float32)]
    for w in words[:10]:
        lw = w.lower().strip("?!")
        loud = 1.0 if w.strip("?!").isupper() else 0.7
        if lw.startswith("whir"):
            dur = 0.35; t = np.arange(int(rate * dur)) / rate
            f = 180 + 60 * np.sin(2 * np.pi * 7 * t)
            tone = 0.6 * np.sin(2 * np.pi * np.cumsum(f) / rate) + 0.25 * np.sin(2 * np.pi * np.cumsum(2 * f) / rate)
        elif lw in ("click", "chunk", "clunk", "clank"):
            dur = 0.09; t = np.arange(int(rate * dur)) / rate
            tone = np.random.RandomState(len(lw)).randn(len(t)) * np.exp(-t * 60) * 0.8 + np.sin(2 * np.pi * 140 * t) * np.exp(-t * 30)
        else:
            f0 = 620.0 + (sum(ord(c) for c in lw) % 7) * 90.0
            dur = 0.08 + 0.02 * min(len(lw), 6); t = np.arange(int(rate * dur)) / rate
            glide = 0.0
            if lw.startswith("bwe") or w.endswith("?"): glide = 0.45
            elif lw.startswith("bwo") or lw.startswith("boo"): glide = -0.35
            f = f0 * (1.0 + glide * t / dur)
            tone = np.sin(2 * np.pi * np.cumsum(f) / rate) + 0.15 * np.sin(2 * np.pi * np.cumsum(2 * f) / rate)
        env = np.minimum(1.0, np.minimum(t / 0.008, np.maximum(0.0, dur - t) / 0.025))
        out.append((tone * env * loud).astype(np.float32))
        out.append(np.zeros(int(rate * 0.06), dtype=np.float32))
    return rate, np.concatenate(out) * 0.3


def write_silence(path, seconds, rate=24000):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"\x00\x00" * int(rate * seconds))


class Synth:
    def __init__(self, voices_dir):
        import generate_suits_and_voices as engine
        self.engine = engine
        self.tts = engine.Synth(voices_dir)

    def say(self, folder, text, path, echo=False):
        e = self.engine
        if echo and folder in e.VOICE_PROFILES:
            y, rate = self.tts.render_echo(folder, text)
            e.write_wav(path, y, rate)
            return
        if folder in ROBOTS:
            rate, y = robot_beeps(text)
            y = e.normalise(y, rate, target=e.TARGET_LUFS - 3.0)  # beeps sit a little under speech
        else:
            if folder not in e.VOICE_PROFILES:
                warnings.append("no voice mapping for %s (using %s)" % (folder, e.DEFAULT_PROFILE))
            y, rate = self.tts.render(folder if folder in e.VOICE_PROFILES else e.DEFAULT_PROFILE, text)
        e.write_wav(path, y, rate)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--voices-dir", default=os.environ.get("PIPER_VOICES", "/workspace/tts-voices"))
    ap.add_argument("--force", action="store_true", help="re-synthesise existing WAVs")
    ap.add_argument("--only", default="", help="only this speaker folder (e.g. MORGAN)")
    ap.add_argument("--csv-only", action="store_true", help="write story_lines.csv, no audio")
    ap.add_argument("--shard", default="0/1", help="i/n: synthesise every n-th take starting at i (parallel runs)")
    ap.add_argument("--missions", default="", help="comma-separated side mission ids to voice (only those)")
    a = ap.parse_args()

    with open(os.path.join(STORY_DIR, "story.json"), encoding="utf-8") as f:
        story = json.load(f)
    morgan = None
    mp = os.path.join(STORY_DIR, "morgan.json")
    if os.path.exists(mp):
        with open(mp, encoding="utf-8") as f:
            morgan = json.load(f)
    missions = [m.strip() for m in a.missions.split(",") if m.strip()]
    events = []
    if missions:
        with open(os.path.join(STORY_DIR, "side_missions.json"), encoding="utf-8") as f:
            side = {m["id"]: m for m in json.load(f).get("missions", [])}
        missing = [m for m in missions if m not in side]
        if missing:
            print("unknown side mission(s): " + ", ".join(missing))
            return 1
        lines = collect_lines({"chapters": [side[m] for m in missions]}, None)
        if "sm_jocasta_second_signal" in missions:
            events = JOCASTA_EVENTS
    else:
        lines = collect_lines(story, morgan)

    ids = [l[0] for l in lines]
    dups = sorted({i for i in ids if ids.count(i) > 1})
    if dups:
        warnings.append("duplicate line ids (later ones win): " + ", ".join(dups))

    synth = None if a.csv_only else Synth(a.voices_dir)
    rows = []
    made = skipped = 0
    si, sn = (int(v) for v in a.shard.split("/"))
    n_take = -1
    for lid, speaker, text in lines:
        folder = folder_for(speaker)
        takes = [(lid, resolve(text))]
        if "{playerName}" in text or "{heroName}" in text:
            takes.append((lid + "_noname", drop_names(text)))
        for take_id, take_text in takes:
            rel = "%s/%s.wav" % (folder, take_id)
            rows.append((take_id, speaker, folder, rel, take_text))
            n_take += 1
            if synth is None or (a.only and folder != a.only.upper()) or n_take % sn != si:
                continue
            path = os.path.join(OUT_DIR, folder, take_id + ".wav")
            if os.path.exists(path) and not a.force:
                skipped += 1
                continue
            if take_id.endswith("_noname") and not is_meaningful(take_text):
                write_silence(path, 1.0)  # the caption still shows the typed name
            else:
                synth.say(folder, take_text, path, echo=lid in ECHO_LINES)
            made += 1

    for eid, text, echo in events:
        rel = "JOCASTA/%s.wav" % eid
        rows.append((eid, "JOCASTA", "JOCASTA", "../Audio/" + rel, text))
        path = os.path.join(AUDIO_DIR, rel)
        if synth is None or (os.path.exists(path) and not a.force):
            skipped += synth is not None
            continue
        synth.say("JOCASTA", text, path, echo=echo)
        made += 1

    os.makedirs(OUT_DIR, exist_ok=True)
    csv_name = "side_lines.csv" if missions else "story_lines.csv"
    with open(os.path.join(OUT_DIR, csv_name), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["id", "speaker", "folder", "file", "text"])
        w.writerows(rows)

    print("story lines: %d takes (%d synthesised, %d already present)" % (len(rows), made, skipped))
    for wmsg in sorted(set(warnings)):
        print("warning:", wmsg)
    return 0


if __name__ == "__main__":
    sys.exit(main())
