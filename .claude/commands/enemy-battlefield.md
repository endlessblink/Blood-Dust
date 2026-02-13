---
description: Compose animated enemy battlefield scenes with scaling, animations, and scene templates (duels, boss fights, patrols, melees)
---

# Animated Enemy Battlefield

<enemy-battlefield>

## Overview

Compose an animated enemy battlefield scene in Blood & Dust. Spawns and positions enemy characters with looping animations, scales them for dramatic effect, and arranges combat vignettes. Supports all three enemy types (KingBot, Giganto, Bell) plus MC-like robot combatants. Multiple scenes can be composed by running with different prefixes.

## Prerequisites

- UnrealMCP server running and connected to Unreal Editor
- Enemy Blueprints imported: BP_Bell, BP_KingBot, BP_Giganto
- Enemy skeletons + animations: `SK_{Type}`, `SK_{Type}_Skeleton`, `SK_{Type}_Anim`
- BP_RobotCharacter imported (for MC-like combatant NPCs)
- Landscape present in level (for ground height queries)

## CRITICAL RULES

1. **ALL MCP calls strictly SEQUENTIAL** — never parallel. Each creates FTSTicker callback; parallel = crash.
2. **Max 2 spawns then breathing room** — call `does_asset_exist()` or `get_height_at_location(0,0)` between every 2 heavy operations.
3. **`spawn_blueprint_actor_in_level` does NOT accept scale** — always use `set_actor_transform` AFTER spawning to set scale.
4. **`set_actor_transform` preserves unset fields** — you CAN set just scale without re-specifying location/rotation.
5. **NEVER call MCP tools while another Claude instance is also making MCP calls.** Only ONE command in-flight at any time.
6. **AnimToPlay CANNOT be set via `set_actor_property`** — `FObjectProperty` is unsupported. MUST use AnimBP approach instead.
7. **AnimationMode CAN be set via `set_actor_property`** — ByteProperty value `1` = `AnimationSingleNode`, value `0` = `AnimationBlueprint`.
8. **Ctrl+S reminder after major phases** — actor persistence requires manual save.
9. **Rotation format**: `[Pitch, Yaw, Roll]` in degrees. Yaw=0 faces +X, Yaw=90 faces +Y.
10. **Compile Blueprint before spawning** — uncompiled BPs spawn with default/broken state.

## Available Enemy Types

| Enemy | BP Path | Animation | Anim Style |
|-------|---------|-----------|------------|
| **KingBot** | `/Game/Characters/Enemies/KingBot/BP_KingBot` | `SK_KingBot_Anim` | Boxing punch (combat) |
| **Giganto** | `/Game/Characters/Enemies/Giganto/BP_Giganto` | `SK_Giganto_Anim` | Elderly shaky walk |
| **Bell** | `/Game/Characters/Enemies/Bell/BP_Bell` | `SK_Bell_Anim` | Slow orc walk |
| **Robot** (MC) | `/Game/Characters/Robot/BP_RobotCharacter` | `walking` / `long-kick` | Walk/kick cycle |

Each enemy type has assets at `/Game/Characters/Enemies/{Type}/`:
- `SK_{Type}` — Skeletal mesh
- `SK_{Type}_Skeleton` — Skeleton asset
- `SK_{Type}_Anim` — Animation sequence
- `T_{Type}_D` — Diffuse texture
- `M_{Type}` — Material
- `BP_{Type}` — Character Blueprint

Robot assets at `/Game/Characters/Robot/`:
- `SK_Robot`, `SK_Robot_Skeleton`, `ABP_Robot`
- Animations at `/Game/Characters/Robot/Animations/`: idle, walking, running, long-hit, long-kick, long-jump

## Scene Templates

### Template: DUEL

Two enemies of any type facing each other, looping combat/walk animations.
- Spacing: `pair_spacing` units apart (default 300)
- Both face each other (computed Yaw via `atan2`)
- Optional scale (e.g., 3x for Bell duels)
- 2 actors spawned

### Template: BOSS_FIGHT

One large enemy (e.g., KingBot at 3x) surrounded by 4-6 smaller enemies in a circle.
- Boss at center, scaled `boss_scale` (default 3x)
- Combatants arranged in circle, radius `circle_radius` units (default 500)
- All combatants face the boss (computed Yaw)
- 5-7 actors spawned

