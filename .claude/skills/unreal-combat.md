# Skill: Combat Foundation — Melee Attack + Enemy Health + Death

Complete pipeline for implementing melee combat in Blood & Dust via MCP. Covers input binding, attack animations, hit detection, enemy health, death effects, and verification.

## Trigger Phrases
- "combat system"
- "melee attack"
- "add combat"
- "LMB attack"
- "enemy health"
- "make enemies killable"

## Prerequisites

Before running this skill, verify these exist:

| Asset | Path | Required |
|-------|------|----------|
| Player BP | `/Game/Characters/Robot/BP_RobotCharacter` | YES |
| Player Skeleton | `/Game/Characters/Robot/SK_Robot_Skeleton` | YES |
| Kick Anim | `/Game/Characters/Robot/Animations/long-kick` | YES |
| Hit Anim | `/Game/Characters/Robot/Animations/long-hit` | YES |
| IMC_Default | `/Game/Input/IMC_Default` | YES |
| GameplayHelpers | `/Script/GameplayHelpers.GameplayHelperLibrary` | YES |
| Enemy BPs | `/Game/Characters/Enemies/*/BP_*` | For damage system |

**Run `does_asset_exist` for each before proceeding.** Missing assets = skill will fail silently.

## CRITICAL RULES

1. **ALL MCP calls STRICTLY SEQUENTIAL** — never parallel. Each creates FTSTicker callback; parallel = crash.
2. **connect_nodes APPENDS connections** — MakeLinkTo adds to existing links. Does NOT replace. If you wire wrong, DELETE the node and recreate.
3. **Always capture node IDs** from `add_node` responses. NEVER predict or guess node IDs.
4. **Compile after each major section** — catches errors early before they cascade.
5. **Max ~5 connect_nodes calls before compiling** to catch wiring errors early.
6. **Comparison node CANNOT change operator** — use CallFunction with `Greater_DoubleDouble`, `LessEqual_DoubleDouble` etc. from KismetMathLibrary.
7. **No ForEachLoop node type** — use alternative iteration patterns or process array elements individually.
8. **Delay is a latent CallFunction** — output pin is `Completed`, not `then`.
9. **DynamicCast needs `_C` suffix** for Blueprint classes: `/Game/Path/BP_Name.BP_Name_C`
10. **PlayAnimationOneShot blocks re-entry** — silently ignores new calls while montage is playing.

---

## SCOPE TIERS

Choose which tier to implement based on time and complexity tolerance:

| Tier | What You Get | MCP Calls | Complexity |
|------|-------------|-----------|------------|
| **Tier 1: Visual Combat** | LMB/RMB plays attack anims | ~8 calls | Easy |
| **Tier 2: + Montages** | Pre-built montages for future use | ~12 calls | Easy |
| **Tier 3: + Enemy Health** | Enemies have Health variable | ~6 calls | Easy |
| **Tier 4: + C++ Melee Damage** | Attacks reduce nearby enemy health via ApplyMeleeDamage | ~16 calls | Easy |
| **Tier 5: Full Combat** | Health + damage + ragdoll + knockback + delayed destroy | ~16 calls | Easy |

**Recommended for demo: Tier 1-4** (visual combat + montages + damage). All handled by C++ helper.

---

## Phase 0: Audit Current State (~5 calls)

### Step 1: Read Player BP
```
read_blueprint_content(
    blueprint_path="/Game/Characters/Robot/BP_RobotCharacter",
    include_event_graph=true,
    include_variables=true,
    include_components=false
)
```

Check for existing combat nodes:
- `EnhancedInputAction IA_Attack_Long` — if exists, note node_id
- `EnhancedInputAction IA_Kick` — if exists, note node_id
- `PlayAnimationOneShot` nodes — check if AnimSequence pins are connected (connections > 0)

### Step 2: Check Input Actions
```
does_asset_exist(asset_path="/Game/Input/Actions/IA_Attack_Long")
does_asset_exist(asset_path="/Game/Input/Actions/IA_Kick")
```

### Decision Tree

| State | Action |
|-------|--------|
| IA_Kick exists + PlayAnimOneShot wired with AnimSequence | Skip to Phase 2 (montages) |
| IA_Kick exists + PlayAnimOneShot has empty AnimSequence | Fix pin defaults (Phase 1b) |
| No attack input actions at all | Full Phase 1a setup |

