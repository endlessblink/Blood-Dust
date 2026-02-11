# Game Designer: Blood & Dust Playable Demo

Build the Blood & Dust level into a playable demo through phased MCP orchestration with interactive checkpoints between phases.

## Trigger Phrases
- "game designer"
- "build demo"
- "make it playable"
- "playable demo"

## Prerequisites
- UnrealMCP server connected (TCP 127.0.0.1:55557)
- Landscape exists in level
- Robot skeletal mesh imported at `/Game/Characters/Robot/SK_Robot`
- BP_RobotCharacter exists at `/Game/Characters/Robot/BP_RobotCharacter`
- ABP_Robot exists at `/Game/Characters/Robot/ABP_Robot`

## CRITICAL RULES

1. **ALL MCP calls strictly SEQUENTIAL** — never parallel. Each creates FTSTicker callback; parallel = crash.
2. **Breathing room** between heavy operations — call `get_actors_in_level` or `list_assets` as a lightweight pause.
3. **Prompt user to Ctrl+S** after each major phase. Unsaved packages accumulate in RAM.
4. **Ask permission before each phase** — user may want to skip, reorder, or stop.
5. **Take screenshot after each visual phase** — before/after comparison proves the change.
6. **Max 2 texture imports, then pause** with lightweight query before importing more.
7. **Never batch >3 spawn operations** in rapid succession. Spread heavy spawning.

---

## Phase 0: Audit & Interview (~5 min, 5-8 MCP calls)

### Step 1: Capture Current State

```
get_actors_in_level()
```
Verify: landscape exists, robot BP exists, lighting actors present.

```
take_screenshot()
```
Save as "before" reference.

### Step 2: Interview — Ask All Questions Upfront

Use `AskUserQuestion` tool with these 6 questions:

**Q1: Time of Day** (header: "Lighting")
- "Golden Hour sunset (Recommended)" — Low-angle warm light, long shadows
- "High Noon harsh" — Overhead dramatic, deep shadows
- "Overcast moody" — Flat diffuse, grey atmosphere
- "Night / Moonlit" — Blue-silver tones, dark

**Q2: Backdrop Style** (header: "Horizon")
- "2D painted backdrops from Godot assets (Recommended)" — Import existing paintings
- "Procedural gradient planes" — Dark sky → golden horizon emissive meshes
- "None / Skip" — Keep current sky

**Q3: World Structures** (header: "Buildings")
- "Scattered ruins (Recommended)" — Broken walls, lone towers
- "Castle fortress" — Full fortress structure
- "Small town" — Houses and streets
- "Bare battlefield" — No structures, open terrain

**Q4: NPC Count** (header: "NPCs")
- "3-5 enemy robots (Recommended)" — Light population
- "8-12 enemies" — Medium population
- "None / Skip" — No NPCs yet

**Q5: Combat System** (header: "Combat")
- "Simple melee (Recommended)" — Punch deals damage, NPCs have health
- "Visual only" — NPCs present but no damage system
- "Skip combat" — No combat this session

**Q6: Which Phases to Run** (header: "Scope", multiSelect: true)
- "Phase 1: Character Controls" — WASD, mouse look, jump
- "Phase 2: Rembrandt Lighting"
- "Phase 3: Post-Processing"
- "Phase 4: Backdrops"
- "Phase 5: World Building"
- "Phase 6: NPCs"
- "Phase 7: Combat"

### Decision Tree
Map answers to phase configuration. Store choices for later phases.

---

## Phase 1: Character Playability (~15 min, ~35 MCP calls)

### Goal
WASD movement, mouse camera look, and jump working on BP_RobotCharacter.

### Approach
**Delegate to `/unreal-character-input` skill, Phase 2** (Enhanced Input wiring).

Pre-check: read BP to see if input is already wired:
```
read_blueprint_content(blueprint_path="/Game/Characters/Robot/BP_RobotCharacter")
```

If IA_Move/IA_Look/IA_Jump enhanced input action nodes exist but are NOT connected to movement functions, wire them. If already wired, skip.

