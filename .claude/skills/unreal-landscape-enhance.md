# Enhance Landscape Material — Dirt Patches, Puddles & Pebble Detail

## Overview

Adds three visual features to an **existing** slope-blended landscape material (M_Landscape_Ground):
1. **Pebble Detail UV** — Give pebbles their own tiling scale so they look distinct from rubble
2. **Dirt Patches** — Large organic soil patches that break up the brown mud layer
3. **Puddles** — Dark reflective wet spots with flat normals and low roughness

Each feature is **off by default** (parameter = 0) so existing look is preserved.
The user controls them from the Material Instance (MI) panel in Unreal.

## Trigger Phrases
- "enhance landscape"
- "add dirt patches"
- "add puddles"
- "landscape enhance"
- "improve ground material"
- "pebble scale"

## Prerequisites

- UnrealMCP server running
- M_Landscape_Ground exists with 44 expressions (the base landscape material)
- MI_Landscape_Ground applied to all landscape sections
- T_Sand_D texture imported at `/Game/Textures/Ground/T_Sand_D` (for soil/dirt patches)

## CRITICAL RULES (Learned from Experience)

1. **ALL MCP calls strictly sequential** — never parallel, wait for each response
2. **NEVER recreate MI_Landscape_Ground** — use `set_material_instance_parameter` to update existing
3. **LandscapeLayerCoords mapping_scale**: LOWER = more tiles (smaller, more detail), HIGHER = fewer tiles (larger, less detail). Scale 5 = standard ground textures, Scale 15 = rubble overlay, Scale 25 = tiny pebbles
4. **ALWAYS create LandscapeLayerCoords with correct mapping_scale** — you cannot reliably change it after creation via set_material_expression_property
5. **Recompile material after EACH phase** — verify visually before proceeding to next phase
6. **After adding ScalarParameters**, push default values to MI via `set_material_instance_parameter`
7. **Lightweight query between heavy operations** — call `does_asset_exist` or `list_assets` to give engine a tick to breathe

## Current Material Architecture (44 Nodes)

```
WHAT YOU SEE IN THE MATERIAL EDITOR:
====================================

LEFT SIDE — Texture Inputs:
  [LayerCoords_1] scale=5  ──UV──> [Rock_D]  [Mud_D]  [Grass_D]     (diffuse)
  [LayerCoords_2] scale=5  ──UV──> [Rock_N]  [Mud_N]  [Grass_N]     (normals)
  [LayerCoords_3] scale=??  ─UV──> [Noise Texture]                   (blend mask)
  [LayerCoords_0] scale=15 ──UV──> [Rubble_D] [Rubble_N] [Pebble_DH] [Pebble_N]

MIDDLE — Blend Logic:
  Slope Detection:   VertexNormalWS → Mask(Z) → Abs → Power(SlopeSharpness) → slope_alpha
  Base Blend:        Rock vs Mud (slope_alpha) → + Grass (GrassAmount)
  Noise Mask:        Noise texture R channel → Saturate
  Rubble Blend:      Rubble overlaid using noise × RubbleAmount
  Pebble Blend:      Pebbles overlaid using noise G × Power × PebbleAmount

  Normal chain mirrors diffuse chain (same blend alphas)
  NormalStrength lerps between blended normal and flat (0,0,1)

RIGHT SIDE — Material Outputs:
  Lerp_7  ──> BaseColor output      (end of diffuse blend chain)
  Lerp_8  ──> Normal output         (end of normal blend chain)
  Roughness param (0.85) ──> Roughness output
  Metallic param (0.0)   ──> Metallic output
```

### Key Node IDs (Reference)

| Node ID | Type | What It Does |
|---------|------|--------------|
| `LandscapeLayerCoords_1` | UV | Scale 5, drives Rock/Mud/Grass diffuse |
| `LandscapeLayerCoords_2` | UV | Scale 5, drives Rock/Mud/Grass normals |
| `LandscapeLayerCoords_3` | UV | Noise texture UV |
| `LandscapeLayerCoords_0` | UV | Scale 15, drives Rubble + Pebble textures |
| `TextureSample_2` | Texture | Pebble diffuse/height (T_Landscape_Pebbles_DH) |
| `TextureSample_10` | Texture | Pebble normal (T_Landscape_Pebbles_N) |
| `TextureSample_6` | Texture | Noise texture (T_Brown_Mud_D at noise scale) |
| `Lerp_7` | Blend | LAST node in BaseColor chain → connected to BaseColor output |
| `Lerp_8` | Blend | LAST node in Normal chain → connected to Normal output |
| `Lerp_4` | Blend | NormalStrength lerp (blended normal → flat normal) |
| `ScalarParameter_1` | Param | Roughness (0.85) → connected to Roughness output |
| `ScalarParameter_2` | Param | Metallic (0.0) → connected to Metallic output |
| `Constant3Vector_0` | Const | Flat normal (0,0,1) — used for normal flattening |

