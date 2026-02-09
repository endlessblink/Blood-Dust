# Skill: Playable Character Pipeline

Complete pipeline for creating a playable third-person character via MCP: Blueprint, input wiring, materials, animations.

## Prerequisites
- Skeletal mesh imported (e.g., `/Game/Characters/Robot/SK_Robot`)
- Skeleton exists (e.g., `/Game/Characters/Robot/SK_Robot_Skeleton`)
- Animation sequences imported (Idle, Walk, Run minimum)
- Textures imported (Diffuse, Normal, Roughness, Metallic)
- Input Action assets at `/Game/Input/Actions/` (IA_Move, IA_Look, IA_Jump, IA_MouseLook)
- MCP server connected with updated C++ plugin

## Pipeline Overview

```
1. create_character_blueprint → BP with capsule, camera, movement
2. Wire Enhanced Input       → WASD/Mouse/Jump controls
3. create_pbr_material       → PBR material from textures
4. set_mesh_asset_material   → Apply material to skeletal mesh ASSET
5. create_anim_blueprint     → Empty AnimBP targeting skeleton
6. setup_locomotion_state_machine → Idle/Walk/Run with speed transitions
7. set_character_properties  → Assign AnimBP to character's mesh CDO
8. Verify & compile
```

**CRITICAL**: All MCP calls STRICTLY SEQUENTIAL. Never parallel.

---

## Phase 1: Character Blueprint

```
create_character_blueprint(
    blueprint_name="BP_RobotCharacter",
    blueprint_path="/Game/Characters/Robot/",
    skeletal_mesh_path="/Game/Characters/Robot/SK_Robot",
    capsule_radius=40, capsule_half_height=90,
    max_walk_speed=500, jump_z_velocity=420,
    camera_boom_length=250, camera_boom_socket_offset_z=150
)
```

**Note**: Do NOT pass `anim_blueprint_path` here — the AnimBP doesn't exist yet. Use `set_character_properties` later.

## Phase 2: Wire Enhanced Input

### Strategy: Direct Wiring (NOT Custom Functions)

**CRITICAL**: Do NOT use custom Blueprint functions via MCP. The `create_function` → `add_function_input` → `CallFunction` pipeline has a fundamental timing problem: SkeletonGeneratedClass signature doesn't reflect added parameters until compile.

Wire everything directly in EventGraph. More nodes but each is independently correct.

### Step 0: Verify Pin Names (MANDATORY)

```
read_blueprint_content(blueprint_path="/Game/Characters/Robot/BP_RobotCharacter")
```

Inspect K2Node_EnhancedInputAction nodes to confirm:
- Axis2D actions have `ActionValue_X` and `ActionValue_Y` sub-pins
- Boolean actions have `ActionValue` as bool
- Capture exact node IDs (use GetName format like `K2Node_EnhancedInputAction_0`)

### Jump (Simplest - validates approach)

```
IA_Jump.Started → Jump.execute
IA_Jump.Completed → StopJumping.execute
```

1. `add_enhanced_input_action_event` for IA_Jump
2. `add_node` CallFunction "Jump"
3. `connect_nodes` IA_Jump.Started → Jump.execute
4. `add_node` CallFunction "StopJumping"
5. `connect_nodes` IA_Jump.Completed → StopJumping.execute
6. `compile_blueprint` — verify before continuing

### Camera Look (IA_Look + IA_MouseLook)

For each action, same pattern:
```
EnhancedInput.Triggered → AddControllerYawInput(Val=ActionValue_X)
                         → AddControllerPitchInput(Val=ActionValue_Y)
```

1. `add_node` CallFunction "AddControllerYawInput"
2. `connect_nodes` EnhancedInput.Triggered → Yaw.execute
3. `connect_nodes` EnhancedInput.ActionValue_X → Yaw.Val
4. `add_node` CallFunction "AddControllerPitchInput"
5. `connect_nodes` Yaw.then → Pitch.execute
6. `connect_nodes` EnhancedInput.ActionValue_Y → Pitch.Val

### Movement (Most Complex)

```
IA_Move.Triggered → AddMovementInput(GetRightVector(GetControlRotation()), ActionValue_X)
                  → AddMovementInput(GetForwardVector(GetControlRotation()), ActionValue_Y)
```