---

## Phase 1a: Create Attack Input Actions (if needed, ~6 calls)

### Step 1: Create Input Actions
```
create_input_action(
    action_name="IA_Kick",
    action_path="/Game/Input/Actions/",
    value_type="Bool"
)
```
```
create_input_action(
    action_name="IA_Attack_Long",
    action_path="/Game/Input/Actions/",
    value_type="Bool"
)
```

### Step 2: Bind to Keys
```
add_input_mapping(
    context_path="/Game/Input/IMC_Default",
    action_path="/Game/Input/Actions/IA_Kick",
    key="LeftMouseButton"
)
```
```
add_input_mapping(
    context_path="/Game/Input/IMC_Default",
    action_path="/Game/Input/Actions/IA_Attack_Long",
    key="RightMouseButton"
)
```

### Step 3: Add Input Event Nodes to Player BP
```
add_enhanced_input_action_event(
    blueprint_name="/Game/Characters/Robot/BP_RobotCharacter",
    input_action_path="/Game/Input/Actions/IA_Kick",
    pos_x=-600, pos_y=2100
)
```
Capture → `IA_Kick_NodeID`

```
add_enhanced_input_action_event(
    blueprint_name="/Game/Characters/Robot/BP_RobotCharacter",
    input_action_path="/Game/Input/Actions/IA_Attack_Long",
    pos_x=-600, pos_y=1800
)
```
Capture → `IA_AttackLong_NodeID`

### Step 4: Add PlayAnimationOneShot Nodes + Wire

For each attack action, add a PlayAnimationOneShot node:

```
add_node(
    blueprint_name="/Game/Characters/Robot/BP_RobotCharacter",
    node_type="CallFunction",
    target_function="PlayAnimationOneShot",
    target_class="/Script/GameplayHelpers.GameplayHelperLibrary",
    pos_x=1600, pos_y=2100
)
```
Capture → `PlayKick_NodeID`

```
add_node(
    blueprint_name="/Game/Characters/Robot/BP_RobotCharacter",
    node_type="CallFunction",
    target_function="PlayAnimationOneShot",
    target_class="/Script/GameplayHelpers.GameplayHelperLibrary",
    pos_x=1600, pos_y=1800
)
```
Capture → `PlayHit_NodeID`

Wire exec chains:
```
connect_nodes(blueprint_name="...", source_node_id="<IA_Kick_NodeID>", source_pin_name="Started", target_node_id="<PlayKick_NodeID>", target_pin_name="execute")
connect_nodes(blueprint_name="...", source_node_id="<IA_AttackLong_NodeID>", source_pin_name="Started", target_node_id="<PlayHit_NodeID>", target_pin_name="execute")
```

Wire Self → Character pin (find existing Self node or create one):
```
connect_nodes(blueprint_name="...", source_node_id="K2Node_Self_0", source_pin_name="self", target_node_id="<PlayKick_NodeID>", target_pin_name="Character")
connect_nodes(blueprint_name="...", source_node_id="K2Node_Self_0", source_pin_name="self", target_node_id="<PlayHit_NodeID>", target_pin_name="Character")
```

**NOTE**: Self output pin supports multiple connections (output pins can fan out). This is correct.

### Step 5: Proceed to Phase 1b to set animation assets.

---

## Phase 1b: Set Animation Assets + Play Rate Differentiation (~8 calls)

**IMPORTANT**: Meshy AI's `long-kick` and `long-hit` animations look very similar (both are generic arm swings). To make them FEEL distinct, use different **play rates** — fast snappy kick (LMB) vs slow heavy strike (RMB).

### Step 1: Set AnimSequence pin defaults

```
set_node_property(
    blueprint_name="/Game/Characters/Robot/BP_RobotCharacter",
    node_id="<PlayKick_NodeID>",
    action="set_pin_default",
    pin_name="AnimSequence",
    default_value="/Game/Characters/Robot/Animations/long-kick.long-kick"
)
```