---

## Phase 1: Pebble Scale Fix (3 MCP Calls)

**Goal:** Give pebbles a dedicated UV at scale 25 (smaller tiles = more visible pebble detail)
Currently pebbles share rubble's UV (scale 15) making them look too large.

### What This Does (For Beginners)

In Unreal, a UV coordinate controls how a texture tiles across a surface.
- Scale 5 = texture repeats every 5 landscape quads (standard ground)
- Scale 15 = texture repeats every 15 quads (large rubble overlay)
- Scale 25 = texture repeats every 25 quads (wait — HIGHER means LARGER tiles)

**IMPORTANT**: mapping_scale in LandscapeLayerCoords means "how many quads per tile."
- LOWER number = SMALLER tiles = MORE repetition = MORE detail visible
- HIGHER number = LARGER tiles = LESS repetition = texture covers more ground

So we want pebbles at scale ~2.5 (very small tiles, lots of pebble detail) NOT 25.
But looking at the existing material: rubble is at 15, pebbles share that. We want pebbles to be SMALLER than rubble, so scale ~8 gives smaller pebbles than scale 15.

### Node Layout

```
NEW (Phase 1):
  [LayerCoords_4] scale=8  ──UV──> [TextureSample_2 (Pebble_DH)]
                            ──UV──> [TextureSample_10 (Pebble_N)]
```

### Steps

| # | Tool | What It Does |
|---|------|-------------|
| 1 | `add_material_expression` | Create new LandscapeLayerCoords at (-1400, 3200) with scale=8 → makes pebble UV |
| 2 | `connect_material_expressions` | Wire LayerCoords_4 → TextureSample_2 (pebble diffuse) UV input |
| 3 | `connect_material_expressions` | Wire LayerCoords_4 → TextureSample_10 (pebble normal) UV input |

### MCP Calls

```
1. add_material_expression("M_Landscape_Ground", "LandscapeLayerCoords", -1400, 3200,
     extra_properties={"mapping_scale": 8.0})
   → Returns: LayerCoords_4 ID

2. connect_material_expressions("M_Landscape_Ground",
     source=LayerCoords_4_ID, source_output=0,
     target="MaterialExpressionTextureSample_2", target_input="uv")

3. connect_material_expressions("M_Landscape_Ground",
     source=LayerCoords_4_ID, source_output=0,
     target="MaterialExpressionTextureSample_10", target_input="uv")
```

### Verify Phase 1

```
4. recompile_material("M_Landscape_Ground")
5. take_screenshot → Check pebbles look smaller/denser than rubble
```

---

## Phase 2: Dirt Patches (18 MCP Calls)

**Goal:** Replace brown mud with soil texture in large organic sweeping patches.
Uses the existing noise texture's R channel at a very large scale for organic shapes.

### What This Does (For Beginners)

We're adding a **mask** that says "in THESE areas, show soil instead of mud."
The mask comes from sampling the brown mud texture at a very large scale (1.5) —
at that scale it becomes a smooth organic noise pattern, perfect for large patches.

A **Power** node sharpens the mask edges (higher exponent = harder edges).
The **DirtAmount** parameter controls how much soil shows (0 = none, 1 = maximum).

```
Texture at large scale → take Red channel → sharpen with Power → multiply by DirtAmount
                                                                         ↓
                        Current BaseColor (Lerp_7) ─── Lerp_9 ──> new BaseColor output
                        Soil texture (T_Sand_D)    ───┘
```

### Node Layout

```
NEW (Phase 2) — ZONE B: Y=3800 to Y=4200
  [LayerCoords_5] scale=1.5 ──UV──> [TextureSample_11 (Mud noise)]
                             ──UV──> [TextureSample_12 (T_Sand_D soil)]
  [TextureSample_11] → [ComponentMask_R] → [Power_2 (DirtMaskSharpness)] → [Multiply_3]
                                            [ScalarParam "DirtAmount" = 0] ──┘
  [Multiply_3] = dirt_mask → goes to Lerp_9 alpha

OUTPUT ZONE D: X=1000
  [Lerp_9]: A=Lerp_7 (current BC), B=TextureSample_12 (soil), Alpha=dirt_mask
  Lerp_9 → BaseColor output (replaces Lerp_7)
```

### Steps

