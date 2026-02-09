# Unreal Material Builder via MCP

## Overview
Reliable, efficient material creation in Unreal Engine 5 via MCP tools.
Covers UV generation, natural ground patterns, Material Instances, and mandatory verification checkpoints.

## Trigger Phrases
- "build material"
- "create material"
- "fix material"
- "natural ground"
- "material nodes"
- "material instance"

## IRON RULES

1. **NEVER build more than 10 nodes without recompiling + taking a screenshot to verify**
2. **NEVER use LandscapeLayerCoords for UV generation** — mapping_scale does NOT persist through editor restart
3. **ALWAYS use WorldPosition x Constant for UV scaling** — Constants always persist
4. **ALWAYS show the user a screenshot after each phase** before proceeding
5. **NEVER add nodes to fix a broken material** — diagnose the actual problem first
6. **KEEP materials under 40 nodes** — more = harder to debug, more fragile
7. **All MCP calls strictly sequential** — never parallel
8. **Fix the existing material when possible** — don't rebuild from scratch unless truly necessary

## Phase-Based Workflow

### PHASE 0: Diagnose (MANDATORY before any changes)
```
1. get_material_info(material_path) — check what's connected
2. get_material_graph(material_path) — count nodes, check IDs
3. take_screenshot() — see current visual state
4. REPORT to user: node count, what's connected, what's broken
5. WAIT for user approval before making changes
```

### PHASE 1: UV System (max 8 nodes)
Build the UV generation system. This is the foundation - get it right first.

**Pattern: WorldPosition UVs (RELIABLE)**
```
WorldPosition → ComponentMask(R,G) → Multiply(Constant) → TextureSample UV
```

**UV Scale Reference (WorldPosition x Constant):**
| Scale Constant | Tile Size | Use For |
|---------------|-----------|---------|
| 0.001 | ~10m | Large ground textures |
| 0.002 | ~5m | Standard ground detail |
| 0.003 | ~3.3m | Pebbles, small detail |
| 0.005 | ~2m | Fine rubble/gravel |
| 0.0002 | ~50m | Noise masks (large organic shapes) |

**Node budget: 1 WorldPosition + 1 ComponentMask + N x (Constant + Multiply)**
- 2 UV scales = 6 nodes
- 3 UV scales = 8 nodes
- 4 UV scales = 10 nodes

**CHECKPOINT: Recompile + screenshot. Do textures tile at correct scale?**

### PHASE 2: Core Textures (max 8-12 TextureSample nodes)
Add TextureSample nodes and connect UVs. Set sampler_type for normals.

**Sampler types:**
| Texture Type | sampler_type | sRGB |
|-------------|-------------|------|
| Diffuse/BaseColor | "Color" | ON |
| Normal map | "Normal" | OFF |
| Roughness/Masks | "Masks" | OFF |
| ARM/ORM packed | "Masks" | OFF |

**Sampler budget: Max 13/16 used.** Leave 3 spare for future additions.

**CHECKPOINT: Recompile. No shader errors?**

### PHASE 3: Blend Logic (max 10-15 nodes)
Create the blend chain. Each layer = 1 Lerp + 1-3 mask nodes.

**Slope Detection Pattern (5 nodes):**
```
VertexNormalWS → ComponentMask(B) → Abs → Power(SlopeSharpness) → slope_mask
ScalarParameter("SlopeSharpness", default=3.0)
```
- slope_mask: 1 = flat, 0 = steep cliff

**Noise Masking Pattern (per channel, 2-3 nodes):**
```
noise_TextureSample → ComponentMask(R/G/B) → [optional Power for sharpness]
```
- Use different RGB channels for different layers
- Power > 1 = sharper mask edges (3=moderate, 6=sharp)

**Layer Blend Pattern (3 nodes per layer):**
```
ScalarParameter("LayerAmount", default=0.0-0.5)
Multiply(noise_channel x LayerAmount) → Lerp alpha
Lerp(previous_result, new_texture, alpha) → next in chain
```

**BaseColor chain order:**
```
Rock ─┐
      Lerp(slope) → Lerp(grass) → Lerp(rubble) → Lerp(dirt) → Lerp(puddle) → BaseColor
Mud  ─┘
```

**CHECKPOINT: Recompile + screenshot. Does slope blending work? Are layers visible?**

### PHASE 4: Normal Chain (mirror BaseColor chain)
Same Lerp cascade structure but with normal map TextureSamples.

**Normal Strength Pattern:**
```
Constant3Vector(0,0,1) ─── Lerp(alpha=NormalStrength) ── Normal output
blended_normals ─────────┘
ScalarParameter("NormalStrength", default=1.0)
```

**Puddle Normal Flatten:**
```
normal_result ──── Lerp(alpha=puddle_mask) ── final Normal
flat_normal(0,0,1)┘
```

**CHECKPOINT: Recompile + screenshot. Do normals look correct?**

### PHASE 5: Roughness & Metallic + Output Connections
```
ScalarParameter("Roughness", 0.85) ─── Lerp(alpha=puddle_mask) ── Roughness output
Constant(0.02) ── wet roughness ────┘

ScalarParameter("Metallic", 0.0) ── Metallic output
```

