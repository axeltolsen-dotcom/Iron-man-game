# Suit Realism: Reactive Surface State (Jocasta)

The suits look most fake when they look the same after a fight as they did before it. U's `contrib/u/suit_materials.json` sets each suit's static paint, metal, and baseline wear. Vison's `contrib/vison/` covers post-process and lighting. This doc covers what's left: how a suit's surface **changes during play** (wet, dirty, scorched, hot, damaged) and how the armor **moves like heavy metal**. Nothing here edits game source. Values are in `suit_reactive_state.json`.

## 1. Dynamic material parameters

Drive these as scalar parameters on a per-suit Material Instance Dynamic (MID) on top of U's static values. Each one is 0 to 1 unless noted, and all of them decay or build over time.

| Param | Builds from | Decays | Visual |
|---|---|---|---|
| `Wetness` | Rain volume, water contact (flooded tunnels in `sm_jocasta_second_signal`), low flight through spray | 0.04/s in dry air, 0.12/s with reactor heat above 0.5 | Roughness × (1 − 0.6·Wetness), albedo darkens up to 15% on painted layers only, and droplet normal detail fades in over 0.4. Clear coat is unchanged because it's already glossy. |
| `GrimeLive` | Ground objectives, landings on dirt or rubble, nearby explosions | Doesn't decay in a mission. It resets at the workshop. Rain washes it to 50% of its value. | It adds to U's static `dirt` and uses U's suggested cavity-mask approach: darken up to 30% and add up to +0.25 roughness. Upward-facing surfaces (shoulders, boot tops) gather it 2× faster. |
| `ScorchLive` | Repulsor fire (palms only), near-miss explosions, tank shells | Persists until repair | It adds to U's `Scorch`. Palms, forearm vents, and chest-plate edges get it first. |
| `HeatTint` | Reactor heat (the same value as the `energy` objective's thermal state) | It follows heat with a 1.5 s lag | Heat-temper discoloration only on bare-metal layers (metallic > 0.8): a straw to bronze to blue ramp near the vents. A faint emissive (≤ 2.0) appears only above 0.85 heat. |
| `DamageLevel` | Health lost, in steps (0, 0.33, 0.66, 1) | Workshop repair | It blends in damage decals and a cracked-panel normal map, and raises EdgeWear by up to +0.3. |

**Rule:** store these per suit in the save, so a suit you walked out of a burning building in still looks like it at the start of the next mission until you repair it at the workshop. That makes the Hall of Armor restoration and workshop loop matter visually.

## 2. Damage states

Use four tiers keyed off `DamageLevel`, with no mesh swaps, just decals and material blends so they stay cheap:

0. **Fresh.** U's baseline.
1. **Scuffed.** Paint chipped on high-contact edges (knuckles, knees, shoulder caps, boot toes) and a few short scratch decals.
2. **Battered.** Dents on the chest and thigh (normal-map only), one cracked eye-lens decal, and the panel-line glow flickers at 0.5 Hz.
3. **Critical.** Exposed substrate (U's `substrateColorLinear`) on two or three large panels, sparks from one random seam every 2 to 4 s, and the reactor emissive stutters.

Decals should come from a small shared atlas (8 scratch, 4 dent, 4 crack, 2 burn). Spread them with a deterministic seed per suit, so the same suit always takes damage in the same believable places.

## 3. Weight and motion

Realistic materials don't help if the suit moves like cloth.

- **Landings:** a 3-tier landing based on fall speed (soft below 6 m/s, hard below 15 m/s, superhero landing above that). Hard and superhero landings add a 2 to 4 frame camera dip, a ground decal, a dust burst, and +0.05 `GrimeLive`.
- **Servo sound:** short, low servo whines on large joint rotations (shoulder, hip, knee) above 90°/s, low in the mix. Heavier suits like War Machine and Hulkbuster pitch down about 3 semitones.
- **Plate overlap:** U's armor kit already layers plates. Add 1 to 2 cm of secondary jiggle on shoulder caps and backpack flaps only (a spring with stiffness about 800 and damping about 0.9), never on helmet or chest.
- **Hover:** a slight 0.3 Hz bob (±1.5 cm) and a heat-shimmer distortion cone under the boot thrusters. The shimmer should be a screen-space distortion, not a particle sprite.
- **Flight attitude:** bank up to 35° into turns and pitch the torso forward with speed. The arms trail slightly at high speed, which sells momentum.

## 4. Jocasta's borrowed chassis (the ally from my side mission)

She isn't an Iron Man suit, so she should read as **different from Stark tech** at a glance.

- **Surface:** satin ceramic-composite shell, not paint. Base color is linear [0.62, 0.66, 0.70] (pale blue-gray), metallic 0.1, roughness 0.38, no clear coat, no metal flake.
- **Seams:** thin seams with a pale-blue emissive (linear [0.55, 0.78, 1.0], intensity 6). It pulses gently with her voice's harmonic tone (see `jocasta_voice.md`).
- **Ultron residue:** faint, half-sanded red trace lines under the shell on the forearms (roughness 0.6, red emissive at 0.3). It's visible only in shadow or under the Mirror Lock's red pulses.
- **Wear:** she gets `GrimeLive` and `ScorchLive` like the suits, but no `HeatTint`, because she doesn't run on an arc reactor. Damage tier 3 exposes a dark graphite substrate, not bare metal.

## 5. Performance

All five dynamic params are scalar MID updates at 10 Hz, which costs nothing. Decals should be capped at 12 per suit. Wetness droplet normals should use a single tiling texture. Anything heavier than that, check against Ultron's `contrib/ultron/perf_budget.md`.

## 6. Integration (for whoever merges)

1. Add the five scalar params to `M_CarPaint`: `Wetness`, `GrimeLive`, `HeatTint`, and `DamageLevel`, with `ScorchLive` summed into the existing `Scorch`. `GrimeLive` also fills the `Dirt` gap U flagged.
2. Create one MID per spawned suit and tick it from the suit's heat, health, and environment queries.
3. Save the four persistent values (grime, scorch, damage, plus wetness = 0) per suit ID. The workshop repair action clears them.