```
set_node_property(
    blueprint_name="/Game/Characters/Robot/BP_RobotCharacter",
    node_id="<PlayHit_NodeID>",
    action="set_pin_default",
    pin_name="AnimSequence",
    default_value="/Game/Characters/Robot/Animations/long-hit.long-hit"
)
```

### Step 2: Differentiate via PlayRate (CRITICAL for feel)

**Kick (LMB)** — fast, snappy jab. PlayRate=1.5 makes it punchy:
```
set_node_property(
    blueprint_name="/Game/Characters/Robot/BP_RobotCharacter",
    node_id="<PlayKick_NodeID>",
    action="set_pin_default",
    pin_name="PlayRate",
    default_value="1.5"
)
```

**Hit (RMB)** — slow, heavy power strike. PlayRate=0.7 adds weight:
```
set_node_property(
    blueprint_name="/Game/Characters/Robot/BP_RobotCharacter",
    node_id="<PlayHit_NodeID>",
    action="set_pin_default",
    pin_name="PlayRate",
    default_value="0.7"
)
```

### Step 3: Stop movement during attacks
```
set_node_property(
    blueprint_name="/Game/Characters/Robot/BP_RobotCharacter",
    node_id="<PlayKick_NodeID>",
    action="set_pin_default",
    pin_name="bStopMovement",
    default_value="true"
)
```
```
set_node_property(
    blueprint_name="/Game/Characters/Robot/BP_RobotCharacter",
    node_id="<PlayHit_NodeID>",
    action="set_pin_default",
    pin_name="bStopMovement",
    default_value="true"
)
```

### Step 4: Use shorter blends for snappier feel
```
set_node_property(
    blueprint_name="/Game/Characters/Robot/BP_RobotCharacter",
    node_id="<PlayKick_NodeID>",
    action="set_pin_default",
    pin_name="BlendIn",
    default_value="0.1"
)
```
```
set_node_property(
    blueprint_name="/Game/Characters/Robot/BP_RobotCharacter",
    node_id="<PlayHit_NodeID>",
    action="set_pin_default",
    pin_name="BlendIn",
    default_value="0.15"
)
```

### Compile and Verify
```
compile_blueprint(blueprint_name="/Game/Characters/Robot/BP_RobotCharacter")
```
**MUST return**: `compiled: true`, `num_errors: 0`

### Checkpoint
> "Phase 1 complete. LMB plays kick animation, RMB plays hit animation. Press **Play (Alt+P)** to test. Press **Ctrl+S** to save."

---

## Phase 2: Create AnimMontages (~4 calls)

Pre-create montages from the attack animations. These are useful for:
- Future montage-based combat (with notifies, sections)
- Testing via `play_montage_on_actor` in PIE
- Blueprint `PlayAnimMontage` function (more control than PlayAnimationOneShot)

### Step 1: Create Kick Montage
```
create_anim_montage(
    animation_path="/Game/Characters/Robot/Animations/long-kick",
    montage_name="AM_Robot_Kick",
    destination_path="/Game/Characters/Robot/Animations/",
    slot_name="DefaultGroup.DefaultSlot"
)
```

### Step 2: Create Hit Montage
```
create_anim_montage(
    animation_path="/Game/Characters/Robot/Animations/long-hit",
    montage_name="AM_Robot_Hit",
    destination_path="/Game/Characters/Robot/Animations/",
    slot_name="DefaultGroup.DefaultSlot"
)
```

### Step 3: Check for Getting-Hit Animation (for enemies)
```
does_asset_exist(asset_path="/Game/Characters/Robot/Animations/getting-hit")
```
If exists:
```
create_anim_montage(
    animation_path="/Game/Characters/Robot/Animations/getting-hit",
    montage_name="AM_Robot_GettingHit",
    destination_path="/Game/Characters/Robot/Animations/"
)
```

### Step 4: Verify Montages Created
```
does_asset_exist(asset_path="/Game/Characters/Robot/Animations/AM_Robot_Kick")
does_asset_exist(asset_path="/Game/Characters/Robot/Animations/AM_Robot_Hit")
```

### Optional: Test in PIE
```
play_montage_on_actor(
    actor_name="BP_RobotCharacter_C_0",
    montage_path="/Game/Characters/Robot/Animations/AM_Robot_Kick",
    play_rate=1.0
)
```
**NOTE**: Only works during Play-In-Editor (Alt+P). Returns error otherwise.