### Template: PATROL

Line of 3-5 enemies walking in formation (same direction).
- Spacing: `patrol_spacing` units between each (default 200)
- All face same direction (`patrol_yaw`, default 0)
- Mix of types for variety
- 3-5 actors spawned

### Template: MELEE

4-6 enemies clustered in a tight group, mixed types, random facing (simulates chaotic combat).
- Cluster radius: `melee_radius` units (default 250)
- Random Yaw per actor
- Random scale variation (0.9-1.3x)
- 4-6 actors spawned

## Arguments

Parse from user input:
```
/enemy-battlefield template=DUEL types=Bell,KingBot center=-2000,1000 scale=3
/enemy-battlefield template=BOSS_FIGHT boss=KingBot combatants=Robot,Robot,Robot,Robot center=-1000,500 boss_scale=3
/enemy-battlefield template=PATROL types=Giganto,Bell,KingBot,Bell center=-3000,2000 patrol_yaw=45
/enemy-battlefield template=MELEE types=Bell,KingBot,Giganto,Robot,Bell center=-500,300
/enemy-battlefield cleanup prefix=BF_  (removes all BF_* actors)
/enemy-battlefield existing scale=3 animate  (scale + animate pre-placed enemies)
```

## Configurable Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| scene_template | string | "DUEL" | Template: DUEL, BOSS_FIGHT, PATROL, MELEE |
| center_x | float | -2000 | Scene center X position |
| center_y | float | 1000 | Scene center Y position |
| enemy_types | list | ["Bell", "Bell"] | Enemy types to spawn (template-dependent) |
| scale | float | 1.0 | Base scale for spawned enemies |
| boss_type | string | "KingBot" | Boss enemy type for BOSS_FIGHT |
| boss_scale | float | 3.0 | Scale for boss in BOSS_FIGHT |
| combatant_types | list | ["Robot","Robot","Robot","Robot"] | Combatant types for BOSS_FIGHT |
| pair_spacing | float | 300 | Distance between duel opponents |
| circle_radius | float | 500 | Radius for BOSS_FIGHT circle |
| patrol_spacing | float | 200 | Distance between patrol members |
| patrol_yaw | float | 0 | Direction patrol faces (degrees) |
| melee_radius | float | 250 | Cluster radius for MELEE |
| prefix | string | "BF_" | Actor name prefix (for cleanup grouping) |
| skip_anim_setup | bool | false | Skip AnimBP creation if already done |

## Phase 0: Asset Verification (~6-9 MCP calls)

Verify all required assets exist before proceeding.

```
For each unique enemy type requested:
    does_asset_exist(path="/Game/Characters/Enemies/{Type}/BP_{Type}")
    does_asset_exist(path="/Game/Characters/Enemies/{Type}/SK_{Type}")
    does_asset_exist(path="/Game/Characters/Enemies/{Type}/SK_{Type}_Anim")

If "Robot" type requested:
    does_asset_exist(path="/Game/Characters/Robot/BP_RobotCharacter")
    does_asset_exist(path="/Game/Characters/Robot/SK_Robot")
    does_asset_exist(path="/Game/Characters/Robot/Animations/walking")
```

If any asset missing, STOP and inform user which assets need importing.

## Phase 1: AnimBP Setup (~4 MCP calls per unique enemy type)

**Skip this phase if `skip_anim_setup=true` or all ABP_BG_{Type} already exist.**

For each unique enemy type that needs animation, create an AnimBP if one doesn't exist.

**KEY TECHNIQUE**: Use `setup_locomotion_state_machine` with the SAME animation for both `idle_animation` and `walk_animation`. NPC never moves, so speed=0 forever. It stays in the Idle state permanently, looping the specified animation.

