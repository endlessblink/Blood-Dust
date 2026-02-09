# Unreal Landscape Material via MCP

## Overview
Create a proper multi-texture blended landscape material in Unreal Engine 5 using MCP tools.
Uses **UV noise distortion + fixed-angle rotation + dissolve blend** for anti-tiling,
**macro brightness variation** for large-scale color variety,
and **slope-based auto-blending** with Lerp cascades for rock/mud/grass distribution.

**Anti-tiling technique (3 layers):**
1. UV noise distortion warps coordinates BEFORE sampling (breaks tile grid position)
2. Fixed 37.5 degree rotation gives a second uncorrelated UV set (breaks tile orientation)
3. Dissolve blend with low-freq noise lerps between original and rotated samples (hides seams)

## Trigger Phrases
- "landscape material"
- "ground material"
- "ground texture"
- "terrain material"
- "landscape ground"

## CRITICAL RULES

1. **NEVER use LandscapeLayerBlend** via MCP -- causes `TextureReferenceIndex != INDEX_NONE` crash
2. **NEVER use TextureSampleParameter2D** -- use plain TextureSample with texture_path
3. **NEVER use LandscapeLayerCoords for UV generation** -- mapping_scale does NOT persist through editor restart
4. **NEVER use texture bombing** (sampling same texture twice at offset UVs) -- noise frequency matches tile frequency, still shows visible grid
5. **NEVER use noise-driven rotation** -- per-pixel varying rotation creates swirl/spiral artifacts
6. **Use fixed-angle rotation + dissolve** -- constant rotation angle (37.5 deg) + UV offset + noise dissolve blend
7. **All MCP calls strictly sequential** -- never parallel
8. **One-shot `create_landscape_material` tool** is the preferred method -- creates entire graph in a single MCP call

## Material Architecture (71 nodes, 12 samplers)

```
SECTION 1: Base UVs (3 nodes)
  WorldPosition -> MaskXY(R,G) -> Multiply(DetailUVScale=0.004) -> BaseUV

SECTION 2: UV Distortion (10 nodes)
  WarpNoiseX = Noise(WorldPos, scale=0.002, 4 levels, output -1..1)
  WarpNoiseY = Noise(WorldPos+offset(1000,2000,0), scale=0.002, 4 levels, output -1..1)
  WarpX = WarpNoiseX * WarpAmount(0.12)
  WarpY = WarpNoiseY * WarpAmount(0.12)
  DistortedUV = BaseUV + AppendVector(WarpX, WarpY)

SECTION 2B: Fixed Rotation + Dissolve (15 nodes)
  Split DistortedUV -> U, V
  RotU = U * cos(37.5) - V * sin(37.5)    [cos=0.7934, sin=0.6088 as constants]
  RotV = U * sin(37.5) + V * cos(37.5)
  RotatedUV = AppendVector(RotU, RotV) + Constant2Vector(0.5, 0.5)
  BlendNoise = Noise(WorldPos+offset(3000,5000,0), scale=0.0003, 3 levels, output 0..1)

SECTION 3: Macro Brightness Variation (4 nodes)
  MacroNoise = Noise(WorldPos, scale=0.00003, 4 levels, output 0.5..1.0)
  MacroMod = Lerp(1.0, MacroNoise, MacroStrength=0.4)

SECTION 4: Per-Layer Textures with Rotation Dissolve (4 samplers each = 12 total)
  Rock:  Lerp(TexSample(Rock_D, DistortedUV), TexSample(Rock_D, RotatedUV), BlendNoise) * MacroMod
         Lerp(TexSample(Rock_N, DistortedUV), TexSample(Rock_N, RotatedUV), BlendNoise)
  Mud:   same pattern
  Grass: same pattern

SECTION 5: Slope Detection (5 nodes)
  VertexNormalWS -> MaskZ -> Abs -> Power(SlopeSharpness=3) -> SlopeMask

SECTION 6: Grass Mask (6 nodes)
  GrassNoise(WorldPos, scale=0.0001, turbulence) -> Power(2) * GrassAmount
  SlopeGrassMask = GrassMask * SlopePow  (grass only on flat areas)

SECTION 7: Blend Chains -> Outputs
  Lerp(Rock, Mud, SlopeMask) -> Lerp(result, Grass, SlopeGrassMask) -> BaseColor
  Same for Normals -> Normal
  Roughness param (0.85) -> Roughness
  Metallic const (0.0) -> Metallic
```

