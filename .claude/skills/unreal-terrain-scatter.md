# Unreal Terrain Scatter

## Overview
Place meshes (rocks, props, vegetation) on an Unreal Engine landscape surface via MCP. Handles height queries, coordinate conversion, and ground snapping.

## Trigger Phrases
- "scatter rocks"
- "place on landscape"
- "place on terrain"
- "put rocks on ground"
- "terrain scatter"
- "landscape placement"

## CRITICAL: MCP Safety Rules

**MUST follow these rules or Unreal WILL crash:**

1. **NEVER call MCP tools in parallel** — always strictly sequential, wait for response before next call.
2. **NEVER batch multiple spawns/deletes in one MCP command** — one actor per call.
3. **Separate deletes from spawns** — delete all old actors first, then spawn new ones.
4. **One operation per FTSTicker tick** — each MCP call = one tick. Parallel calls = multiple tickers firing same frame = crash.

## Workflow

### Step 1: Get Landscape Bounds
```
get_landscape_info()
```
Returns landscape proxy bounds (min/max X, Y, Z). All placements must be within these bounds.

### Step 2: Choose Center Point
Pick a center [X, Y] on the landscape for the scatter cluster. Good defaults:
- Near PlayerStart (check with `find_actors_by_name("PlayerStart")`)
- Center of landscape (midpoint of bounds)

### Step 3: Define Items with Relative Offsets
Each item needs:
- `name` — unique actor name
- `static_mesh` — content browser path (e.g., `/Game/Meshes/Rocks/SM_Boulder_01`)
- `offset` — [dX, dY] from center in Unreal units (cm)
- `rotation` — [Pitch, Yaw, Roll] in degrees
- `scale` — [X, Y, Z] scale factors

### Step 4: For Each Item (SEQUENTIAL, one at a time)

```
1. get_height_at_location(center_x + offset_x, center_y + offset_y)
   → returns surface Z

2. spawn_actor(
     name="Rock_01",
     type="StaticMeshActor",
     static_mesh="/Game/Meshes/Rocks/SM_Boulder_01",
     location=[X, Y, surface_Z],
     rotation=[pitch, yaw, roll],
     scale=[sx, sy, sz]
   )
   → wait for response before next rock
```

### Alternative: Spawn then Snap
```
1. spawn_actor(name, mesh, location=[X, Y, 10000], ...)
   → spawn high in the air

2. snap_actor_to_ground(actor_name)
   → snaps down to landscape surface
```

## Converting Blender Positions

If source positions come from Blender:

**Coordinate conversion (axis swap + scale):**
```
UE_offset_X = Blender_offset_Y * 100
UE_offset_Y = -Blender_offset_X * 100
```
**DO NOT convert Z from Blender** — always query landscape height instead.

**Rotation conversion:**
```
UE_Pitch = -Blender_rot_X * (180/pi)
UE_Yaw   = -Blender_rot_Z * (180/pi)
UE_Roll  =  Blender_rot_Y * (180/pi)
```

**Scale (axis swap):**
```
UE_Scale_X = Blender_Scale_Y
UE_Scale_Y = Blender_Scale_X
UE_Scale_Z = Blender_Scale_Z
```
(For uniform scales, no swap needed.)

## Available MCP Tools

| Tool | Purpose |
|------|---------|
| `get_landscape_info` | Get landscape bounds, scale, layers |
| `get_height_at_location(x, y)` | Line trace down at XY, return surface Z |
| `snap_actor_to_ground(name)` | Snap actor to ground below it |
| `spawn_actor(name, type, mesh, loc, rot, scale)` | Place StaticMeshActor |
| `delete_actor(name)` | Remove actor by name |
| `set_actor_transform(name, loc, rot, scale)` | Reposition existing actor |
| `find_actors_by_name(pattern)` | Find actors matching pattern |
| `apply_material_to_actor(actor, material)` | Assign material |

## Troubleshooting