### Checkpoint
> "Phase 2 complete. AnimMontages created for kick, hit, and getting-hit. These are ready for advanced combat wiring. Press **Ctrl+S** to save."

---

## Phase 3: Enemy Health Variables (~6 calls)

Add Health variable to each enemy Blueprint. The C++ `ApplyMeleeDamage` function reads this via reflection.

### Step 1: Add Health to Each Enemy

For **each** enemy BP:
```
create_variable(blueprint_name="/Game/Characters/Enemies/KingBot/BP_KingBot", variable_name="Health", variable_type="float", default_value=100.0, is_public=true, category="Combat")
create_variable(blueprint_name="/Game/Characters/Enemies/Giganto/BP_Giganto", variable_name="Health", variable_type="float", default_value=150.0, is_public=true, category="Combat")
create_variable(blueprint_name="/Game/Characters/Enemies/Bell/BP_Bell", variable_name="Health", variable_type="float", default_value=75.0, is_public=true, category="Combat")
```

### Step 2: Compile Each
```
compile_blueprint(blueprint_name="/Game/Characters/Enemies/KingBot/BP_KingBot")
compile_blueprint(blueprint_name="/Game/Characters/Enemies/Giganto/BP_Giganto")
compile_blueprint(blueprint_name="/Game/Characters/Enemies/Bell/BP_Bell")
```

> "Phase 3 complete. Enemies have Health variables. The C++ ApplyMeleeDamage function will read these via reflection. Press **Ctrl+S** to save."

---

## Phase 4: Wire C++ ApplyMeleeDamage (~10 calls)

The `ApplyMeleeDamage` function in GameplayHelperLibrary handles ALL combat logic in C++:
- Sphere overlap to find enemies in range
- Reads "Health" Blueprint variable via reflection
- Subtracts damage, checks death
- Ragdoll + knockback + delayed destroy on death

Player BP just needs to call this function after each attack animation.

### Prerequisites
- GameplayHelpers plugin rebuilt with ApplyMeleeDamage function
- Editor restarted to load new C++ code

### Step 1: Read Current Player BP State
```
read_blueprint_content(blueprint_path="/Game/Characters/Robot/BP_RobotCharacter", include_event_graph=true)
```
Identify the existing PlayAnimOneShot node IDs for kick and hit.

### Step 2: Add ApplyMeleeDamage for Kick
```
add_node(
    blueprint_name="/Game/Characters/Robot/BP_RobotCharacter",
    node_type="CallFunction",
    target_function="ApplyMeleeDamage",
    target_class="/Script/GameplayHelpers.GameplayHelperLibrary",
    pos_x=2200, pos_y=2100
)
```
Capture → `DmgKick_ID`

Wire and configure:
```
connect_nodes(source_node_id="<PlayKick_NodeID>", source_pin_name="then", target_node_id="<DmgKick_ID>", target_pin_name="execute")
set_node_property(node_id="<DmgKick_ID>", action="set_pin_default", pin_name="Damage", default_value="15.0")
set_node_property(node_id="<DmgKick_ID>", action="set_pin_default", pin_name="Radius", default_value="200.0")
```

### Step 3: Add ApplyMeleeDamage for Heavy Hit
```
add_node(
    blueprint_name="/Game/Characters/Robot/BP_RobotCharacter",
    node_type="CallFunction",
    target_function="ApplyMeleeDamage",
    target_class="/Script/GameplayHelpers.GameplayHelperLibrary",
    pos_x=2200, pos_y=1800
)
```
Capture → `DmgHit_ID`

Wire and configure:
```
connect_nodes(source_node_id="<PlayHit_NodeID>", source_pin_name="then", target_node_id="<DmgHit_ID>", target_pin_name="execute")
set_node_property(node_id="<DmgHit_ID>", action="set_pin_default", pin_name="Damage", default_value="35.0")
set_node_property(node_id="<DmgHit_ID>", action="set_pin_default", pin_name="Radius", default_value="250.0")
set_node_property(node_id="<DmgHit_ID>", action="set_pin_default", pin_name="KnockbackImpulse", default_value="75000.0")
```

