# Skill: Wire Enhanced Input to Character Blueprint

Wire Enhanced Input Action events (movement, camera, jump) to a Character Blueprint via MCP tools. Creates a fully playable third-person character.

## Prerequisites
- Character Blueprint exists (created via `create_character_blueprint`)
- Input Action assets exist at `/Game/Input/Actions/` (IA_Move, IA_Look, IA_Jump, IA_MouseLook)
- Input Mapping Context configured on PlayerController
- MCP server connected with updated C++ plugin (parent class search fallback)

## Strategy: Direct Wiring (NOT Custom Functions)

**CRITICAL**: Do NOT use custom Blueprint functions (Move/Aim) via MCP. The `create_function` → `add_function_input` → `CallFunction` pipeline has a fundamental timing problem: SkeletonGeneratedClass signature doesn't reflect added parameters until compile, causing CallFunction nodes to be created with wrong pin signatures.

Instead, wire everything directly in EventGraph. More nodes but each is independently correct.

## Phase 0: Verify Pin Names (MANDATORY)

```
read_blueprint_content(blueprint_path="/Game/Characters/Robot/BP_RobotCharacter")
```

Inspect the existing K2Node_EnhancedInputAction nodes to confirm:
- Axis2D actions have `ActionValue_X` and `ActionValue_Y` sub-pins
- Boolean actions have `ActionValue` as bool
- Capture exact node IDs (use GetName format like `K2Node_EnhancedInputAction_0`)

## Phase 1: Jump (Simplest - Validates Approach)

### Step 1: Create Enhanced Input Action Nodes
```
add_enhanced_input_action_event(
    blueprint_path="/Game/Characters/Robot/BP_RobotCharacter",
    input_action_path="/Game/Input/Actions/IA_Jump",
    pos_x=-600, pos_y=800
)
```
Note: Skip if already created. Capture node_id.

### Step 2: Create Jump CallFunction
```
add_node(
    blueprint_name="/Game/Characters/Robot/BP_RobotCharacter",
    node_type="CallFunction",
    target_function="Jump",
    pos_x=200, pos_y=800
)
```
- Found via parent class search: ACharacter::Jump
- Capture returned node_id (e.g., `K2Node_CallFunction_0`)

### Step 3: Connect IA_Jump.Started → Jump.execute
```
connect_nodes(
    blueprint_name="/Game/Characters/Robot/BP_RobotCharacter",
    source_node_id="<IA_Jump_node_id>",
    source_pin_name="Started",
    target_node_id="<Jump_node_id>",
    target_pin_name="execute"
)
```

### Step 4: Create StopJumping CallFunction
```
add_node(..., target_function="StopJumping", pos_x=200, pos_y=1000)
```

### Step 5: Connect IA_Jump.Completed → StopJumping.execute
```
connect_nodes(..., source_pin_name="Completed", target_pin_name="execute")
```

### Checkpoint: Compile and verify
```
compile_blueprint(blueprint_name="/Game/Characters/Robot/BP_RobotCharacter")
```

## Phase 2: Camera Look (IA_Look + IA_MouseLook)

For each action (IA_Look and IA_MouseLook), create the same pattern:

### Pattern: Aim Wiring
```
EnhancedInputAction.Triggered → AddControllerYawInput(Val=ActionValue_X)
                                → AddControllerPitchInput(Val=ActionValue_Y)
```

### Steps (repeat for both IA_Look and IA_MouseLook):

1. `add_node` CallFunction "AddControllerYawInput" at appropriate position
2. `connect_nodes` EnhancedInput.Triggered → Yaw.execute
3. `connect_nodes` EnhancedInput.ActionValue_X → Yaw.Val
4. `add_node` CallFunction "AddControllerPitchInput"
5. `connect_nodes` Yaw.then → Pitch.execute
6. `connect_nodes` EnhancedInput.ActionValue_Y → Pitch.Val

**Functions found by**: Parent class walk (APawn::AddControllerYawInput, APawn::AddControllerPitchInput)

## Phase 3: Movement (Most Complex)

