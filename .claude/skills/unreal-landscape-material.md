# Unreal Landscape Material via MCP

## Overview
Create a proper multi-texture blended landscape material in Unreal Engine 5 using MCP tools.
Uses **UV noise distortion** for anti-tiling, **macro brightness variation** for large-scale color variety,
and **slope-based auto-blending** with Lerp cascades for rock/mud/grass distribution.

**Anti-tiling technique**: Continuous noise warps UV coordinates BEFORE texture sampling,
breaking the tile grid at every pixel. This is the industry-standard approach used in AAA games.

## Trigger Phrases
- "landscape material"
- "ground material"
- "ground texture"
- "terrain material"
- "landscape ground"

## CRITICAL RULES

1. **NEVER use LandscapeLayerBlend** via MCP — causes `TextureReferenceIndex != INDEX_NONE` crash in HLSLMaterialTranslator.cpp
2. **NEVER use TextureSampleParameter2D** — use plain TextureSample with texture_path
3. **NEVER use LandscapeLayerCoords for UV generation** — mapping_scale does NOT persist through editor restart. Use WorldPosition x Constant instead
4. **NEVER use texture bombing** (sampling same texture twice at offset UVs) — noise frequency matches tile frequency, still shows visible grid
5. **Use UV noise distortion** — warp UVs with continuous noise BEFORE sampling, every pixel gets unique UV offset
6. **All MCP calls strictly sequential** — never parallel
7. **Roughness = constant 0.85, Metallic = constant 0** unless you have ORM/ARM packed textures
8. **sRGB OFF** for normal maps, roughness, ARM/ORM textures
9. **sRGB ON** for diffuse/base color textures only
10. **One-shot `create_landscape_material` tool** is the preferred method — creates entire graph in a single MCP call

## Prerequisites
- UnrealMCP server running (C++ plugin rebuilt with UV distortion support)
- Ground textures imported to `/Game/Textures/Ground/` with correct compression:
  - Diffuse: Default compression, sRGB=true
  - Normal: Normalmap compression, sRGB=false, flip_green=true (Blender OpenGL -> UE DirectX)
  - Roughness: Masks compression, sRGB=false
  - ARM: Masks compression, sRGB=false

## Material Architecture (UV Noise Distortion)

```
SECTION 1: Base UVs
  WorldPosition -> MaskXY(R,G) -> Multiply(0.004) -> BaseUV

SECTION 2: UV Distortion (breaks tiling at every pixel)
  WarpNoiseX = Noise(WorldPos, scale=0.00005, 4 levels, output -1..1)
  WarpNoiseY = Noise(WorldPos+offset, scale=0.00005, 4 levels, output -1..1)
  WarpX = WarpNoiseX * WarpAmount(0.05)
  WarpY = WarpNoiseY * WarpAmount(0.05)
  DistortedUV = BaseUV + AppendVector(WarpX, WarpY)

SECTION 3: Macro Brightness Variation
  MacroNoise = Noise(WorldPos, scale=0.00003, 4 levels, output 0.5..1.0)
  MacroMod = Lerp(1.0, MacroNoise, MacroStrength=0.4)

SECTION 4: Per-Layer Textures (2 samplers each = 6 total)
  Rock:  TextureSample(Rock_D, DistortedUV) * MacroMod -> RockDiffuse
         TextureSample(Rock_N, DistortedUV)             -> RockNormal
  Mud:   TextureSample(Mud_D, DistortedUV) * MacroMod  -> MudDiffuse
         TextureSample(Mud_N, DistortedUV)              -> MudNormal
  Grass: TextureSample(Grass_D, DistortedUV) * MacroMod -> GrassDiffuse
         TextureSample(Grass_N, DistortedUV)             -> GrassNormal

SECTION 5: Slope Detection
  VertexNormalWS -> MaskZ -> Abs -> Power(SlopeSharpness=3) -> SlopeMask

SECTION 6: Grass Mask (separate noise from UV warp)
  GrassNoise(WorldPos, scale=0.0001, 3 levels, turbulence) -> Power(2) * GrassAmount
  SlopeGrassMask = GrassMask * SlopePow  (grass only on flat areas)

SECTION 7: Blend Chains -> Outputs
  Lerp(Rock, Mud, SlopeMask) -> Lerp(result, Grass, SlopeGrassMask) -> BaseColor
  Lerp(RockN, MudN, SlopeMask) -> Lerp(result, GrassN, SlopeGrassMask) -> Normal
  Roughness param (0.85) -> Roughness
  Metallic const (0.0) -> Metallic
```

### Why UV Distortion Works (vs Texture Bombing)

**Texture bombing (WRONG)**: Sample twice at fixed UV offset, blend with noise.
Problem: Noise blob size = tile size, so each blob covers exactly 1 tile = visible grid.

**UV distortion (CORRECT)**: Warp UV coordinates with continuous noise BEFORE sampling.
Every pixel gets a unique, continuously varying UV offset. The noise operates at a
frequency 80x lower than the tile frequency, creating smooth, natural-looking variation.

Key parameters:
- `warp_scale` = 0.00005 (noise frequency, must be ~80x smaller than detail_uv_scale)
- `warp_amount` = 0.05 (distortion strength in UV space, ~5% of a tile)
- Two noise nodes with offset positions give different X and Y warp patterns

## One-Shot Workflow (Recommended)