### Wiring Sequence (if needed)

#### Jump (5 calls)
1. `add_node` CallFunction "Jump" → capture node_id
2. `connect_nodes` IA_Jump.Started → Jump.execute
3. `add_node` CallFunction "StopJumping" → capture node_id
4. `connect_nodes` IA_Jump.Completed → StopJumping.execute
5. `compile_blueprint` — verify before continuing

#### Camera Look (14 calls for IA_Look + IA_MouseLook)
For each look action:
1. `add_node` CallFunction "AddControllerYawInput"
2. `connect_nodes` EnhancedInput.Triggered → Yaw.execute
3. `connect_nodes` EnhancedInput.ActionValue_X → Yaw.Val
4. `add_node` CallFunction "AddControllerPitchInput"
5. `connect_nodes` Yaw.then → Pitch.execute
6. `connect_nodes` EnhancedInput.ActionValue_Y → Pitch.Val
7. `compile_blueprint`

#### Movement (13 calls)
1. `add_node` CallFunction "GetControlRotation"
2. `add_node` CallFunction "GetRightVector"
3. `connect_nodes` GetControlRotation.ReturnValue → GetRightVector.InRot
4. `add_node` CallFunction "GetForwardVector"
5. `connect_nodes` GetControlRotation.ReturnValue → GetForwardVector.InRot
6. `add_node` CallFunction "AddMovementInput" (right strafe)
7. `connect_nodes` IA_Move.Triggered → AddMovementInput_Right.execute
8. `connect_nodes` GetRightVector.ReturnValue → AddMovementInput_Right.WorldDirection
9. `connect_nodes` IA_Move.ActionValue_X → AddMovementInput_Right.ScaleValue
10. `add_node` CallFunction "AddMovementInput" (forward)
11. `connect_nodes` AddMovementInput_Right.then → AddMovementInput_Forward.execute
12. `connect_nodes` GetForwardVector.ReturnValue → AddMovementInput_Forward.WorldDirection
13. `connect_nodes` IA_Move.ActionValue_Y → AddMovementInput_Forward.ScaleValue

#### Final Compile
```
compile_blueprint(blueprint_name="/Game/Characters/Robot/BP_RobotCharacter")
```

### Checkpoint
> "Phase 1 complete. Character input wired. Press **Play (Alt+P)** to test WASD movement, mouse look, and jump. Press **Ctrl+S** to save. Ready for Phase 2?"

---

## Phase 2: Rembrandt Lighting & Atmosphere (~10 min, ~25 MCP calls)

### Goal
Transform flat default lighting into dramatic Rembrandt chiaroscuro.

### Step 1: Screenshot Before
```
take_screenshot()
```

### Step 2: Directional Light (Sun) — Golden Low-Angle

Actor: `DirectionalLight_UAID_F4A475FF15A3736A02_1961932697`

```
set_actor_property(
    actor_name="DirectionalLight_UAID_F4A475FF15A3736A02_1961932697",
    property_name="LightColor",
    property_value={"R": 0.702, "G": 0.447, "B": 0.2, "A": 1.0}
)
```
Golden ochre: RGB derived from #B37233.

```
set_actor_property(
    actor_name="DirectionalLight_UAID_F4A475FF15A3736A02_1961932697",
    property_name="Intensity",
    property_value=8.0
)
```
High intensity for dramatic shadows. Lumen handles the rest.

```
set_actor_transform(
    actor_name="DirectionalLight_UAID_F4A475FF15A3736A02_1961932697",
    location={"X": 0, "Y": 0, "Z": 0},
    rotation={"Pitch": -15, "Yaw": -45, "Roll": 0},
    scale={"X": 1, "Y": 1, "Z": 1}
)
```
Low angle (-15 pitch) for long dramatic shadows. Yaw -45 for 3/4 angle.