```
For each unique enemy type (KingBot, Bell, Giganto):

    # Check if background AnimBP already exists
    does_asset_exist(path="/Game/Characters/Enemies/{Type}/ABP_BG_{Type}")
    -> if exists, skip to next type

    # Create AnimBP targeting the enemy's skeleton
    create_anim_blueprint(
        blueprint_name="ABP_BG_{Type}",
        skeleton_path="/Game/Characters/Enemies/{Type}/SK_{Type}_Skeleton",
        blueprint_path="/Game/Characters/Enemies/{Type}/",
        preview_mesh_path="/Game/Characters/Enemies/{Type}/SK_{Type}"
    )

    # breathing room
    does_asset_exist(path="/Game/Characters/Enemies/{Type}/ABP_BG_{Type}")

    # Locomotion state machine - same anim for idle+walk = permanent loop
    setup_locomotion_state_machine(
        anim_blueprint_path="/Game/Characters/Enemies/{Type}/ABP_BG_{Type}",
        idle_animation="/Game/Characters/Enemies/{Type}/SK_{Type}_Anim",
        walk_animation="/Game/Characters/Enemies/{Type}/SK_{Type}_Anim"
    )

    # breathing room
    does_asset_exist(path="/Game/Characters/Enemies/{Type}/ABP_BG_{Type}")
```

**Robot type**: Use existing `ABP_Robot` at `/Game/Characters/Robot/ABP_Robot`. If a different NPC animation is needed, create `ABP_BG_Robot` with the `walking` or `long-kick` animation from `/Game/Characters/Robot/Animations/`.

### Checkpoint
> "Phase 1 complete. AnimBPs created/verified for {N} enemy types. Ready for Phase 2?"

## Phase 2: Configure Character Blueprints (~3 MCP calls per unique type)

Assign skeletal mesh and AnimBP to each enemy Blueprint so spawned instances are animated.

```
For each unique enemy type:

    set_character_properties(
        blueprint_path="/Game/Characters/Enemies/{Type}/BP_{Type}",
        skeletal_mesh_path="/Game/Characters/Enemies/{Type}/SK_{Type}",
        anim_blueprint_path="/Game/Characters/Enemies/{Type}/ABP_BG_{Type}"
    )

    compile_blueprint(blueprint_name="BP_{Type}")

    # breathing room
    does_asset_exist(path="/Game/Characters/Enemies/{Type}/BP_{Type}")
```

**WARNING**: `set_character_properties` modifies the shared BP. All existing instances of that BP in the level will use the new AnimBP after recompilation. If you need different AnimBPs for gameplay vs. battlefield, create a separate Blueprint variant first:
```
create_character_blueprint(
    blueprint_name="BP_BF_{Type}",
    blueprint_path="/Game/Characters/Enemies/{Type}/",
    skeletal_mesh_path="/Game/Characters/Enemies/{Type}/SK_{Type}",
    anim_blueprint_path="/Game/Characters/Enemies/{Type}/ABP_BG_{Type}",
    capsule_half_height=90, capsule_radius=40
)
compile_blueprint(blueprint_name="BP_BF_{Type}")
```
Then use `BP_BF_{Type}` instead of `BP_{Type}` in Phase 4 spawning. This keeps the original BP unchanged for gameplay use.

### Checkpoint
> "Phase 2 complete. BPs configured with AnimBPs. Press **Ctrl+S** to save. Ready for Phase 3?"

## Phase 3: Cleanup Previous Scene (~1-2 MCP calls)

Optional — only if replacing a previous battlefield scene.

```
delete_actors_by_pattern(pattern="{prefix}")
# e.g., delete_actors_by_pattern(pattern="BF_")

# breathing room
get_height_at_location(x=0, y=0)
```

## Phase 4: Spawn & Position (~3-4 MCP calls per actor)

### DUEL Template

```
# Scene math
center_x, center_y = <from parameters>
spacing = pair_spacing  # default 300
type_a, type_b = enemy_types[0], enemy_types[1]

# Fighter positions (offset along Y axis from center)
a_x = center_x
a_y = center_y - spacing / 2
b_x = center_x
b_y = center_y + spacing / 2

# Facing: A faces B (Yaw=90), B faces A (Yaw=-90)
a_yaw = atan2(b_y - a_y, b_x - a_x) in degrees  # = 90
b_yaw = atan2(a_y - b_y, a_x - b_x) in degrees  # = -90

# Get ground height
get_height_at_location(x=center_x, y=center_y)
-> record ground_z

# Spawn Fighter A
spawn_blueprint_actor_in_level(
    blueprint_path="/Game/Characters/Enemies/{type_a}/BP_{type_a}",
    actor_name="{prefix}Duel_{type_a}_A",
    location=[a_x, a_y, ground_z],
    rotation=[0, a_yaw, 0]
)

# breathing room
get_height_at_location(x=0, y=0)

# Scale Fighter A
set_actor_transform(
    name="{prefix}Duel_{type_a}_A",
    scale=[scale, scale, scale]
)

# Spawn Fighter B
spawn_blueprint_actor_in_level(
    blueprint_path="/Game/Characters/Enemies/{type_b}/BP_{type_b}",
    actor_name="{prefix}Duel_{type_b}_B",
    location=[b_x, b_y, ground_z],
    rotation=[0, b_yaw, 0]
)

# breathing room
get_height_at_location(x=0, y=0)

# Scale Fighter B
set_actor_transform(
    name="{prefix}Duel_{type_b}_B",
    scale=[scale, scale, scale]
)
```