### Step 1: Create Material
```python
create_landscape_material("M_Landscape_Final",
    rock_d="/Game/Textures/Ground/T_Rocky_Terrain_D",
    rock_n="/Game/Textures/Ground/T_Rocky_Terrain_N",
    mud_d="/Game/Textures/Ground/T_Brown_Mud_D",
    mud_n="/Game/Textures/Ground/T_Brown_Mud_N",
    grass_d="/Game/Textures/Ground/T_Grass_Dry_D",
    grass_n="/Game/Textures/Ground/T_Grass_Dry_N")
```

### Step 2: Create Material Instance
```python
create_material_instance("MI_Landscape_Final",
    parent_path="/Game/Materials/M_Landscape_Final",
    path="/Game/Materials/")
```

### Step 3: Apply to Landscape
```python
set_landscape_material("/Game/Materials/MI_Landscape_Final")
```

### Step 4: Verify
```python
take_screenshot()
# Check: no visible tiling grid, natural variation, proper slope blending
```

## Parameters (MI-Editable)

| Parameter | Default | Range | Effect |
|-----------|---------|-------|--------|
| MacroStrength | 0.4 | 0-1 | Large-scale brightness variation intensity |
| SlopeSharpness | 3.0 | 1-10 | Steepness of rock/mud transition |
| GrassAmount | 0.5 | 0-1 | Grass overlay coverage |
| Roughness | 0.85 | 0-1 | Surface roughness |

## Build Parameters (set during creation)

| Parameter | Default | What It Does |
|-----------|---------|-------------|
| detail_uv_scale | 0.004 | Tile frequency (WorldPos multiplier) |
| warp_scale | 0.00005 | UV noise distortion frequency (~80x < detail) |
| warp_amount | 0.05 | UV distortion strength (in UV space) |
| macro_scale | 0.00003 | Large-scale brightness noise frequency |

## Sampler Budget

| # | Texture | UV | Purpose |
|---|---------|-----|---------|
| 1 | Rock_D | DistortedUV | Rock diffuse |
| 2 | Rock_N | DistortedUV | Rock normal |
| 3 | Mud_D | DistortedUV | Mud diffuse |
| 4 | Mud_N | DistortedUV | Mud normal |
| 5 | Grass_D | DistortedUV | Grass diffuse |
| 6 | Grass_N | DistortedUV | Grass normal |

**6 of 16 samplers used** — 10 free for future features (dirt, puddles, pebbles, etc.)

## Graph Layout (7 Color-Coded Comment Boxes)

| # | Label | Color | Content |
|---|-------|-------|---------|
| 1 | UV Generation | Yellow | WorldPos -> BaseUV |
| 2 | UV Noise Distortion | Orange | Warp noise -> DistortedUV |
| 3 | Macro Brightness Variation | Cyan | Low-freq noise -> MacroMod |
| 4 | Rock Layer (slopes) | Red | Rock diffuse + normal samples |
| 5 | Mud Layer (flat areas) | Brown | Mud diffuse + normal samples |
| 6 | Grass Layer (overlay) | Green | Grass diffuse + normal samples |
| 7 | Slope Detection + Outputs | Purple | Slope math + blend chains + outputs |

~35 expression nodes + 7 comment boxes total.

## Blood & Dust Defaults

**Textures:**
- Rocky terrain: `/Game/Textures/Ground/T_Rocky_Terrain_D` / `_N` (slopes)
- Brown mud: `/Game/Textures/Ground/T_Brown_Mud_D` / `_N` (flat areas)
- Dry grass: `/Game/Textures/Ground/T_Grass_Dry_D` / `_N` (sparse overlay)

**Landscape Info:**
- Size: 25200 x 25200 units (252m x 252m)
- Components: 16 (4x4 grid of 63m components)
- Scale: 100, 100, 100

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Shader compiler crash | Used LandscapeLayerBlend — delete material, use create_landscape_material instead |
| Mirror/reflective surface | Roughness not connected or = 0. Check Roughness parameter |
| Checkerboard pattern | Shader compilation error — check for ERROR nodes in material graph |
| Visible tiling grid | Warp amount too low or warp_scale too close to detail_uv_scale. Increase warp_amount or decrease warp_scale |
| Blurry/smeared textures | Warp amount too high — reduce warp_amount (try 0.03) |
| No slope blending | SlopePow not connected to Lerp alpha |
| All one texture | SlopeSharpness too extreme. Try 2.0-4.0 |
| White spots on slopes | GrassAmount too high + grass not slope-filtered. Use SlopeGrassMask |
| No grass visible | GrassAmount = 0 in MI. Increase in MI panel |
| Uniform appearance | MacroStrength = 0. Increase to 0.3-0.5 in MI |

## NEVER DO These Things

- NEVER use LandscapeLayerBlend (shader compiler crash)
- NEVER use texture bombing / dual-UV sampling (visible grid pattern)
- NEVER use LandscapeLayerCoords (mapping_scale doesn't persist)
- NEVER call MCP tools in parallel
- NEVER import textures back-to-back without breathing room
- NEVER skip visual verification after material creation

## Required MCP Expression Types
- [x] WorldPosition
- [x] ComponentMask
- [x] Multiply, Add
- [x] Constant, Constant3Vector
- [x] Noise (GradientALU, Position input, Scale, Levels, Turbulence, OutputMin/Max)
- [x] AppendVector (combines two scalars into float2)
- [x] TextureSample
- [x] LinearInterpolate (Lerp)
- [x] Power, Abs
- [x] VertexNormalWS
- [x] ScalarParameter
- [x] MaterialExpressionComment (comment boxes)
