# Skill: Playable Character Input Pipeline

Complete pipeline for wiring Enhanced Input to a Character Blueprint via MCP. Handles movement, camera, jump, sprint, and attack with self-contained IMC setup.

## Prerequisites

Before running this skill, verify these exist:
- Character Blueprint (e.g., `/Game/Characters/Robot/BP_RobotCharacter`)
- Input Mapping Contexts: `/Game/Input/IMC_Default`, `/Game/Input/IMC_MouseLook`
- Input Actions: `IA_Move`, `IA_Look`, `IA_Jump`, `IA_MouseLook` (and optionally `IA_Sprint`, `IA_Attack_Long`, `IA_Kick`)
- GameplayHelpers plugin compiled (provides `AddInputMappingContextToCharacter`, `SetCharacterWalkSpeed`, `PlayAnimationOneShot`)

**Run `does_asset_exist` for each before proceeding.** Missing assets = skill will fail silently.

## CRITICAL RULES

1. **ALL MCP calls STRICTLY SEQUENTIAL** — never parallel. Each creates FTSTicker callback; parallel = crash.
2. **connect_nodes APPENDS connections** — MakeLinkTo adds to existing links. Does NOT replace. If you need to fix a wrong connection, DELETE the node and recreate it.
3. **Always capture node IDs** from `add_node` responses. NEVER predict or guess node IDs.
4. **Compile after each major section** — catches errors early before they cascade.
5. **Axis mapping depends on IMC_Default configuration** — verify with `read_blueprint_content` after wiring if movement feels wrong.

---

## Phase 0: Prerequisites Check

```
does_asset_exist(asset_path="/Game/Characters/Robot/BP_RobotCharacter")
does_asset_exist(asset_path="/Game/Input/IMC_Default")
does_asset_exist(asset_path="/Game/Input/IMC_MouseLook")
does_asset_exist(asset_path="/Game/Input/Actions/IA_Move")
does_asset_exist(asset_path="/Game/Input/Actions/IA_Look")
does_asset_exist(asset_path="/Game/Input/Actions/IA_Jump")
```

Then inspect the current BP state:
```
read_blueprint_content(blueprint_path="/Game/Characters/Robot/BP_RobotCharacter")
```

Check: Are EnhancedInputAction nodes already present? Are they already wired? If fully wired, skip to Phase 6.

---

## Phase 1: IMC Self-Setup in BeginPlay

**WHY THIS IS MANDATORY**: `set_game_mode_default_pawn` double-compiles GameMode BP, which can reset PlayerControllerClass CDO, breaking IMC setup on the PlayerController. The character MUST add its own IMC in BeginPlay to be self-contained.

### Wiring Pattern

```
BeginPlay → AddInputMappingContextToCharacter(IMC_Default, Priority=0)
          → AddInputMappingContextToCharacter(IMC_MouseLook, Priority=1)
```

### MCP Calls (~8 calls)

1. Find the existing BeginPlay node:
```
read_blueprint_content(blueprint_path="/Game/Characters/Robot/BP_RobotCharacter")
```
Capture the BeginPlay node ID (usually `K2Node_Event_0` or similar).

2. Add first IMC setup node:
```
add_node(
    blueprint_path="/Game/Characters/Robot/BP_RobotCharacter",
    node_type="CallFunction",
    function_name="AddInputMappingContextToCharacter",
    target_class="/Script/GameplayHelpers.GameplayHelperLibrary"
)
```
Capture node_id → `AddIMC_Default_ID`

3. Connect exec chain:
```
connect_nodes(
    blueprint_path="/Game/Characters/Robot/BP_RobotCharacter",
    source_node_id="<BeginPlay_ID>",
    source_pin_name="then",
    target_node_id="<AddIMC_Default_ID>",
    target_pin_name="execute"
)
```

4. Set IMC parameter (object pin):
```
connect_nodes: Self → Character pin
```
Set the `MappingContext` pin to `/Game/Input/IMC_Default` and `Priority` to `0`.

**NOTE**: The exact pin names for the function parameters depend on the C++ function signature. Use `analyze_blueprint_graph` after adding the node to discover exact pin names.

5. Add second IMC node:
```
add_node(... function_name="AddInputMappingContextToCharacter", target_class="/Script/GameplayHelpers.GameplayHelperLibrary")
```
Capture → `AddIMC_MouseLook_ID`

6. Chain exec:
```
connect_nodes: AddIMC_Default.then → AddIMC_MouseLook.execute
```

7. Set IMC parameter to `/Game/Input/IMC_MouseLook`, Priority=1