### BOSS_FIGHT Template

```
center_x, center_y = <from parameters>
radius = circle_radius  # default 500
N = len(combatant_types)

# Get ground height at center
get_height_at_location(x=center_x, y=center_y)
-> record ground_z

# Spawn Boss at center
spawn_blueprint_actor_in_level(
    blueprint_path="/Game/Characters/Enemies/{boss_type}/BP_{boss_type}",
    actor_name="{prefix}Boss_{boss_type}",
    location=[center_x, center_y, ground_z],
    rotation=[0, 0, 0]
)

# Scale Boss
set_actor_transform(
    name="{prefix}Boss_{boss_type}",
    scale=[boss_scale, boss_scale, boss_scale]
)

# breathing room
get_height_at_location(x=0, y=0)

# Spawn combatants in circle around boss
For i in range(N):
    angle = (2 * pi * i) / N
    cx = center_x + radius * cos(angle)
    cy = center_y + radius * sin(angle)
    # Face toward boss
    yaw = atan2(center_y - cy, center_x - cx) in degrees
    type = combatant_types[i]

    # BP path depends on type
    bp_path = "/Game/Characters/Robot/BP_RobotCharacter" if type == "Robot"
              else "/Game/Characters/Enemies/{type}/BP_{type}"

    get_height_at_location(x=cx, y=cy)
    -> record combatant_z

    spawn_blueprint_actor_in_level(
        blueprint_path=bp_path,
        actor_name="{prefix}Fighter_{type}_{i+1}",
        location=[cx, cy, combatant_z],
        rotation=[0, yaw, 0]
    )

    # breathing room every 2 spawns
    if (i+1) % 2 == 0:
        get_height_at_location(x=0, y=0)
```

### PATROL Template

```
direction_yaw = patrol_yaw  # default 0 degrees
dx = cos(radians(direction_yaw)) * patrol_spacing
dy = sin(radians(direction_yaw)) * patrol_spacing

For i, type in enumerate(enemy_types):
    px = center_x + i * dx
    py = center_y + i * dy

    get_height_at_location(x=px, y=py)
    -> record pz

    spawn_blueprint_actor_in_level(
        blueprint_path="/Game/Characters/Enemies/{type}/BP_{type}",
        actor_name="{prefix}Patrol_{type}_{i+1}",
        location=[px, py, pz],
        rotation=[0, direction_yaw, 0]
    )

    # breathing room every 2 spawns
    if (i+1) % 2 == 0:
        get_height_at_location(x=0, y=0)
```

### MELEE Template

```
# Reproducible random layout
seed = 42

For i, type in enumerate(enemy_types):
    angle = seeded_random(0, 2*pi)
    dist = seeded_random(0, melee_radius)
    mx = center_x + dist * cos(angle)
    my = center_y + dist * sin(angle)
    yaw = seeded_random(-180, 180)
    scale_var = seeded_random(0.9, 1.3)

    get_height_at_location(x=mx, y=my)
    -> record mz

    spawn_blueprint_actor_in_level(
        blueprint_path="/Game/Characters/Enemies/{type}/BP_{type}",
        actor_name="{prefix}Melee_{type}_{i+1}",
        location=[mx, my, mz],
        rotation=[0, yaw, 0]
    )

    set_actor_transform(
        name="{prefix}Melee_{type}_{i+1}",
        scale=[scale_var, scale_var, scale_var]
    )

    # breathing room every 2 spawns
    if (i+1) % 2 == 0:
        get_height_at_location(x=0, y=0)
```

