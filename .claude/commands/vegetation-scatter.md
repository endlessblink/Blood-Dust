---
description: Scatter all vegetation (grass, pebbles, trees) across the Blood & Dust landscape using HISM Poisson disk. Runs 12 sequential passes with hardcoded asset paths and proven parameters.
---

# Blood & Dust Full Vegetation Scatter

<vegetation-scatter>

## Overview

Scatter all vegetation across the full landscape using `scatter_foliage` MCP tool.
All 12 passes MUST be called **strictly sequentially** (never parallel - will crash Unreal).

## Prerequisites

1. Unreal Editor running with MCP plugin loaded
2. MCP Python server connected
3. Landscape exists at bounds `[-25200, 0, 0, 25200]`

## Step 0: Clean Up Existing HISM Actors

```
mcp__unrealMCP__delete_actors_by_pattern(pattern="HISM_")
```

## Step 1: Scatter Grass (6 passes)

### Pass 1 - Grass Mid A
```
scatter_foliage(
    mesh_path="/Game/Meshes/Vegetation/Grass/SM_Grass_Mid_A",
    bounds=[-25200, 0, 0, 25200],
    count=5000, min_distance=80, max_slope=45,
    scale_range=[5, 10], z_offset=-5,
    actor_name="HISM_Grass_Mid_A"
)
```

### Pass 2 - Grass Mid B
```
scatter_foliage(
    mesh_path="/Game/Meshes/Vegetation/Grass/SM_Grass_Mid_B",
    bounds=[-25200, 0, 0, 25200],
    count=5000, min_distance=80, max_slope=45,
    scale_range=[5, 10], z_offset=-5,
    actor_name="HISM_Grass_Mid_B"
)
```

### Pass 3 - Grass Small A
```
scatter_foliage(
    mesh_path="/Game/Meshes/Vegetation/Grass/SM_Grass_Small_A",
    bounds=[-25200, 0, 0, 25200],
    count=5000, min_distance=70, max_slope=45,
    scale_range=[5, 9], z_offset=-5,
    actor_name="HISM_Grass_Small_A"
)
```

### Pass 4 - Grass Small B
```
scatter_foliage(
    mesh_path="/Game/Meshes/Vegetation/Grass/SM_Grass_Small_B",
    bounds=[-25200, 0, 0, 25200],
    count=5000, min_distance=70, max_slope=45,
    scale_range=[5, 9], z_offset=-5,
    actor_name="HISM_Grass_Small_B"
)
```

### Pass 5 - Grass Large A
```
scatter_foliage(
    mesh_path="/Game/Meshes/Vegetation/Grass/SM_Grass_Large_A",
    bounds=[-25200, 0, 0, 25200],
    count=4000, min_distance=90, max_slope=45,
    scale_range=[6, 11], z_offset=-5,
    actor_name="HISM_Grass_Large"
)
```

### Pass 6 - Pebbles (small rocks)
```
scatter_foliage(
    mesh_path="/Game/Meshes/Rocks/rock_moss_set_01_rock01",
    bounds=[-25200, 0, 0, 25200],
    count=3000, min_distance=100, max_slope=45,
    scale_range=[0.08, 0.2], z_offset=-3,
    actor_name="HISM_Pebbles"
)
```

## Step 2: Scatter Trees (6 passes)

### Pass 7 - Searsia Lucida A (main tree)
```
scatter_foliage(
    mesh_path="/Game/Meshes/Trees/SM_Searsia_Lucida_A",
    bounds=[-25200, 0, 0, 25200],
    count=25, min_distance=2500, max_slope=30,
    scale_range=[2.5, 3.5], z_offset=-10,
    actor_name="HISM_Tree_Lucida_A"
)
```

### Pass 8 - Searsia Lucida D (variant)
```
scatter_foliage(
    mesh_path="/Game/Meshes/Trees/SM_Searsia_Lucida_D",
    bounds=[-25200, 0, 0, 25200],
    count=20, min_distance=2500, max_slope=30,
    scale_range=[2, 3], z_offset=-10,
    actor_name="HISM_Tree_Lucida_D"
)
```

### Pass 9 - Searsia Lucida F (bush/low)
```
scatter_foliage(
    mesh_path="/Game/Meshes/Trees/SM_Searsia_Lucida_F",
    bounds=[-25200, 0, 0, 25200],
    count=30, min_distance=2000, max_slope=35,
    scale_range=[1.5, 2.5], z_offset=-8,
    actor_name="HISM_Tree_Bush"
)
```

### Pass 10 - Searsia Burchellii
```
scatter_foliage(
    mesh_path="/Game/Meshes/Trees/SM_Searsia_Burchellii",
    bounds=[-25200, 0, 0, 25200],
    count=20, min_distance=2500, max_slope=30,
    scale_range=[2, 3], z_offset=-10,
    actor_name="HISM_Tree_Burchellii"
)
```

