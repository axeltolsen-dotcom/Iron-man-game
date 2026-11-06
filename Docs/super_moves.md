# Suit super moves (USuitSuperComponent)

Source: `Source/IronManArena/Private/SuitSuperComponent.cpp` (table = `USuitSuperComponent::Table()`).

**Trigger:** when the bottom-centre ring is full, HOLD Right-click for 0.3 s, or DOUBLE-click Left-click. (Gamepad: hold LT and click R3.)
A right-click *tap* still fires the suit's normal special. While a super runs, normal weapons are locked and the camera
moves to third person so the body animation can be seen. It goes back to the helmet view afterwards.

**How every move is built (research: flight_and_combat.md, suits_and_suitup.md):**
ANTICIPATION (wind-up pose) -> IMPACT beat (hit-stop 60-150 ms at 0.15-0.3x time, camera shake profile + trauma, heavy rumble,
VFX and SFX on the same frame, camera punch-in: spring arm pulls in and FOV narrows) -> FOLLOW-THROUGH -> RECOVERY (the pose
weight eases out over 0.3 s). The pose is keyed on `AIronManCharacter::PoseOverride` (root / root offset / shoulders / hips,
quaternion slerp + ease in-out, so nothing pops or T-poses). Root motion uses the capsule velocity (LaunchCharacter /
Velocity), so the feet never slide in place.

| Suit id(s) | Move | Animation (timeline) | VFX | Sound events | Cooldown |
|---|---|---|---|---|---|
| mark-lxxxv (flagship) | **Endgame: Nano Blades / Beam Stomp** (context: enemy inside 7 m = Nano Blades, otherwise Beam Stomp) | Stomp: 0-0.45 s leap with arms raised overhead and knees tucked; dive at -4200 cm/s; HERO BEAT = fist hits the ground in a kneel (punch-in to 1.9 m, FOV -10); 0.85-1.75 s chest unibeam sweep rising from the kneel; recovery stand | ground telegraph ring, blast flash, impact decal, sparks, debris; triple hitscan beam with tracer and sparks | unibeam_charge, suitup_servo_heavy, thruster_boost, explosion_large, melee_hit_heavy, unibeam_fire | 14 s |
| mark-lxxxv (near), mark-l, mark-xlvi, arachnid (far) | **Nanotech Blades** | blades grow out of both forearms (0.05-0.25 s, close-up punch-in); 3-slash combo: R diagonal, L diagonal, overhead finisher. Each slash = wind-up -> strike -> follow-through, with a lunge dash at the target; blades retract | glowing nano blades, slash trails | suitup_plate_lock, hb_deploy, melee_swish, melee_hit_heavy | 10 s |
| arachnid (flagship) | **Iron Spider: Waldoes** (target inside 5.2 m = STAB & PIN, otherwise SWEEP; no target inside 12 m = Nano Blades lunge) | 4 gold legs unfold from the back (servo); Stab: front pair impales at 0.46 s and pins the target in place, rear pair finishes at 0.8 s while the idle legs brace on the ground; Sweep: legs fan out and spin 2 turns with 3 hits; legs fold back | leg pivots + glowing tips, sparks | suitup_servo, punch, hit_metal, melee_swish, melee_hit_heavy | 9 s |
| mark-iii, mark-ii, mark-iv, mark-vi, mark-xvii, mark-xx, mark-xxii, mark-xxxiii, mark-xlv, ironheart, deep-space, mark-iii-alt | **Unibeam Overcharge** | 0-0.6 s charge: arms swing back, chest out, crouch, sparks drawn into the reactor; 0.62 s release: one big beam, recoil knockback; arms snap forward and settle | 95 cm beam tracer, muzzle flash, 380 cm blast | unibeam_charge, reactor_overheat_vent, unibeam_fire | 12-13 s |
| mark-vii, mark-xxi, mark-xxiii, mark-xl | **Shoulder-Missile Barrage** | shoulder pods rise and tilt (0.05-0.35 s), hop and hang in the air; 4 salvos of 3 homing micro-missiles (0.4 / 0.6 / 0.8 / 1.0 s), arms spread wide; pods close | pods, launch flashes, missile trails and blasts | suitup_servo, missile_launch, missile_explosion | 12-13 s |
| mark-xlii, mark-xliii, mark-xlvii, mark-xli-bones | **Remote-Piece Strike** | 6 armour pieces assemble behind you and hover; right arm points at the target; the pieces launch one by one (0.13 s apart), home in and explode on contact | flying pieces with tracer trails, blasts | suitup_servo, thruster_boost, hit_metal | 12 s |
| rescue, rescue-mk2, aurora, mark-xxvii | **Rescue Pulse** | arms cross over the chest while crouching (0-0.45 s), then throw them open (HERO BEAT): healing pulse, enemies blown back | ring telegraph, 700 cm pulse flash | unibeam_charge, unibeam_impact, pickup_health | 11 s |
| mark-xxxviii-igor, Hulkbuster (any suit while it is deployed), mark-i, mark-v, mark-xxv, valkyrie | **Ground Pound / Mega Stomp** | leap (1300 up) with both fists raised overhead; slam down at -5200; HERO BEAT = landing in a three-point kneel (radius 9.5 m, 13 m for big suits); rise | telegraph ring, big blast, decal, debris | suitup_servo_heavy, thruster_boost, explosion_large, hb_footstep_concrete | 11-14 s |
| war-machine, iron-patriot | **Minigun + Micro-Rocket Pod** | the shoulder minigun unfolds and spins up (0.1-0.35 s); HOLD = 18 rounds/s hitscan with a small climbing recoil (up to about 3 s); RELEASE = spin-down and an 8-rocket pod salvo (HERO BEAT) | spinning barrel, tracers, muzzle flashes, rockets | suitup_servo, wm_minigun_fire_loop, missile_launch | 13 s |
| mark-xv-sneaky, nightshade, mark-xvi, impulse | **Cloak Slash** | the suit flickers out (0.12-0.32 s), invisible blink behind the target (0.38 s) and turns to face it; reveal with a big back-hand slash (0.45 s), then recovery | flicker, slash trail | hud_low_power, thruster_boost, melee_swish, melee_hit_heavy | 9-10 s |
| (others) | fallback by special weapon: Missiles -> Barrage, Unibeam -> Overcharge, Hulkbuster -> Ground Pound, Cloak -> Cloak Slash | | | | 12 s |

## Iron Spider web swing
`B` toggles WEB SWING <-> FLIGHT (Iron Spider only). In swing mode, jump and hold **Space** in the air to shoot a web
up and ahead. The trace tries 4 directions out to 42 m; over open sky it uses a virtual anchor. While the web is attached
you swing on a pendulum: an inelastic rope constraint, W pumps along the tangent, the rope reels in slowly, speed is capped at 52 m/s,
and the right arm reaches up the web line while the legs tuck. The camera rolls into the arc (up to 12 degrees).
Releasing Space flings you (x1.12 speed, +4.2 m/s up). **R** = web-zip to the aimed point (up to 60 m).

## Faceplate
`H` (gamepad D-pad left) opens the faceplate in two stages: unlock (it drops and pops forward 0-0.25), then lifts over the head on the
Mark III hinge line (98 degrees, ease in-out, 0.55 s). The pilot's own avatar head (same skin, hair and face as the avatar creator)
shows inside, and the camera moves to third person with a punch-in. Press `H` again to close it: the eyes light up, helmet_close plays,
the HUD beeps on and the camera goes back to the helmet view.
