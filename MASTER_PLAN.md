# Blood & Dust - Master Plan

## Overview

3D third-person game with Rembrandt Dutch Golden Age oil painting visual style. Multi-engine development workspace demonstrating AI-driven game creation via MCP.

## Task ID Format

| Prefix | Usage |
|--------|-------|
| `TASK-XXX` | Features and improvements |
| `BUG-XXX` | Bug fixes |
| `FEATURE-XXX` | Major features |
| `INQUIRY-XXX` | Research/investigation |
| `IDEA-XXX` | Ideas for future consideration |
| `ISSUE-XXX` | General issues |

**Rules:**
- IDs are sequential (TASK-001, TASK-002...)
- Completed: ~~TASK-001~~ with strikethrough + DONE status
- Never reuse IDs

## Roadmap

| ID | Title | Priority | Status | Dependencies |
|----|-------|----------|--------|--------------|
| **TASK-001** | **Fix texture stretching on boulder stones** | **P2** | IN PROGRESS | - |
| ~~**TASK-002**~~ | ~~**Create skill for natural rock/element placement**~~ | **P2** | DONE | - |
| ~~**TASK-003**~~ | ~~**Create high-quality vegetation scattering skill**~~ | **P2** | DONE | - |
| ~~**TASK-004**~~ | ~~**Research Blender sky visualization techniques**~~ | **P2** | DONE | - |
| ~~**TASK-005**~~ | ~~**Create skill for 2D-to-backdrop in Blender**~~ | **P2** | DONE | - |
| **TASK-006** | **Create procedural desert ground material** | **P2** | IN PROGRESS | - |
| **INQUIRY-008** | **Can Blender scene quality transfer to Godot?** | **P1** | OPEN | - |
| ~~**TASK-007**~~ | ~~**Create Blender material skill (UV, reflections, blending, export)**~~ | **P2** | DONE | - |
| **TASK-009** | **Fix robot walk cycle - realistic ground contact & locomotion** | **P1** | IN PROGRESS | - |
| ~~**FEATURE-011**~~ | ~~**Reliable landscape material & viewport parameter controls**~~ | **P1** | **DONE** (2026-02-09) | - |
| ~~TASK-011A~~ | ~~Rebuild M_Landscape_Ground from scratch with reliable UV~~ | P1 | ~~DONE~~ | - |
| ~~TASK-011B~~ | ~~Use Multiply+Constant for UV scale (persistence fix)~~ | P1 | ~~DONE~~ | TASK-011A |
| ~~TASK-011C~~ | ~~Research: expose MI params in viewport Details panel~~ | P1 | ~~DONE~~ | - |
| ~~TASK-011D~~ | ~~Expose 6 ScalarParameters in MI (DetailUVScale, WarpAmount, MacroStrength, SlopeSharpness, GrassAmount, Roughness)~~ | P1 | ~~DONE~~ | TASK-011C |
| ~~TASK-011E~~ | ~~Apply MI to all landscape sections, verify persistence~~ | P1 | ~~DONE~~ | TASK-011A |
| ~~TASK-011F~~ | ~~Update landscape skills with reliability fixes~~ | P2 | ~~DONE~~ | TASK-011A |
| **FEATURE-013** | **Landscape detail: user-controllable rocks, grass, puddles, mud** | **P1** | OPEN | FEATURE-011 |
| ~~TASK-013A~~ | ~~Expose rock density/scale as controllable parameter (`/rock-scatter` command)~~ | P1 | ~~DONE~~ (2026-02-09) | - |
| ~~TASK-013B~~ | ~~Expose grass density/scale as controllable parameter (`/grass-scatter` command)~~ | P1 | ~~DONE~~ (2026-02-09) | - |
| TASK-013C | Add puddle system to landscape material (noise mask, dark color, low roughness, flat normal) | P1 | OPEN | FEATURE-011 |
| TASK-013D | Add muddy area system to landscape material (noise mask, brown color blend) | P1 | OPEN | FEATURE-011 |
| TASK-013E | Expose PuddleAmount, MudAmount as MI ScalarParameters | P1 | OPEN | TASK-013C, TASK-013D |
| ~~**FEATURE-010**~~ | ~~**Playable character pipeline: import, rig, animate, play**~~ | **P0** | **DONE** (2026-02-12) | - |
| ~~TASK-010A~~ | ~~C++ `import_skeletal_mesh` MCP tool~~ | P0 | ~~DONE~~ (2026-02-09) | - |
| ~~TASK-010B~~ | ~~C++ `import_animation` MCP tool~~ | P0 | ~~DONE~~ (2026-02-09) | TASK-010A |
| ~~TASK-010C~~ | ~~C++ `create_character_blueprint` MCP tool~~ | P0 | ~~DONE~~ (2026-02-09) | - |
| ~~TASK-010D~~ | ~~C++ `create_anim_blueprint` MCP tool~~ | P0 | ~~DONE~~ (2026-02-09) | TASK-010A |
| ~~TASK-010E~~ | ~~Python MCP server: register 4 new tools~~ | P0 | ~~DONE~~ (2026-02-09) | TASK-010A..D |
| ~~TASK-010F~~ | ~~Rebuild C++ plugin & test each tool~~ | P0 | ~~DONE~~ (2026-02-09) | TASK-010E |
| ~~TASK-010G~~ | ~~Import Meshy robot skeletal mesh + textures~~ | P0 | ~~DONE~~ (2026-02-09) | TASK-010F |
| ~~TASK-010H~~ | ~~Import robot animations (walk, idle, run, kick)~~ | P0 | ~~DONE~~ (2026-02-09) | TASK-010G |
| ~~TASK-010I~~ | ~~Create robot PBR material~~ | P0 | ~~DONE~~ (2026-02-09) | TASK-010G |
| ~~TASK-010J~~ | ~~Create Character Blueprint with camera + movement~~ | P0 | ~~DONE~~ (2026-02-09) | TASK-010G |
| ~~TASK-010K~~ | ~~Create Animation Blueprint (shell)~~ | P0 | ~~DONE~~ (2026-02-09) | TASK-010H, TASK-010J |
| ~~TASK-010L~~ | ~~Wire Enhanced Input (WASD, mouse look, sprint, attack)~~ | P0 | ~~DONE~~ (2026-02-11) | TASK-010J |
| ~~TASK-010M~~ | ~~Integration test: fully playable character in level~~ | P0 | ~~DONE~~ (2026-02-12) | TASK-010K, TASK-010L |
| ~~TASK-010N~~ | ~~Rewrite `/unreal-character-input` skill (reliable + extensible)~~ | P1 | ~~DONE~~ (2026-02-12) | TASK-010M |
| ~~**FEATURE-012**~~ | ~~**Vegetation scatter with HISM persistence**~~ | **P1** | ✅ **DONE** (2026-02-09) | - |
| ~~TASK-012A~~ | ~~Fix Poisson disk grid pattern (random seeds)~~ | P1 | ~~DONE~~ (2026-02-09) | - |
| ~~TASK-012B~~ | ~~Landscape-only trace filter (no floating on rocks)~~ | P1 | ~~DONE~~ (2026-02-09) | - |
| ~~TASK-012C~~ | ~~HISM persistence (RF_Transactional, AddInstanceComponent, OFPA)~~ | P1 | ~~DONE~~ (2026-02-09) | - |
| ~~TASK-012D~~ | ~~Import trees from PolyHaven (6 variants + textures + materials)~~ | P1 | ~~DONE~~ (2026-02-09) | - |
| ~~TASK-012E~~ | ~~Fix tree material slot assignments (per-slot)~~ | P1 | ~~DONE~~ (2026-02-09) | TASK-012D |
| ~~TASK-012F~~ | ~~Create `/vegetation-scatter` skill with all paths~~ | P1 | ~~DONE~~ (2026-02-09) | TASK-012A..E |
| ~~TASK-012G~~ | ~~Fix AnimBP transition crash (AllocateDefaultPins order)~~ | P1 | ~~DONE~~ (2026-02-09) | - |
| ~~BUG-012~~ | ~~MCP apply_material_to_actor unreliable~~ | P0 | ~~DONE~~ (2026-02-10) | - |
| ~~BUG-012b~~ | ~~Robot material UV seams & harsh specular (Meshy AI UV channel 1)~~ | P0 | ~~DONE~~ (2026-02-10) | - |
| **FEATURE-014** | **Game Designer skill — phased demo builder** | **P1** | **DONE** (2026-02-11) | - |
| **FEATURE-015** | **Playable Demo — 8-phase pipeline** | **P0** | OPEN | FEATURE-014, FEATURE-010 |
| TASK-015A | Phase 0: Audit & Interview | P0 | OPEN | FEATURE-014 |
| TASK-015B | Phase 1: Character Playability (delegates to /unreal-character-input) | P0 | OPEN | TASK-010L |
| ~~**TASK-015C**~~ | ~~Phase 2: Rembrandt Lighting & Atmosphere~~ | **P1** | ~~**DONE**~~ (2026-02-12) | - |
| ~~**TASK-015D**~~ | ~~Phase 3: Post-Processing Color Grade~~ | **P1** | ~~**DONE**~~ (2026-02-12) | - |
| TASK-015E | Phase 4: Backdrop Billboards | P2 | OPEN | - |
| TASK-015F | Phase 5: World Building (ruins/castle/town) | P2 | OPEN | - |
| TASK-015G | Phase 6: NPC Spawn & Patrol | P2 | OPEN | TASK-015B |
| TASK-015H | Phase 7: Combat Foundation | P2 | OPEN | TASK-015G |
| TASK-015I | Phase 8: Sound & Music | P2 | OPEN | - |
| **FEATURE-016** | **Audio Pipeline: import_sound + AmbientSound + BP AudioComponent** | **P1** | **DONE** (2026-02-11) | - |
| ~~**FEATURE-017**~~ | ~~**Gameplay Commands (set_game_mode_default_pawn, create_anim_montage, play_montage, apply_impulse, trigger_post_process, spawn_niagara)**~~ | **P0** | **DONE** (2026-02-11) | - |
| ~~**FEATURE-018**~~ | ~~**Screenshot Tool (SceneCapture2D rewrite for Linux)**~~ | **P1** | **DONE** (2026-02-11) | - |
| ~~**FEATURE-019**~~ | ~~**Widget/HUD Commands (create_widget_blueprint, add_widget_to_viewport, set_widget_property)**~~ | **P1** | **DONE** (2026-02-11) | - |
| ~~**FEATURE-020**~~ | ~~**AI/Behavior Tree Commands (create_behavior_tree, create_blackboard, add_bt_task, add_bt_decorator, assign_behavior_tree)**~~ | **P1** | **DONE** (2026-02-11) | - |
| ~~**FEATURE-021**~~ | ~~**Audio Pipeline (import_sound, AmbientSound, BP AudioComponent)**~~ | **P1** | **DONE** (2026-02-11) | - |
| ~~**FEATURE-022**~~ | ~~**compile_blueprint fix (proper error checking via FCompilerResultsLog)**~~ | **P2** | **DONE** (2026-02-11) | - |
| ~~**FEATURE-023**~~ | ~~**GameplayHelperLibrary runtime module (SetCharacterWalkSpeed, PlayAnimationOneShot)**~~ | **P3** | **DONE** (2026-02-11) | - |
| ~~**BUG-024**~~ | ~~**Character moves during attack/kick animations (bStopMovement=false)**~~ | **P1** | **DONE** (2026-02-12) | FEATURE-023 |
| ~~**FEATURE-030**~~ | ~~**SetPropertyValue expansion: ByteProperty, EnumProperty, ClassProperty, ObjectProperty, SoftObjectProperty, SoftClassProperty, NameProperty, TextProperty + struct dot-notation navigation + snap_actor_to_ground capsule height fix**~~ | **P0** | ✅ **DONE** (2026-02-13) | - |
| **FEATURE-032** | **Pixel Streaming — Claude Plays the Game (PixelStreaming2 + Playwright MCP)** | **P1** | IN PROGRESS | - |
| TASK-032A | Enable PixelStreaming2 plugin in .uproject | P1 | DONE (2026-02-12) | - |
| TASK-032B | Fix signalling server config for Linux (port 8080, relative http_root) | P1 | DONE (2026-02-12) | - |
| TASK-032C | Build signalling server (Common → Signalling → SignallingWebServer) | P1 | DONE (2026-02-12) | 032B |
| TASK-032D | Create `scripts/start-pixel-streaming.sh` launch script | P1 | DONE (2026-02-12) | 032C |
| TASK-032E | Fix screenshot deadlock (2-phase FTSTicker, reduce default to 960x540) | P0 | DONE (2026-02-13) | - |
| TASK-032F | Test full pipeline: signalling → game → browser → Playwright screenshot | P1 | OPEN | 032D, 032E |
| TASK-032G | Create `/play-game` skill (screenshot → decide → input loop) | P2 | OPEN | 032F |
| **FEATURE-031** | **Enemy Animation & AI Pipeline (Full Battlefield)** | **P0** | IN PROGRESS | FEATURE-026 |
| TASK-031A | Fix enemy materials (all 3 types: dark patches, UVs, glossy, missing) | P0 | OPEN | - |
| TASK-031B | Fix enemy ground positioning (all 19 enemies) | P0 | OPEN | - |
| TASK-031C | Verify SingleNode animation works in PIE (CDO clearing test) | P0 | OPEN | - |
| TASK-031D | Build KingBot AI: patrol → detect → chase → attack | P0 | OPEN | 031A, 031C, 026E |
| TASK-031E | Build combat loop: health, damage, death (KingBot proof of concept) | P0 | OPEN | 031D |
| TASK-031F | User rigs Bell + Giganto on Mixamo, downloads animation sets | P1 | OPEN | 031E (gate) |
| TASK-031G | Import Mixamo-rigged animations + assign to all 19 enemies | P1 | OPEN | 031F |
| TASK-031H | Clean up distorted generic Mixamo animation assets | P2 | OPEN | 031G |
| **IDEA-025** | **Mixamo character/animation pipeline — import Mixamo FBX, retarget to existing skeleton, wire actions** | **P3** | OPEN | FEATURE-010 |
| **FEATURE-026** | **Enemy AI & Combat System** | **P0** | **IN PROGRESS** | FEATURE-010, FEATURE-020 |
| ~~TASK-026A~~ | ~~Player melee damage (ApplyMeleeDamage C++ function)~~ | P0 | ~~DONE~~ (2026-02-12) | FEATURE-010 |
| ~~TASK-026B~~ | ~~Enemy Health variables (KingBot=100, Giganto=150, Bell=75)~~ | P0 | ~~DONE~~ (2026-02-12) | FEATURE-010 |
| ~~TASK-026C~~ | ~~Player BP wiring (ApplyMeleeDamage after attacks)~~ | P0 | ~~DONE~~ (2026-02-12) | TASK-026B |
| TASK-026D | Hit feedback: red screen flash (PostProcess), knockback impulse | P1 | OPEN | TASK-026C |
| TASK-026E | Block/parry mechanic (RMB input + animation + damage reduction) | P1 | OPEN | TASK-026C |
| TASK-026F | Blood/impact VFX (Niagara particle on hit) | P2 | OPEN | TASK-026C, FEATURE-017 |
| ~~TASK-026G~~ | ~~Enemy death (ragdoll + knockback + delayed destroy)~~ | P1 | ~~DONE~~ (2026-02-12) | TASK-026B |
| ~~TASK-026H~~ | ~~Player death + "YOU DIED" screen + level restart~~ | P1 | ~~DONE~~ (2026-02-13) | TASK-026B |
| ~~TASK-026I~~ | ~~Enemy AI: chase/attack/leash C++ state machine (UpdateEnemyAI)~~ | P0 | ~~DONE~~ (2026-02-13) | TASK-026A |
| TASK-026J | Background NPC combatants (ambient pairs fighting in distance, no player interaction) | P2 | OPEN | TASK-026I |
| **FEATURE-027** | **World Building & Structures** | **P1** | OPEN | - |
| ~~TASK-027A~~ | ~~Ruins/start zone enclosure (spawn point with walls/arches)~~ | P1 | ~~DONE~~ (2026-02-12) | - |
| ~~TASK-027B~~ | ~~Middle zone: scattered cover (walls, broken pillars, rubble)~~ | P1 | ~~DONE~~ (2026-02-12) | - |
| ~~TASK-027C~~ | ~~Background structures: castle/ruins silhouettes in fog~~ | P2 | ~~DONE~~ (2026-02-12) | - |
| TASK-027G | Ruin decay effects: moss overlay, edge wear, cracks on M_Ruin_Wall | P2 | OPEN | TASK-027A |
| TASK-027D | Escape portal/archway with glow effect (end zone) | P1 | OPEN | - |
| TASK-027E | Torch props with fire VFX on ruins (Niagara) | P2 | OPEN | FEATURE-017 |
| TASK-027F | Dust/wind particles floating in air (Niagara ambient) | P2 | OPEN | FEATURE-017 |
| **FEATURE-028** | **HUD & UI** | **P1** | OPEN | FEATURE-019 |
| ~~TASK-028A~~ | ~~Create health bar widget (player HP, update on damage)~~ | P1 | ~~DONE~~ (2026-02-13) | TASK-026B |
| TASK-028B | Create crosshair/reticle widget | P2 | OPEN | FEATURE-019 |
| TASK-028C | "Demo Complete" win screen (triggered on portal overlap) | P1 | OPEN | TASK-027D, FEATURE-019 |
| ~~TASK-028D~~ | ~~"You Died" screen + restart prompt~~ | P1 | ~~DONE~~ (2026-02-13) | TASK-026H |
| TASK-028E | Main menu / pause menu (Esc key) | P3 | OPEN | FEATURE-019 |
| **FEATURE-029** | **Ambient Audio** | **P2** | OPEN | FEATURE-016 |
| TASK-029A | Place wind ambient sound (looping, 3D spatialized) | P2 | OPEN | FEATURE-016 |
| TASK-029B | Place distant combat sounds (background atmosphere) | P2 | OPEN | FEATURE-016 |
| TASK-029C | Background music track (2D, looping) | P2 | OPEN | FEATURE-016 |
| TASK-029D | Combat music trigger (switch tracks on enemy detection) | P3 | OPEN | TASK-029C, TASK-026I |

