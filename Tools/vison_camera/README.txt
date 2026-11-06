Vison camera realism layer (merged 2026-10-30). Source of truth: gen_camera_realism.py.
It reads ../lighting_presets.json and ../postprocess_realism.json, so run it from a copy next to them
(contrib/vison/camera/ in the workspace; the same files are Content/Story/lighting_presets.json and postprocess_realism.json here).
Outputs are copied to Content/Story/{camera_realism,color_grade,camera_shake}.json (runtime) and Content/Story/camera_textures/.
WaveOscillator frequency factor fixed to 1.0 (UE 5.8 uses Hz); see camera_notes.md #14.