8. Compile:
```
compile_blueprint(blueprint_name="/Game/Characters/Robot/BP_RobotCharacter")
```
Expect: 0 errors, 0 warnings.

---

## Phase 2: Jump (Validation Step — ~5 calls)

Jump is the simplest action. Wire it first to validate the approach works.

### Wiring Pattern
```
IA_Jump.Started → Jump()
IA_Jump.Completed → StopJumping()
```

### MCP Calls

1. `add_enhanced_input_action_event` for IA_Jump (if not already present)
2. `add_node` CallFunction "Jump" → capture `Jump_ID`
3. `connect_nodes` IA_Jump.Started → Jump.execute
4. `add_node` CallFunction "StopJumping" → capture `StopJump_ID`
5. `connect_nodes` IA_Jump.Completed → StopJumping.execute

### Verify
```
compile_blueprint(blueprint_name="/Game/Characters/Robot/BP_RobotCharacter")
```
Must return 0 errors. If errors, stop and debug before continuing.

---

## Phase 3: Camera Look (~12 calls for IA_Look + IA_MouseLook)

### Wiring Pattern (same for both IA_Look and IA_MouseLook)
```
EnhancedInput.Triggered → AddControllerYawInput(Val=ActionValue_X)
                         → AddControllerPitchInput(Val=ActionValue_Y)
```

**IMPORTANT — NO pitch negation needed.** The UE5 template passes ActionValue_Y directly to AddControllerPitchInput without negation. If camera feels inverted, the fix is in the IMC modifier (Negate Y), NOT in the Blueprint wiring.

### MCP Calls (per action — repeat for both IA_Look and IA_MouseLook)

1. `add_node` CallFunction "AddControllerYawInput" → capture `Yaw_ID`
2. `connect_nodes` EnhancedInput.Triggered → Yaw.execute
3. `connect_nodes` EnhancedInput.ActionValue_X → Yaw.Val
4. `add_node` CallFunction "AddControllerPitchInput" → capture `Pitch_ID`
5. `connect_nodes` Yaw.then → Pitch.execute
6. `connect_nodes` EnhancedInput.ActionValue_Y → Pitch.Val

### After Both Actions
```
compile_blueprint(blueprint_name="/Game/Characters/Robot/BP_RobotCharacter")
```

---

## Phase 4: Movement (CORRECTED Axis Mapping — ~13 calls)

### CRITICAL: Axis Mapping Explanation

The mapping between `ActionValue_X`/`ActionValue_Y` and Forward/Right depends on **how IMC_Default maps keys to axes**:

| IMC_Default Mapping | ActionValue_X | ActionValue_Y |
|---------------------|---------------|---------------|
| W/S → X axis, A/D → Y axis | **Forward/Back** | **Left/Right** |
| W/S → Y axis, A/D → X axis | Left/Right | Forward/Back |

**Check your IMC_Default configuration.** The standard Blood & Dust setup maps **W/S → X axis** and **A/D → Y axis**, so:

- `ActionValue_X` → **ForwardVector** → `AddMovementInput` (W/S = forward/back)
- `ActionValue_Y` → **RightVector** → `AddMovementInput` (A/D = strafe left/right)

### Wiring Pattern

```
IA_Move.Triggered → GetControlRotation() → GetForwardVector(InRot) → AddMovementInput(WorldDirection, ScaleValue=ActionValue_X)
                  → GetControlRotation() → GetRightVector(InRot)    → AddMovementInput(WorldDirection, ScaleValue=ActionValue_Y)
```

**KEY**: Connect full `FRotator ReturnValue` → `InRot`. Do NOT split into Yaw/Pitch/Roll sub-pins.

### MCP Calls

1. `add_node` CallFunction "GetControlRotation" → capture `GetRot_ID`
2. `add_node` CallFunction "GetForwardVector" → capture `FwdVec_ID`
3. `connect_nodes` GetRot.ReturnValue → FwdVec.InRot
4. `add_node` CallFunction "GetRightVector" → capture `RightVec_ID`
5. `connect_nodes` GetRot.ReturnValue → RightVec.InRot
6. `add_node` CallFunction "AddMovementInput" → capture `MoveFwd_ID`
7. `connect_nodes` IA_Move.Triggered → MoveFwd.execute
8. `connect_nodes` FwdVec.ReturnValue → MoveFwd.WorldDirection
9. `connect_nodes` IA_Move.ActionValue_X → MoveFwd.ScaleValue
10. `add_node` CallFunction "AddMovementInput" → capture `MoveRight_ID`
11. `connect_nodes` MoveFwd.then → MoveRight.execute
12. `connect_nodes` RightVec.ReturnValue → MoveRight.WorldDirection
13. `connect_nodes` IA_Move.ActionValue_Y → MoveRight.ScaleValue