1. `add_node` CallFunction "GetControlRotation"
2. `add_node` CallFunction "GetRightVector"
3. `connect_nodes` GetControlRotation.ReturnValue → GetRightVector.InRot (full FRotator, NOT sub-pins)
4. `add_node` CallFunction "GetForwardVector"
5. `connect_nodes` GetControlRotation.ReturnValue → GetForwardVector.InRot
6. `add_node` CallFunction "AddMovementInput" (right)
7. `connect_nodes` IA_Move.Triggered → AddMovementInput_Right.execute
8. `connect_nodes` GetRightVector.ReturnValue → WorldDirection
9. `connect_nodes` IA_Move.ActionValue_X → ScaleValue
10. `add_node` CallFunction "AddMovementInput" (forward)
11. `connect_nodes` AddMovementInput_Right.then → AddMovementInput_Forward.execute
12. `connect_nodes` GetForwardVector.ReturnValue → WorldDirection
13. `connect_nodes` IA_Move.ActionValue_Y → ScaleValue

**Key**: Connect full FRotator ReturnValue → InRot, NOT individual Yaw/Pitch/Roll sub-pins.

### Final Compile
```
compile_blueprint(blueprint_name="/Game/Characters/Robot/BP_RobotCharacter")
```

---

## Phase 3: Material

```
create_pbr_material(
    name="M_Robot",
    path="/Game/Characters/Robot/",
    diffuse_texture="/Game/Characters/Robot/Textures/T_Robot_D",
    normal_texture="/Game/Characters/Robot/Textures/T_Robot_N",
    roughness_texture="/Game/Characters/Robot/Textures/T_Robot_R",
    metallic_texture="/Game/Characters/Robot/Textures/T_Robot_M"
)
```

Then apply to the **mesh asset** (NOT the actor instance — PIE spawns new instances from BP defaults):
```
set_mesh_asset_material(
    mesh_path="/Game/Characters/Robot/SK_Robot",
    material_path="/Game/Characters/Robot/M_Robot"
)
```

**CRITICAL**: `apply_material_to_actor` only affects the placed instance. For PIE, must use `set_mesh_asset_material` on the asset itself.

---

## Phase 4: Animation Blueprint

### Step 1: Create AnimBP
```
create_anim_blueprint(
    blueprint_name="ABP_Robot",
    skeleton_path="/Game/Characters/Robot/SK_Robot_Skeleton",
    blueprint_path="/Game/Characters/Robot/",
    preview_mesh_path="/Game/Characters/Robot/SK_Robot"
)
```

### Step 2: Setup Locomotion State Machine
```
setup_locomotion_state_machine(
    anim_blueprint_path="/Game/Characters/Robot/ABP_Robot",
    idle_animation="/Game/Characters/Robot/Animations/IdleAnim",
    walk_animation="/Game/Characters/Robot/Animations/WalkAnim",
    run_animation="/Game/Characters/Robot/Animations/RunAnim",
    walk_speed_threshold=5,
    run_speed_threshold=300,
    crossfade_duration=0.2
)
```

**What this creates internally**:
- State Machine node in AnimGraph → connected to Root output
- 3 states (Idle/Walk/Run) each with SequencePlayer → StateResult
- 4 transitions with speed-based rules (Greater/Less_DoubleDouble comparisons)
- Speed variable in AnimBP
- EventBlueprintUpdateAnimation event graph:
  - TryGetPawnOwner → GetVelocity → VSize → Set Speed
  - Event.Then → SetSpeed.Execute (pure nodes have NO exec pins)

### Step 3: Verify Compile
```
compile_blueprint(blueprint_name="/Game/Characters/Robot/ABP_Robot")
```

Check EventGraph with `analyze_blueprint_graph` — verify:
- SetSpeed.Speed pin has 1 connection (VSize.ReturnValue)
- Event.then has 1 connection (SetSpeed.execute)
- No orphaned nodes (delete any disconnected TryGetPawnOwner)

### Step 4: Assign AnimBP to Character
```
set_character_properties(
    blueprint_path="/Game/Characters/Robot/BP_RobotCharacter",
    anim_blueprint_path="/Game/Characters/Robot/ABP_Robot"
)
```

This sets `AnimInstanceClass` on the Character's CDO SkeletalMeshComponent.

---

## Key Technical Details

### C++ Tools Used (in EpicUnrealMCPBlueprintCommands)
| Tool | Purpose |
|------|---------|
| `create_character_blueprint` | Character BP with camera, capsule, movement |
| `create_anim_blueprint` | AnimBP targeting a skeleton |
| `setup_locomotion_state_machine` | Full state machine + speed logic |
| `set_character_properties` | Update CDO: AnimBP, mesh, offset |
| `set_mesh_asset_material` | Set default material on USkeletalMesh |

