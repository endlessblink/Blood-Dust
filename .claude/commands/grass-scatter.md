---
description: Scatter grass on the Blood & Dust landscape with adjustable density and scale. Handles 5 grass variants via HISM. Deletes existing grass before re-scattering.
---

# Grass Scatter with Adjustable Density & Scale

<grass-scatter>

## Overview

Re-scatter grass across the landscape with user-controllable density and scale.
Uses 5 grass mesh variants in HISM for performance.

All MCP calls MUST be **strictly sequential** (never parallel).

## Arguments

| Argument | Default | Description |
|----------|---------|-------------|
| `count` | 5000 | Instances per grass variant (total = count x 5) |
| `scale` | `[5, 10]` | Scale range [min, max] for all grass |
| `density` | 1.0 | Multiplier for count (0.5 = half, 2.0 = double) |
| `max_slope` | 45 | Max terrain slope for grass placement |

## Available Grass Meshes (5 currently used)

1. `/Game/Meshes/Vegetation/Grass/SM_Grass_Mid_A`
2. `/Game/Meshes/Vegetation/Grass/SM_Grass_Mid_B`
3. `/Game/Meshes/Vegetation/Grass/SM_Grass_Small_A`
4. `/Game/Meshes/Vegetation/Grass/SM_Grass_Small_B`
5. `/Game/Meshes/Vegetation/Grass/SM_Grass_Large_A`

## Landscape Bounds
- Full bounds: `[-25200, 0, 0, 25200]`

## Step 0: Parse Arguments

- `/grass-scatter` -> defaults (5000 per type, ~25000 total)
- `/grass-scatter density=0.5` -> 2500 per type, ~12500 total
- `/grass-scatter density=2 scale=8,15` -> 10000 per type, bigger grass
- `/grass-scatter count=2000 max_slope=30` -> sparse, flat areas only

Compute effective count: `effective_count = round(count * density)`

## Step 1: Delete Existing Grass

```
delete_actors_by_pattern(pattern="HISM_Grass")
```
Wait for completion.

## Step 2: Scatter Each Variant (SEQUENTIAL)

### Pass 1 - Mid A
```
scatter_foliage(
    mesh_path="/Game/Meshes/Vegetation/Grass/SM_Grass_Mid_A",
    bounds=[-25200, 0, 0, 25200],
    count=<effective_count>,
    min_distance=80,
    max_slope=<max_slope>,
    align_to_surface=true,
    random_yaw=true,
    scale_range=<scale>,
    z_offset=-5,
    actor_name="HISM_Grass_Mid_A"
)
```

### Pass 2 - Mid B (wait for Pass 1!)
```
scatter_foliage(
    mesh_path="/Game/Meshes/Vegetation/Grass/SM_Grass_Mid_B",
    bounds=[-25200, 0, 0, 25200],
    count=<effective_count>,
    min_distance=80,
    max_slope=<max_slope>,
    scale_range=<scale>,
    z_offset=-5,
    actor_name="HISM_Grass_Mid_B"
)
```

### Pass 3 - Small A
```
scatter_foliage(
    mesh_path="/Game/Meshes/Vegetation/Grass/SM_Grass_Small_A",
    bounds=[-25200, 0, 0, 25200],
    count=<effective_count>,
    min_distance=70,
    max_slope=<max_slope>,
    scale_range=<scale adjusted: scale[0]-1, scale[1]-1>,
    z_offset=-5,
    actor_name="HISM_Grass_Small_A"
)
```

### Pass 4 - Small B
```
scatter_foliage(
    mesh_path="/Game/Meshes/Vegetation/Grass/SM_Grass_Small_B",
    bounds=[-25200, 0, 0, 25200],
    count=<effective_count>,
    min_distance=70,
    max_slope=<max_slope>,
    scale_range=<scale adjusted>,
    z_offset=-5,
    actor_name="HISM_Grass_Small_B"
)
```

### Pass 5 - Large A
```
scatter_foliage(
    mesh_path="/Game/Meshes/Vegetation/Grass/SM_Grass_Large_A",
    bounds=[-25200, 0, 0, 25200],
    count=<round(effective_count * 0.8)>,
    min_distance=90,
    max_slope=<max_slope>,
    scale_range=<scale adjusted: scale[0]+1, scale[1]+1>,
    z_offset=-5,
    actor_name="HISM_Grass_Large"
)
```

## Step 3: Report

After all 5 passes complete:
- Total grass instances placed
- Scale range used
- Remind user to save (Ctrl+S)

## CRITICAL RULES

1. **NEVER call MCP tools in parallel**
2. **ALWAYS wait for each scatter to complete** before starting the next
3. **Delete old HISM_Grass* FIRST**, then scatter new
4. **align_to_surface=true** for grass (blades follow terrain slope)
5. **z_offset=-5** sinks grass roots into ground

## Examples

```
/grass-scatter                        # Default: ~25000 total instances
/grass-scatter density=0.3            # Sparse: ~7500 total
/grass-scatter density=2 scale=8,15   # Dense & tall: ~50000 total
/grass-scatter count=1000             # Light coverage: ~5000 total
/grass-scatter max_slope=20           # Only flat areas
```

</grass-scatter>