### Why Fixed-Angle Rotation Works

**UV distortion alone (INSUFFICIENT)**: Warps sampling position but texture orientation repeats.
At medium-to-far distances the repeating orientation is still visible.

**Noise-driven rotation (WRONG)**: Per-pixel rotation from noise creates angular gradients visible
as swirl/spiral artifacts. Transitions between rotation angles are obvious.

**Fixed-angle rotation + dissolve (CORRECT)**: Sample texture at two orientations (original + 37.5 deg).
37.5 deg is irrational w.r.t. 90 deg tile grid so the rotated grid never aligns with the original.
+0.5,+0.5 UV offset decorrelates the two grids spatially. Low-frequency dissolve noise (scale=0.0003)
creates large smooth patches where each orientation dominates.

## Exact Reproduction Workflow (COPY-PASTE)

### Step 1: Create Material
```python
create_landscape_material("M_Landscape_Distorted",
    rock_d="/Game/Textures/Ground/T_Rocky_Terrain_D",
    rock_n="/Game/Textures/Ground/T_Rocky_Terrain_N",
    mud_d="/Game/Textures/Ground/T_Brown_Mud_D",
    mud_n="/Game/Textures/Ground/T_Brown_Mud_N",
    grass_d="/Game/Textures/Ground/T_Grass_Dry_D",
    grass_n="/Game/Textures/Ground/T_Grass_Dry_N",
    detail_uv_scale=0.004,
    warp_scale=0.002,
    warp_amount=0.12,
    macro_scale=3e-05,
    macro_strength=0.4,
    slope_sharpness=3,
    grass_amount=0.5,
    roughness=0.85)
```

### Step 2: Create Material Instance (with all params set)
```python
create_material_instance("MI_Landscape_Ground_v6",
    parent_material="/Game/Materials/M_Landscape_Distorted",
    path="/Game/Materials/",
    scalar_parameters={
        "DetailUVScale": 0.004,
        "WarpAmount": 0.12,
        "MacroStrength": 0.4,
        "SlopeSharpness": 3,
        "GrassAmount": 0.5,
        "Roughness": 0.85
    })
```

### Step 3: Apply to Landscape
```python
set_landscape_material("/Game/Materials/MI_Landscape_Ground_v6")
```

### Step 4: Verify
```python
take_screenshot()
# Check: no visible tiling grid, no swirls, natural variation, proper slope blending
```

## All Parameters

### MI-Editable Parameters (6 total, tuneable in viewport Details panel)

| Parameter | Default | Range | Effect |
|-----------|---------|-------|--------|
| DetailUVScale | 0.004 | 0.001-0.01 | Texture tile size (smaller = bigger tiles) |
| WarpAmount | 0.12 | 0.0-0.3 | UV distortion strength |
| MacroStrength | 0.4 | 0.0-1.0 | Large-scale brightness variation |
| SlopeSharpness | 3.0 | 1.0-10.0 | Rock/mud transition sharpness |
| GrassAmount | 0.5 | 0.0-1.0 | Grass overlay intensity |
| Roughness | 0.85 | 0.0-1.0 | Surface roughness |

### Build-Time Parameters (set during creation, baked into noise nodes)

| Parameter | Default | What It Does |
|-----------|---------|-------------|
| warp_scale | 0.002 | UV noise distortion frequency |
| macro_scale | 0.00003 | Large-scale brightness noise frequency |
| Rotation angle | 37.5 deg | Fixed rotation (sin=0.6088, cos=0.7934) |
| UV offset | 0.5, 0.5 | Decorrelates rotated grid |
| BlendNoise scale | 0.0003 | Dissolve patch size (~3300 units) |

## Sampler Budget