**If user chose "High Noon"**: Pitch=-75, Intensity=12.0
**If user chose "Overcast"**: Intensity=2.0, LightColor={"R":0.6,"G":0.6,"B":0.65,"A":1}
**If user chose "Night"**: Intensity=0.5, LightColor={"R":0.3,"G":0.35,"B":0.5,"A":1}, Pitch=-30

### Step 3: Exponential Height Fog — Warm Ochre Atmosphere

Actor: `ExponentialHeightFog_UAID_F4A475FF15A3736A02_1961936700`

```
set_actor_property(
    actor_name="ExponentialHeightFog_UAID_F4A475FF15A3736A02_1961936700",
    property_name="FogDensity",
    property_value=0.02
)
```

```
set_actor_property(
    actor_name="ExponentialHeightFog_UAID_F4A475FF15A3736A02_1961936700",
    property_name="FogHeightFalloff",
    property_value=0.2
)
```

```
set_actor_property(
    actor_name="ExponentialHeightFog_UAID_F4A475FF15A3736A02_1961936700",
    property_name="FogInscatteringColor",
    property_value={"R": 0.55, "G": 0.43, "B": 0.27, "A": 1.0}
)
```
Warm ochre inscattering — #8C6D46 derived.

```
set_actor_property(
    actor_name="ExponentialHeightFog_UAID_F4A475FF15A3736A02_1961936700",
    property_name="VolumetricFog",
    property_value=true
)
```

```
set_actor_property(
    actor_name="ExponentialHeightFog_UAID_F4A475FF15A3736A02_1961936700",
    property_name="VolumetricFogScatteringDistribution",
    property_value=0.7
)
```
Forward scattering to catch sun direction.

### Step 4: Sky Atmosphere — Dark Stormy with Golden Horizon

Actor: `SkyAtmosphere_UAID_F4A475FF15A3736A02_1961927691`

Note: SkyAtmosphere has limited directly-settable properties via reflection. The most impactful change is via PostProcess (Phase 3) atmospheric tinting.

If `get_actor_properties` reveals settable params, adjust:
- `RayleighScattering` — shift toward warm
- `MieScatteringColor` — golden tint

Otherwise, skip sky atmosphere direct modification and handle via post-process color grade.

### Step 5: Screenshot After
```
take_screenshot()
```

### Checkpoint
> "Phase 2 complete. Lighting changed to Rembrandt golden chiaroscuro. Compare before/after screenshots. Press **Ctrl+S** to save. Ready for Phase 3?"

---

## Phase 3: Post-Processing Color Grade (~8 min, ~15 MCP calls)

### Goal
Oil painting color grade — desaturated, high contrast, golden gain, vignette.

Actor: `PostProcessVolume_UAID_F4A475FF15A3736A02_1961895679`

### Step 1: Desaturation

```
set_actor_property(
    actor_name="PostProcessVolume_UAID_F4A475FF15A3736A02_1961895679",
    property_name="Settings.ColorSaturation",
    property_value={"X": 0.75, "Y": 0.72, "Z": 0.65, "W": 1.0}
)
```
Slightly desaturated with blue channel pulled most (warm bias). W=1 is global multiplier.

### Step 2: Contrast

```
set_actor_property(
    actor_name="PostProcessVolume_UAID_F4A475FF15A3736A02_1961895679",
    property_name="Settings.ColorContrast",
    property_value={"X": 1.15, "Y": 1.15, "Z": 1.1, "W": 1.0}
)
```
Increased contrast for dramatic light/shadow separation.

### Step 3: Color Gain (Golden Bias)

```
set_actor_property(
    actor_name="PostProcessVolume_UAID_F4A475FF15A3736A02_1961895679",
    property_name="Settings.ColorGain",
    property_value={"X": 1.05, "Y": 0.98, "Z": 0.85, "W": 1.0}
)
```
Warm golden gain — boost red/green, reduce blue.

### Step 4: Tone Curve

```
set_actor_property(
    actor_name="PostProcessVolume_UAID_F4A475FF15A3736A02_1961895679",
    property_name="Settings.FilmSlope",
    property_value=0.78
)
```