### Pass 11 - Quiver Tree
```
scatter_foliage(
    mesh_path="/Game/Meshes/Trees/SM_Quiver_Tree",
    bounds=[-25200, 0, 0, 25200],
    count=15, min_distance=3000, max_slope=25,
    scale_range=[2, 3.5], z_offset=-10,
    actor_name="HISM_Tree_Quiver"
)
```

### Pass 12 - Dead Tree Trunk (sunk into ground)
```
scatter_foliage(
    mesh_path="/Game/Meshes/Trees/SM_Dead_Tree_Trunk",
    bounds=[-25200, 0, 0, 25200],
    count=20, min_distance=2500, max_slope=30,
    scale_range=[1, 1.5], z_offset=-30,
    actor_name="HISM_Dead_Trunk"
)
```

## Step 3: Fix Tree Material Slots

IMPORTANT: Use `material_slot` parameter (NOT `slot_index`). Default -1 = all slots.

### Searsia Lucida A/D/F (3 slots each: bark, leaves, twigs)
```
# For EACH of SM_Searsia_Lucida_A, SM_Searsia_Lucida_D, SM_Searsia_Lucida_F:
set_mesh_asset_material(mesh_path="...", material_path="/Game/Materials/Trees/M_Searsia_Lucida_Bark", material_slot=0)
set_mesh_asset_material(mesh_path="...", material_path="/Game/Materials/Trees/M_Searsia_Lucida_Leaves", material_slot=1)
set_mesh_asset_material(mesh_path="...", material_path="/Game/Materials/Trees/M_Searsia_Lucida_Bark", material_slot=2)
```

### Searsia Burchellii (3 slots: leaves, bark, twigs)
```
set_mesh_asset_material(mesh_path="/Game/Meshes/Trees/SM_Searsia_Burchellii", material_path="/Game/Materials/Trees/M_Searsia_Burchellii_Leaves", material_slot=0)
set_mesh_asset_material(mesh_path="/Game/Meshes/Trees/SM_Searsia_Burchellii", material_path="/Game/Materials/Trees/M_Searsia_Burchellii_Bark", material_slot=1)
set_mesh_asset_material(mesh_path="/Game/Meshes/Trees/SM_Searsia_Burchellii", material_path="/Game/Materials/Trees/M_Searsia_Burchellii_Bark", material_slot=2)
```

### Quiver Tree & Dead Trunk (single slot each - already correct)
No action needed.

## Step 4: Save

Remind user to save (Ctrl+S) after all passes complete.
HISM instances persist across restarts via RF_Transactional + AddInstanceComponent + PostEditChangeProperty(PerInstanceSMData) + OFPA package marking.

## Expected Results

| Category | Instances | Note |
|----------|-----------|------|
| Grass (5 types) | ~23,000 | Dense ground cover |
| Pebbles | ~2,500 | Small rock scatter |
| Trees (4 species) | ~65-90 | Scattered copses |
| Dead trunks | ~17 | Sunken into ground |
| **Total** | ~25,500 | |

## Additional Available Grass Meshes (not currently used)

- `/Game/Meshes/Vegetation/Grass/SM_Grass_Mid_C`
- `/Game/Meshes/Vegetation/Grass/SM_Grass_Large_B`
- `/Game/Meshes/Vegetation/Grass/SM_Grass_Large_C`

## Available Rock Meshes

- `/Game/Meshes/Rocks/SM_Boulder_01`
- `/Game/Meshes/Rocks/SM_Namaqualand_Boulder_05`
- `/Game/Meshes/Rocks/SM_Drone_Rock_02`
- `/Game/Meshes/Rocks/rock_moss_set_01_rock01` through `rock06`

## Tree Materials (assigned to mesh assets)

- `/Game/Materials/Trees/M_Searsia_Lucida_Leaves` (masked, two-sided)
- `/Game/Materials/Trees/M_Searsia_Lucida_Bark`
- `/Game/Materials/Trees/M_Searsia_Burchellii_Leaves` (masked, two-sided)
- `/Game/Materials/Trees/M_Searsia_Burchellii_Bark`
- `/Game/Materials/Trees/M_Quiver_Tree`
- `/Game/Materials/Trees/M_Dead_Tree_Trunk`

## Tree Textures

- `/Game/Textures/Trees/T_Searsia_Lucida_D` (diffuse)
- `/Game/Textures/Trees/T_Searsia_Lucida_Alpha`
- `/Game/Textures/Trees/T_Searsia_Burchellii_D` (diffuse)
- `/Game/Textures/Trees/T_Searsia_Burchellii_Alpha`
- `/Game/Textures/Trees/T_Quiver_Tree_D` (diffuse)
- `/Game/Textures/Trees/T_Dead_Tree_Trunk_D` (diffuse)

</vegetation-scatter>