### Pattern: Movement Wiring
```
IA_Move.Triggered → AddMovementInput(WorldDirection=RightVec, ScaleValue=ActionValue_X)
                   → AddMovementInput(WorldDirection=ForwardVec, ScaleValue=ActionValue_Y)
Where:
  RightVec = GetRightVector(GetControlRotation().Yaw)
  ForwardVec = GetForwardVector(GetControlRotation().Yaw)
```

### Steps:

1. `add_node` CallFunction "GetControlRotation" - returns Yaw/Pitch/Roll
2. `add_node` CallFunction "GetRightVector" (KismetMathLibrary)
3. `connect_nodes` GetControlRotation.ReturnValue_Yaw → GetRightVector.InRot_Yaw
4. `add_node` CallFunction "AddMovementInput" (right direction)
5. `connect_nodes` IA_Move.Triggered → AddMovementInput_Right.execute
6. `connect_nodes` GetRightVector.ReturnValue → AddMovementInput_Right.WorldDirection
7. `connect_nodes` IA_Move.ActionValue_X → AddMovementInput_Right.ScaleValue
8. `add_node` CallFunction "GetForwardVector" (KismetMathLibrary)
9. `connect_nodes` GetControlRotation.ReturnValue_Yaw → GetForwardVector.InRot_Yaw
10. `add_node` CallFunction "AddMovementInput" (forward direction)
11. `connect_nodes` AddMovementInput_Right.then → AddMovementInput_Forward.execute
12. `connect_nodes` GetForwardVector.ReturnValue → AddMovementInput_Forward.WorldDirection
13. `connect_nodes` IA_Move.ActionValue_Y → AddMovementInput_Forward.ScaleValue

**Note**: Single GetControlRotation node can connect ReturnValue_Yaw to BOTH GetRightVector and GetForwardVector (MakeLinkTo appends, doesn't replace).

## Phase 4: Final Compile
```
compile_blueprint(blueprint_name="/Game/Characters/Robot/BP_RobotCharacter")
```

## Key Technical Details

### CallFunction Node Search (C++ UtilityNodes.cpp)
Order: target_class → KismetSystemLibrary → KismetMathLibrary → SkeletonGeneratedClass → ParentClass hierarchy

### Pin Names Reference
| Node | Key Input Pins | Key Output Pins |
|------|---------------|-----------------|
| Jump | execute | then |
| StopJumping | execute | then |
| AddControllerYawInput | execute, Val | then |
| AddControllerPitchInput | execute, Val | then |
| AddMovementInput | execute, WorldDirection, ScaleValue | then |
| GetControlRotation | (none) | ReturnValue_Yaw, ReturnValue_Pitch, ReturnValue_Roll |
| GetForwardVector | InRot_Yaw, InRot_Pitch, InRot_Roll | ReturnValue (FVector) |
| GetRightVector | InRot_Yaw, InRot_Pitch, InRot_Roll | ReturnValue (FVector) |
| K2Node_EnhancedInputAction (Axis2D) | (none) | Triggered, Started, Completed, ActionValue_X, ActionValue_Y |

### Common Gotchas
- **NEVER predict node IDs** - always capture from add_node response
- **connect_nodes auto-compiles** after every connection (BPConnector.cpp:153)
- **Custom functions timing bug** - SkeletonGeneratedClass doesn't update params until compile
- **Enhanced Input node GUIDs often all-zeros** - FindNodeById falls back to GetName() matching
- **Multiple output connections work** - MakeLinkTo appends to link array
- **target_class not needed** if C++ has parent class search fallback (UtilityNodes.cpp update 2026-02-09)
- **CDO components (Mesh, CapsuleComponent)** not visible in SCS - can't set material via apply_material_to_blueprint

## Integration Test Checklist
After wiring:
1. Spawn BP in level: `spawn_blueprint_actor_in_level`
2. Set as default pawn in GameMode or PlayerStart
3. PIE test: WASD movement follows camera direction
4. PIE test: Mouse look rotates camera
5. PIE test: Space jumps, release stops jumping
