# Seamless Ground Material Skill

## Overview
Creates multi-texture blended ground materials in Unreal Engine using the Material Graph MCP tools. Produces seamless, non-tiling ground surfaces using World Position-based blending with Sine smoothing.

## Trigger Phrases
- "seamless ground material"
- "create ground blend"
- "multi-texture ground"
- "no-tile ground material"

## Required MCP Tools
These tools must be available via UnrealMCP:
- `add_material_expression`
- `connect_material_expressions`
- `connect_to_material_output`
- `recompile_material`
- `apply_material_to_actor` (optional)

## Workflow

### Step 1: Create Material Asset
```python
create_material_asset("M_GroundBlend", "/Game/Materials/")
```

### Step 2: Add Texture Samples with TexCoords
For each texture, add a TextureSample and TextureCoordinate pair with different tiling scales:

| Texture | TexCoord Tiling | Position (X, Y) |
|---------|-----------------|-----------------|
| ground_1 | 0.08 | (-800, 0) |
| ground_2 | 0.11 | (-800, 300) |
| ground_4 | 0.09 | (-800, 600) |
| ground_5 | 0.12 | (-800, 900) |

```python
# Example for one texture
texcoord1 = add_material_expression(material_path, "TextureCoordinate", -1000, 0,
    {"u_tiling": 0.08, "v_tiling": 0.08})
tex1 = add_material_expression(material_path, "TextureSample", -800, 0,
    {"texture_path": "/Game/assets/material/textures/ground_textures/ground_1"})
connect_material_expressions(material_path, texcoord1["expression_id"],
    tex1["expression_id"], "coordinates")
```

### Step 3: Create World Position Blend Alpha
```
WorldPosition → ComponentMask(RG) → Divide(5000) → Sine → Add(0.5) → BlendAlpha
```

```python
worldpos = add_material_expression(material_path, "WorldPosition", -600, 1200)
mask = add_material_expression(material_path, "ComponentMask", -400, 1200,
    {"r": True, "g": True, "b": False, "a": False})
divide = add_material_expression(material_path, "Divide", -200, 1200,
    {"const_b": 5000.0})
sine = add_material_expression(material_path, "Sine", 0, 1200)
add_half = add_material_expression(material_path, "Add", 200, 1200,
    {"const_b": 0.5})

# Connect the chain
connect_material_expressions(material_path, worldpos["expression_id"],
    mask["expression_id"], "input")
connect_material_expressions(material_path, mask["expression_id"],
    divide["expression_id"], "a")
connect_material_expressions(material_path, divide["expression_id"],
    sine["expression_id"], "input")
connect_material_expressions(material_path, sine["expression_id"],
    add_half["expression_id"], "a")
```

### Step 4: Chain Lerps for 4-Texture Blend
```
Lerp1: tex1 + tex2 → BlendAlpha
Lerp2: Lerp1 + tex3 → BlendAlpha * 0.7
Lerp3: Lerp2 + tex4 → BlendAlpha * 0.5 → BaseColor
```

```python
lerp1 = add_material_expression(material_path, "Lerp", -200, 150)
lerp2 = add_material_expression(material_path, "Lerp", 0, 300)
lerp3 = add_material_expression(material_path, "Lerp", 200, 450)

# Connect textures to lerps
connect_material_expressions(material_path, tex1_id, lerp1["expression_id"], "a")
connect_material_expressions(material_path, tex2_id, lerp1["expression_id"], "b")
connect_material_expressions(material_path, blend_alpha_id, lerp1["expression_id"], "alpha")

connect_material_expressions(material_path, lerp1["expression_id"], lerp2["expression_id"], "a")
connect_material_expressions(material_path, tex3_id, lerp2["expression_id"], "b")
# ... continue chain

# Connect final lerp to BaseColor
connect_to_material_output(material_path, lerp3["expression_id"], "BaseColor")
```

### Step 5: Add Roughness
```python
roughness = add_material_expression(material_path, "Constant", 400, 100, {"value": 0.85})
connect_to_material_output(material_path, roughness["expression_id"], "Roughness")
```

### Step 6: Recompile and Apply
```python
recompile_material(material_path)
apply_material_to_actor("Floor_ActorName", material_path)
```

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| material_name | string | "M_GroundBlend" | Name for the new material |
| material_path | string | "/Game/Materials/" | Content browser path |
| texture_paths | list[str] | - | Paths to 2-4 ground textures |
| tiling_scales | list[float] | [0.08, 0.11, 0.09, 0.12] | TexCoord tiling per texture |
| blend_scale | float | 5000.0 | World position divisor for blend |
| roughness | float | 0.85 | Material roughness |
| target_actor | string | None | Optional actor to apply material to |

## Visual Reference
```
                    ┌─────────────────────────────────────────┐
                    │         WORLD POSITION BLENDING          │
                    ├─────────────────────────────────────────┤
                    │                                          │
                    │  WorldPosition ──► ComponentMask(RG)     │
                    │                         │                │
                    │                         ▼                │
                    │                   Divide(5000)           │
                    │                         │                │
                    │                         ▼                │
                    │                      Sine ──► Add(0.5)   │
                    │                                  │       │
                    │                                  ▼       │
                    │                            BlendAlpha    │
                    │                                          │
                    │  tex_1 ──┐                               │
                    │  tex_2 ──┼──► Lerp1 ──┐                  │
                    │                       │                  │
                    │  tex_3 ──────────────┼──► Lerp2 ──┐     │
                    │                       │           │      │
                    │  tex_4 ──────────────────────────┼──► Lerp3 ──► BaseColor
                    │                                          │
                    └─────────────────────────────────────────┘
```

## Blood & Dust Project Specifics

**Texture Locations:**
- `/Game/assets/material/textures/ground_textures/ground_1`
- `/Game/assets/material/textures/ground_textures/ground_2`
- `/Game/assets/material/textures/ground_textures/ground_4`
- `/Game/assets/material/textures/ground_textures/ground_5`

**Floor Actor:** `Floor_UAID_F4A475FF15A3736A02_1961940706`

**Color Palette (Rembrandt):**
- Dark: `#2D261E` (0.176, 0.149, 0.118)
- Mid: `#8C6D46` (0.549, 0.427, 0.275)
- Golden: `#B37233` (0.702, 0.447, 0.200)

## Troubleshooting

### Material Not Updating
- Call `recompile_material()` after all changes
- Check Unreal's Output Log for shader compilation errors

### Visible Seams
- Increase `blend_scale` (try 10000)
- Use more varied tiling scales
- Ensure textures are seamless/tileable

### Black Material
- Verify texture paths are correct
- Check that TextureCoordinate is connected to TextureSample

## Source
Created for Blood & Dust arena - Dutch Golden Age aesthetic with chiaroscuro lighting.