| Issue | Solution |
|-------|----------|
| **Unreal crashes** | You called MCP tools in parallel. ALWAYS sequential. |
| No surface found | Position is outside landscape bounds. Check with `get_landscape_info`. |
| Rocks floating | The mesh pivot isn't at the bottom. Adjust Z offset after snapping. |
| Wrong rotation | Verify Blender-to-UE rotation conversion formula above. |
| Actor already exists | Delete old actor first, or use a different name. |

## Example: Scatter 3 Rocks

```
# Step 1: Query landscape
get_landscape_info()
# → bounds: X [-25200, 0], Y [0, 25200]

# Step 2: Choose center
center = [-5000, 5000]

# Step 3: Rock 1 - query height then spawn
get_height_at_location(-5500, 7000)
# → z: 63.7
spawn_actor("Rock_01", "StaticMeshActor", "/Game/Meshes/Rocks/SM_Boulder",
            location=[-5500, 7000, 63.7], rotation=[0, -45, 0], scale=[2, 2, 2])

# Step 4: Rock 2 - wait for Rock 1 to complete first!
get_height_at_location(-4800, 4900)
# → z: 73.5
spawn_actor("Rock_02", "StaticMeshActor", "/Game/Meshes/Rocks/SM_Moss_01",
            location=[-4800, 4900, 73.5], rotation=[0, 80, 0], scale=[0.5, 0.5, 0.5])

# Step 5: Rock 3
get_height_at_location(-3900, 4000)
# → z: 115.1
spawn_actor("Rock_03", "StaticMeshActor", "/Game/Meshes/Rocks/SM_Drone_Rock",
            location=[-3900, 4000, 115.1], rotation=[0, 160, 0], scale=[0.1, 0.1, 0.1])
```

---

## HISM Bulk Vegetation Scatter (`scatter_foliage`)

For large-scale vegetation placement (grass, ground detail, pebbles), use the `scatter_foliage` tool which creates Hierarchical Instanced Static Mesh (HISM) actors with thousands of instances.

### Overview

`scatter_foliage` uses Poisson disk distribution with slope filtering to scatter mesh instances:
- Grid-accelerated dart-throwing algorithm (O(N), 5x5 neighbor check, 30 attempts per point)
- Line traces each candidate for height + slope validation
- Rejects placements where slope > max_slope
- Creates single AActor container with UHierarchicalInstancedStaticMeshComponent child
- Batch adds all instances in one call for performance

### Tool Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mesh_path` | string | UStaticMesh asset path (e.g., `/Game/Meshes/Vegetation/Grass/SM_Grass_Large_A`) |
| `center` | [float, float] | World coordinates [X, Y] for scatter origin |
| `radius` | float | Scatter radius in Unreal units (cm) |
| `count` | int | Target instance count (max 50000, safety cap) |
| `min_distance` | float | Poisson disk minimum spacing between instances |
| `max_slope` | float | Maximum terrain slope in degrees (0-90) |
| `align_to_surface` | bool | Align instance Z-axis to terrain normal (true for grass, false for pebbles/rocks) |
| `random_yaw` | bool | Randomize yaw rotation per instance |
| `scale_range` | [float, float] | Uniform scale range [min, max] |
| `z_offset` | float | Vertical offset in UU (negative = sink into ground) |
| `actor_name` | string | Container actor name (prefix with `HISM_` by convention) |
| `cull_distance` | float | LOD culling distance (0 = no culling) |
| `material_path` | string | Optional material override path |

### v4 Tuned Parameters (PROVEN VALUES for hilly terrain)

These ranges are battle-tested on rolling landscape with 20-35° slopes:

| Grass Size | count | min_distance | max_slope | scale_range | z_offset | Notes |
|------------|-------|--------------|-----------|-------------|----------|-------|
| Large | 3000 | 120 | 35 | [5, 10] | -5 | Base grass layer, visible from distance |
| Mid | 4000-5000 | 80 | 40 | [5, 10] | -4 | Mid-detail fill, moderate density |
| Small | 5000-6000 | 60 | 45 | [6, 12] | -3 | High-detail close-up, densest coverage |
| Pebbles | 1500 | 120 | 35 | [0.5, 1.0] | -3 | Ground detail, subtle rocks |

