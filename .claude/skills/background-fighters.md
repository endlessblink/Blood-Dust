# Background Fighters: Animated NPC Silhouettes for Blood & Dust

## Overview
Spawn pairs of animated enemy characters in the far distance as atmospheric background fighters. They loop fighting/walking animations and appear as dark silhouettes in fog, adding battlefield depth. Purely decorative — no AI, no player interaction.

## Trigger Phrases
- "spawn background fighters"
- "place background NPCs"
- "add fighting silhouettes"
- "background fighters"
- "battlefield atmosphere NPCs"

## Prerequisites
- UnrealMCP server running and connected to Unreal Editor
- Enemy skeletal meshes imported (at least one of: SK_KingBot, SK_Giganto, SK_Bell)
- Enemy animations imported (SK_KingBot_Anim, SK_Giganto_Anim, SK_Bell_Anim)
- Landscape present in the level (for ground height queries)

## CRITICAL RULES
1. **ALL MCP calls strictly SEQUENTIAL** — never parallel. Each creates FTSTicker callback; parallel = crash.
2. **Max 2 spawns then breathing room** — call `get_height_at_location(0, 0)` or `does_asset_exist()` between every 2 spawn/heavy operations.
3. **`set_actor_transform` must specify ALL fields** (location, rotation, scale) every call.
4. **Actor names MUST be unique** — use pattern `BG_Fighter_{EnemyType}_{PairNum}_{A|B}`.
5. **NEVER call MCP tools while another Claude instance is also making MCP calls.** Only ONE command in-flight at any time.
6. **Compile Blueprint before spawning instances** — uncompiled BPs spawn with default/broken state.
7. **Ctrl+S reminder after all spawns** — HISM/actor persistence requires manual save.

## Available Enemy Types

| Enemy | Skeleton | Animation | Anim Type | UE Paths |
|-------|----------|-----------|-----------|----------|
| **KingBot** | SK_KingBot_Skeleton | SK_KingBot_Anim | Boxing/Punching (combat) | `/Game/Characters/Enemies/KingBot/` |
| **Giganto** | SK_Giganto_Skeleton | SK_Giganto_Anim | Elderly Shaky Walk (movement) | `/Game/Characters/Enemies/Giganto/` |
| **Bell** | SK_Bell_Skeleton | SK_Bell_Anim | Slow Orc Walk (movement) | `/Game/Characters/Enemies/Bell/` |

Each enemy has: `SK_{Name}`, `SK_{Name}_Skeleton`, `SK_{Name}_Anim`, `T_{Name}_D`, `M_{Name}`, `BP_{Name}`

## Configurable Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| num_pairs | int | 3 | Number of fighter pairs to spawn |
| pair_spacing | float | 300 | Distance (Unreal units) between fighters in a pair |
| distance_range | [float, float] | [-22000, -18000] | X range for placement (negative = far from player at origin) |
| y_spread | [float, float] | [5000, 18000] | Y range to spread pairs across |
| kingbot_scale | float | 2.5 | Scale multiplier for KingBot (huge boss look) |
| pair_assignments | list | [["Bell","Bell"],["KingBot","Giganto"],["Giganto","Bell"]] | Enemy types per pair [A, B] |

## Phase 0: Asset Verification (~6 MCP calls)

Verify all required assets exist before proceeding.

```
For each enemy type in pair_assignments:
    does_asset_exist(path="/Game/Characters/Enemies/{Type}/SK_{Type}")
    does_asset_exist(path="/Game/Characters/Enemies/{Type}/SK_{Type}_Anim")
    does_asset_exist(path="/Game/Characters/Enemies/{Type}/SK_{Type}_Skeleton")
```

If any asset is missing, STOP and inform user which enemy needs importing first.

## Phase 1: Create AnimBPs (~4 MCP calls per unique enemy type)

For each UNIQUE enemy type used in pair_assignments, create an AnimBP that loops its animation.

**KEY TECHNIQUE**: Use `setup_locomotion_state_machine` with the SAME animation for both `idle_animation` and `walk_animation`. Since background NPCs never move, speed=0 forever, and they stay in the Idle state permanently playing the specified animation on loop.