```
set_actor_property(
    actor_name="PostProcessVolume_UAID_F4A475FF15A3736A02_1961895679",
    property_name="Settings.FilmToe",
    property_value=0.6
)
```

```
set_actor_property(
    actor_name="PostProcessVolume_UAID_F4A475FF15A3736A02_1961895679",
    property_name="Settings.FilmShoulder",
    property_value=0.2
)
```
Low slope + high toe = crushed blacks with smooth roll-off. Oil painting look.

### Step 5: Bloom & Vignette

```
set_actor_property(
    actor_name="PostProcessVolume_UAID_F4A475FF15A3736A02_1961895679",
    property_name="Settings.BloomIntensity",
    property_value=0.4
)
```

```
set_actor_property(
    actor_name="PostProcessVolume_UAID_F4A475FF15A3736A02_1961895679",
    property_name="Settings.VignetteIntensity",
    property_value=0.6
)
```
Moderate vignette focuses attention center-frame, like a painting.

### Step 6: Auto Exposure

```
set_actor_property(
    actor_name="PostProcessVolume_UAID_F4A475FF15A3736A02_1961895679",
    property_name="Settings.AutoExposureBias",
    property_value=-0.5
)
```
Slightly darker exposure for moody atmosphere.

### Step 7: Screenshot After
```
take_screenshot()
```

### Checkpoint
> "Phase 3 complete. Oil painting post-processing applied — desaturated, high contrast, golden gain, vignette. Compare with Phase 2 screenshot. Press **Ctrl+S** to save. Ready for Phase 4?"

---

## Phase 4: Backdrop Billboards (~20 min, ~45 MCP calls)

### Goal
Panoramic horizon — either imported 2D paintings or procedural gradient planes at extreme distance.

### Option A: Import Godot Backdrop Paintings

Source files:
- `godot/blood&dust/assets/Backdrop/backdrop_n.png` (North)
- `godot/blood&dust/assets/Backdrop/backdrop_s.png` (South)
- `godot/blood&dust/assets/Backdrop/backdrop_e.png` (East)

For each backdrop direction:
1. `import_texture` — import painting to `/Game/Textures/Backdrops/T_Backdrop_N`
2. **Pause** with `list_assets` (max 2 imports before pause)
3. Create unlit emissive material:
   ```
   create_material_asset(name="M_Backdrop_N", path="/Game/Materials/Backdrops/")
   add_material_expression(material_path="...", expression_type="TextureSample", ...)
   connect_to_material_output(... pin="EmissiveColor")
   ```
   Set material blend mode to Translucent or Unlit.
4. `spawn_actor` — large plane mesh at distance
   - North: location Y=-80000, facing south (Yaw=0)
   - South: location Y=80000, facing north (Yaw=180)
   - East: location X=80000, facing west (Yaw=-90)
   - West: location X=-80000, facing east (Yaw=90)
   - Scale: X=500, Y=1, Z=200 (massive billboard)
5. `apply_material_to_actor`
6. `snap_actor_to_ground` or set Z manually for horizon alignment

### Option B: Gradient Emissive Planes

No texture import needed. Create a gradient material:
1. `create_material_asset` — unlit material
2. Use WorldPosition Z → normalize → lerp between dark sky color and golden horizon
3. Spawn 4 large planes at cardinal directions

### Checkpoint
> "Phase 4 complete. Backdrop billboards placed at 4 cardinal directions. Press **Ctrl+S** to save. Ready for Phase 5?"

---

## Phase 5: World Building (~15-30 min, 10-50 MCP calls)

### Goal
Place structures based on user's Phase 0 choice.

### Option: Scattered Ruins
```
get_height_at_location(x=2000, y=1500)  // Find ground height
create_wall(location=[2000, 1500, Z], length=800, height=400, thickness=60, material="/Game/Materials/Rocks/M_Rock_A")
create_wall(...)  // 3-5 broken wall segments
create_tower(location=[...], radius=200, height=600, segments=8)  // Lone watchtower
```
Place 4-6 ruin elements spread across the landscape. Use `get_height_at_location` for each to sit on terrain.