| # | Tool | What It Does |
|---|------|-------------|
| 1 | `add_material_expression` | LandscapeLayerCoords at (-1400, 3800) scale=1.5 → very large noise UV |
| 2 | `add_material_expression` | TextureSample at (-1000, 3800) using T_Brown_Mud_D → organic noise source |
| 3 | `add_material_expression` | ComponentMask at (-800, 3800) R only → extracts one noise channel |
| 4 | `add_material_expression` | Power at (-600, 3800) exponent=3 → sharpens mask edges |
| 5 | `add_material_expression` | ScalarParameter at (-600, 3950) "DirtAmount" default=0.0 |
| 6 | `add_material_expression` | Multiply at (-400, 3800) → Power × DirtAmount = dirt_mask |
| 7 | `add_material_expression` | TextureSample at (-1000, 4100) T_Sand_D → soil color texture |
| 8 | `add_material_expression` | Lerp at (1000, 100) → final dirt blend into BaseColor |
| 9 | `connect` | LayerCoords_5 → TextureSample_11 (noise) UV |
| 10 | `connect` | LayerCoords_5 → TextureSample_12 (soil) UV |
| 11 | `connect` | TextureSample_11 → ComponentMask_2 input |
| 12 | `connect` | ComponentMask_2 → Power_2 base |
| 13 | `connect` | Power_2 → Multiply_3 A |
| 14 | `connect` | DirtAmount param → Multiply_3 B |
| 15 | `connect` | Lerp_7 (current BC end) → Lerp_9 A |
| 16 | `connect` | TextureSample_12 (soil) → Lerp_9 B |
| 17 | `connect` | Multiply_3 (dirt_mask) → Lerp_9 Alpha |
| 18 | `connect_to_material_output` | Lerp_9 → BaseColor (replaces Lerp_7 connection) |

### Verify Phase 2

```
19. recompile_material("M_Landscape_Ground")
20. set_material_instance_parameter("MI_Landscape_Ground", "DirtAmount", 0.5)
21. take_screenshot → Should see soil patches where mud was
22. set_material_instance_parameter("MI_Landscape_Ground", "DirtAmount", 0.0)  ← reset
```

**+2 texture samplers** (noise + soil) → total 13/16

---

## Phase 3: Puddles (19 MCP Calls)

**Goal:** Add dark, reflective wet patches. Affects BaseColor (darker), Roughness (lower = shiny),
and Normal (flattened = smooth water surface).

### What This Does (For Beginners)

Puddles need THREE material changes at once:
1. **Color** → Dark blue-brown (0.02, 0.02, 0.03) to simulate standing water
2. **Roughness** → Very low (0.02) so light reflects sharply = wet/shiny look
3. **Normal** → Flat (0,0,1) so the surface is perfectly smooth = still water

The puddle mask comes from the noise texture's **B channel** (blue), sharpened
with Power(6) for crisp puddle edges. PuddleAmount controls intensity.

```
Existing noise TextureSample_6 → take Blue channel → Power(6) → × PuddleAmount = puddle_mask

puddle_mask controls THREE Lerp nodes:
  Lerp_10: current BC → dark water color        → new BaseColor
  Lerp_11: Roughness 0.85 → wet roughness 0.02  → new Roughness
  Lerp_12: current Normal → flat (0,0,1)        → new Normal
```

### Node Layout

```
NEW (Phase 3) — Puddle Mask: Y=4400 to Y=4800
  [ComponentMask_B from TextureSample_6] → [Power_3 exp=6] → [Multiply_4]
  [ScalarParam "PuddleAmount" = 0] ─────────────────────────┘
  [Multiply_4] = puddle_mask

OUTPUT — Three new Lerps:
  ZONE D (BaseColor):  [Lerp_10] A=Lerp_9 (dirt BC), B=dark water, Alpha=puddle_mask → BaseColor
  ZONE E (Roughness):  [Lerp_11] A=Roughness param, B=0.02 wet, Alpha=puddle_mask → Roughness
  ZONE F (Normal):     [Lerp_12] A=Lerp_4 (normal), B=flat (0,0,1), Alpha=puddle_mask → Normal
```

### Steps

**Puddle Mask (4 nodes):**

| # | Tool | What It Does |
|---|------|-------------|
| 1 | `add_material_expression` | ComponentMask at (-50, 4400) B only → extracts blue noise channel |
| 2 | `add_material_expression` | Power at (100, 4400) exponent=6 → sharp puddle edges |
| 3 | `add_material_expression` | ScalarParameter at (100, 4550) "PuddleAmount" default=0.0 |
| 4 | `add_material_expression` | Multiply at (250, 4400) → Power × PuddleAmount = puddle_mask |

**Puddle Effects (5 nodes):**