### Verify
```
compile_blueprint(blueprint_name="/Game/Characters/Robot/BP_RobotCharacter")
```

**If W moves sideways**: X and Y axes are swapped. Swap steps 9 and 13 (which ActionValue goes to which AddMovementInput).

---

## Phase 5: Sprint, Attack, Kick (Extensibility — ~12 calls per action)

These use `GameplayHelperLibrary` runtime functions instead of built-in Blueprint nodes.

### Sprint

```
IA_Sprint.Started → SetCharacterWalkSpeed(Self, 800.0)
IA_Sprint.Completed → SetCharacterWalkSpeed(Self, 500.0)
```

#### MCP Calls

1. `add_enhanced_input_action_event` for IA_Sprint (if not present)
2. `add_node` CallFunction "SetCharacterWalkSpeed", target_class="/Script/GameplayHelpers.GameplayHelperLibrary" → capture `SprintOn_ID`
3. `connect_nodes` IA_Sprint.Started → SprintOn.execute
4. Wire Self → Character pin, set NewSpeed=800.0
5. `add_node` CallFunction "SetCharacterWalkSpeed", target_class="/Script/GameplayHelpers.GameplayHelperLibrary" → capture `SprintOff_ID`
6. `connect_nodes` IA_Sprint.Completed → SprintOff.execute
7. Wire Self → Character pin, set NewSpeed=500.0

### Attack (Long)

```
IA_Attack_Long.Started → PlayAnimationOneShot(Self, AnimSequence, BlendIn=0.15, BlendOut=0.15, PlayRate=1.0)
```

#### MCP Calls

1. `add_enhanced_input_action_event` for IA_Attack_Long (if not present)
2. `add_node` CallFunction "PlayAnimationOneShot", target_class="/Script/GameplayHelpers.GameplayHelperLibrary" → capture `Attack_ID`
3. `connect_nodes` IA_Attack_Long.Started → Attack.execute
4. Wire Self → Character pin, set AnimSequence to kick/attack anim asset path

### Kick

Same pattern as Attack but with different IA and animation asset.

### Template for Adding More Actions

Every new action follows this pattern:
1. `add_enhanced_input_action_event` for the IA_ asset
2. `add_node` CallFunction with the target function
3. `connect_nodes` for exec chain (Started/Triggered/Completed → function.execute)
4. `connect_nodes` for data pins (ActionValue, Self, parameters)
5. `compile_blueprint` to verify

---

## Phase 6: GameMode Setup (~2 calls)

```
set_game_mode_default_pawn(
    game_mode_path="/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode",
    pawn_class_path="/Game/Characters/Robot/BP_RobotCharacter"
)
```

**WARNING**: This double-compiles the GameMode BP. If you did NOT wire IMC in BeginPlay (Phase 1), the PlayerController's IMC setup may break. Phase 1 makes the character self-contained, so this is safe.

Then verify:
```
read_blueprint_content(blueprint_path="/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode")
```
Confirm DefaultPawnClass points to BP_RobotCharacter.

---

## Phase 7: Verification Protocol

### Step 1: Compile
```
compile_blueprint(blueprint_name="/Game/Characters/Robot/BP_RobotCharacter")
```
**MUST return**: `compiled: true`, `num_errors: 0`

### Step 2: Analyze Graph
```
analyze_blueprint_graph(blueprint_path="/Game/Characters/Robot/BP_RobotCharacter")
```

### Step 3: Verify Connection Counts

Check that each critical pin has **exactly 1** input connection:

| Node | Pin | Expected Connections |
|------|-----|---------------------|
| AddIMC_Default | execute | 1 (from BeginPlay.then) |
| AddIMC_MouseLook | execute | 1 (from AddIMC_Default.then) |
| Jump | execute | 1 (from IA_Jump.Started) |
| StopJumping | execute | 1 (from IA_Jump.Completed) |
| AddControllerYawInput (IA_Look) | execute | 1 (from IA_Look.Triggered) |
| AddControllerYawInput (IA_Look) | Val | 1 (from IA_Look.ActionValue_X) |
| AddControllerPitchInput (IA_Look) | execute | 1 (from Yaw.then) |
| AddControllerPitchInput (IA_Look) | Val | 1 (from IA_Look.ActionValue_Y) |
| AddMovementInput (forward) | ScaleValue | 1 (from IA_Move.ActionValue_X) |
| AddMovementInput (right) | ScaleValue | 1 (from IA_Move.ActionValue_Y) |