### Option: Castle Fortress
```
create_castle_fortress(
    location=[0, 5000, Z],
    outer_radius=3000, wall_height=800,
    num_towers=4, tower_radius=300,
    has_gate=true, has_keep=true
)
```
Single dramatic fortress. Place off-center for visual interest.

### Option: Small Town
```
create_town(
    center_location=[0, 3000, Z],
    num_buildings=8,
    town_radius=2000,
    building_size_min=300,
    building_size_max=600
)
```

### Option: Bare Battlefield
Skip this phase entirely.

### Checkpoint
> "Phase 5 complete. Structures placed in the level. Press **Ctrl+S** to save. Ready for Phase 6?"

---

## Phase 6: NPCs (~10-40 min)

### 6a: Static NPC Placement (~10 min, ~20 MCP calls)

Spawn copies of BP_RobotCharacter as enemy robots:

For each NPC position:
```
get_height_at_location(x=X, y=Y)
spawn_blueprint_actor_in_level(
    blueprint_path="/Game/Characters/Robot/BP_RobotCharacter",
    actor_name="NPC_Robot_1",
    location=[X, Y, Z+90],
    rotation=[0, YAW_FACING_CENTER, 0]
)
```

Suggested positions (spread around center):
- NPC_Robot_1: X=3000, Y=2000 (near ruins if placed)
- NPC_Robot_2: X=-2000, Y=4000 (flanking)
- NPC_Robot_3: X=1000, Y=-3000 (sentry)
- NPC_Robot_4: X=-4000, Y=-1000 (patrol start)
- NPC_Robot_5: X=5000, Y=5000 (distant lookout)

Adjust count based on user's Phase 0 choice.

### 6b: NPC Patrol (Optional, ~30-45 min per NPC)

**WARNING**: This is complex Blueprint work. Only attempt if user specifically requested it.

For each patrolling NPC:
1. Create `BP_NPC_Robot` as a child of BP_RobotCharacter (or standalone Character BP)
2. Add variables: `PatrolPointA` (Vector), `PatrolPointB` (Vector), `PatrolSpeed` (float), `bMovingToB` (bool)
3. In EventGraph, on Tick:
   - Get current location
   - If near target point, flip `bMovingToB`
   - `AddMovementInput` toward current target
4. Spawn BP_NPC_Robot instances with patrol point variables set

This requires ~40-60 sequential MCP calls per NPC. Only create 1-2 patrolling NPCs.

### Checkpoint
> "Phase 6 complete. Enemy NPCs placed in the level. Press **Ctrl+S** to save. Ready for Phase 7?"

---

## Phase 7: Combat Foundation (~30-60 min, 80-120 MCP calls)

### Goal
Basic melee combat: player punch deals damage to NPCs, NPCs with health that can "die" (be destroyed).

**WARNING**: This is the most complex phase. Blueprint graph wiring is fragile via MCP.

### Step 1: Add Health Variable to NPC Blueprint

If using BP_RobotCharacter for NPCs:
```
create_variable(
    blueprint_path="/Game/Characters/Robot/BP_RobotCharacter",
    variable_name="Health",
    variable_type="float",
    default_value="100.0"
)
```

Or create a separate BP_NPC_Robot blueprint with Health.

### Step 2: Damage System Concept

**Simplest approach**: Box collision on player's hand bone socket → OnOverlap → reduce NPC health

1. Add BoxCollision component to BP_RobotCharacter (attack hitbox)
2. On attack input → enable collision briefly
3. OnComponentBeginOverlap → if overlapping actor has Health variable → reduce it
4. If Health <= 0 → DestroyActor on NPC

**CAVEAT**: Full damage system requires ~80+ Blueprint nodes. Consider implementing via C++ for reliability.

### Alternative: Visual-Only Combat
Skip damage calculation. Just play attack animation when LMB pressed. NPCs stand still. This is the safe option for a demo.

### Checkpoint
> "Phase 7 complete. Basic combat system in place. Press **Ctrl+S** to save. Take a final screenshot for the demo reel!"