## Demo Vision: "The Escape"

**Core loop**: 2-3 min sprint through a Rembrandt evening battlefield. Fight through patrolling robot enemies. Reach the glowing escape portal.

### Player Experience
1. Press Play → robot character spawns at PlayerStart (FEATURE-017)
2. Evening/dusk lighting — low golden sun, warm fog, long shadows (Phase 2/3, existing tools)
3. Health bar + crosshair HUD on screen (FEATURE-019)
4. Wind ambient + distant combat music (FEATURE-016, DONE)
5. Dust particles floating in air, torches with fire FX on ruins (FEATURE-023)
6. Distant background: castle/ruins silhouettes in fog (existing world building tools)
7. Distant background: pairs of NPC robots fighting each other in the haze (FEATURE-021)
8. 3-5 patrol enemies between player and escape point (FEATURE-021)
9. Melee attack (LMB) + block/parry (RMB) with animations (FEATURE-018)
10. Getting hit = red screen flash + knockback (FEATURE-020, FEATURE-022)
11. Kill enemies → blood impact FX (FEATURE-023)
12. Reach glowing escape portal → "Demo Complete" screen (FEATURE-019)

**Progress (2026-02-13) — TASK-015J Game Flow:** Portal beacon (500K PointLight at tower), start zone light, 2 breadcrumb path lights, BP_PortalTrigger (Tick distance check → PrintString + SetGamePaused), "REACH THE LIGHT" on BeginPlay, WBP_Objective + WBP_DemoComplete widgets. Goal = Ruin_BG_Tower_02. IN PROGRESS.