| # | Texture | UV Set | Purpose |
|---|---------|--------|---------|
| 1 | Rock_D | DistortedUV | Rock diffuse (original) |
| 2 | Rock_D | RotatedUV | Rock diffuse (rotated) |
| 3 | Rock_N | DistortedUV | Rock normal (original) |
| 4 | Rock_N | RotatedUV | Rock normal (rotated) |
| 5 | Mud_D | DistortedUV | Mud diffuse (original) |
| 6 | Mud_D | RotatedUV | Mud diffuse (rotated) |
| 7 | Mud_N | DistortedUV | Mud normal (original) |
| 8 | Mud_N | RotatedUV | Mud normal (rotated) |
| 9 | Grass_D | DistortedUV | Grass diffuse (original) |
| 10 | Grass_D | RotatedUV | Grass diffuse (rotated) |
| 11 | Grass_N | DistortedUV | Grass normal (original) |
| 12 | Grass_N | RotatedUV | Grass normal (rotated) |

**12 of 16 samplers used** -- 4 free for future features (puddles, mud detail)

## Graph Layout (8 Color-Coded Comment Boxes)

| # | Label | Color | Content |
|---|-------|-------|---------|
| 1 | UV Generation | Yellow | WorldPos -> BaseUV |
| 2 | UV Noise Distortion | Orange | Warp noise -> DistortedUV |
| 3 | Fixed Rotation + Dissolve | Pink | 37.5 deg rotation -> RotatedUV + BlendNoise |
| 4 | Macro Brightness Variation | Cyan | Low-freq noise -> MacroMod |
| 5 | Rock Layer (slopes) | Red | 4 samplers (orig+rot D, orig+rot N) |
| 6 | Mud Layer (flat areas) | Brown | 4 samplers |
| 7 | Grass Layer (overlay) | Green | 4 samplers |
| 8 | Slope Detection + Outputs | Purple | Slope math + blend chains + outputs |

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Shader compiler crash | Used LandscapeLayerBlend -- delete material, use create_landscape_material instead |
| Mirror/reflective surface | Roughness not connected or = 0. Check Roughness parameter |
| Checkerboard pattern | Shader compilation error -- check for ERROR nodes in material graph |
| Visible tiling grid | WarpAmount too low. Increase to 0.12-0.2 |
| Swirl/spiral patterns | Used noise-driven rotation. Must use FIXED rotation (37.5 deg constants) |
| Blurry/smeared textures | WarpAmount too high -- reduce (try 0.08) |
| All one texture | SlopeSharpness too extreme. Try 2.0-4.0 |
| White spots on slopes | GrassAmount too high + grass not slope-filtered |
| No grass visible | GrassAmount = 0 in MI. Increase in MI panel |
| Uniform appearance | MacroStrength = 0. Increase to 0.3-0.5 |
| MI parent ref broken | Material was recreated. Create NEW MI and re-apply via set_landscape_material |

## NEVER DO These Things

- NEVER use LandscapeLayerBlend (shader compiler crash)
- NEVER use texture bombing / dual-UV sampling (visible grid pattern)
- NEVER use noise-driven rotation (swirl artifacts)
- NEVER use LandscapeLayerCoords (mapping_scale doesn't persist)
- NEVER call MCP tools in parallel
- NEVER add extra nodes to materials created by create_landscape_material (breaks UV connections)
- NEVER import textures back-to-back without breathing room

## Iteration History (2026-02-09)

| Version | Approach | Result |
|---------|----------|--------|
| v1 | Texture bombing | Grid visible (noise freq = tile freq) |
| v2 | UV distortion only (scale=0.00005, amount=0.05) | UV shift 0.06%, invisible |
| v3 | UV distortion (scale=0.002, amount=0.12) | Better but orientation repetition |
| v4 | + noise-driven rotation | Swirl artifacts |
| v5/v6 | + fixed 37.5 deg rotation + dissolve | SUCCESS |

## Current State (2026-02-09)
- Material: `/Game/Materials/M_Landscape_Distorted` (71 nodes, 12 samplers)
- MI: `/Game/Materials/MI_Landscape_Ground_v6` (6 exposed ScalarParameters)
- Applied to all 5 landscape sections
- C++ tool: `HandleCreateLandscapeMaterial()` in `EpicUnrealMCPEditorCommands.cpp`
