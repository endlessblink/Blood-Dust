---
description: Scatter rocks on the Blood & Dust landscape with adjustable density and scale. Handles both large boulders (individual actors) and pebbles (HISM). Deletes existing rocks before re-scattering.
---

# Rock Scatter with Adjustable Density & Scale

<rock-scatter>

## Overview

Re-scatter rocks across the landscape with user-controllable density and scale.
Handles two tiers: **large boulders** (StaticMeshActor, individual placement) and **pebbles** (HISM batch).

All MCP calls MUST be **strictly sequential** (never parallel).

## Arguments

The user may provide any of these when invoking. Use defaults for anything not specified.

| Argument | Default | Description |
|----------|---------|-------------|
| `boulder_count` | 16 | Number of large boulders |
| `boulder_scale` | `[10, 40]` | Scale range [min, max] for boulders |
| `pebble_count` | 3000 | Number of small pebble instances (HISM) |
| `pebble_scale` | `[0.08, 0.2]` | Scale range [min, max] for pebbles |
| `boulders_only` | false | Only scatter boulders, skip pebbles |
| `pebbles_only` | false | Only scatter pebbles, skip boulders |

## Available Rock Meshes

### Large Boulders (3 types, cycle through them)
1. `/Game/Meshes/Rocks/SM_Boulder_01` - Classic rounded boulder
2. `/Game/Meshes/Rocks/SM_Namaqualand_Boulder_05` - Flat desert boulder
3. `/Game/Meshes/Rocks/SM_Drone_Rock_02` - Dramatic angular rock

### Pebbles (6 types, use rock01 as primary)
- `/Game/Meshes/Rocks/rock_moss_set_01_rock01` through `rock06`

## Landscape Bounds
- Full bounds: `[-25200, 0, 0, 25200]`

## Step 0: Parse Arguments

Read the user's arguments and set variables. Example parsing:
- `/rock-scatter boulder_count=30 boulder_scale=15,50` -> 30 boulders at scale 15-50
- `/rock-scatter pebble_count=5000` -> keep default boulders, 5000 pebbles
- `/rock-scatter pebbles_only pebble_count=1000 pebble_scale=0.5,1.5` -> only pebbles

## Step 1: Delete Existing Rocks

### If scattering boulders:
```
delete_actors_by_pattern(pattern="Rock_Main_")
delete_actors_by_pattern(pattern="Rock_Sec_")
delete_actors_by_pattern(pattern="Rock_Lone_")
delete_actors_by_pattern(pattern="Rock_Edge_")
```
Wait for EACH delete to complete before the next.

### If scattering pebbles:
```
delete_actors_by_pattern(pattern="HISM_Pebbles")
```

## Step 2: Scatter Large Boulders

Use `scatter_foliage` with the 3 large rock meshes, distributing `boulder_count` across them:
- Type A: SM_Boulder_01 -> ~40% of count
- Type B: SM_Namaqualand_Boulder_05 -> ~30% of count
- Type C: SM_Drone_Rock_02 -> ~30% of count

Each as a SEPARATE scatter_foliage call with `align_to_surface=false`, `random_yaw=true`.

```
# Type A boulders
scatter_foliage(
    mesh_path="/Game/Meshes/Rocks/SM_Boulder_01",
    bounds=[-25200, 0, 0, 25200],
    count=<40% of boulder_count>,
    min_distance=2000,
    max_slope=40,
    align_to_surface=false,
    random_yaw=true,
    scale_range=<boulder_scale>,
    z_offset=-20,
    actor_name="HISM_Boulder_A"
)

# Type B boulders (wait for A to complete!)
scatter_foliage(
    mesh_path="/Game/Meshes/Rocks/SM_Namaqualand_Boulder_05",
    bounds=[-25200, 0, 0, 25200],
    count=<30% of boulder_count>,
    min_distance=2000,
    max_slope=40,
    align_to_surface=false,
    random_yaw=true,
    scale_range=<boulder_scale>,
    z_offset=-20,
    actor_name="HISM_Boulder_B"
)

# Type C boulders (wait for B to complete!)
scatter_foliage(
    mesh_path="/Game/Meshes/Rocks/SM_Drone_Rock_02",
    bounds=[-25200, 0, 0, 25200],
    count=<30% of boulder_count>,
    min_distance=2000,
    max_slope=40,
    align_to_surface=false,
    random_yaw=true,
    scale_range=<boulder_scale>,
    z_offset=-20,
    actor_name="HISM_Boulder_C"
)
```

## Step 3: Scatter Pebbles

Use multiple rock mesh variants for variety:

```
# Pebble scatter (3 variants, split count)
scatter_foliage(
    mesh_path="/Game/Meshes/Rocks/rock_moss_set_01_rock01",
    bounds=[-25200, 0, 0, 25200],
    count=<40% of pebble_count>,
    min_distance=100,
    max_slope=45,
    align_to_surface=false,
    random_yaw=true,
    scale_range=<pebble_scale>,
    z_offset=-3,
    actor_name="HISM_Pebbles_A"
)

scatter_foliage(
    mesh_path="/Game/Meshes/Rocks/rock_moss_set_01_rock03",
    bounds=[-25200, 0, 0, 25200],
    count=<30% of pebble_count>,
    min_distance=100,
    max_slope=45,
    align_to_surface=false,
    random_yaw=true,
    scale_range=<pebble_scale>,
    z_offset=-3,
    actor_name="HISM_Pebbles_B"
)

scatter_foliage(
    mesh_path="/Game/Meshes/Rocks/rock_moss_set_01_rock05",
    bounds=[-25200, 0, 0, 25200],
    count=<30% of pebble_count>,
    min_distance=100,
    max_slope=45,
    align_to_surface=false,
    random_yaw=true,
    scale_range=<pebble_scale>,
    z_offset=-3,
    actor_name="HISM_Pebbles_C"
)
```

## Step 4: Report

After all scatters complete, report:
- Number of boulder instances placed (across 3 types)
- Number of pebble instances placed (across 3 types)
- Remind user to save (Ctrl+S)

## CRITICAL RULES

1. **NEVER call MCP tools in parallel** - strictly sequential
2. **ALWAYS wait for each scatter to complete** before starting the next
3. **Delete old actors FIRST**, then scatter new ones
4. **align_to_surface=false** for all rocks (prevents tombstone effect on slopes)
5. **min_distance=2000** for boulders (prevents overlap of large rocks)
6. **min_distance=100** for pebbles (dense ground detail)
7. **z_offset=-20** for boulders (sinks base into ground), **-3** for pebbles

## Examples

```
/rock-scatter                                    # Default: 16 boulders + 3000 pebbles
/rock-scatter boulder_count=30                   # More boulders, default pebbles
/rock-scatter boulder_count=8 boulder_scale=20,60  # Fewer but bigger boulders
/rock-scatter pebbles_only pebble_count=5000     # Just pebbles, dense
/rock-scatter boulders_only boulder_count=40 boulder_scale=5,25  # Many small boulders
```

</rock-scatter>