### AnimBP State Machine Architecture (C++)
```
AnimGraph:
  AnimGraphNode_StateMachine → AnimGraphNode_Root

State Machine Graph:
  EntryNode → IdleState
  IdleState ↔ WalkState (transitions with Speed > / < WalkThreshold)
  WalkState ↔ RunState  (transitions with Speed > / < RunThreshold)

Each State:
  AnimGraphNode_SequencePlayer(AnimAsset) → AnimGraphNode_StateResult

Each Transition BoundGraph:
  K2Node_VariableGet(Speed) → ComparisonFunc.A
  ComparisonFunc(A, B=Threshold).ReturnValue → TransitionResult.bCanEnterTransition

EventGraph:
  Event BlueprintUpdateAnimation
    → SetSpeed.Execute
  TryGetPawnOwner.ReturnValue → GetVelocity.Self (pure, data-only)
  GetVelocity.ReturnValue → VSize.A (pure, data-only)
  VSize.ReturnValue → SetSpeed.Speed (pure, data-only)
```

### Critical Implementation Notes (AnimBP)

1. **Compile after adding Speed variable** — K2Node_VariableSet::AllocateDefaultPins needs the variable in the compiled class to create the Speed pin. Without the intermediate compile, VSize→SetSpeed connection silently fails.

2. **Transition node ordering** — `PostPlacedNewNode()` → `AllocateDefaultPins()` → `CreateConnections(Source, Target)`. Calling CreateConnections before AllocateDefaultPins crashes (Pins array is empty).

3. **Pure functions have NO exec pins** — TryGetPawnOwner, GetVelocity, VSize are all const/BlueprintPure. Wire Event.Then directly to SetSpeed.Execute. Blueprint VM evaluates pure nodes lazily.

4. **UE5.5 comparison function names** — `Greater_DoubleDouble` / `Less_DoubleDouble` (NOT FloatFloat). Fallback to FloatFloat for older versions.

5. **Orphaned TryGetPawnOwner** — `create_anim_blueprint` places a default TryGetPawnOwner node at (0,100). The state machine handler creates its own at (300,400). Delete the orphaned one at (0,100) after setup.

6. **set_mesh_asset_material supports SkeletalMesh** — The C++ handler tries UStaticMesh first, then USkeletalMesh. For characters, material must be on the ASSET not the actor instance.

7. **set_character_properties for AnimBP assignment** — `create_character_blueprint` rejects existing BPs. Use `set_character_properties` to assign AnimBP after creation.

### Pin Names Reference
| Node | Key Input Pins | Key Output Pins |
|------|---------------|-----------------|
| Jump | execute | then |
| StopJumping | execute | then |
| AddControllerYawInput | execute, Val | then |
| AddControllerPitchInput | execute, Val | then |
| AddMovementInput | execute, WorldDirection, ScaleValue | then |
| GetControlRotation | (none) | ReturnValue (FRotator) |
| GetForwardVector | InRot (FRotator) | ReturnValue (FVector) |
| GetRightVector | InRot (FRotator) | ReturnValue (FVector) |
| EnhancedInputAction (Axis2D) | (none) | Triggered, Started, Completed, ActionValue_X, ActionValue_Y |

### Common Gotchas
- **NEVER predict node IDs** — always capture from add_node response
- **connect_nodes auto-compiles** after every connection (BPConnector.cpp:153)
- **Custom functions timing bug** — SkeletonGeneratedClass doesn't update params until compile
- **Enhanced Input node GUIDs often all-zeros** — FindNodeById falls back to GetName() matching
- **Multiple output connections work** — MakeLinkTo appends to link array
- **CDO components (Mesh, CapsuleComponent)** not visible in SCS — use set_character_properties
- **FRotator pin connection** — Connect full ReturnValue (FRotator) → InRot, NOT individual sub-pins

## Integration Test Checklist
After full pipeline:
1. PIE test: WASD movement follows camera direction
2. PIE test: Mouse look rotates camera
3. PIE test: Space jumps, release stops jumping
4. PIE test: Robot has PBR material (not grey)
5. PIE test: Idle animation plays when stationary
6. PIE test: Walk animation transitions when moving slowly
7. PIE test: Run animation transitions at higher speeds
8. Verify: No "Blueprint Compilation Errors" dialog on PIE start