### Level Layout Concept
- **Start zone**: Small ruin enclosure, player spawns here
- **Middle zone**: Open battlefield with patrol enemies, scattered cover (walls, rocks)
- **Background**: Far structures (castle silhouette), NPC pairs fighting, fog obscures distance
- **End zone**: Glowing portal/archway, final 1-2 enemies guarding it
- **Atmosphere**: Evening, volumetric fog, wind sound, distant combat sounds

## Active Work

---

### FEATURE-011: Reliable Landscape Material & Viewport Parameter Controls

**Status**: DONE (2026-02-09)
**Priority**: P1
**Result**: Complete landscape material with anti-tiling (UV noise distortion + fixed 37.5° rotation + dissolve blend). 71 nodes, 12 samplers, 6 exposed MI ScalarParameters. Applied as MI_Landscape_Ground_v6 on all 5 landscape sections.

#### Solution Summary
- **Anti-tiling**: Three-layer approach — UV distortion (warp_scale=0.002, warp_amount=0.12) + fixed-angle rotation (37.5°, +0.5 UV offset) + dissolve blend (noise scale=0.0003)
- **Persistence**: WorldPosition-based UVs (not LandscapeLayerCoords), all values in Constants/ScalarParameters
- **C++ tool**: `create_landscape_material` in EpicUnrealMCPEditorCommands.cpp builds entire graph in one tick
- **Iteration history**: texture bombing → UV distortion only → noise rotation (swirls) → fixed rotation (success)
- **See**: `memory/anti-tiling-research.md` for full technical details

#### MI Parameters (tuneable on MI_Landscape_Ground_v6)

| Parameter | Default | Range | Effect |
|-----------|---------|-------|--------|
| DetailUVScale | 0.004 | 0.001-0.01 | Texture tile size (smaller = bigger tiles) |
| WarpAmount | 0.12 | 0.0-0.3 | UV distortion strength |
| MacroStrength | 0.4 | 0.0-1.0 | Large-scale brightness variation |
| SlopeSharpness | 3.0 | 1.0-10.0 | Rock/mud transition sharpness |
| GrassAmount | 0.5 | 0.0-1.0 | Grass overlay intensity |
| Roughness | 0.85 | 0.0-1.0 | Surface roughness |

#### Exact MCP Command to Reproduce

```python
create_landscape_material("M_Landscape_Distorted",
    rock_d="/Game/Textures/Ground/T_Rocky_Terrain_D",
    rock_n="/Game/Textures/Ground/T_Rocky_Terrain_N",
    mud_d="/Game/Textures/Ground/T_Brown_Mud_D",
    mud_n="/Game/Textures/Ground/T_Brown_Mud_N",
    grass_d="/Game/Textures/Ground/T_Grass_Dry_D",
    grass_n="/Game/Textures/Ground/T_Grass_Dry_N",
    detail_uv_scale=0.004, warp_scale=0.002, warp_amount=0.12,
    macro_scale=3e-05, macro_strength=0.4,
    slope_sharpness=3, grass_amount=0.5, roughness=0.85)

create_material_instance("MI_Landscape_Ground_v6",
    parent_material="/Game/Materials/M_Landscape_Distorted",
    scalar_parameters={"DetailUVScale": 0.004, "WarpAmount": 0.12,
        "MacroStrength": 0.4, "SlopeSharpness": 3,
        "GrassAmount": 0.5, "Roughness": 0.85})

set_landscape_material("/Game/Materials/MI_Landscape_Ground_v6")
```