```
For each unique enemy type (e.g., KingBot, Giganto, Bell):

    # Check if AnimBP already exists
    does_asset_exist(path="/Game/Characters/Enemies/{Type}/ABP_BG_{Type}")
    → if exists, skip to next type

    # Create AnimBP targeting the enemy's skeleton
    create_anim_blueprint(
        blueprint_name="ABP_BG_{Type}",
        skeleton_path="/Game/Characters/Enemies/{Type}/SK_{Type}_Skeleton",
        blueprint_path="/Game/Characters/Enemies/{Type}/",
        preview_mesh_path="/Game/Characters/Enemies/{Type}/SK_{Type}"
    )

    # breathing room
    does_asset_exist(path="/Game/Characters/Enemies/{Type}/ABP_BG_{Type}")

    # Set up locomotion with SAME animation for idle and walk
    # NPC never moves → speed=0 → stays in Idle state → loops the animation
    setup_locomotion_state_machine(
        anim_blueprint_path="/Game/Characters/Enemies/{Type}/ABP_BG_{Type}",
        idle_animation="/Game/Characters/Enemies/{Type}/SK_{Type}_Anim",
        walk_animation="/Game/Characters/Enemies/{Type}/SK_{Type}_Anim"
    )

    # breathing room
    does_asset_exist(path="/Game/Characters/Enemies/{Type}/ABP_BG_{Type}")
```

## Phase 2: Configure Character Blueprints (~3 MCP calls per unique enemy type)

Assign the skeletal mesh and AnimBP to each enemy's Character Blueprint.

```
For each unique enemy type:

    # Check if BP exists; if not, create a Character Blueprint
    does_asset_exist(path="/Game/Characters/Enemies/{Type}/BP_{Type}")
    → if missing:
        create_character_blueprint(
            blueprint_name="BP_{Type}",
            blueprint_path="/Game/Characters/Enemies/{Type}/",
            skeletal_mesh_path="/Game/Characters/Enemies/{Type}/SK_{Type}",
            anim_blueprint_path="/Game/Characters/Enemies/{Type}/ABP_BG_{Type}",
            capsule_half_height=90,
            capsule_radius=40
        )
    → if exists:
        # Assign mesh + AnimBP to existing Character BP
        set_character_properties(
            blueprint_path="/Game/Characters/Enemies/{Type}/BP_{Type}",
            skeletal_mesh_path="/Game/Characters/Enemies/{Type}/SK_{Type}",
            anim_blueprint_path="/Game/Characters/Enemies/{Type}/ABP_BG_{Type}"
        )

    # Compile to ensure clean state
    compile_blueprint(blueprint_name="BP_{Type}")

    # breathing room
    does_asset_exist(path="/Game/Characters/Enemies/{Type}/BP_{Type}")
```

### Checkpoint
> "Phase 2 complete. AnimBPs and Character BPs configured for {N} enemy types. Ready for spawning."

## Phase 3: Position Planning & Ground Heights (~1 MCP call per pair)

Determine ground Z for each pair position.

```
positions = []
For i in range(num_pairs):
    # Spread pairs across the distance and Y ranges
    x = lerp(distance_range[0], distance_range[1], i / max(num_pairs-1, 1))
    y = lerp(y_spread[0], y_spread[1], i / max(num_pairs-1, 1))

    get_height_at_location(x=x, y=y)
    → record ground_z

    positions.append({x, y, ground_z})
```

Example for 3 pairs on the Blood & Dust landscape (X: -25200 to 0, Y: 0 to 25200):
- Pair 1: X=-18000, Y=6000  (nearest background)
- Pair 2: X=-21000, Y=10000 (mid-distance)
- Pair 3: X=-22000, Y=16000 (far distance, barely visible)

## Phase 4: Spawn Fighter Pairs (~2-3 MCP calls per fighter, 4-6 per pair)

For each pair, spawn two fighters facing each other.

```
For each pair (i, position, [type_a, type_b]):

    # Fighter A — faces +Y direction (Yaw=90)
    spawn_blueprint_actor_in_level(
        blueprint_path="/Game/Characters/Enemies/{type_a}/BP_{type_a}",
        actor_name="BG_Fighter_{type_a}_{i+1}_A",
        location=[position.x, position.y - pair_spacing/2, position.ground_z],
        rotation=[0, 90, 0]
    )

    # breathing room
    get_height_at_location(x=0, y=0)

    # Fighter B — faces -Y direction (Yaw=-90)
    spawn_blueprint_actor_in_level(
        blueprint_path="/Game/Characters/Enemies/{type_b}/BP_{type_b}",
        actor_name="BG_Fighter_{type_b}_{i+1}_B",
        location=[position.x, position.y + pair_spacing/2, position.ground_z],
        rotation=[0, -90, 0]
    )

    # breathing room after every pair
    get_height_at_location(x=0, y=0)
```