### Step 4: Compile
```
compile_blueprint(blueprint_name="/Game/Characters/Robot/BP_RobotCharacter")
```
Must return 0 errors.

> "Phase 4 complete. Attacks now deal damage. Kick: 15 dmg / 200 radius. Heavy hit: 35 dmg / 250 radius / 75k knockback. Press **Ctrl+S** to save."

### ApplyMeleeDamage Pin Reference
| Pin | Direction | Type | Default |
|-----|-----------|------|---------|
| `execute` | Input | exec | |
| `then` | Output | exec | |
| `Attacker` | Input | ACharacter* | DefaultToSelf |
| `Damage` | Input | float | 15.0 |
| `Radius` | Input | float | 200.0 |
| `KnockbackImpulse` | Input | float | 50000.0 |

### What ApplyMeleeDamage Does Internally
1. Sphere overlap (ECC_Pawn) around attacker position
2. For each ACharacter found (excluding attacker):
   - FindPropertyByName("Health") → FFloatProperty or FDoubleProperty
   - Subtract damage, write back
   - If Health <= 0: ragdoll mesh, disable capsule, knockback impulse, 1.5s delayed destroy

---

## Phase 5: Verification

All death effects (ragdoll, knockback, delayed destroy) are handled by ApplyMeleeDamage in C++. No additional Blueprint wiring needed.

### Final Verification
1. `compile_blueprint` on all 4 BPs → 0 errors each
2. `take_screenshot` → visual check
3. Test in PIE (Alt+P):
   - Walk to KingBot, LMB kick × 7 → should ragdoll and vanish (7 × 15 = 105 > 100)
   - Walk to Bell, RMB heavy × 3 → should die (3 × 35 = 105 > 75)
   - Giganto takes most hits (150 HP, needs 10 kicks or 5 heavies)
4. Press Ctrl+S to save

---

## MCP Tool Quick Reference for Combat

### Animation Tools
| Tool | Purpose | Editor/PIE |
|------|---------|------------|
| `create_anim_montage` | Create montage from AnimSequence | Editor |
| `play_montage_on_actor` | Play montage on character | PIE only |

### Effect Tools
| Tool | Purpose | Editor/PIE |
|------|---------|------------|
| `apply_impulse` | Physics knockback/ragdoll | Both (visual in PIE) |
| `trigger_post_process_effect` | Screen flash/slow-mo | Both (slow-mo PIE only) |
| `spawn_niagara_system` | Particle VFX | Editor |

### Blueprint Wiring Tools
| Tool | Purpose |
|------|---------|
| `add_node` (CallFunction) | Add function calls |
| `add_node` (Branch) | Conditional logic |
| `add_node` (VariableGet/Set) | Health read/write |
| `connect_nodes` | Wire pins together |
| `set_node_property` (set_pin_default) | Set damage values, asset refs |
| `create_variable` | Add Health, IsDead vars |
| `add_event_node` (ReceiveAnyDamage) | Damage event handler |
| `compile_blueprint` | Verify after wiring |

---

## Pin Names Reference (Combat-Specific)

### PlayAnimationOneShot (GameplayHelperLibrary)
| Pin | Direction | Type | Notes |
|-----|-----------|------|-------|
| `execute` | Input | exec | |
| `then` | Output | exec | |
| `Character` | Input | object (ACharacter*) | DefaultToSelf |
| `AnimSequence` | Input | object (UAnimSequence*) | Set via set_pin_default |
| `PlayRate` | Input | float | Default 1.0 |
| `BlendIn` | Input | float | Default 0.25 |
| `BlendOut` | Input | float | Default 0.25 |
| `bStopMovement` | Input | bool | Default false |

### ReceiveAnyDamage (Event)
| Pin | Direction | Type |
|-----|-----------|------|
| `then` | Output | exec |
| `Damage` | Output | float |
| `DamageType` | Output | object |
| `InstigatedBy` | Output | object (Controller) |
| `DamageCauser` | Output | object (Actor) |

### Branch
| Pin | Direction | Type |
|-----|-----------|------|
| `execute` | Input | exec |
| `Condition` | Input | bool |
| `Then` | Output | exec (true path) |
| `Else` | Output | exec (false path) |