---

### FEATURE-010: Playable Character Pipeline — Import, Rig, Animate, Play

**Status**: DONE (2026-02-12) — All 14 subtasks complete (010A-010N)
**Priority**: P0
**Goal**: Import the Meshy AI robot character into Unreal Engine 5.7 via MCP tools and make it fully playable with third-person camera, movement, sprint, and attack — matching the Godot implementation.

#### Source Assets

| Asset | Path | Size | Notes |
|-------|------|------|-------|
| Base mesh (FBX) | `assets/3d_models/robot_1/Meshy_AI_biped/Meshy_AI_Character_output.fbx` | 7.2 MB | Skeletal mesh with bones |
| Merged animations | `assets/3d_models/robot_1/Meshy_AI_biped/Meshy_AI_Meshy_Merged_Animations.fbx` | 8.4 MB | All anims in one FBX |
| Walk animation | `assets/3d_models/robot_1/Meshy_AI_biped/Meshy_AI_Animation_Walking_withSkin.fbx` | 7.4 MB | Walk only |
| Kick animation | `assets/3d_models/robot_1/Kicking.fbx` | 7.6 MB | Kick only |
| Normal map | `assets/3d_models/robot_1/Meshy_AI_biped/Meshy_AI_texture_0_normal.png` | 7.1 MB | |
| Metallic map | `assets/3d_models/robot_1/Meshy_AI_biped/Meshy_AI_texture_0_metallic.png` | 1.2 MB | |
| Roughness map | `assets/3d_models/robot_1/Meshy_AI_biped/Meshy_AI_texture_0_roughness.png` | 1.7 MB | |

> **Note**: FBX has misnamed animations. Godot mapping: "Jump_with_Arms_Open"=walk, "Walking"=idle, "Crouch_and_Step_Back"=run, "Block5"=kick

#### Godot Character Specs (Target to Replicate)

```
Movement:     5.0 m/s walk, 8.0 m/s sprint (UE: 500/800 units)
Collision:    Capsule radius=0.4m height=1.8m (UE: 40/180 cm)
Camera:       SpringArm 2.5m, pivot Y=1.5m, pitch -60/+60 deg
Mouse sens:   0.003 rad/pixel
Rotation:     Lerp factor 10.0 to face movement dir
Input:        WASD move, Shift sprint, LMB attack, RMB kick, Q block, Esc mouse toggle
Animations:   Idle(loop), Walk(loop), Run(loop), Kick(oneshot, 0.15s blend)
```

---

#### Phase 1: C++ MCP Tool Development

##### TASK-010A: `import_skeletal_mesh`

**File changes:**
| File | Change |
|------|--------|
| `Plugins/UnrealMCP/Source/UnrealMCP/Public/Commands/EpicUnrealMCPEditorCommands.h` | Add `HandleImportSkeletalMesh()` declaration |
| `Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/EpicUnrealMCPEditorCommands.cpp` | Add handler (~160 lines), new includes, dispatch entry |
| `Plugins/UnrealMCP/Source/UnrealMCP/Private/EpicUnrealMCPBridge.cpp` | Add `"import_skeletal_mesh"` to EditorCommands dispatch |

**New includes:**
```cpp
#include "Factories/FbxSkeletalMeshImportData.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
```

**Implementation pattern** (mirrors `HandleImportMesh`):
```cpp
// Key differences from static mesh import:
FbxUI->MeshTypeToImport = FBXIT_SkeletalMesh;   // not FBXIT_StaticMesh
FbxUI->bImportAsSkeletal = true;
FbxUI->bImportMesh = true;
FbxUI->bCreatePhysicsAsset = bCreatePhysicsAsset; // user param, default true
FbxUI->bImportAnimations = bImportAnimations;      // user param, default false
FbxUI->bImportMorphTargets = bImportMorphTargets;  // user param, default true
// Optional: FbxUI->Skeleton = LoadObject<USkeleton>(...) for reusing existing skeleton

// Post-import: cast to USkeletalMesh*, report bone count, skeleton path
```

**Parameters:**
- `fbx_path` (required) — absolute path to FBX file
- `asset_path` (required) — e.g. `/Game/Characters/Robot/SK_Robot`
- `asset_name` (required) — e.g. `SK_Robot`
- `import_animations` (optional, default false) — import anims embedded in FBX
- `create_physics_asset` (optional, default true)
- `import_morph_targets` (optional, default true)
- `skeleton_path` (optional) — reuse existing skeleton
- `import_materials` (optional, default false) — skip, we create our own
- `import_textures` (optional, default false) — skip, we import separately

**Response:** asset_path, skeleton_path, bone_count, physics_asset_path, morph_target_names[], material_slot_count

**SAFETY**: This is a HEAVY operation. Add to `HEAVY_COMMANDS_COOLDOWN` with 5s cooldown. Add to `LARGE_OPERATION_COMMANDS` with 30s timeout.

##### TASK-010B: `import_animation`

**File changes:** Same files as TASK-010A (editor commands + bridge dispatch)

**New includes:**
```cpp
#include "Factories/FbxAnimSequenceImportData.h"
#include "Animation/AnimSequence.h"
```

**Implementation pattern:**
```cpp
FbxUI->MeshTypeToImport = FBXIT_Animation;
FbxUI->bImportAnimations = true;
FbxUI->bImportMesh = false;
FbxUI->Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);  // REQUIRED
// If Skeleton is null → return error immediately, do NOT proceed
```

**Parameters:**
- `fbx_path` (required) — absolute path to animation FBX
- `asset_path` (required) — e.g. `/Game/Characters/Robot/Animations/`
- `skeleton_path` (required) — existing skeleton from skeletal mesh import
- `animation_name` (optional) — override name

**Response:** animation_asset_paths[], duration_seconds, frame_count, bone_track_count

**SAFETY**: HEAVY operation. Add to cooldown lists. Skeleton MUST exist or handler returns error.

##### TASK-010C: `create_character_blueprint`

**File changes:**
| File | Change |
|------|--------|
| `Plugins/UnrealMCP/Source/UnrealMCP/Public/Commands/EpicUnrealMCPBlueprintCommands.h` | Add `HandleCreateCharacterBlueprint()` declaration |
| `Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/EpicUnrealMCPBlueprintCommands.cpp` | Add handler (~120 lines), new includes, dispatch entry |
| `Plugins/UnrealMCP/Source/UnrealMCP/Private/EpicUnrealMCPBridge.cpp` | Add to BlueprintCommands dispatch |

**New includes in BlueprintCommands:**
```cpp
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/SkeletalMesh.h"
#include "KismetCompilerModule.h"
```

**Implementation (CDO pattern — CRITICAL):**
```cpp
// 1. Create blueprint with ACharacter parent
Factory->ParentClass = ACharacter::StaticClass();
UBlueprint* BP = Factory->FactoryCreateNew(...);

// 2. COMPILE FIRST (CDO components only exist after compilation)
FKismetEditorUtilities::CompileBlueprint(BP);

// 3. Access CDO to configure inherited components
ACharacter* CDO = BP->GeneratedClass->GetDefaultObject<ACharacter>();
// Set skeletal mesh on CDO->GetMesh()
// Set capsule dimensions on CDO->GetCapsuleComponent()
// Set movement defaults on CDO->GetCharacterMovement()

// 4. Add SpringArm + Camera via SCS (these don't exist on ACharacter by default)
USCS_Node* SpringArmNode = BP->SimpleConstructionScript->CreateNode(USpringArmComponent::StaticClass());
USCS_Node* CameraNode = BP->SimpleConstructionScript->CreateNode(UCameraComponent::StaticClass());

// 5. Compile again + save
FKismetEditorUtilities::CompileBlueprint(BP);
```