| # | Tool | What It Does |
|---|------|-------------|
| 5 | `add_material_expression` | Constant3Vector at (1000, 250) → dark water color (0.02, 0.02, 0.03) |
| 6 | `add_material_expression` | Lerp at (1200, 100) → puddle BaseColor blend |
| 7 | `add_material_expression` | Constant at (400, 4700) value=0.02 → wet roughness |
| 8 | `add_material_expression` | Lerp at (600, 400) → puddle Roughness blend |
| 9 | `add_material_expression` | Lerp at (600, 2100) → puddle Normal flatten |

**Connections (10 wires):**

| # | Source → Target | What It Does |
|---|----------------|-------------|
| 10 | TextureSample_6 → ComponentMask_B | Feed noise into blue channel extractor |
| 11 | ComponentMask_B → Power_3 | Sharpen the mask |
| 12 | Power_3 → Multiply_4 A | Sharpened mask × amount |
| 13 | PuddleAmount → Multiply_4 B | User control |
| 14 | Lerp_9 (dirt BC) → Lerp_10 A | Current color without puddles |
| 15 | Dark water Constant3V → Lerp_10 B | Puddle color |
| 16 | puddle_mask → Lerp_10 Alpha | Where puddles appear |
| 17 | Lerp_10 → BaseColor output | Replaces Lerp_9 as final BaseColor |
| 18 | ScalarParameter_1 (Roughness) → Lerp_11 A | Dry roughness |
| 19 | Constant 0.02 → Lerp_11 B | Wet roughness |
| 20 | puddle_mask → Lerp_11 Alpha | Same mask for roughness |
| 21 | Lerp_11 → Roughness output | Replaces static roughness param |
| 22 | Lerp_4 (NormalStrength) → Lerp_12 A | Current bumpy normal |
| 23 | Constant3Vector_0 (0,0,1) → Lerp_12 B | Flat normal for smooth water |
| 24 | puddle_mask → Lerp_12 Alpha | Same mask flattens normals |
| 25 | Lerp_12 → Normal output | Replaces Lerp_4 as final Normal |

### Verify Phase 3

```
26. recompile_material("M_Landscape_Ground")
27. set_material_instance_parameter("MI_Landscape_Ground", "PuddleAmount", 0.5)
28. take_screenshot → Should see dark shiny patches
29. set_material_instance_parameter("MI_Landscape_Ground", "PuddleAmount", 0.0)  ← reset
```

**+0 texture samplers** (reuses existing noise B channel) → stays at 13/16

---

## Phase 4: Push All Parameters to MI & Final Verify

After all phases, push new parameter defaults to MI:

```
30. set_material_instance_parameter("MI_Landscape_Ground", "DirtAmount", 0.0)
31. set_material_instance_parameter("MI_Landscape_Ground", "PuddleAmount", 0.0)
32. take_screenshot → Should look exactly like before (both features off)
```

Then the user can open MI_Landscape_Ground in Unreal and adjust:
- **DirtAmount**: 0.0 (off) → 1.0 (maximum soil patches)
- **PuddleAmount**: 0.0 (off) → 1.0 (maximum puddle coverage)

---

## Summary

| Feature | New Nodes | New Samplers | New Parameters |
|---------|-----------|-------------|----------------|
| Pebble scale | 1 (LayerCoords) | 0 | none (static UV) |
| Dirt patches | 7 + 1 Lerp | +2 (noise + soil) | DirtAmount (0.0) |
| Puddles | 4 mask + 5 effect | 0 (reuse noise B) | PuddleAmount (0.0) |
| **Total** | **18** | **+2** | **+2** |

**Final material: ~62 expressions, 13/16 samplers, 9 parameters**

## Troubleshooting

| Problem | Cause | Fix |
|---------|-------|-----|
| Dirt patches not visible | DirtAmount=0 in MI | Set DirtAmount > 0 in MI_Landscape_Ground |
| Puddles not visible | PuddleAmount=0 in MI | Set PuddleAmount > 0 in MI_Landscape_Ground |
| New params not in MI panel | MI created before params | Push via set_material_instance_parameter |
| Texture too large/blurry | Wrong mapping_scale | Delete LayerCoords, recreate with correct scale |
| MI changes have no effect | MI disconnected from landscape | Call set_landscape_material with MI path |
| Soil texture missing | T_Sand_D not imported | Import from blender/textures/sand_01_diff.jpg |
| Engine crash after import | Too many heavy ops | Add lightweight query between heavy MCP calls |
| Pebbles same size as rubble | Still using rubble UV | Check LayerCoords_4 connected to pebble textures |

## NEVER DO These Things

- NEVER call `create_material_instance` to recreate MI_Landscape_Ground (breaks landscape reference)
- NEVER change mapping_scale via `set_material_expression_property` (silently fails)
- NEVER use `LandscapeLayerBlend` (shader compiler crash)
- NEVER call MCP tools in parallel
- NEVER import textures back-to-back without breathing room
- NEVER skip recompile between phases
