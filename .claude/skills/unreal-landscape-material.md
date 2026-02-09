# Unreal Landscape Material via MCP

## Overview
Create a proper multi-texture blended landscape material in Unreal Engine 5 using MCP tools.
Uses slope-based auto-blending (WorldAlignedBlend) and height variation with Lerp cascades.
NO LandscapeLayerBlend — it crashes the shader compiler when created programmatically.

## Trigger Phrases
- "landscape material"
- "ground material"
- "ground texture"
- "terrain material"
- "landscape ground"

## CRITICAL RULES

1. **NEVER use LandscapeLayerBlend** via MCP — causes `TextureReferenceIndex != INDEX_NONE` crash in HLSLMaterialTranslator.cpp
2. **NEVER use TextureSampleParameter2D** — use plain TextureSample with texture_path
3. **NEVER use LandscapeLayerCoords for UV generation** — mapping_scale does NOT persist through editor restart. Use WorldPosition x Constant instead (see `unreal-material-builder` skill)
4. **Use WorldAlignedBlend** for slope-based auto-blending (rock on slopes, grass/mud on flats)
5. **All MCP calls strictly sequential** — never parallel
6. **Roughness = constant 0.85, Metallic = constant 0** unless you have ORM/ARM packed textures
7. **sRGB OFF** for normal maps, roughness, ARM/ORM textures
8. **sRGB ON** for diffuse/base color textures only
9. **SEE `unreal-material-builder` skill for full phase-based workflow with verification checkpoints**

## Prerequisites
- UnrealMCP server running
- Ground textures imported to `/Game/Textures/Ground/` with correct compression:
  - Diffuse: Default compression, sRGB=true
  - Normal: Normalmap compression, sRGB=false, flip_green=true (Blender OpenGL→UE DirectX)
  - Roughness: Masks compression, sRGB=false
  - ARM: Masks compression, sRGB=false

## Material Architecture

```
LandscapeLayerCoords (MappingScale: 5.0)
    ↓ (shared UV for all textures)
TextureSample (Rock_D)  ──┐
TextureSample (Mud_D)   ──┤── WorldAlignedBlend alpha → Lerp1 (Rock vs Mud)
TextureSample (Grass_D) ──┘── Lerp2 (Lerp1 vs Grass, alpha from height or second blend)
    ↓
BaseColor output

Constant 0.85 → Roughness output
Constant 0.0  → Metallic output
```

### Slope-Based Blending (WorldAlignedBlend)
- Compares surface normal against world up vector
- Output: 0 = slopes/cliffs, 1 = flat surfaces
- **Blend Bias**: -0.3 to -0.8 (negative = more slope triggers rock texture)
- **Blend Sharpness**: 5.0 to 20.0 (higher = harder transition)
- Result: Rock shows on slopes, Grass/Mud shows on flat areas

### Height-Based Variation (Optional)
- Use WorldPosition Z component for height-based blending
- Divide by landscape height range, clamp 0-1
- Mix with slope alpha for more natural distribution

## Step-by-Step Workflow

### Step 1: Create Material Asset
```
create_material_asset("M_Landscape_Auto", "/Game/Materials/")
```

### Step 2: Add UV Coordinate Node
```
add_material_expression(material, "LandscapeLayerCoords", -1200, 0,
    {"mapping_scale": 5.0})
```
**mapping_scale**: Controls texture density. 5.0 = each texture tile covers 5 landscape quads.
- Lower = more tiling (smaller tiles)
- Higher = less tiling (larger tiles)
- Typical range: 3.0-10.0

### Step 3: Add Texture Samples (one per ground type)
For each texture (Rock_D, Mud_D, Grass_D):
```
add_material_expression(material, "TextureSample", -800, Y_offset,
    {"texture_path": "/Game/Textures/Ground/T_Rocky_Terrain_D", "sampler_type": "Color"})
```
Connect LandscapeLayerCoords → each TextureSample UV input.