**Parameters:**
- `blueprint_name` (required) — e.g. `BP_RobotCharacter`
- `blueprint_path` (required) — e.g. `/Game/Characters/Robot/`
- `skeletal_mesh_path` (optional) — assign mesh to character
- `capsule_radius` (optional, default 40.0)
- `capsule_half_height` (optional, default 90.0)
- `max_walk_speed` (optional, default 500.0)
- `max_sprint_speed` (optional, default 800.0)
- `jump_z_velocity` (optional, default 420.0)
- `camera_boom_length` (optional, default 250.0)
- `camera_boom_socket_offset_z` (optional, default 150.0)

**Response:** blueprint_path, generated_class_path, component_list[]

**GOTCHA**: ACharacter CDO components (Capsule, Mesh, CharacterMovement) are C++ constructor components — they are NOT in the SCS. Must access via `GetDefaultObject<ACharacter>()` AFTER first compile. SpringArm and Camera ARE added via SCS since they don't exist on base ACharacter.

##### TASK-010D: `create_anim_blueprint`

**File changes:** Same BlueprintCommands files + bridge dispatch

**New includes:**
```cpp
#include "Factories/AnimBlueprintFactory.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimInstance.h"
```

**Implementation:**
```cpp
UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
Factory->TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath); // REQUIRED
Factory->ParentClass = UAnimInstance::StaticClass();

// VALIDATE: TargetSkeleton MUST be non-null or crash
if (!Factory->TargetSkeleton) return CreateErrorResponse("Skeleton not found");

UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(
    Factory->FactoryCreateNew(UAnimBlueprint::StaticClass(), Package, ...)
);
```

**Parameters:**
- `blueprint_name` (required) — e.g. `ABP_Robot`
- `blueprint_path` (required) — e.g. `/Game/Characters/Robot/`
- `skeleton_path` (required) — skeleton from skeletal mesh import
- `preview_mesh_path` (optional) — skeletal mesh for preview

**Response:** anim_blueprint_path, skeleton_path, parent_class

**GOTCHA**: `UAnimBlueprintFactory` CRASHES if TargetSkeleton is null and bTemplate is false. Always validate.

---

#### Phase 2: Python MCP Server Registration

##### TASK-010E: Register 4 new Python tools

**File:** `unreal/mcp/Python/unreal_mcp_server_advanced.py`

Add 4 `@mcp.tool()` functions + update these dicts:
```python
HEAVY_COMMANDS_COOLDOWN = {
    ...,
    "import_skeletal_mesh": 5.0,
    "import_animation": 3.0,
}
LARGE_OPERATION_COMMANDS = {
    ...,
    "import_skeletal_mesh": 30,
    "import_animation": 20,
}
```

---

#### Phase 3: Build & Test

##### TASK-010F: Build C++ plugin and test tools

```bash
# Rebuild plugin
/path/to/engine/Build.sh bood_and_dustEditor Linux Development -Project="/path/to/bood_and_dust.uproject"
```

**Test sequence (one at a time, strictly sequential):**
1. `import_skeletal_mesh` with robot FBX → verify skeleton + mesh created
2. `import_animation` with merged animations FBX → verify anim sequences
3. `create_character_blueprint` → verify BP with capsule + camera
4. `create_anim_blueprint` → verify AnimBP targeting skeleton

---

#### Phase 4: Robot Import Pipeline

##### TASK-010G: Import Meshy robot skeletal mesh

```
import_skeletal_mesh(
  fbx_path="/abs/path/to/Meshy_AI_Character_output.fbx",
  asset_path="/Game/Characters/Robot",
  asset_name="SK_Robot",
  create_physics_asset=true,
  import_materials=false,
  import_textures=false
)
```

**Pause** (lightweight query to let engine breathe)

##### TASK-010H: Import robot animations

```
# Import merged animations (contains walk, idle, run, kick)
import_animation(
  fbx_path="/abs/path/to/Meshy_AI_Meshy_Merged_Animations.fbx",
  asset_path="/Game/Characters/Robot/Animations",
  skeleton_path="/Game/Characters/Robot/SK_Robot_Skeleton"
)
```

**Pause** (lightweight query)

```
# Import kick separately if not in merged
import_animation(
  fbx_path="/abs/path/to/Kicking.fbx",
  asset_path="/Game/Characters/Robot/Animations",
  skeleton_path="/Game/Characters/Robot/SK_Robot_Skeleton",
  animation_name="Anim_Kick"
)
```

##### TASK-010I: Create robot PBR material

**REVISED (BUG-012b)**: `create_pbr_material` does NOT support UV channel override. Meshy AI skeletal meshes have texture UVs on **UV channel 1** (not UV0). Separate PBR textures (N/R/M) have different UV layout than the embedded diffuse.

**Correct approach**: Build material manually with `create_material_asset` + `add_material_expression`:
```
TextureCoordinate(index=1) → TextureSample(T_Robot_D) → BaseColor
Constant(0.6) → Roughness
Constant(0.85) → Metallic
(No normal map - causes tangent space seam artifacts with Meshy AI UV layout)
```
See `memory/meshy-robot-material-sop.md` for full SOP.

##### TASK-010J: Create Character Blueprint

```
create_character_blueprint(
  blueprint_name="BP_RobotCharacter",
  blueprint_path="/Game/Characters/Robot",
  skeletal_mesh_path="/Game/Characters/Robot/SK_Robot",
  capsule_radius=40,
  capsule_half_height=90,
  max_walk_speed=500,
  camera_boom_length=250,
  camera_boom_socket_offset_z=150
)
```

##### TASK-010K: Create Animation Blueprint

```
create_anim_blueprint(
  blueprint_name="ABP_Robot",
  blueprint_path="/Game/Characters/Robot",
  skeleton_path="/Game/Characters/Robot/SK_Robot_Skeleton",
  preview_mesh_path="/Game/Characters/Robot/SK_Robot"
)
```

Then assign ABP to character blueprint's skeletal mesh component via `set_actor_property` or manual editor step.

> **NOTE**: Animation state machine wiring (idle↔walk↔run↔kick transitions) is extremely complex to do programmatically. Plan: create the ABP shell via MCP, then the state machine may need manual editor setup OR a dedicated future tool. The C++ `NativeUpdateAnimation` override approach (code-driven rather than graph-driven) is the simpler alternative for MCP automation.

##### TASK-010L: Wire Enhanced Input

Options:
1. **Use existing ThirdPerson template**: The UE5 project already has `BP_ThirdPersonCharacter` with Enhanced Input wired. Copy/adapt its input setup to our robot BP.
2. **Create from scratch via MCP**: Would need `create_input_action`, `create_input_mapping_context` tools (not yet built). Defer to Phase 5 if needed.
3. **C++ character class**: Create a C++ `ARobotCharacter` subclass with hardcoded Enhanced Input bindings, then make the BP inherit from it.

**Recommended**: Option 1 — reparent or adapt the existing ThirdPerson template BP.

##### TASK-010M: Integration test

- Spawn `BP_RobotCharacter` in the Blood & Dust level
- Set as default pawn in GameMode
- Test: WASD movement, mouse look, sprint, attack animation
- Verify camera collision avoidance, no clipping through landscape
- Verify animation transitions (idle↔walk↔run, attack interrupts)

---

#### Phase 5: Skill Creation

##### TASK-010N: Create `/unreal-character-import` skill