### Checkpoint
> "Phase 4 complete. {N} actors spawned and positioned. Press **Ctrl+S** to save. Ready for verification?"

## Phase 5: Scale & Animate Pre-Existing Actors (Optional)

For ALREADY-PLACED enemy actors in the level (not spawned by this skill) that need scaling and animation.

### Discover existing enemies

```
find_actors_by_name(pattern="BP_Bell_C")
find_actors_by_name(pattern="BP_KingBot_C")
find_actors_by_name(pattern="BP_Giganto_C")
-> note each actor's name, current location, rotation, scale
```

### Scale existing actors

```
For each actor that needs rescaling:
    set_actor_transform(
        name="BP_Bell_C_UAID_...",
        scale=[3, 3, 3]
    )

    # breathing room every 2 transforms
    get_height_at_location(x=0, y=0)
```

### Animate existing actors

Pre-existing actors need their BP to have an AnimBP assigned (Phase 1 + Phase 2 must be done first). Then set AnimationMode on the component:

```
For each existing enemy actor:
    # Set AnimationMode to AnimationBlueprint (value 0)
    # The BP's AnimBP will drive the animation
    set_actor_property(
        actor_name="BP_Bell_C_UAID_...",
        property_name="AnimationMode",
        property_value=0,
        component_name="CharacterMesh0"
    )
```

**IMPORTANT**: Pre-existing actors spawned BEFORE the AnimBP was assigned may not pick up the new AnimBP automatically. They may need to be deleted and re-spawned from the updated BP. Verify in PIE.

## Phase 6: Verification (~2-3 MCP calls)

```
take_screenshot()

find_actors_by_name(pattern="{prefix}")
-> Verify all expected actors present
-> Check positions look reasonable
```

**Editor vs Runtime Animation Note:**
- **Editor viewport**: Enemies show static pose (first frame of animation). This is normal.
- **PIE (Play In Editor)**: All animations play correctly via AnimBP. This is where you verify.
- `bUpdateAnimationInEditor` is transient — it won't persist between editor sessions.

### Checkpoint
> "Battlefield complete. {N} actors spawned/configured. Enter PIE to verify animations play. Press **Ctrl+S** to save."

## Composing a Full Battlefield

Run this skill multiple times with different prefixes and templates to build a complete scene:

```
# Scene 1: Two giant Bells dueling
/enemy-battlefield template=DUEL types=Bell,Bell center=-4000,1500 scale=3 prefix=BF_Duel1_

# Scene 2: KingBot boss surrounded by MC robots
/enemy-battlefield template=BOSS_FIGHT boss=KingBot combatants=Robot,Robot,Robot,Robot center=-1000,500 boss_scale=3 prefix=BF_Boss1_

# Scene 3: Patrol line of mixed enemies
/enemy-battlefield template=PATROL types=Giganto,Bell,Giganto,KingBot center=-6000,3000 patrol_yaw=45 prefix=BF_Patrol1_

# Scene 4: Chaotic melee
/enemy-battlefield template=MELEE types=Bell,KingBot,Giganto,Robot,Bell center=-2500,4000 prefix=BF_Melee1_
```

Each prefix creates an independent cleanup group.

## Total MCP Call Estimate

| Phase | Calls | Notes |
|-------|-------|-------|
| Phase 0: Asset verification | ~6-9 | 3 per unique enemy type |
| Phase 1: AnimBP creation | ~4 per type | Skip if ABP already exists |
| Phase 2: BP configuration | ~3 per type | |
| Phase 3: Cleanup | ~2 | Optional |
| Phase 4: Spawn + position | ~3-4 per actor | Height query + spawn + scale |
| Phase 5: Existing actors | ~2 per actor | Optional |
| Phase 6: Verification | ~2-3 | Screenshot + find |
| **Typical DUEL** | **~25** | 2 types, 2 actors |
| **Typical BOSS_FIGHT** | **~40** | 1 boss + 4 combatants |
| **Full battlefield (4 scenes)** | **~100-120** | Multiple templates composed |

## Scene Composition Math Reference