## Phase 5: Snap to Ground + Scale Adjustments (~2 MCP calls per fighter)

`snap_actor_to_ground` places the actor ORIGIN at ground level. For ACharacter subclasses, the origin is at the capsule bottom, so feet should be at ground level — no manual offset needed.

```
For each spawned fighter actor:

    snap_actor_to_ground(actor_name="BG_Fighter_{type}_{pair}_{A|B}")

    # breathing room every 2 snaps
    get_height_at_location(x=0, y=0)
```

**Scale adjustments** — apply after snapping:

```
# KingBot should be huge
For each KingBot fighter:
    # Must re-read current position after snap
    # Use set_actor_transform with ALL fields
    set_actor_transform(
        name="BG_Fighter_KingBot_{pair}_{side}",
        location=[current_x, current_y, current_z],
        rotation=[0, current_yaw, 0],
        scale=[kingbot_scale, kingbot_scale, kingbot_scale]
    )

# Optional: slight scale variation on other fighters (1.0-1.2x range)
For each non-KingBot fighter:
    scale_var = random_choice([1.0, 1.05, 1.1, 1.15])
    set_actor_transform(
        name="BG_Fighter_{type}_{pair}_{side}",
        location=[current_x, current_y, current_z],
        rotation=[0, current_yaw, 0],
        scale=[scale_var, scale_var, scale_var]
    )
```

## Phase 6: Verification (~2 MCP calls)

```
take_screenshot()
get_actors_in_level()
→ Verify all BG_Fighter_* actors are present
→ At far distance with fog, they should appear as dark silhouettes
→ If completely invisible (too much fog), move closer (increase X toward 0)
→ If too clearly visible, move further out (decrease X)
```

### Checkpoint
> "All {num_pairs} fighter pairs placed and animated. {total_fighters} actors spawned. Press **Ctrl+S** to save."

## Total MCP Call Estimate

| Phase | Calls per item | Items | Subtotal |
|-------|---------------|-------|----------|
| Phase 0: Asset check | 3 | 3 enemy types | ~9 |
| Phase 1: AnimBP creation | 4 | 3 types | ~12 |
| Phase 2: BP config | 3 | 3 types | ~9 |
| Phase 3: Height queries | 1 | 3 positions | ~3 |
| Phase 4: Spawning | 4 | 3 pairs (6 fighters) | ~12 |
| Phase 5: Snap + scale | 3 | 6 fighters | ~18 |
| Phase 6: Verification | 2 | 1 | ~2 |
| **Total** | | | **~65 calls** |

## Troubleshooting

| Issue | Cause | Fix |
|-------|-------|-----|
| "No surface found" from get_height_at_location | Position outside landscape bounds | Check landscape bounds with get_landscape_info, adjust coordinates |
| Fighter T-posing (no animation) | AnimBP not assigned or not compiled | Re-run set_character_properties + compile_blueprint |
| Fighter clipping into ground | snap_actor_to_ground Z offset wrong | Add manual Z offset (+90 for standard capsule half-height) via set_actor_transform |
| Fighter floating above ground | Ground height changed after spawn | Re-run snap_actor_to_ground |
| "Unknown component type" | Wrong class path for SkeletalMeshComponent | Use `/Script/Engine.SkeletalMeshComponent` full path |
| AnimBP not found after creation | Package not saved | The ABP exists in memory; compile_blueprint should trigger save |
| Fighters invisible in fog | Too far away + dense fog | Move closer (increase X toward player) or reduce fog density |
| Fighters too clearly visible | Too close to player | Move further out (decrease X away from player) |
| Editor crash on spawn | Too many spawns without breathing room | Ensure lightweight query between every 2 spawns |
| "Blueprint not compiled" warning | Spawned before compile | Always compile_blueprint before spawn_blueprint_actor_in_level |

## NEVER Do These
- NEVER call MCP tools in parallel
- NEVER spawn more than 2 actors without a breathing room query
- NEVER use BP_RobotCharacter for background fighters (it's the player character)
- NEVER skip compile_blueprint before spawning instances
- NEVER use `create_character_blueprint` with camera/spring arm for background NPCs (set camera_boom_length=0 or remove after creation)
- NEVER import new assets mid-spawn-loop (texture imports are HEAVY and can crash)

## Cleanup Command

To remove all background fighters:
```
delete_actors_by_pattern(pattern="BG_Fighter_")
```
This deletes all actors whose name contains "BG_Fighter_".