Reusable skill for importing any character with the full pipeline:
1. Import skeletal mesh
2. Import animations
3. Create material
4. Create character blueprint
5. Create anim blueprint
6. Wire input

---

#### Safety Rules to Add to CLAUDE.md

```
### Skeletal Mesh & Animation Import Rules
25. Skeletal mesh import is HEAVIER than static mesh — creates skeleton, physics asset,
    processes skin weights. Allow 5+ seconds cooldown before next heavy operation.
26. Animation-only import REQUIRES an existing skeleton. Always validate skeleton_path
    resolves to a valid USkeleton before calling import. Null skeleton = crash.
27. ACharacter CDO components (CapsuleComponent, Mesh, CharacterMovement) are created
    in C++ constructor, NOT via SimpleConstructionScript. Must compile BP first, then
    access via GetDefaultObject<ACharacter>() to modify defaults.
28. UAnimBlueprintFactory CRASHES if TargetSkeleton is null and bTemplate is false.
    Always validate skeleton exists before AnimBlueprint creation.
29. When importing FBX with multiple assets (mesh + animations), iterate
    ImportTask->GetObjects() to find all created assets. Don't assume single result.
30. Never import skeletal mesh + animation in parallel MCP calls. Always sequential
    with breathing room between each.
```

---

### FEATURE-014: Game Designer Skill

**Status**: DONE (2026-02-11)
**Priority**: P1
**Result**: Created `.claude/skills/game-designer.md` — a phased orchestration skill that builds the Blood & Dust level into a playable demo.

#### What It Does
- Phase 0: Audit level state + interactive interview (6 questions with AskUserQuestion)
- Phase 1: Character playability (delegates to `/unreal-character-input`)
- Phase 2: Rembrandt golden chiaroscuro lighting via set_actor_property
- Phase 3: Oil painting post-processing (desaturation, contrast, golden gain, vignette)
- Phase 4: Backdrop billboards (imported 2D paintings or procedural gradients)
- Phase 5: World building (ruins, castle, town via procedural tools)
- Phase 6: NPC spawn and optional patrol
- Phase 7: Combat foundation (health variable, melee damage)

#### Key Design Decisions
- Interactive interview between phases — asks preferences upfront, confirms before each phase
- Biggest visual impact first — Character → Lighting → Post-Process in ~40 min
- Honest about MCP limitations — documents what needs manual editor work
- Checkpoint saves — prompts Ctrl+S after each major phase

---

### FEATURE-015: Playable Demo Pipeline

**Status**: OPEN
**Priority**: P0
**Goal**: Execute the game-designer skill phases to build a playable demo with Rembrandt visual style.
**Depends on**: FEATURE-014 (skill created), FEATURE-010 (character pipeline)

#### Phases
| Phase | Status | MCP Calls | Time Est. |
|-------|--------|-----------|-----------|
| 0: Audit & Interview | OPEN | 5-8 | 5 min |
| 1: Character Controls | OPEN (blocked by TASK-010L) | ~35 | 15 min |
| 2: Rembrandt Lighting | **DONE** (2026-02-12) | ~25 | 10 min |
| 3: Post-Processing | **DONE** (2026-02-12) | ~15 | 8 min |
| 4: Backdrops | OPEN | ~45 | 20 min |
| 5: World Building | OPEN | 10-50 | 15-30 min |
| 6: NPCs | OPEN | 20-60 | 10-40 min |
| 7: Combat | OPEN | 80-120 | 30-60 min |
| 8: Sound & Music | OPEN | ~20 | 15 min |

---

### FEATURE-016: Audio Pipeline

**Status**: DONE (2026-02-11)
**Priority**: P1
**Goal**: C++ MCP tools for importing sound assets, spawning ambient sound actors, and adding audio components to Blueprints.

#### Delivered
- `import_sound` — Imports WAV/OGG as USoundWave (with looping, volume params)
- `spawn_actor(type="AmbientSound")` — 3D spatialized or 2D music via `is_ui_sound`
- `add_component_to_blueprint` — AudioComponent with sound_asset, volume, looping, auto_activate
- Python `import_sound` @mcp.tool() + HEAVY dicts (2.0s cooldown)
- Game designer skill Phase 8: Sound & Music

---

### FEATURE-017: Gameplay Commands

**Status**: DONE (2026-02-11)
**Priority**: P0
**Delivered**: 6 new MCP tools in `EpicUnrealMCPGameplayCommands` handler class:
- `set_game_mode_default_pawn` — auto-configure GameMode BP + PlayerStart
- `create_anim_montage` — create UAnimMontage from AnimSequence
- `play_montage_on_actor` — trigger montage playback
- `apply_impulse` — physics impulse on actors
- `trigger_post_process_effect` — set PostProcess properties with bOverride flags
- `spawn_niagara_system` — place Niagara particle systems

---

### FEATURE-018: Screenshot Tool

**Status**: DONE (2026-02-11)
**Priority**: P1
**Delivered**: Rewrote `take_screenshot` using SceneCapture2D + UTextureRenderTarget2D (old ReadPixels approach returned 0x0 on Linux). Returns `[TextContent, ImageContent]` inline.

---

### FEATURE-019: Widget/HUD Commands

**Status**: DONE (2026-02-11)
**Priority**: P1
**Delivered**: 3 new MCP tools in `EpicUnrealMCPWidgetCommands` handler class:
- `create_widget_blueprint` — UMG widget BP with canvas panel
- `add_widget_to_viewport` — display widget on screen
- `set_widget_property` — modify widget element properties

---

### FEATURE-020: AI/Behavior Tree Commands

**Status**: DONE (2026-02-11)
**Priority**: P1
**Delivered**: 5 new MCP tools in `EpicUnrealMCPAICommands` handler class:
- `create_behavior_tree` — BT asset creation
- `create_blackboard` — blackboard data asset
- `add_bt_task` — add task nodes (MoveTo, Wait, PlayAnimation)
- `add_bt_decorator` — add decorator nodes (Blackboard-based)
- `assign_behavior_tree` — assign BT to AI controller
**Note**: BT task/decorator property setting limited by protected UE members — use BT editor for detailed config.

---

### FEATURE-021: Audio Pipeline

**Status**: DONE (2026-02-11) — see FEATURE-016 above for details.

---

### FEATURE-022: compile_blueprint Fix

**Status**: DONE (2026-02-11)
**Priority**: P2
**Delivered**: Fixed `compile_blueprint` to use `FCompilerResultsLog` and check `Blueprint->Status` for `BS_Error`. Now returns: `compiled` (bool), `status`, `num_errors`, `num_warnings`, `errors[]`, `warnings[]`.

---

### FEATURE-023: GameplayHelperLibrary Runtime Module

**Status**: DONE (2026-02-11)
**Priority**: P3
**Delivered**: Runtime `GameplayHelpers` plugin with `UGameplayHelperLibrary` (BlueprintCallable):
- `SetCharacterWalkSpeed` — change MaxWalkSpeed at runtime
- `PlayAnimationOneShot` — play AnimSequence via montage with blend params
**Note**: MCPHelperLibrary is editor-only (UnrealMCP module) — GameplayHelperLibrary is the runtime alternative for Blueprint use.

---

### BUG-024: Character Moves During Attack/Kick Animations

**Status**: DONE (2026-02-12)
**Priority**: P1
**Root Cause**: `PlayAnimationOneShot` nodes in BP_RobotCharacter had `bStopMovement` pin defaulting to `false`. The function already supports stopping movement — it saves `MaxWalkSpeed`, sets it to 0, calls `StopMovementImmediately()`, and restores speed via timer during blend-out.
**Fix**: Set `bStopMovement=true` via `set_node_property(action="set_pin_default")` on both nodes:
- `K2Node_CallFunction_18` (Attack / IA_Attack_Long)
- `K2Node_CallFunction_19` (Kick / IA_Kick)
**Also updated**: `.claude/skills/unreal-character-input.md` Phase 5 now includes `bStopMovement=true` step.