### Two actors facing each other
```python
import math
yaw_a_to_b = math.degrees(math.atan2(by - ay, bx - ax))
yaw_b_to_a = yaw_a_to_b + 180  # or yaw_a_to_b - 180
```

### Circle of N actors around center
```python
for i in range(N):
    angle = (2 * math.pi * i) / N
    x = center_x + radius * math.cos(angle)
    y = center_y + radius * math.sin(angle)
    yaw = math.degrees(math.atan2(center_y - y, center_x - x))  # face center
```

### Semicircle formation
```python
for i in range(N):
    angle = math.pi * i / (N - 1) if N > 1 else 0
    x = center_x + radius * math.cos(angle)
    y = center_y + radius * math.sin(angle)
```

## Troubleshooting

| Issue | Cause | Fix |
|-------|-------|-----|
| T-posing in editor | Normal — editor preview doesn't animate | Enter PIE to verify |
| T-posing in PIE | AnimBP not assigned or failed to compile | Re-run Phase 1 + 2, compile_blueprint |
| Actor floating | Wrong ground_z | Use snap_actor_to_ground or re-query height |
| Actor underground | Capsule origin below surface | Add +90 Z offset via set_actor_transform |
| Scale not applied | spawn_blueprint_actor_in_level ignores scale | Use set_actor_transform AFTER spawning |
| "No surface found" | Position outside landscape bounds | Blood & Dust landscape: -25200 to 25200 XY |
| Crash on rapid spawns | MCP calls too fast or parallel | Ensure sequential with breathing room |
| AnimBP wrong animation | setup_locomotion_state_machine wrong path | Verify animation path with does_asset_exist; ensure skeleton matches |
| setup_locomotion_state_machine fails | Skeleton mismatch or invalid animation path | Verify: (1) ABP skeleton matches SK_{Type}_Skeleton, (2) animation was imported for that skeleton, (3) use full content path for anim_blueprint_path |
| Robot NPC has controls | BP_RobotCharacter has input bindings | Input only activates on possessed pawn; NPC spawns are safe. NOTE: background-fighters.md prohibits this, but it's technically correct since Enhanced Input only fires on the PlayerController's possessed pawn |
| Actors gone after reload | Forgot Ctrl+S | ALWAYS save after spawning |
| "Blueprint not compiled" | Spawned before compile | Always compile_blueprint before spawn |
| Existing actors ignore AnimBP | BP was modified after they were placed | Delete + re-spawn from updated BP |

## NEVER Do These

- NEVER call MCP tools in parallel — each creates FTSTicker callback, parallel = crash
- NEVER spawn more than 2 actors without a breathing room query
- NEVER try to set AnimToPlay via set_actor_property — FObjectProperty is unsupported
- NEVER skip compile_blueprint before spawning instances from modified BPs
- NEVER import textures/meshes mid-spawn-loop — HEAVY operations that crash
- NEVER assume spawn_blueprint_actor_in_level sets scale — it doesn't
- NEVER use play_montage_on_actor in editor — PIE only
- NEVER mix up rotation format — it's `[Pitch, Yaw, Roll]` NOT `[Roll, Pitch, Yaw]`
- NEVER use `create_character_blueprint` with camera/spring arm for NPCs (unnecessary bloat)

## Cleanup Command

Remove all actors spawned by this skill:
```
delete_actors_by_pattern(pattern="{prefix}")
```

Default cleanup: `delete_actors_by_pattern(pattern="BF_")`

To clean up specific scenes: `delete_actors_by_pattern(pattern="BF_Duel1_")`

## Future Enhancement: set_skeletal_animation C++ Tool

A dedicated `set_skeletal_animation` MCP command would vastly simplify this workflow by calling `OverrideAnimationData()` directly on placed actors' SkeletalMeshComponents, eliminating the AnimBP creation overhead entirely.

**Implementation** (~30 lines in EpicUnrealMCPEditorCommands.cpp):
1. Find actor by name
2. Find SkeletalMeshComponent on actor
3. Load UAnimationAsset from content path
4. Call `OverrideAnimationData(Asset, bLooping=true, bPlaying=true, 0.f, PlayRate=1.f)`
5. The data serializes with the component — persists on save

This would reduce the entire animation setup from ~20 MCP calls (create AnimBP + configure BP + compile) to just 1 call per actor.

</enemy-battlefield>
