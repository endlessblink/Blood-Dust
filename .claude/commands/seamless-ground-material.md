---
description: Create seamless multi-texture blended ground materials using MCP
---

# Seamless Ground Material Creator

<seamless-ground-material>

## Objective

Create a seamless, non-tiling ground material in Unreal Engine using World Position-based blending with multiple textures. This eliminates visible texture repetition across large surfaces.

## Prerequisites

Verify MCP connection to Unreal Engine:
```
mcp__unrealMCP__get_actors_in_level
```

## Parameters

Parse from user input or use defaults:
- **material_name**: Name for the material (default: "M_GroundBlend")
- **texture_paths**: Array of 2-4 texture asset paths
- **tiling_scales**: Array of tiling values per texture (default: [0.08, 0.11, 0.09, 0.12])
- **blend_scale**: World position divisor (default: 5000)
- **roughness**: Material roughness (default: 0.85)
- **target_actor**: Optional actor to apply material to

## Workflow

### Step 1: Create Material Asset

```python
create_material_asset(material_name, "/Game/Materials/")
```

### Step 2: Build Texture Sample Chain

For each texture:
1. Create TextureCoordinate with unique tiling
2. Create TextureSample with texture path
3. Connect TexCoord → TextureSample coordinates

Position nodes in a vertical stack (-800, 0), (-800, 300), etc.

### Step 3: Create World Position Blend Chain

Build the alpha blend calculator:
```
WorldPosition → ComponentMask(RG) → Divide(blend_scale) → Sine → Add(0.5)
```

This creates a smooth, non-repeating blend pattern based on world XY.

### Step 4: Chain Lerps

Connect textures through Lerp cascade:
- Lerp1: tex1 + tex2, alpha = blend
- Lerp2: Lerp1 + tex3, alpha = blend * 0.7
- Lerp3: Lerp2 + tex4, alpha = blend * 0.5

### Step 5: Connect to Material Outputs

```python
connect_to_material_output(material_path, lerp3_id, "BaseColor")
connect_to_material_output(material_path, roughness_id, "Roughness")
```

### Step 6: Recompile and Apply

```python
recompile_material(material_path)
# Optional: apply to actor
apply_material_to_actor(target_actor, material_path)
```

## Blood & Dust Defaults

If no parameters provided, use these project-specific values:

**Textures:**
- `/Game/assets/material/textures/ground_textures/ground_1`
- `/Game/assets/material/textures/ground_textures/ground_2`
- `/Game/assets/material/textures/ground_textures/ground_4`
- `/Game/assets/material/textures/ground_textures/ground_5`

**Floor Actor:** `Floor_UAID_F4A475FF15A3736A02_1961940706`

**Color Palette:** Dutch Golden Age ochre/earth tones

## Success Criteria

- [ ] Material created in Content Browser
- [ ] No visible tiling at any zoom level
- [ ] Smooth transitions between textures
- [ ] Applied to target actor (if specified)

## Troubleshooting

**"Expression not found"**: Use the expression_id returned from add_material_expression, not a custom name.

**Black material**: Check texture paths are valid, verify TextureCoordinate connections.

**Visible seams**: Increase blend_scale or adjust tiling_scales for more variation.

</seamless-ground-material>