### K2_DestroyActor
| Pin | Direction | Type |
|-----|-----------|------|
| `execute` | Input | exec |
| `then` | Output | exec |
| `self` | Input | object (auto-filled) |

### SphereOverlapActors (KismetSystemLibrary)
| Pin | Direction | Type |
|-----|-----------|------|
| `execute` | Input | exec |
| `then` | Output | exec |
| `WorldContext` | Input | object |
| `SpherePos` | Input | vector |
| `SphereRadius` | Input | float |
| `ObjectTypes` | Input | array |
| `ActorClassFilter` | Input | class |
| `ActorsToIgnore` | Input | array |
| `OutActors` | Output | array |
| `ReturnValue` | Output | bool |

### Subtract_DoubleDouble (KismetMathLibrary)
| Pin | Direction | Type |
|-----|-----------|------|
| `A` | Input | double |
| `B` | Input | double |
| `ReturnValue` | Output | double |

### LessEqual_DoubleDouble (KismetMathLibrary)
| Pin | Direction | Type |
|-----|-----------|------|
| `A` | Input | double |
| `B` | Input | double |
| `ReturnValue` | Output | bool |

### Delay (KismetSystemLibrary, LATENT)
| Pin | Direction | Type | Notes |
|-----|-----------|------|-------|
| `execute` | Input | exec | |
| `Duration` | Input | float | Seconds |
| `Completed` | Output | exec | **NOT "then"** |

---

## Combat Function Reference (for CallFunction nodes)

| Function | target_class | Pure? | Use Case |
|----------|-------------|-------|----------|
| `PlayAnimationOneShot` | `/Script/GameplayHelpers.GameplayHelperLibrary` | No | Play attack anim |
| `PlayAnimMontage` | (auto - ACharacter) | No | Play montage |
| `K2_DestroyActor` | (auto - AActor) | No | Kill enemy |
| `GetActorLocation` | (auto - AActor) | Yes | Get position |
| `GetDistanceTo` | (auto - AActor) | Yes | Range check |
| `SphereOverlapActors` | `/Script/Engine.KismetSystemLibrary` | No | Area detection |
| `ApplyDamage` | `/Script/Engine.GameplayStatics` | No | UE damage system |
| `Subtract_DoubleDouble` | `/Script/Engine.KismetMathLibrary` | Yes | Health - Damage |
| `LessEqual_DoubleDouble` | `/Script/Engine.KismetMathLibrary` | Yes | Health <= 0 |
| `Greater_DoubleDouble` | `/Script/Engine.KismetMathLibrary` | Yes | Health > 0 |
| `LaunchCharacter` | (auto - ACharacter) | No | Death knockback |
| `Delay` | `/Script/Engine.KismetSystemLibrary` | No (latent) | Timed destroy |
| `SetCharacterWalkSpeed` | `/Script/GameplayHelpers.GameplayHelperLibrary` | No | Slow during attack |
| `PlaySound2D` | `/Script/Engine.GameplayStatics` | No | Hit sound |
| `PlaySoundAtLocation` | `/Script/Engine.GameplayStatics` | No | Impact sound |

---

## Troubleshooting

### "Both attacks look the same / like regular hits"
**Cause**: Meshy AI animations `long-kick` and `long-hit` are visually similar (generic arm swings).
**Fix**: Differentiate via PlayRate — set kick to 1.5 (fast jab) and hit to 0.7 (slow power strike). Also reduce BlendIn to 0.1 for snappier transitions. If still too similar, consider importing new animations from `Meshy_AI_Meshy_Merged_Animations.fbx` (may contain additional clips not yet extracted) or generate new distinct animations from Meshy AI.

### "Attack animation doesn't play"
**Cause**: AnimSequence pin not set on PlayAnimationOneShot, or another montage already playing.
**Fix**: Check with `analyze_blueprint_graph` that AnimSequence pin has a connection or default value. PlayAnimationOneShot silently ignores calls while any montage is active (even from a previous attack).

### "Attack plays but no damage"
**Cause**: SphereOverlapActors not wired, or enemy doesn't have ReceiveAnyDamage event.
**Fix**: Verify player BP has overlap → ApplyDamage wired after attack anim. Verify each enemy BP has ReceiveAnyDamage → Health subtraction → Branch → Destroy.

