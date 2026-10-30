#!/usr/bin/env python3
"""Synthesise the prologue flashback score swell (original, procedurally generated, CC0 / no samples).

Quiet D-minor string pad that breathes under the battlefield shots, swells as the sun breaks
through near the end, then thins to a single music-box note. Pure numpy; writes a 48 kHz
16-bit stereo WAV to SourceMusic/flashback_swell.wav (imported as /Game/Music/S_FlashbackSwell).
"""
import os, sys, wave
import numpy as np

RATE = 32000
LENGTH = 112.0
rng = np.random.default_rng(1985)

def hz(midi):
    return 440.0 * 2.0 ** ((midi - 69) / 12.0)

def pad_voice(freq, t, detune_cents, vib_rate, vib_depth=0.0025):
    """Band-limited saw-ish string voice (additive, soft rolloff) with slow vibrato."""
    out = np.zeros_like(t)
    f = freq * 2.0 ** (detune_cents / 1200.0)
    phase_mod = vib_depth * np.sin(2 * np.pi * vib_rate * t + rng.uniform(0, 6.28))
    for n in range(1, 11):
        if f * n > 9000:
            break
        amp = (1.0 / n) * np.exp(-0.28 * n)
        out += amp * np.sin(2 * np.pi * f * n * t * (1 + phase_mod) + rng.uniform(0, 6.28))
    return out

def main():
    t = np.arange(int(RATE * LENGTH)) / RATE
    # Chords (MIDI): Dm, Bb, F/A, C, Dm, Bb, F, A (sus) -> Dm add9 for the swell.
    chords = [
        [50, 57, 62, 65], [46, 58, 62, 65], [45, 57, 60, 65], [48, 55, 60, 64],
        [50, 57, 62, 65], [46, 58, 62, 67], [41, 57, 60, 65], [45, 57, 62, 64],
        [50, 57, 62, 65, 69], [50, 57, 64, 65, 69],
    ]
    seg = LENGTH / len(chords)
    left = np.zeros_like(t)
    right = np.zeros_like(t)
    for ci, chord in enumerate(chords):
        start = ci * seg - 2.5
        end = (ci + 1) * seg + 2.5
        env = np.clip((t - start) / 3.0, 0, 1) * np.clip((end - t) / 3.0, 0, 1)
        env = np.sin(env * np.pi / 2) ** 2
        mask = env > 0
        if not mask.any():
            continue
        tt = t[mask]
        for k, note in enumerate(chord):
            f = hz(note)
            l = pad_voice(f, tt, -6 - k, 4.6 + 0.3 * k)
            r = pad_voice(f, tt, +6 + k, 4.9 + 0.2 * k)
            w = 0.8 if note < 48 else 1.0
            left[mask] += env[mask] * l * w
            right[mask] += env[mask] * r * w

    # Dynamics: barely there, a small rise at the reactor shots, the swell as the sun breaks through, fade.
    dyn = np.interp(t, [0, 8, 40, 60, 78, 92, 100, 108, LENGTH],
                       [0.0, 0.18, 0.22, 0.3, 0.28, 0.75, 0.6, 0.12, 0.0])
    left *= dyn
    right *= dyn

    # Closing music-box note (A5) over the "years later" card.
    nb = t > 101.0
    tn = t[nb] - 101.0
    box = (np.sin(2 * np.pi * hz(81) * tn) + 0.3 * np.sin(2 * np.pi * hz(81) * 3 * tn)) * np.exp(-tn * 1.2) * 0.25
    left[nb] += box
    right[nb] += box * 0.9

    # Room: convolve with a short decaying-noise impulse (2.2 s), mixed 35 %.
    ir_len = int(RATE * 2.2)
    decay = np.exp(-np.arange(ir_len) / (RATE * 0.45))
    for ch in (0, 1):
        ir = rng.standard_normal(ir_len) * decay
        ir /= np.sqrt((ir ** 2).sum())
        sig = left if ch == 0 else right
        n = 1 << int(np.ceil(np.log2(len(sig) + ir_len)))
        wet = np.fft.irfft(np.fft.rfft(sig, n) * np.fft.rfft(ir, n), n)[: len(sig)]
        mixed = 0.65 * sig + 0.35 * wet
        if ch == 0:
            left = mixed
        else:
            right = mixed

    peak = max(np.abs(left).max(), np.abs(right).max(), 1e-9)
    gain = 10 ** (-3.0 / 20) / peak          # -3 dBFS peak at the swell; the director plays it at 0.45
    pcm = np.stack([left, right], axis=1) * gain
    pcm = (np.clip(pcm, -1, 1) * 32767).astype("<i2")

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "SourceMusic", "flashback_swell.wav")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with wave.open(out, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())
    print(f"wrote {out}: {LENGTH:.0f} s, 32 kHz stereo")

if __name__ == "__main__":
    main()