### Zone Coverage Strategy

For full-landscape coverage without visible gaps:

1. **Divide landscape into overlapping zones** — radius should exceed section diagonal to ensure overlap at edges.
2. **Use different mesh variants per zone** — rotate through grass_large_A/B/C, grass_mid_A/B/C, etc. for visual variety.
3. **Center each zone at section midpoint** — ensures even coverage across landscape streaming sections.
4. **Example**: 4-section landscape (25200x25200 UU) = 2 zones with radius 14000 UU each.

### Critical Lessons Learned

1. **max_slope is the #1 coverage driver** — Rolling hills naturally have 20-35° slopes. max_slope=15-20 rejects 55% of candidates, leaving terrain bare. Use 35° for large grass, 40° for mid, 45° for small.

2. **Pebble align_to_surface=false** — When true, pebbles on slopes tilt to match surface normal, looking like tombstones. Set false so they stay naturally upright.

3. **Scale range matters** — Grass meshes are tiny (~30-60 UU height). Scale [5, 12] makes them visible from gameplay camera height. Pebble rocks at [0.5, 1.0] stay subtle ground detail.

4. **z_offset sinks roots** — Negative z_offset hides the flat base of grass meshes, making them appear rooted in terrain. -3 to -5 for grass, -3 for pebbles.

5. **HEAVY operation** — scatter_foliage is in HEAVY_COMMANDS_COOLDOWN (2.0s). Always call strictly sequentially, never in parallel.

6. **Actor naming convention** — Use descriptive names: `HISM_Grass_Large_A`, `HISM_Grass_Mid_B`, `HISM_Pebbles_A` etc. Makes scene management easier.

7. **Mesh paths** — Grass typically at `/Game/Meshes/Vegetation/Grass/SM_Grass_*`, rocks at `/Game/Meshes/Rocks/rock_moss_*`.

8. **Delete before re-scatter** — Use `delete_actors_by_pattern("HISM_Grass")` and `delete_actors_by_pattern("HISM_Pebbles")` to clean up before re-running scatter operations.

### Example: Full-Landscape 2-Zone Scatter

```python
# Step 1: Clean up old vegetation
delete_actors_by_pattern("HISM_Grass")
delete_actors_by_pattern("HISM_Pebbles")

# Step 2: Zone 1 - Left half of landscape
scatter_foliage(
    mesh_path="/Game/Meshes/Vegetation/Grass/SM_Grass_Large_A",
    center=[-18900, 12600],
    radius=14000,
    count=3000,
    min_distance=120,
    max_slope=35,
    align_to_surface=True,
    random_yaw=True,
    scale_range=[5, 10],
    z_offset=-5,
    actor_name="HISM_Grass_Large_A",
    material_path="/Game/Materials/Vegetation/M_Grass_Foliage"
)

# Wait for response, then mid grass
scatter_foliage(
    mesh_path="/Game/Meshes/Vegetation/Grass/SM_Grass_Mid_A",
    center=[-18900, 12600],
    radius=14000,
    count=4500,
    min_distance=80,
    max_slope=40,
    align_to_surface=True,
    random_yaw=True,
    scale_range=[5, 10],
    z_offset=-4,
    actor_name="HISM_Grass_Mid_A",
    material_path="/Game/Materials/Vegetation/M_Grass_Foliage"
)

# Small grass for high detail
scatter_foliage(
    mesh_path="/Game/Meshes/Vegetation/Grass/SM_Grass_Small",
    center=[-18900, 12600],
    radius=14000,
    count=5500,
    min_distance=60,
    max_slope=45,
    align_to_surface=True,
    random_yaw=True,
    scale_range=[6, 12],
    z_offset=-3,
    actor_name="HISM_Grass_Small_A",
    material_path="/Game/Materials/Vegetation/M_Grass_Foliage"
)

# Step 3: Zone 2 - Right half (use different mesh variants for variety)
scatter_foliage(
    mesh_path="/Game/Meshes/Vegetation/Grass/SM_Grass_Large_B",
    center=[-6300, 12600],
    radius=14000,
    count=3000,
    min_distance=120,
    max_slope=35,
    align_to_surface=True,
    random_yaw=True,
    scale_range=[5, 10],
    z_offset=-5,
    actor_name="HISM_Grass_Large_B",
    material_path="/Game/Materials/Vegetation/M_Grass_Foliage"
)

# ... repeat with Mid and Small variants for Zone 2 ...

# Step 4: Pebbles (align_to_surface=false to prevent tombstone effect!)
scatter_foliage(
    mesh_path="/Game/Meshes/Rocks/rock_moss_set_01_rock03",
    center=[-18900, 12600],
    radius=14000,
    count=1500,
    min_distance=120,
    max_slope=35,
    align_to_surface=False,  # CRITICAL: false for pebbles!
    random_yaw=True,
    scale_range=[0.5, 1.0],
    z_offset=-3,
    actor_name="HISM_Pebbles_A"
)

scatter_foliage(
    mesh_path="/Game/Meshes/Rocks/rock_moss_set_01_rock03",
    center=[-6300, 12600],
    radius=14000,
    count=1500,
    min_distance=120,
    max_slope=35,
    align_to_surface=False,
    random_yaw=True,
    scale_range=[0.5, 1.0],
    z_offset=-3,
    actor_name="HISM_Pebbles_B"
)
```

