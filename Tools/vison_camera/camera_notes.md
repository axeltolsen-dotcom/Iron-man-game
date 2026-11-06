# Vison camera and post-process realism layer (UE 5.8)

Folder: `contrib/vison/camera/` (this is the only folder this layer writes to).
Layer order: `apply_lighting_preset.py`, then `apply_realism.py`, then **`apply_camera_realism.py`**.
This layer writes camera, lens, exposure and grade keys only. It does not touch sun, sky, fog, clouds, lights, VFX or destruction (Ultron owns those), or suit materials (`contrib/vison/suits/`).

| File | What it is |
|---|---|
| `gen_camera_realism.py` | Generator and single source of truth. It reads `../lighting_presets.json` and `../postprocess_realism.json` so EV100, white balance, histogram bounds and cine bodies are copied, not re-typed. |
| `camera_realism.json` | Bodies, lens sets, shot types, cutscene packages per environment, exposure, lens effects, DOF, motion blur, per-tier summary |
| `color_grade.json` | Tonemapper (SDR/HDR), white balance per environment, 5 grades, composed per-environment values, LUT spec |
| `camera_shake.json` | 10 shakes plus a calibration shake, in real units (cm, deg, Hz). Includes falloff, accessibility settings and ArenaFeel equivalents. |
| `textures/` | 256x16 neutral and subtle teal-orange LUT strips; anamorphic streak and 9-blade flare bokeh shapes |
| `apply_camera_realism.py` | Editor script. Supports `--dry-run`. Applies to the PPV and CineCameras, creates shake Blueprints and imports textures. Missing assets are skipped with a warning. |
| `validate_camera.py` | Runs about 530 consistency checks, including py_compile of every script in this folder |

Regenerate the data and validate:

```
python3 gen_camera_realism.py && python3 validate_camera.py
```

Editor examples:
- `py apply_camera_realism.py --env city --tier High --apply-ppv --dry-run`
- `... --env ch5_titan --apply-cine --shot close`
- `... --create-shakes --import-textures`

## Physical model

- **Scene EV100** = log2(lux / 2.5), taken from lighting_presets.
- **Camera EV100** = log2(N²/t) − log2(ISO/100). ND is folded into the effective ISO, the same convention as lighting_presets.
  - Every cutscene package exposes within 0.03 EV of its scene. The validator enforces ±0.1.