---

### IDEA-025: Mixamo Character/Animation Pipeline

**Status**: OPEN (optional, future)
**Priority**: P3
**Goal**: Enable importing Mixamo characters and animations for rapid prototyping.

#### Two Approaches

**Option A: Full Mixamo character** (easiest)
- Download character + animations from Mixamo as FBX
- `import_skeletal_mesh` → `import_animation` (all share same skeleton)
- Wire actions via existing `/unreal-character-input` skill pattern
- Thousands of free animations, instantly compatible

**Option B: Mixamo animations on existing robot** (needs retargeting)
- Download animations-only from Mixamo
- Import with Mixamo skeleton
- Set up **IK Retargeter** in UE5 editor (one-time manual step, ~10 min):
  - Create IK Rig for Mixamo skeleton
  - Create IK Rig for Robot skeleton
  - Create IK Retargeter mapping bones between them
- Once retarget asset exists, any future Mixamo animation auto-retargets
- **No MCP tools for IK retargeting currently** — editor UI only

#### MCP Calls Per New Animation (after setup)
1. `import_animation` (with correct skeleton)
2. `add_enhanced_input_action_event` (new IA)
3. `add_node` CallFunction "PlayAnimationOneShot"
4. `connect_nodes` (exec + data pins)
5. `set_node_property` (AnimSequence path, bStopMovement=true)

---

### FEATURE-026: Enemy AI & Combat System

**Status**: IN PROGRESS
**Priority**: P0
**Goal**: Full combat loop — player attacks enemies, enemies fight back with chase AI, health/damage/death for both sides.

#### Architecture: C++ Helper Functions (No NavMesh, No BT, No AIController)

Research (2026-02-12) confirmed: NavMesh + BehaviorTree + AIController is overkill for open-terrain melee combat. Direct `AddMovementInput()` works on landscape without pathfinding. All AI logic lives in a single C++ tick function in GameplayHelperLibrary.

#### Completed Sub-tasks
- **TASK-026A: Player Melee Damage** — DONE (2026-02-12). `ApplyMeleeDamage()` in GameplayHelperLibrary: sphere overlap, Health reflection, ragdoll + knockback + delayed destroy. Kick=15dmg/200r, Heavy=35dmg/250r/75k knockback.
- **TASK-026B: Enemy Health Variables** — DONE (2026-02-12). KingBot=100HP, Giganto=150HP, Bell=75HP via `create_variable`.
- **TASK-026C: Player BP Wiring** — DONE (2026-02-12). ApplyMeleeDamage chained after PlayAnimOneShot for both attacks.
- **TASK-026D: `/unreal-combat` Skill Update** — DONE (2026-02-12). Replaced Tier 3-5 with C++ ApplyMeleeDamage approach.

#### Open Sub-tasks

**TASK-026E: `UpdateEnemyAI` C++ Function** — OPEN
Add to GameplayHelperLibrary. Tick-based state machine:
```
States: Idle → Chase → Attack → Return
- Idle: player beyond AggroRange → do nothing (play idle anim)
- Chase: player within AggroRange → AddMovementInput toward player, face player
- Attack: player within AttackRange → PlayAnimationOneShot + ApplyMeleeDamage
- Return: enemy beyond LeashDistance from spawn → move back to spawn point
```
Signature:
```cpp
static void UpdateEnemyAI(ACharacter* Enemy, float AggroRange=1500, float AttackRange=200,
    float LeashDistance=3000, float MoveSpeed=400, float AttackCooldown=2.0,
    float AttackDamage=10, float AttackRadius=150, UAnimSequence* AttackAnim=nullptr);
```
State stored in `static TMap<TWeakObjectPtr<AActor>, FEnemyAIState>` (no extra BP variables needed).
Internal: reuses `PlayAnimationOneShot` + `ApplyMeleeDamage` for enemy attacks.
Rotation: direct `SetActorRotation(FRotator(0, LookYaw, 0))` — no AIController needed.

**TASK-026F: Rebuild Plugin** — OPEN (blocked by 026E)
Build GameplayHelpers .so, restart editor.

**TASK-026G: Wire Enemy BPs** — OPEN (blocked by 026F)
For each enemy BP (KingBot, Giganto, Bell):
1. `add_node` Event Tick (if not existing)
2. `add_node` CallFunction `UpdateEnemyAI`
3. `connect_nodes` Tick.then → UpdateEnemyAI.execute
4. `set_node_property` per-enemy tuning:
   | Enemy | AggroRange | AttackRange | MoveSpeed | AttackCooldown | AttackDamage |
   |-------|-----------|-------------|-----------|----------------|--------------|
   | KingBot | 1500 | 200 | 400 | 2.0 | 10 |
   | Giganto | 1200 | 250 | 250 | 3.0 | 25 |
   | Bell | 2000 | 180 | 500 | 1.5 | 8 |
5. `set_node_property` AttackAnim per-enemy (reuse their existing anims)
6. `compile_blueprint` each → 0 errors
~10 MCP calls per enemy, ~30 total.

**TASK-026H: `/enemy-ai` Skill** — OPEN
Create `.claude/skills/enemy-ai.md` documenting the full pipeline.

**TASK-026I: Hit Feedback (Optional)** — OPEN
- Flash red PostProcess for 0.2s on player damage
- Screen shake on heavy enemy hits
- Sound effect on hit (requires `import_sound`)

**TASK-026J: Player Death (Optional)** — OPEN
- Add Health variable to player BP
- Enemy attacks reduce player health via ApplyMeleeDamage
- At 0: "You Died" overlay widget + respawn

#### Key Design Decisions
- **No NavMesh**: Open terrain, direct movement. Add NavMesh later if obstacles needed.
- **No BehaviorTree**: BT BlackboardKey is protected in UE5.7, can't wire via MCP. C++ state machine is simpler.
- **No AIController**: `AddMovementInput()` works directly on ACharacter without controller.
- **Static state storage**: `TMap<TWeakObjectPtr<AActor>, FEnemyAIState>` avoids needing extra BP variables.
- **Reuses existing functions**: `PlayAnimationOneShot` for attack anims, `ApplyMeleeDamage` for damage.

---

### FEATURE-032: Pixel Streaming — Claude Plays the Game

**Status**: IN PROGRESS
**Priority**: P1
**Goal**: Enable Claude Code to see and interact with the running Blood & Dust game through Pixel Streaming 2 + Playwright MCP. Claude sees the game via browser screenshots and sends keyboard/mouse input through the browser's WebRTC data channel.

#### Architecture
```
Unreal Game (standalone) → NVENC H.264 → WebRTC → Signalling Server → Browser
Playwright MCP → Browser → screenshot (Claude sees) + keyboard/mouse (Claude acts)
```

#### Completed
- **TASK-032A**: PixelStreaming2 plugin enabled in `.uproject`
- **TASK-032B**: Signalling server config fixed for Linux — `player_port=8080` (no sudo), `http_root=./www`
- **TASK-032C**: Built Common → Signalling → SignallingWebServer (Node v20 works despite wanting v22)
- **TASK-032D**: Created `scripts/start-pixel-streaming.sh` — launches signalling + standalone game, cleanup on exit
- **TASK-032E**: Fixed screenshot deadlock — 2-phase FTSTicker (CaptureScene tick 1, ReadPixels tick 2+3), reduced default resolution to 960x540, added to LARGE_OPERATION_COMMANDS (300s timeout) and HEAVY_COMMANDS_COOLDOWN (1s)