### v4 Production Results (Actual Data)

From real production scatter on 4-section landscape:

**Instance Counts:**
- Zone 1 grass: 11,699 instances
  - Large: 2,880
  - Mid A: 3,030
  - Mid B: 2,533
  - Small: 3,256
- Zone 2 grass: 13,518 instances
  - Large B: 2,679
  - Large C: 2,313
  - Mid C: 3,834
  - Small: 4,692
- Pebbles: 1,635 instances
  - Zone 1: 745
  - Zone 2: 890

**Total: 26,852 instances across 10 HISM actors**

**Performance:**
- Average slope rejection: ~20% (down from 55% in v3 with max_slope 15-20°)
- Coverage: ~85% of terrain (only steep cliff faces remain bare)
- Frame time impact: Negligible with HISM (all instances batched into single draw call per actor)

### Debugging Failed Scatters

If scatter returns low instance counts:

| Issue | Cause | Solution |
|-------|-------|----------|
| 50-80% rejection rate | max_slope too restrictive for terrain | Increase max_slope to 35-45° for rolling hills |
| All placements rejected | Center outside landscape bounds | Use `get_landscape_info()` to verify center is within bounds |
| Instances invisible | Scale too small for camera distance | Increase scale_range minimum (grass needs 5+ for visibility) |
| Tombstone effect (pebbles) | align_to_surface=true on slopes | Set align_to_surface=false for rocks/pebbles |
| Patchy coverage | min_distance too large for count | Decrease min_distance or increase count |

### Performance Considerations

- **Safety cap**: 4M grid cells max, count clamped to 50000 per actor
- **HISM benefits**: All instances of one actor are a single draw call, extremely efficient
- **Cull distance**: Set to 0 for no culling, or tune per grass size (e.g., 10000 for small, 20000 for large)
- **Container actor organization**: All vegetation actors placed in "Foliage" editor folder automatically

### Mesh Asset Requirements

Grass meshes need:
- FBX exports at `blender/models/grass_medium_01/fbx/` (8 variants tested: 3 large, 3 mid, 2 small)
- Textures at `blender/models/grass_medium_01/textures/` (diff, alpha, nor_gl, rough - 1k resolution)
- Imported to Unreal at `/Game/Meshes/Vegetation/Grass/SM_Grass_*`
- Material setup with alpha masking for grass blades

### When to Use Manual vs HISM Scatter

| Use Manual Placement | Use HISM Scatter |
|---------------------|------------------|
| Hero assets (unique rocks, props) | Ground cover (grass, pebbles) |
| < 20 objects | > 100 objects |
| Need individual material overrides | Shared material OK |
| Precise artistic placement | Organic natural distribution |
| Large objects (boulders, trees) | Small repetitive detail |
