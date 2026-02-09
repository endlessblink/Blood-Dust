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