#### Remaining
- **TASK-032F**: Test full pipeline end-to-end (signalling → game → browser → Playwright screenshot + input)
- **TASK-032G**: Create `/play-game` skill — automated game loop (screenshot → Claude decides action → Playwright sends input → repeat)

#### Key Details
- Game runs as standalone process (`-game` flag), not PIE in editor
- Editor can stay open simultaneously for MCP level editing tools
- RTX 4070 Ti dual NVENC encoders — 1280x720 H.264 is trivial
- Player URL: `http://127.0.0.1:8080`
- Ports: player=8080, streamer=8888, SFU=8889

---

### FEATURE-031: Enemy Animation & AI Pipeline (Full Battlefield)

**Status**: IN PROGRESS
**Priority**: P0
**Goal**: Fix all enemy issues and build a living battlefield. One KingBot demonstrates full patrol→chase→attack→death loop as proof of concept, then scale to all 19 enemies.
**PRD**: `.omc/prd.json`

#### Current State (2026-02-13)
- **19 enemies placed**: 8 Bell, 6 KingBot, 5 Giganto
- **Mixamo-rigged**: KingBot only (Bell/Giganto NOT rigged)
- **Animations**: KingBot has 2 Mixamo-rigged (Walking, TurningRight) + 1 original Meshy (boxing). Bell/Giganto have 1 original Meshy each. Generic Mixamo anims imported but DISTORT all 3 types.
- **C++ fixes deployed**: `set_skeletal_animation` clears CDO AnimClass + recompiles BP. `set_character_properties` accepts "None" to clear AnimBP. **Untested in PIE.**

#### Known Issues
1. **Enemy materials broken** — dark patches, wrong UV mapping (UV1 not UV0), too glossy, missing textures on all 3 types
2. **Enemies sunk into ground** — snap_actor_to_ground doesn't compensate for mesh pivot offset
3. **AnimBP overrides SingleNode in PIE** — CDO clearing fix deployed but untested
4. **No AI behavior** — enemies are static placed actors, need patrol→chase→attack
5. **No combat damage loop** — player attacks exist but don't deal damage to enemies yet
6. **Only KingBot Mixamo-rigged** — Bell/Giganto need Mixamo rigging for diverse animations

#### Sub-tasks

**TASK-031A: Fix enemy materials (all 3 types)**
- Audit M_KingBot, M_Bell, M_Giganto — identify dark patches, UV issues, glossy artifacts
- Fix: TextureCoordinate(index=1) for all samplers, disconnect AO, scalar roughness ~0.7
- Rebuild materials if needed using manual material creation (not create_pbr_material)

**TASK-031B: Fix enemy ground positioning**
- Determine mesh pivot offset per type (center vs feet)
- snap_actor_to_ground + manual Z offset for all 19 enemies
- Account for different scales (some at 2.5x-3x)

**TASK-031C: Verify SingleNode animation in PIE**
- Apply KingBot_Walking_Mixamo to one KingBot, enter PIE, confirm it plays
- CRITICAL GATE — determines animation architecture

**TASK-031D: Build KingBot AI (patrol → detect → chase → attack)**
- Depends on FEATURE-026 TASK-026E (`UpdateEnemyAI` C++ function)
- Needs 2 animations: walk + attack (use Mixamo walking + original Meshy boxing if needed)
- Wire EventTick → UpdateEnemyAI in KingBot BP

**TASK-031E: Combat loop (health, damage, death)**
- Wire ApplyMeleeDamage to player attacks → KingBot
- KingBot attacks deal damage to player
- Death: ragdoll or death anim + delayed destroy

**TASK-031F: User rigs Bell + Giganto on Mixamo** (USER task)
- Blocked until 031E proves the pipeline on KingBot
- Upload Character_output.fbx for each to Mixamo
- Download 4+ animations per type: walk, idle, attack, stagger

**TASK-031G: Import + assign animations to all 19 enemies**
- Import Mixamo-rigged animations for Bell + Giganto
- Assign diverse animations: near enemies get AI, distant get looping SingleNode

**TASK-031H: Clean up distorted generic Mixamo animations**
- Delete Anim_Idle/Running/BodyBlock/ZombieStandUp/Walking from all enemy folders

#### Success Gate
One KingBot: patrol → detect player → chase (walk anim) → reach player → attack (attack anim) → deal damage → take damage from player → die. If this works → user rigs Bell + Giganto on Mixamo → scale to all 19.

#### Battlefield Vision
- **Near enemies** (~5000 units): Real AI with patrol/chase/attack
- **Distant enemies**: Atmospheric background fighters — looping animations, silhouettes in fog

---

### FEATURE-027: World Building & Structures

**Status**: OPEN
**Priority**: P1
**Goal**: Build the level layout for "The Escape" — start zone, middle battlefield, background, end zone.

#### Level Zones
- **Start zone**: Small ruin enclosure. Player spawns inside. Tutorial-feeling safe area.
- **Middle zone**: Open battlefield. 3-5 patrol enemies. Scattered cover (broken walls, pillars, rocks).
- **Background**: Castle/fortress silhouettes in fog. Ambient NPC pairs fighting. Not interactable.
- **End zone**: Glowing portal/archway. 1-2 final enemies guarding it. Overlap triggers win.

#### Tools Available
- `construct_house`, `construct_mansion`, `create_castle_fortress` — procedural structures
- `create_arch`, `create_wall`, `create_tower`, `create_staircase` — individual elements
- `spawn_niagara_system` — fire/torch VFX, dust particles
- `scatter_meshes_on_landscape` — rubble, debris

---

### FEATURE-028: HUD & UI

**Status**: OPEN
**Priority**: P1
**Goal**: On-screen UI for gameplay feedback — health, crosshair, win/lose screens, menus.

#### Widgets Needed
| Widget | Type | Trigger |
|--------|------|---------|
| Health bar | ProgressBar + Text | Updates on damage event |
| Crosshair | Image (center screen) | Always visible during gameplay |
| "Demo Complete" | Full-screen overlay | Player overlaps escape portal |
| "You Died" | Full-screen overlay + restart button | Player health reaches 0 |
| Pause menu | Overlay with Resume/Quit | Esc key toggle |

#### Tools Available
- `create_widget_blueprint` — creates UMG widget with canvas
- `add_widget_to_viewport` — displays widget
- `set_widget_property` — configure elements

---

### FEATURE-029: Ambient Audio

**Status**: OPEN
**Priority**: P2
**Goal**: Atmospheric sound design — wind, distant combat, music.

#### Audio Layers
- **Wind**: Looping ambient, 3D spatialized, placed at multiple points
- **Distant combat**: Looping metal clashing sounds, low volume, placed far from player
- **Music**: 2D background track (is_ui_sound=true), looping, low volume
- **Combat music**: Triggered when enemies detect player, crossfade from ambient track

#### Tools Available
- `import_sound` — import WAV/OGG assets
- `spawn_actor(type="AmbientSound")` — place 3D/2D sounds in level
- `add_component_to_blueprint` with AudioComponent — attach sounds to BPs

---

### TASK-009: Fix robot walk cycle - realistic ground contact & locomotion
- **Status**: IN PROGRESS
- **Scene**: `blender/projects/blood_and_dust_terrain_4.blend` (or whichever is open)
- **Changes made**:
  - Rebuilt `ArmatureAction` Z curve with terrain raycasting (101 samples) for ground contact
  - Extended animation from 192 → 400 frames
  - NLA walk cycle speed: 31 → 67 frames/cycle (slower stride)
  - Forward speed halved: 0.15 → 0.07 units/frame
  - Eliminated upward drift (Z no longer rises from -3.19 to 0)
- **Remaining**: Verify foot sliding at new speed, fine-tune stride matching if needed

## Completed

<!-- Move completed tasks here with strikethrough -->