### "Enemy doesn't die"
**Cause**: Health not being reduced, or LessEqual comparison wired wrong.
**Fix**: Add a Print node after health subtraction to debug the value. Check that Subtract output feeds both VariableSet AND the comparison check.

### "Duplicate connections on pins"
**Cause**: connect_nodes called twice on the same target pin.
**Fix**: DELETE the affected node (`delete_node`), recreate it (`add_node`), and wire it ONCE. connect_nodes APPENDS — it never replaces.

### "compile_blueprint returns errors after wiring"
**Cause**: Pin type mismatch, broken connection, or wrong pin name.
**Fix**: Run `analyze_blueprint_graph` to see all connections. Look for pins with 0 connections that should have 1. Check pin types match (float→float, exec→exec, object→object).

### "PlayAnimMontage not found"
**Cause**: Function name is case-sensitive.
**Fix**: The exact UE5 function name is `PlayAnimMontage` (on ACharacter). No target_class needed — auto-found via parent class hierarchy.

### "bStopMovement doesn't work"
**Cause**: PlayAnimationOneShot's bStopMovement implementation saves/restores MaxWalkSpeed with a timer. If the character BP overrides MaxWalkSpeed elsewhere (e.g., sprint), it may conflict.
**Fix**: Ensure sprint logic doesn't fire during attack animations. Or use a separate IsAttacking bool variable to gate sprint.

---

## What CANNOT Be Done via MCP (Manual Editor Work)

| Feature | Why | Workaround |
|---------|-----|-----------|
| AnimMontage notify events | create_anim_montage creates single-section montage, no notifies | Add notifies in AnimMontage editor |
| ForEachLoop iteration | No ForEachLoop node type in add_node | Use single-target damage or manual Blueprint wiring |
| Combo attack chains | Requires montage sections + state machine | Wire in AnimBP editor |
| Lock-on targeting | Requires camera + input system changes | Manual Blueprint setup |
| Hitbox collision (bone-attached) | Requires socket-attached collision component + anim notify enable/disable | Manual Blueprint setup |
| Health bar HUD | Widget Blueprint creation via MCP is limited | Use create_widget_blueprint + manual layout |
| AI combat response | BT task properties are protected in UE5.7 | Use BT editor after MCP creates structure |

---

## Verification Protocol

### After Each Phase

1. **Compile**: `compile_blueprint` → must return `compiled: true`, `num_errors: 0`
2. **Analyze**: `analyze_blueprint_graph` → verify connection counts on critical pins
3. **Screenshot**: `take_screenshot` → visual verification

### Final Verification Checklist

Tell the user to test in Play-In-Editor (Alt+P):
- [ ] LMB plays kick animation
- [ ] RMB plays hit/punch animation
- [ ] Attack animation plays to completion (no interruption)
- [ ] Character stops briefly during attack (if bStopMovement=true)
- [ ] Enemies have Health variable visible in Details panel
- [ ] Attacking near enemy reduces their health (if Tier 4+)
- [ ] Enemy at 0 health is destroyed (if Tier 4+)
- [ ] No "Blueprint Compilation Errors" dialog on PIE start

### Connection Count Verification

| Node | Pin | Expected |
|------|-----|----------|
| IA_Kick EnhancedInput | Started (output) | 1 connection |
| IA_Attack_Long EnhancedInput | Started (output) | 1 connection |
| PlayAnimOneShot (kick) | execute (input) | 1 connection |
| PlayAnimOneShot (kick) | Character (input) | 1 connection |
| PlayAnimOneShot (kick) | AnimSequence (input) | 0 connections (uses default value) |
| PlayAnimOneShot (hit) | execute (input) | 1 connection |
| PlayAnimOneShot (hit) | Character (input) | 1 connection |
| PlayAnimOneShot (hit) | AnimSequence (input) | 0 connections (uses default value) |
| ReceiveAnyDamage (enemy) | then (output) | 1 connection |
| Branch (enemy) | Condition (input) | 1 connection |
| Branch (enemy) | Then (output) | 1 connection |
| K2_DestroyActor (enemy) | execute (input) | 1 connection |