### Step 4: Add WorldAlignedBlend Node
```
add_material_expression(material, "WorldAlignedBlend", -400, 400,
    {"blend_bias": -0.5, "blend_sharpness": 10.0})
```
This outputs a 0-1 alpha: 0=slopes, 1=flat.

### Step 5: Create Lerp Cascade
```
# Lerp1: Rock (slopes) vs Mud (flat)
add_material_expression(material, "Lerp", -200, 0)
connect: Rock_D → Lerp1.A
connect: Mud_D → Lerp1.B
connect: WorldAlignedBlend → Lerp1.Alpha

# Lerp2: Lerp1 result vs Grass (adds grass variation)
add_material_expression(material, "Lerp", 0, 0)
connect: Lerp1 → Lerp2.A
connect: Grass_D → Lerp2.B
# Use height-based or secondary blend for Lerp2 alpha
```

### Step 6: Add Constants
```
add_material_expression(material, "Constant", 0, 400, {"value": 0.85})  # Roughness
add_material_expression(material, "Constant", 0, 500, {"value": 0.0})   # Metallic
```

### Step 7: Connect to Material Outputs
```
connect_to_material_output(material, lerp2_id, "BaseColor")
connect_to_material_output(material, roughness_id, "Roughness")
connect_to_material_output(material, metallic_id, "Metallic")
```

### Step 8: Recompile and Apply
```
recompile_material(material)
set_landscape_material(material)
```

## Blood & Dust Defaults

**Textures (from Blender scene):**
- Rocky terrain: `/Game/Textures/Ground/T_Rocky_Terrain_D` (dominant, slopes)
- Brown mud: `/Game/Textures/Ground/T_Brown_Mud_D` (flat areas, base)
- Dry grass: `/Game/Textures/Ground/T_Grass_Dry_D` (sparse variation)

**Material Parameters:**
- LandscapeLayerCoords mapping_scale: 5.0
- WorldAlignedBlend bias: -0.5, sharpness: 10.0
- Roughness: 0.85 constant
- Metallic: 0.0 constant

**Landscape Info:**
- Size: 25200 x 25200 units (252m x 252m)
- Components: 16 (4x4 grid of 63m components)
- Scale: 100, 100, 100

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Shader compiler crash | You used LandscapeLayerBlend — delete material, use this skill instead |
| Mirror/reflective surface | Roughness not connected or = 0. Add Constant 0.85 → Roughness |
| Checkerboard pattern | Missing layer weights. Use auto-blend (this skill), not layer painting |
| Textures too large/small | Adjust LandscapeLayerCoords mapping_scale (lower = more tiles) |
| No slope blending | WorldAlignedBlend not connected to Lerp alpha |
| All one texture | Blend bias too extreme. Try -0.3 with sharpness 8.0 |
| Visible tiling | Use different mapping_scale per texture layer via separate LandscapeLayerCoords |

## Required MCP Expression Types
These must exist in the C++ plugin's `CreateExpression()`:
- [x] TextureSample
- [x] TextureCoordinate
- [x] Lerp (LinearInterpolate)
- [x] Constant
- [x] WorldPosition
- [x] ComponentMask
- [x] Multiply, Divide, Add
- [x] **LandscapeLayerCoords** — Added with mapping_scale, mapping_rotation, mapping_pan_u, mapping_pan_v, mapping_type params
- [x] **VertexNormalWS** — Added for slope-based blending (replaces WorldAlignedBlend material function)
- [x] **PixelNormalWS** — Added
- [x] **DotProduct** — Added with inputs "a", "b"
- [x] **Saturate** — Added with input "input"
- [x] **Power** — Already existed, added const_exponent param

## Sources
- Epic docs: Landscape Materials in Unreal Engine
- Epic forums: UE5.5 layered landscape material crash reports
- Epic forums: Setting SamplerType crashes from code
- WorldAlignedBlend: slope threshold + sharpness for auto-material
- LandscapeLayerCoords: proper landscape UV mapping with MappingScale