- **Shutter:** t = (angle/360)/fps, and MotionBlurAmount = angle/360.
  - Gameplay uses 180° at 60 fps (1/120 s; MotionBlurTargetFPS 60, so blur length doesn't stretch when dynamic resolution or frame rate dips).
  - Cutscenes use 180° at 24 fps (1/48 s). Ruins and titan use 90° (1/96 s) for combat staccato.
- **Gameplay FOV maps to lens units.** The user's horizontal FOV stays authoritative, expressed as a full-frame equivalent f = 18 / tan(hFOV/2):

  | View | hFOV | FF focal |
  |---|---|---|
  | Third person | 90° | 18 mm |
  | Aim (×0.8) | 72° | 24.8 mm |
  | Helmet POV | 95° | 16.5 mm |

  At these focal lengths and f/4 or slower, the hyperfocal distance is about 3 m. A real lens would show everything past the suit sharp, so **gameplay DOF is off**. That is the realistic choice, not a shortcut. An optional aim DOF on Epic only blurs geometry closer than about 3 m.
- **Cutscene bodies** match lighting_presets: S35 (prologue, ruins, city_night), ALEXA LF (workshop, workshop_night, overcast), 2× anamorphic (rooftops, storm), and IMAX 70 (city, titan, finale).
  - StoryDirector's shot focal lengths (20/22/35/50/60/70) assume S35 framing. `cutscene_packages.*.story_shots` gives f × (SensorWidth × squeeze / 24.89), snapped to the body's prime set. For example, IMAX medium 35 mm becomes 110 mm.
  - The IMAX ECU (198 mm) is outside the 40–150 set and is flagged.
- **Exposure:**
  - Gameplay uses the histogram with Min/Max EV100 from lighting_presets, HistogramLog −2…18, and Bilateral local exposure. Fusion is used only in Epic cutscenes; verify it in 5.8.
  - A workshop→outdoors door volume uses Min 6.41 / Max 17.05 and SpeedUp 2.5, giving about 3 s for 7.6 stops. SpeedDown 1.0 gives about 7.6 s back to dark.
  - An optional bias curve keeps night dark.
  - Gameplay manual triangles (photo mode) are provided per environment. Example: city is f/8, 1/120 s, ISO 16 effective, which is ISO 800 with 5.6 stops of ND.
- **Lens effects:**
  - Vignette: cos⁴ falloff. Gameplay = min(env × 0.6, 0.3); cutscenes use the env value; helmet POV is 0.
  - Grain: shadow-weighted digital-noise model. Gameplay ≤ 0.03 (night ≤ 0.05). Cutscene grain follows native ISO (ISO 800 → 0.05, ISO 1600 → 0.08).
  - Bloom: SOG with threshold −1 (energy-conserving) on all tiers; FFT only in Epic cutscenes (copied from the realism layer, BloomGaussianIntensity 0).
  - Flares: minimal (gameplay = min(env, 0.4) × 0.5). The anamorphic streak option applies to the rooftops environments.
  - Lens distortion: Brown-Conrady coefficients are specified per lens set but are **off by default**.
- **Grades** are deltas on each environment's lighting_presets grade. Composed results are clamped so they stay subtle: gain within 0.95–1.05, offset within ±0.006, saturation ≤ 1, and contrast ≤ max(base, 1.12).
  - Defaults per environment: neutral (prologue, workshop, city, ruins), teal-orange (rooftops, titan, finale), night (the three night presets), overcast (city_overcast).
  - Combat is an overlay PPV with weight 0..1 (fade in 0.6 s, out 2.5 s).
  - Tonemapper: **Filmic** for SDR, which keeps the authored Film* curve. **StandardACES** (ACES 2.0, PaperWhite 203) for HDR output.
- **Shakes:**
  - Peak values: landing 1.6° and −7 cm; heavy hit 1.8°/2.2° roll; explosion 1.4° with a 3 Hz rumble. Falloff uses inner R, outer 6R + 400 cm, exponent 1.6, and a speed-of-sound delay.
  - All gameplay frequencies are ≤ 20 Hz (60 fps / 3, so nothing aliases into a slow wobble), except the unibeam at ≤ 30 Hz. All rotations are ≥ 0.023° (0.5 px).
  - Accessibility: off 0 / reduced 0.35 (roll ×0.3, FOV 0, loops off) / standard 1 / intense 1.25. First-person rotation is ×0.6.

## GPU cost per tier (ESTIMATES, RTX 2070 SUPER at 1080p, not measured)

| Tier | Gameplay camera layer | Cutscene total | Notes |
|---|---|---|---|
| Epic | ~1.15 ms (+0.5 ms with optional aim DOF) | ~4.25 ms | FFT bloom ~2.0, Fusion ~0.4, DOF ~0.7 |
| High | ~0.91 ms | ~1.31 ms | Fits the existing post budget lines (bloom 0.25 + motion blur 0.3 + tonemap/grain/flare/LE 0.3, about 0.85 ms). Net +~0.06 ms. |
| Medium | ~0.75 ms | ~1.0 ms | Half-res motion blur gather, MotionBlurPerObjectSize 0.5 |

The lens distortion option would add about 1.0–1.4 ms at High (5% overscan plus one full-screen pass), which is why it is off. Shakes are CPU-only and cost under 0.01 ms.

## Conflicts and questions

**Project / code owner (Jarvis):**
1. **Chromatic aberration is disabled everywhere.** The live `DefaultScalability.ini` sets `r.SceneColorFringeQuality=0` on all tiers, and `ContribLighting::ApplyPhotorealGrade` forces SceneFringeIntensity 0 for cine cameras too.
   - Gameplay CA here is 0 (consistent). The cutscene CA spec (0.10–0.25) is inert unless you allow it for cinematics.
   - The same function clamps grain to ≤0.08/0.15, bloom ≤0.6, flare ≤0.4 and saturation ≤1. Several of *my* lighting_presets values exceed those (titan grain 0.2, flare up to 1.0, city_night bloom 0.8, rooftops saturation 1.05). The runtime clamps win. The camera layer stays inside them.
2. **StoryDirector focal lengths vs filmbacks.** `ApplyCamera` swaps in IMAX/LF/anamorphic filmbacks, but `UpdateCamera` sets S35 focal lengths every frame. On IMAX that is 2.8× wider than intended. Suggested fix: multiply by `focal_scale_vs_s35`, or use `story_shots.focal_prime_mm`.
3. **No landing shake hook.** There is no Landed() trauma, so `superhero_landing` needs one call.
4. **Shakes have three drivers.** ArenaFeel trauma, StoryDirector's sine handheld, and these CameraShake assets would stack. Pick one: either route AddTrauma to StartCameraShake, or retune trauma with the `arenafeel_trauma_equivalent` values.
5. **Shake intensity setting.** ArenaGameUserSettings only has booleans (bCameraShake, bHudReducedMotion). I propose a float CameraShakeIntensity (0–150%).
6. **Camera-rotation motion blur off in aim and helmet POV** needs C++: `FSceneView.bCameraMotionBlur=false` in a custom UGameViewportClient. Until then the layer uses MotionBlurAmount ×0.7.
7. **Third-person FOV.** TP = FP − 5 gives 90°, which is wide for a 4.2 m chase and stretches the suit at the edges. I suggest FP − 10 (85°).
8. **Lens distortion plugin.** The Lens Distortion / CameraCalibrationCore plugin is not in `IronManArena.uproject`. Enabling it is a project change. The ST-map material is the plugin-free alternative.
9. **Supersonic shake can't trigger.** Flight tops out around 29 m/s (Mach 0.085), so `supersonic_flight` never fires in gameplay. It is for cutscenes or a future high-speed mode.
10. **`r.DefaultFeature.AutoExposure.Bias=1.0`** in DefaultEngine applies wherever no PPV overrides the bias (menus, test maps). Every preset overrides it to 0.
11. **SuitUpCinematic is hard-coded.** It sets vignette 0.55, and the Ultron style sets fringe 1.5, grain 0.6 and bias −2…−0.5. It is stylised and outside this layer.
12. **Runtime JSON.** ContribLighting loads `Content/Story/postprocess_realism.json`. If runtime use of the camera layer is wanted, copy `camera_realism.json`, `color_grade.json` and `camera_shake.json` there too. I did not do this because it is outside my folder.
13. **TonemappingMethod.** If HDR output ships, a settings toggle has to switch to StandardACES. Film* values are ignored under ACES; the grades still apply.
14. **Shake frequency units: verified (2026-10-30, Jarvis, UE 5.8 source).** `FWaveOscillator::Update` does `offset += dt * Frequency * 2π`, so WaveOscillator Frequency is already **Hz**: the factor is now 1.0 in `camera_shake.json` (was 2π, which would have run every wave shake 6.3× too fast). `FPerlinNoiseShaker::Update` does `offset += dt * Frequency` into `PerlinNoise1D` (unit lattice), so the Hz/0.6 heuristic stays. The game runtime (ArenaFeel) uses the same conventions.

**Ultron (world, VFX, destruction). These are overlaps I did not author:**
- **Explosion shake:** `ArenaBlastFX` needs to pass the epicenter and blast radius to `PlayWorldCameraShake` (inner R, outer 6R + 400 cm). Today it only calls `AddBlastTrauma`.
- **Landing VFX:** the ground-impact decal and dust should fire on the same frame as `superhero_landing`.
- **Flashes:** lightning (rooftops_storm) and big blasts. AutoExposureHighPercent 98.3 ignores small flashes. Screen-filling flashes pump about 0.5 EV at combat SpeedUp 2.0. Is that acceptable, or do you want to author the flash as a light rather than emissive?
- **Bloom and flare thresholds** assume physically scaled emissives (LensFlareThreshold 8). Please keep VFX emissive values in calibrated nits/EV so flares don't trigger on every spark.
- **Perf budget:** Ultron's perf_budget lists 0.9 ms for post. The camera layer at High is about 0.91 ms gameplay. FFT, Fusion and DOF are cutscene-only.
- **True anamorphic streaks** from VFX sprites would be VFX, which is Ultron's domain. This layer only provides the image-based option.

**EDITH (HUD):** the visor already applies barrel warp, a 1.5 px chromatic split, 0.02 noise and a warning vignette. Helmet POV therefore sets scene vignette and CA to 0, halves grain, and never applies lens distortion, so the effects don't double.