**CHECKPOINT: Recompile + screenshot. Final visual check.**

## Natural Ground Recipe (Blood & Dust)

**Target: Non-repetitive arid landscape**

| Layer | Texture | UV Scale | Mask Source | Parameter |
|-------|---------|----------|-------------|-----------|
| Base (steep) | T_Rocky_Terrain | 0.002 | slope_mask | SlopeSharpness |
| Base (flat) | T_Brown_Mud | 0.002 | slope_mask | — |
| Grass | T_Grass_Dry | 0.002 | noise R | GrassAmount (0.5) |
| Rubble | T_Rocky_Terrain | 0.005 | noise G inverted | RubbleAmount (0.3) |
| Pebbles | T_Pebbles | 0.003 | noise G | PebbleAmount (0.15) |
| Dirt | T_Sand | 0.002 | noise R ^ Power(3) | DirtAmount (0.0) |
| Puddle | Constant dark color | — | noise B ^ Power(6) | PuddleAmount (0.0) |

**Total: ~10 textures, ~35-40 nodes, 7-9 MI parameters**

**Anti-Tiling Techniques:**
- Different UV scales per layer break up visible repetition
- Noise mask at very large scale (0.0002) creates organic 50m patches
- Multiple overlapping layers hide individual texture tiling
- Slope blending adds natural variation tied to geometry

## Material Instance Workflow

### When to use MI vs editing the material:
| Need | Approach |
|------|----------|
| Change blend amounts | MI: set scalar parameter |
| Change UV scale | MUST edit material (replace Constant node) |
| Add new texture layer | MUST edit material (add nodes) |
| Tweak roughness/metallic | MI: set scalar parameter |
| Test different values | MI: iterate on parameters |

### MI Creation & Application:
```
1. create_material_instance("MI_Name", "/Game/Materials/", parent_material_path)
2. set_material_instance_parameter(mi_path, "ParamName", value)
3. set_landscape_material(mi_path)  # applies to ALL landscape proxies
```

### MI Parameter Naming Convention:
All ScalarParameters in the parent material become MI-editable.
Use clear names: SlopeSharpness, GrassAmount, RubbleAmount, DirtAmount, PuddleAmount, NormalStrength, Roughness, Metallic.

## Fixing Broken Materials

### Decision Tree:
```
Material looks wrong?
├── Take screenshot first!
├── Check get_material_info — are outputs connected?
│   └── NO → connect_to_material_output (1 call)
├── Check get_material_graph — how many nodes?
│   ├── > 50 nodes → Consider clean rebuild
│   └── < 50 nodes → Fix in place
├── UV scaling wrong (grid/tiling)?
│   ├── Has LandscapeLayerCoords? → DELETE them, replace with WorldPosition UVs
│   └── Has WorldPosition UVs? → Check Constant values
├── Features not visible (no puddles etc)?
│   └── Check MI parameters — DirtAmount/PuddleAmount probably = 0
└── Still broken after above?
    └── STOP. Show user the screenshot. Discuss before proceeding.
```

### Minimal UV Fix (replace LandscapeLayerCoords):
```
1. Identify which TextureSamples connect to which LayerCoords
2. Delete LayerCoords nodes
3. Add WorldPosition → ComponentMask(R,G)
4. Add Constant + Multiply per UV scale needed
5. Reconnect to TextureSample UV inputs
6. Recompile + screenshot
```
**Estimated calls: 6 deletes + 8-10 adds + 10-12 connects + 1 recompile = ~30 calls**

## NEVER Do These

| Bad Pattern | Why | Do Instead |
|------------|-----|------------|
| Build 50+ nodes without recompiling | Can't verify anything works | Recompile every 10 nodes |
| Use LandscapeLayerCoords for UVs | mapping_scale doesn't persist | WorldPosition x Constant |
| Add nodes to "fix" a tangled graph | Makes it worse | Diagnose first, minimal fix |
| Build M_v2, M_v3, M_v4... | Wastes time, old versions clutter project | Fix the existing material |
| Set MI params without verifying material works | MI can't fix broken graph | Verify material compiles clean first |
| Skip screenshots between phases | Flying blind | ALWAYS screenshot to verify |
| Use LandscapeLayerBlend | Crashes shader compiler | Lerp cascade with slope mask |
| Import textures mid-material-build | Engine instability | Import all textures BEFORE material work |

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| Grid/tiling pattern | UV scale wrong or reverted | Check Constants, replace LayerCoords |
| All one color | Lerp alphas all 0 or all 1 | Check MI params, check slope mask |
| Black landscape | No BaseColor connected | connect_to_material_output |
| Mirror/reflective | Roughness = 0 | Set to 0.85 |
| Features invisible | Amount parameters = 0 | Set in MI: DirtAmount, PuddleAmount > 0 |
| Shader compile error | Too many samplers or bad connections | Check sampler count < 16 |
| Material not on landscape | MI not applied to all proxies | Use set_landscape_material (hits all proxies) |
