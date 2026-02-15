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
| **BUG-011** | **Landscape material persists on streaming proxies after reset** | **P0** | OPEN | - |
| **FEATURE-010** | **Playable character pipeline: import, rig, animate, play** | **P0** | IN PROGRESS | - |
| TASK-010A | C++ `import_skeletal_mesh` MCP tool | P0 | OPEN | - |
| TASK-010B | C++ `import_animation` MCP tool | P0 | OPEN | TASK-010A |
| TASK-010C | C++ `create_character_blueprint` MCP tool | P0 | OPEN | - |
| TASK-010D | C++ `create_anim_blueprint` MCP tool | P0 | OPEN | TASK-010A |
| TASK-010E | Python MCP server: register 4 new tools | P0 | OPEN | TASK-010A..D |
| TASK-010F | Rebuild C++ plugin & test each tool | P0 | OPEN | TASK-010E |
| TASK-010G | Import Meshy robot skeletal mesh + textures | P0 | OPEN | TASK-010F |
| TASK-010H | Import robot animations (walk, idle, run, kick) | P0 | OPEN | TASK-010G |
| TASK-010I | Create robot PBR material | P0 | OPEN | TASK-010G |
| TASK-010J | Create Character Blueprint with camera + movement | P0 | OPEN | TASK-010G |
| TASK-010K | Create Animation Blueprint with state machine | P0 | OPEN | TASK-010H, TASK-010J |
| TASK-010L | Wire Enhanced Input (WASD, mouse look, sprint, attack) | P0 | OPEN | TASK-010J |
| TASK-010M | Integration test: fully playable character in level | P0 | OPEN | TASK-010K, TASK-010L |
| TASK-010N | Create `/unreal-character-import` skill | P1 | OPEN | TASK-010M |
| **TASK-012** | **Add materials to all vegetation meshes** | **P2** | OPEN | - |
| **TASK-013** | **Fix floating vegetation - snap to ground** | **P2** | OPEN | - |
| ~~**TASK-014**~~ | ~~**Fix floating enemies - snap to ground**~~ | **P0** | ✅ DONE (2026-02-14) | - |
| **BUG-015** | **Enemies float/sink/clip — need reliable grounding** | **P0** | IN PROGRESS | - |
| ~~**BUG-016**~~ | ~~**Player can't move until attack button is pressed**~~ | **P0** | ~~DONE (2026-02-15)~~ | - |
| **TASK-017** | **Custom health bar graphic in HUD** | **P2** | OPEN | - |

## Active Work

---

### FEATURE-010: Playable Character Pipeline — Import, Rig, Animate, Play

**Status**: IN PROGRESS (research complete, implementation pending)
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

Use existing `create_pbr_material` + `import_texture` tools:
```
# Import textures (max 2, then breathe)
import_texture(normal map) → /Game/Characters/Robot/Textures/T_Robot_N
import_texture(metallic)   → /Game/Characters/Robot/Textures/T_Robot_M
# breathe
import_texture(roughness)  → /Game/Characters/Robot/Textures/T_Robot_R

# Create material
create_pbr_material(name="M_Robot", normal=T_Robot_N, ...)
```

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
