# Seamless Ground Material Skill

Create non-tiling, irregular ground materials in Blender using Voronoi-based techniques.

## When to Use
- Creating terrain/ground materials that don't show visible tiling
- Adding irregular wet/mud patches to terrain
- Blending multiple ground textures naturally
- Adding hills/displacement to flat terrain

## Key Techniques (Research-Based)

### 1. Use VORONOI, Not Noise for Irregular Patterns
- **Voronoi F1 Distance** creates cell-based irregular shapes
- **Randomness = 1.0** for maximum irregularity
- Different scales create different patch sizes

### 2. CONSTANT Interpolation in Color Ramps
- Creates HARD breaks between materials (not smooth gradients)
- Essential for distinct patches vs uniform blending

### 3. Multiple Voronoi Masks at Different Scales
- Scale 2-4: Large irregular regions
- Scale 5-8: Medium patches
- Scale 10+: Small details

### 4. Subdivision BEFORE Displacement
- Hills won't show without enough geometry
- Add Subdivision Surface modifier (level 3+) BEFORE geometry nodes

### 5. Random Texture Rotations
- Each texture should have different rotation (0°, 67°, 143°, 211°)
- Breaks up any remaining repetition

## Blender MCP Commands

### Create Complete Terrain Material
```python
import bpy
import math

terrain = bpy.data.objects.get('Terrain')

# 1. Add subdivision for hills
terrain.modifiers.clear()
subdiv = terrain.modifiers.new(name="Subdivision", type='SUBSURF')
subdiv.subdivision_type = 'SIMPLE'
subdiv.levels = 3

# 2. Create material with Voronoi blending
mat = bpy.data.materials.new(name="Terrain_Seamless")
mat.use_nodes = True
nodes = mat.node_tree.nodes
links = mat.node_tree.links
nodes.clear()

# Setup nodes...
# (See full implementation in execute_blender_code calls)
```

### Key Node Setup Pattern
```
Texture Coordinate (Object)
  → Voronoi (F1, Scale: 3-8, Randomness: 1.0)
    → Color Ramp (CONSTANT interpolation)
      → Mix RGB (Factor)
        → Blend textures
```

### Puddle Pattern
```
Voronoi (F1, Scale: 2-4)
  → Color Ramp (CONSTANT, threshold at 0.15 for sparse puddles)
    → Mix Factor (dry texture vs wet texture)
    → Roughness Mix (0.9 dry, 0.1 wet for reflections)
```

## Parameters to Adjust

| Node | Parameter | Effect |
|------|-----------|--------|
| Voronoi | Scale | Larger = smaller cells, more patches |
| Voronoi | Randomness | 1.0 = maximum irregularity |
| Color Ramp | Element positions | Controls patch threshold |
| Color Ramp | Interpolation | CONSTANT = hard edges |
| Mapping | Scale | Texture tiling density |
| Mapping | Rotation | Break up repetition |
| Roughness Mix | Color2 | Lower = more reflective wet areas |

## Common Issues & Fixes

| Problem | Solution |
|---------|----------|
| Hills not showing | Add Subdivision modifier level 3+ BEFORE displacement |
| Patches too uniform | Use Voronoi instead of Noise texture |
| Soft blends instead of patches | Set Color Ramp to CONSTANT interpolation |
| Puddles everywhere | Adjust Color Ramp threshold (lower = less wet) |
| Textures look the same | Add random rotations to each texture mapping |

## Sources
- Voronoi for irregular masking (not Perlin noise)
- CONSTANT interpolation for discrete patches
- F1 Distance output for cell-based shapes
- Object coordinates for world-space consistency