---

## Manual Phases Checklist (Cannot Be Done via MCP)

After automated phases, inform the user about manual editor work:

| Feature | What to Do Manually | Where |
|---------|-------------------|-------|
| M1: Fire/Smoke FX | Add Niagara particle at torch locations | Niagara Editor |
| M2: Health Bar HUD | Create UMG Widget BP, bind to Health var | Widget Blueprint |
| M3: Audio | Import WAV/OGG, add AudioComponents | Content Browser |
| M4: AI Behavior | Create Behavior Tree for NPC patrol/combat | BT Editor |
| M5: Game Mode | Set Default Pawn to BP_RobotCharacter | Project Settings > Maps & Modes |
| M6: Player Start | Place PlayerStart actor in level | Level Editor |

Provide variable names and asset paths so manual work is easy.

---

## Troubleshooting

### "Actor not found" error
Actor names include UAID suffixes. Run `get_actors_in_level()` to get exact names. Names change if actors are re-created.

### Post-process property not taking effect
The `set_actor_property` handler auto-sets `bOverride_X` flags. If property still doesn't work, the PostProcessVolume may have `bUnbound=false` (limited extent). Check with `get_actor_properties`.

### Lighting looks wrong after Phase 2
Take a screenshot and compare. Common issues:
- Auto-exposure fighting the dark look → Set `Settings.AutoExposureBias` to -1.0
- Sky too bright → Reduce sky light intensity or add second post-process pass

### Character not moving (Phase 1)
- Check that Enhanced Input actions exist at `/Game/Input/Actions/`
- Verify Input Mapping Context is assigned in BP defaults
- Test: `read_blueprint_content` to verify node connections

### Structures floating or underground (Phase 5)
Always use `get_height_at_location` before placing. Add capsule half-height offset (+90) for characters.

### NPC spawning crashes
- Never spawn more than 3 actors in rapid succession
- Use `get_actors_in_level` as breathing room between spawns
- Actor names must be unique — append _1, _2, etc.

---

## Timeline Reference

| Phase | Time | MCP Calls | Cumulative |
|-------|------|-----------|-----------|
| 0: Audit & Interview | 5 min | 5-8 | 5 min |
| 1: Character Controls | 15 min | ~35 | 20 min |
| 2: Rembrandt Lighting | 10 min | ~25 | 30 min |
| 3: Post-Processing | 8 min | ~15 | 38 min |
| **Milestone: Playable + Atmosphere** | | | **~40 min** |
| 4: Backdrops | 20 min | ~45 | 60 min |
| 5: World Building | 15-30 min | 10-50 | 90 min |
| **Milestone: Environment Complete** | | | **~90 min** |
| 6: NPCs | 10-40 min | 20-60 | 130 min |
| 7: Combat | 30-60 min | 80-120 | 210 min |
| **Milestone: Playable Demo** | | | **~3-3.5 hrs** |

## What CANNOT Be Done via MCP

| Feature | Why | Workaround |
|---------|-----|-----------|
| Niagara FX (fire, smoke) | No Niagara MCP tools | Place torch meshes, add FX manually |
| Health Bar HUD | No UMG/Widget tools | Create Widget BP manually, bind to Health var |
| Audio/sound | No audio MCP tools | Import audio manually, add AudioComponent |
| AI behavior trees | No BT MCP tools | Simple Blueprint patrol (Phase 6b) |
| Animation montages | No montage playback API | State machine transitions only |
| Physics-based combat | No physics impulse tools | Overlap-based damage only |

## Future C++ Tools Wishlist (Would Unlock More Phases)

- `create_niagara_system` — particle effects for torches, dust, blood
- `create_widget_blueprint` — HUD elements (health bar, crosshair)
- `play_animation_montage` — one-shot attack animations
- `create_behavior_tree` — AI patrol and combat behavior
- `add_audio_component` — ambient sound, SFX
- `set_game_mode_default_pawn` — auto-configure game mode