**If any pin has 0 connections**: Node is disconnected — add the missing connection.
**If any pin has 2+ connections**: Duplicate connection bug — DELETE the node, recreate it, and wire only once.

### Step 4: PIE Test Checklist

Tell the user to test in Play-In-Editor (Alt+P):
- [ ] WASD moves character relative to camera direction
- [ ] Mouse moves camera
- [ ] Space bar jumps
- [ ] Shift sprints (faster movement)
- [ ] LMB plays attack animation
- [ ] Character faces movement direction
- [ ] No "Blueprint Compilation Errors" dialog on PIE start

---

## Troubleshooting

### "No input at all" — character doesn't respond to any key
**Cause**: IMC not added in BeginPlay.
**Fix**: Check Phase 1 wiring. Run `analyze_blueprint_graph` and verify BeginPlay.then connects to AddInputMappingContextToCharacter.

### "W moves right / A moves forward" — axes are swapped
**Cause**: IMC_Default axis mapping doesn't match the wiring.
**Fix**: Open IMC_Default in the editor (or check with `read_blueprint_content`). Swap which ActionValue (X vs Y) connects to ForwardVector vs RightVector AddMovementInput.

### "Camera is inverted" — looking up moves camera down
**Cause**: IMC modifier missing Negate on Y axis.
**Fix**: In IMC_MouseLook, add a "Negate" modifier to the IA_MouseLook action and enable Y axis negation. Do NOT add negation in the Blueprint — keep the BP clean.

### "Duplicate connections" — node has 2+ connections on a pin
**Cause**: `connect_nodes` was called twice on the same pin. MakeLinkTo appends, never replaces.
**Fix**: DELETE the affected node entirely with `delete_node`, recreate it with `add_node`, then wire it once.

### "Sprint doesn't work"
**Cause**: GameplayHelperLibrary not compiled, or IA_Sprint not in IMC_Default.
**Fix**: Verify the GameplayHelpers plugin is built. Check IMC_Default includes IA_Sprint mapping.

### "Attack animation doesn't play"
**Cause**: AnimSequence asset path wrong, or PlayAnimationOneShot function not found.
**Fix**: Verify anim asset exists with `does_asset_exist`. Verify target_class is `/Script/GameplayHelpers.GameplayHelperLibrary`.

---

## Pin Names Reference

| Node | Key Input Pins | Key Output Pins |
|------|---------------|-----------------|
| BeginPlay | (none) | then, execute |
| Jump | execute | then |
| StopJumping | execute | then |
| AddControllerYawInput | execute, Val | then |
| AddControllerPitchInput | execute, Val | then |
| AddMovementInput | execute, WorldDirection, ScaleValue | then |
| GetControlRotation | (pure, no exec) | ReturnValue (FRotator) |
| GetForwardVector | InRot (FRotator) | ReturnValue (FVector) |
| GetRightVector | InRot (FRotator) | ReturnValue (FVector) |
| EnhancedInputAction (Axis2D) | (none) | Triggered, Started, Completed, ActionValue_X, ActionValue_Y |
| EnhancedInputAction (Bool) | (none) | Triggered, Started, Completed, ActionValue |
| AddInputMappingContextToCharacter | execute, Character, MappingContext, Priority | then |
| SetCharacterWalkSpeed | execute, Character, NewSpeed | then |
| PlayAnimationOneShot | execute, Character, AnimSequence, BlendInTime, BlendOutTime, PlayRate | then |

## Common Gotchas

- **NEVER predict node IDs** — always capture from `add_node` response
- **connect_nodes auto-compiles** after every connection (BPConnector.cpp:153) — slow but ensures consistency
- **Enhanced Input node GUIDs often all-zeros** — FindNodeById falls back to GetName() matching
- **Multiple output connections work** — MakeLinkTo appends to link array (this is correct for outputs)
- **Multiple INPUT connections are WRONG** — an input pin should only have 1 source. 2+ = duplicate bug
- **CDO components (Mesh, CapsuleComponent)** not visible in SCS — use `set_character_properties`
- **FRotator pin connection** — Connect full ReturnValue (FRotator) → InRot, NOT individual sub-pins
- **Self pin** — For functions on the character itself (Jump, AddMovementInput), Self is implicit. For GameplayHelperLibrary functions, wire `Self` to the `Character` pin explicitly
