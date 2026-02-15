# Skill: Enemy AI — Chase, Attack, Leash, Personality, Death

Complete pipeline for adding enemy AI behavior via a single C++ tick function. Enemies chase the player with personality-driven behavior (Normal/Berserker/Stalker/Brute/Crawler), attack with multiple animations, react to hits, play death sequences, and return to spawn when the player escapes.

## Trigger Phrases
- "enemy AI"
- "enemies chase"
- "make enemies attack"
- "enemy behavior"
- "AI movement"
- "leash distance"
- "enemy personality"
- "hit reactions"

## Prerequisites

| Asset | Path | Required |
|-------|------|----------|
| Enemy BPs | `/Game/Characters/Enemies/*/BP_*` | YES |
| Health variables | `Health` float on each enemy BP | YES (created by `/unreal-combat`) |
| GameplayHelpers plugin | `UpdateEnemyAI` function | YES (must be built + editor restarted) |
| Player BP | `/Game/Characters/Robot/BP_RobotCharacter` | YES |
| Enemy AnimBPs | `/Game/Characters/Enemies/*/ABP_BG_*` | YES (created in Phase 1) |
| Enemy animations | See "Available Animations" section | YES |

## CRITICAL RULES

1. **ALL MCP calls STRICTLY SEQUENTIAL** — never parallel.
2. **Build C++ FIRST, restart editor, THEN wire BPs.**
3. **AnimBP + Slot(DefaultSlot) + montages** is the ONLY correct animation pattern.
4. **NEVER use OverrideAnimationData or SingleNode mode for runtime AI** — resets CurrentTime to 0, destroys AnimBP, no crossfade.
5. **ALWAYS auto-fit capsule BEFORE wiring AI** — enemies float if capsule doesn't match mesh.
6. **After signature change, DELETE old UpdateEnemyAI nodes and recreate fresh** — old pins will be orphaned.
7. **`set_node_property action=set_pin_default` for anim pins**: `default_value="/Game/Path/Anim.Anim"` (with `.Anim` suffix)
8. **NEVER use SingleNode except for death freeze-frame** — `MeshComp->PlayAnimation(DeathAnim, false)` is the ONLY valid use.
9. **`setup_locomotion_state_machine` creates AnimBP** — no need for manual state machine wiring.

## Animation Architecture Rules (CRITICAL)

### Core Pattern: AnimBP + Slot + Montages

- **AnimBP** drives locomotion (Idle ↔ Walk state machine based on velocity)
- **Slot(DefaultSlot)** in the AnimBP allows montages to overlay on top of locomotion
- **Montages** for one-shot actions: attacks, hit reactions, screams
- **Death** is the ONLY exception — uses `MeshComp->PlayAnimation(Anim, false)` to freeze on last frame

### PlayAnimationOneShot: bForceInterrupt Parameter

```cpp
PlayAnimationOneShot(Character, Animation, PlayRate, BlendIn, BlendOut, bForceInterrupt)
```

| Parameter | Value | Use Case |
|-----------|-------|----------|
| `bForceInterrupt` | `false` | **Attacks** — prevents rapid restart, skips if montage already playing |
| `bForceInterrupt` | `true` | **Hit-react, scream** — instantly stops current montage first |

### Why NOT SingleNode for Runtime AI

`MeshComp->OverrideAnimationData()` or `AnimationMode = AnimationSingleNode`:
- Resets `CurrentTime = 0` on every call
- Destroys AnimBP (no locomotion blending)
- No crossfade between animations
- Breaks montage system

**ONLY valid SingleNode use**: Death (freeze on last frame).

### Personality System Does NOT Affect Locomotion Anims

Enemy personalities (Normal/Berserker/Stalker/Brute/Crawler) affect:
- Movement speed multipliers
- Aggro range multipliers
- Attack animation selection
- Attack cooldown multipliers
- Damage multipliers

Personalities do NOT change idle/walk animations — those are fixed in the AnimBP state machine.

---

## Available Animations

### KingBot — 20 animations
Location: `/Game/Characters/Enemies/KingBot/Animations/`
Skeleton: `/Game/Characters/Enemies/KingBot/SK_KingBot_Skeleton`

**CRITICAL: NEVER use zombie animations (KingBot_ZombieAttack, KingBot_ZombieDying) for KingBot. Use proper humanoid combat animations only.**

| Animation | Pin Assignment | Notes |
|-----------|---------------|-------|
| `KingBot_Idle` | AnimBP Idle state | Locomotion |
| `KingBot_Walking_v2` | AnimBP Walk state | Regular walk (NOT KingBot_Walk which is zombie walk) |
| `KingBot_Run` | AnimBP Run state + RunAnim pin | 3-state locomotion |
| `KingBot_Punching` | **AttackAnim** | Primary attack |
| `KingBot_Biting` | **AttackAnim2** | Secondary attack |
| `KingBot_NeckBite` | **AttackAnim3** | Tertiary attack |
| `KingBot_Scream` | **HitReactAnim** + **ScreamAnim** | Flinch/pain reaction + idle scream |
| `KingBot_Death` | **DeathAnim** | Primary death |
| `KingBot_Dying` | **DeathAnim2** | Alternate death |
| `KingBot_Crawl` | **CrawlAnim** | Crawler personality locomotion |
| `KingBot_ZombieAttack` | NEVER USE | Zombie animation — wrong style |
| `KingBot_ZombieDying` | NEVER USE | Zombie animation — wrong style |
| `KingBot_Walk` | NEVER USE | Zombie walk — use Walking_v2 instead |
| `KingBot_RunCrawl` | (Unused) | |
| `KingBot_TurnLeft/Right` | (Unused) | |

### Bell — 27 animations
Location: `/Game/Characters/Enemies/Bell/Animations/`
Skeleton: `/Game/Characters/Enemies/Bell/SK_Bell_Skeleton` (RESTORED from git — NOT SK_Bell_New_Skeleton)
Skeletal Mesh: `/Game/Characters/Enemies/Bell/SK_Bell` (NOT SK_Bell_New)

**CRITICAL: Bell animations in `Animations/` folder target SK_Bell_Skeleton. If this skeleton is deleted, ALL Bell animations break. SK_Bell_New_Skeleton is a DIFFERENT skeleton — animations are NOT compatible.**

| Animation | Pin Assignment | Notes |
|-----------|---------------|-------|
| `Bell_Idle` | AnimBP Idle state | Locomotion |
| `Bell_Walking` | AnimBP Walk state | Locomotion |
| `Bell_Running` | AnimBP Run state + RunAnim pin | 3-state locomotion |
| `Bell_ZombieAttack` | NEVER USE | Root motion breaks geometry, causes floating |
| `Bell_BodyBlock` | **AttackAnim** + **ScreamAnim** | Primary attack + idle scream (grounded anim) |
| `Bell_RifleHitBack` | **HitReactAnim** | Hit reaction (stumble back) |
| `Bell_FallingToRoll` | **DeathAnim** | Death animation |
| `Bell_BodyBlock` | **ScreamAnim** | Idle scream/block behavior |
| `Bell_Idle2-5` | (Unused) | Idle variants |
| `Bell_SneakLeft/Right` | (Unused) | |
| `Bell_LeftTurn/RightTurn` | (Unused) | |

### Giganto — 7 animations
Location: `/Game/Characters/Enemies/Giganto/`
Skeleton: `/Game/Characters/Enemies/Giganto/SK_Giganto_Skeleton`

| Animation | Pin Assignment | Notes |
|-----------|---------------|-------|
| `Anim_Idle` | AnimBP Idle state | Locomotion |
| `Anim_Walking` | AnimBP Walk state | Locomotion |
| `Anim_Running` | AnimBP Run state + RunAnim pin | 3-state locomotion |
| `Anim_BodyBlock` | **AttackAnim** + **HitReactAnim** | Attack and hit react (limited anim set) |
| `Anim_ZombieStandUp` | **DeathAnim** + **ScreamAnim** | Death and idle scream |

---

## Personality System (5 Archetypes)

Assigned randomly on first tick (30% Normal, 15% Berserker, 20% Stalker, 15% Brute, 20% Crawler).

| Personality | Speed Mult | Aggro Mult | Attack Cooldown Mult | Damage Mult | Behavior |
|-------------|-----------|-----------|---------------------|-------------|----------|
| **Normal** | 1.0 | 1.0 | 1.0 | 1.0 | Balanced |
| **Berserker** | 1.6 | 0.5 | 0.4 | 0.7 | Fast, close aggro, rapid attacks, low damage |
| **Stalker** | 0.55 | 2.0 | 1.5 | 1.0 | Slow, long aggro, stares, weaves |
| **Brute** | 0.8 | 1.0 | 1.8 | 1.8 | Charges straight, slow attacks, heavy damage |
| **Crawler** | 0.4 | 1.4 | 0.9 | 1.2 | Very slow, quick start, slow anim |

Attack animation selection varies by personality:
- **Normal/Brute**: cycles AttackAnim → AttackAnim2 → AttackAnim3
- **Berserker**: always AttackAnim (fast spam)
- **Stalker**: always AttackAnim2 (precise)
- **Crawler**: always AttackAnim3 (clumsy)

---

## State Machine (6 States)

```
┌─────────┐
│  IDLE   │ Random behaviors: Stand, LookAround, Wander, Scream
└────┬────┘
     │ player enters AggroRange
     ▼
┌─────────┐
│  CHASE  │ Move toward player with lateral wobble, reaction delay
└────┬────┘
     │ player within AttackRange
     ▼
┌─────────┐
│ ATTACK  │ Face player, cooldown-based attacks with personality-selected anim
└────┬────┘
     │ enemy beyond LeashDistance from spawn
     ▼
┌─────────┐
│ RETURN  │ Walk back to spawn, ignore player
└────┬────┘
     │ reached spawn point
     ▼
   (IDLE)

HIT-REACT: Stunned for animation duration, face player, return to pre-hit state
DEAD: Play death anim (SingleNode freeze), destroy after anim finishes
```

### Hit Reaction Trigger

When enemy takes damage via `ReceiveAnyDamage` event:
- Play HitReactAnim with `bForceInterrupt=true` (instantly stops current montage)
- Store current state (Chase/Attack/Idle)
- Enter HitReact state (duration = anim length)
- After anim finishes, return to stored state

### Death Trigger

When `Health <= 0`:
- Call `Montage_Stop(0.0f)` to stop all montages
- Call `MeshComp->PlayAnimation(DeathAnim, false)` — SingleNode mode, freezes on last frame
- Destroy actor after animation finishes

---

## Phase 0: Capsule Auto-Fit (MANDATORY — before any other phase)

### Why This Is Mandatory

`create_character_blueprint` sets a DEFAULT capsule size (e.g., 860 half-height). This is almost never correct for imported meshes. Without auto-fitting, enemies will FLOAT above the terrain because the capsule is too tall, lifting the actor origin high above ground.

### How auto_fit_capsule Works

The `set_character_properties(auto_fit_capsule=True)` tool:
1. Reads `GetImportedBounds()` from the skeletal mesh (NOT `GetBounds()` which returns inflated worst-case skeleton bounds)
2. Calculates capsule dimensions: HalfHeight = mesh_height/2 * 1.05, Radius = min(width/2, HalfHeight) * 1.05
3. **Accounts for mesh origin position**: mesh_offset_z = -HalfHeight - mesh_min_z (works for both feet-at-origin and center-at-origin meshes)
4. Compiles the Blueprint and marks dirty

### MCP Calls (3 calls — one per enemy type)

```
set_character_properties(
    blueprint_path="/Game/Characters/Enemies/Bell/BP_Bell",
    auto_fit_capsule=true
)

set_character_properties(
    blueprint_path="/Game/Characters/Enemies/KingBot/BP_KingBot",
    auto_fit_capsule=true
)

set_character_properties(
    blueprint_path="/Game/Characters/Enemies/Giganto/BP_Giganto",
    auto_fit_capsule=true
)
```

### After Auto-Fit: Snap All Placed Instances to Ground

```
For each enemy actor in the level:
    snap_actor_to_ground(actor_name="<enemy_actor_name>")
```

Use `find_actors_by_name(pattern="Bell")`, `find_actors_by_name(pattern="KingBot")`, `find_actors_by_name(pattern="Giganto")` to get all placed instances.

### Verify with Screenshot

```
take_screenshot()
```

Capsule wireframes should fully envelop the character meshes. Characters should be standing ON the ground, not floating above it.

### CRITICAL: GetImportedBounds vs GetBounds

| API | Returns | Use For |
|-----|---------|---------|
| `GetBounds()` | Worst-case skeleton bounds (all bones extended) — INFLATED | NEVER use for capsule sizing |
| `GetImportedBounds()` | Actual imported mesh geometry bounds — ACCURATE | Always use for capsule sizing |

The `auto_fit_capsule` tool uses `GetImportedBounds()` internally. If you ever need manual capsule sizing, the formula is:
- CapsuleHalfHeight = (ImportedBounds.BoxExtent.Z * 2) / 2 * 1.05
- CapsuleRadius = min(max(ImportedBounds.BoxExtent.X, ImportedBounds.BoxExtent.Y), CapsuleHalfHeight) * 1.05
- MeshZ = -CapsuleHalfHeight - (ImportedBounds.Origin.Z - ImportedBounds.BoxExtent.Z)

---

## Phase 1: AnimBP Setup (uses setup_locomotion_state_machine)

For each enemy, create an AnimBP with 3-state locomotion (Idle/Walk/Run) + Slot(DefaultSlot) for montages.

**The tool is IDEMPOTENT** — it cleans old AnimGraph + EventGraph nodes before creating new ones. Safe to re-run after editor restart.

### KingBot
```
setup_locomotion_state_machine(
    anim_blueprint_path="/Game/Characters/Enemies/KingBot/ABP_BG_KingBot",
    idle_animation="/Game/Characters/Enemies/KingBot/Animations/KingBot_Idle",
    walk_animation="/Game/Characters/Enemies/KingBot/Animations/KingBot_Walking_v2",
    run_animation="/Game/Characters/Enemies/KingBot/Animations/KingBot_Run",
    walk_speed_threshold=5.0,
    run_speed_threshold=300.0,
    crossfade_duration=0.2
)
```

### Bell
```
setup_locomotion_state_machine(
    anim_blueprint_path="/Game/Characters/Enemies/Bell/ABP_BG_Bell",
    idle_animation="/Game/Characters/Enemies/Bell/Animations/Bell_Idle",
    walk_animation="/Game/Characters/Enemies/Bell/Animations/Bell_Walking",
    run_animation="/Game/Characters/Enemies/Bell/Animations/Bell_Running",
    walk_speed_threshold=5.0,
    run_speed_threshold=300.0,
    crossfade_duration=0.2
)
```

**CRITICAL: Bell ABP MUST target SK_Bell_Skeleton (NOT SK_Bell_New_Skeleton). If ABP needs recreation:**
```
delete_asset(asset_path="/Game/Characters/Enemies/Bell/ABP_BG_Bell", force_delete=true)
create_anim_blueprint(
    blueprint_name="ABP_BG_Bell",
    skeleton_path="/Game/Characters/Enemies/Bell/SK_Bell_Skeleton",
    blueprint_path="/Game/Characters/Enemies/Bell/"
)
```

### Giganto
```
setup_locomotion_state_machine(
    anim_blueprint_path="/Game/Characters/Enemies/Giganto/ABP_BG_Giganto",
    idle_animation="/Game/Characters/Enemies/Giganto/Anim_Idle",
    walk_animation="/Game/Characters/Enemies/Giganto/Anim_Walking",
    run_animation="/Game/Characters/Enemies/Giganto/Anim_Running",
    walk_speed_threshold=5.0,
    run_speed_threshold=300.0,
    crossfade_duration=0.2
)
```

This creates per AnimBP:
- **EventGraph**: TryGetPawnOwner → GetVelocity → VSize → SetSpeed (float variable)
- **AnimGraph**: State machine (Idle ↔ Walk ↔ Run based on Speed thresholds) → Slot(DefaultSlot) → Output
- Automatically compiles

### Assign AnimBP to Enemy BP

```
set_character_properties(blueprint_path="/Game/Characters/Enemies/KingBot/BP_KingBot",
    anim_blueprint_path="/Game/Characters/Enemies/KingBot/ABP_BG_KingBot")

set_character_properties(blueprint_path="/Game/Characters/Enemies/Bell/BP_Bell",
    anim_blueprint_path="/Game/Characters/Enemies/Bell/ABP_BG_Bell",
    skeletal_mesh_path="/Game/Characters/Enemies/Bell/SK_Bell")

set_character_properties(blueprint_path="/Game/Characters/Enemies/Giganto/BP_Giganto",
    anim_blueprint_path="/Game/Characters/Enemies/Giganto/ABP_BG_Giganto")
```

---

## Phase 2: Wire Enemy BPs (UpdateEnemyAI with Full Signature)

### Current Function Signature

```cpp
UFUNCTION(BlueprintCallable, Category="Gameplay|AI", meta=(DefaultToSelf="Enemy"))
static void UpdateEnemyAI(
    ACharacter* Enemy,
    float AggroRange = 1500.0f,
    float AttackRange = 200.0f,
    float LeashDistance = 3000.0f,
    float MoveSpeed = 400.0f,
    float AttackCooldown = 2.0f,
    float AttackDamage = 10.0f,
    float AttackRadius = 150.0f,
    UAnimSequence* AttackAnim = nullptr,
    UAnimSequence* DeathAnim = nullptr,
    UAnimSequence* HitReactAnim = nullptr,
    UAnimSequence* AttackAnim2 = nullptr,
    UAnimSequence* AttackAnim3 = nullptr,
    UAnimSequence* ScreamAnim = nullptr,
    UAnimSequence* DeathAnim2 = nullptr
);
```

### For Each Enemy BP:

#### Step 1: Check Existing UpdateEnemyAI Node

```
read_blueprint_content(blueprint_path="/Game/Characters/Enemies/KingBot/BP_KingBot", include_event_graph=true)
```

Look for:
- Existing UpdateEnemyAI node with OLD signature (has `IdleAnim`, `WalkAnim`, `RunAnim`, `CrawlAnim` pins) → DELETE and recreate
- Existing UpdateEnemyAI node with NEW signature (has `AttackAnim`, `DeathAnim`, `HitReactAnim`, etc.) → keep it, just update pin defaults

#### Step 2: Add Event Tick (if missing)

```
add_node(blueprint_name="/Game/Characters/Enemies/KingBot/BP_KingBot", node_type="Event", event_type="Tick", pos_x=0, pos_y=0)
```

#### Step 3: Add UpdateEnemyAI CallFunction

If you deleted the old node or it doesn't exist:

```
add_node(
    blueprint_name="/Game/Characters/Enemies/KingBot/BP_KingBot",
    node_type="CallFunction",
    target_function="UpdateEnemyAI",
    target_class="/Script/GameplayHelpers.GameplayHelperLibrary",
    pos_x=400, pos_y=0
)
```

Capture the returned node ID → `AI_NodeID`

#### Step 4: Wire Event Tick → UpdateEnemyAI

```
connect_nodes(
    blueprint_name="/Game/Characters/Enemies/KingBot/BP_KingBot",
    source_node_id="<EventTick_NodeID>",
    source_pin_name="then",
    target_node_id="<AI_NodeID>",
    target_pin_name="execute"
)
```

#### Step 5: Set Scalar Parameters

**KingBot tuning:**
```
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="AggroRange", default_value="1500")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="AttackRange", default_value="200")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="LeashDistance", default_value="3000")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="MoveSpeed", default_value="400")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="AttackCooldown", default_value="2.0")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="AttackDamage", default_value="10")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="AttackRadius", default_value="150")
```

#### Step 6: Set Animation Pins

**KingBot animations (NEVER use zombie animations):**
```
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="AttackAnim",
    default_value="/Game/Characters/Enemies/KingBot/Animations/KingBot_Punching.KingBot_Punching")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="AttackAnim2",
    default_value="/Game/Characters/Enemies/KingBot/Animations/KingBot_Biting.KingBot_Biting")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="AttackAnim3",
    default_value="/Game/Characters/Enemies/KingBot/Animations/KingBot_NeckBite.KingBot_NeckBite")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="HitReactAnim",
    default_value="/Game/Characters/Enemies/KingBot/Animations/KingBot_Scream.KingBot_Scream")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="DeathAnim",
    default_value="/Game/Characters/Enemies/KingBot/Animations/KingBot_Death.KingBot_Death")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="DeathAnim2",
    default_value="/Game/Characters/Enemies/KingBot/Animations/KingBot_Dying.KingBot_Dying")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="RunAnim",
    default_value="/Game/Characters/Enemies/KingBot/Animations/KingBot_Run.KingBot_Run")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="CrawlAnim",
    default_value="/Game/Characters/Enemies/KingBot/Animations/KingBot_Crawl.KingBot_Crawl")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="ScreamAnim",
    default_value="/Game/Characters/Enemies/KingBot/Animations/KingBot_Scream.KingBot_Scream")
```

**Bell animations:**
```
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="AttackAnim",
    default_value="/Game/Characters/Enemies/Bell/Animations/Bell_BodyBlock.Bell_BodyBlock")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="HitReactAnim",
    default_value="/Game/Characters/Enemies/Bell/Animations/Bell_RifleHitBack.Bell_RifleHitBack")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="DeathAnim",
    default_value="/Game/Characters/Enemies/Bell/Animations/Bell_FallingToRoll.Bell_FallingToRoll")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="RunAnim",
    default_value="/Game/Characters/Enemies/Bell/Animations/Bell_Running.Bell_Running")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="ScreamAnim",
    default_value="/Game/Characters/Enemies/Bell/Animations/Bell_BodyBlock.Bell_BodyBlock")
```

**Giganto animations:**
```
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="AttackAnim",
    default_value="/Game/Characters/Enemies/Giganto/Anim_BodyBlock.Anim_BodyBlock")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="HitReactAnim",
    default_value="/Game/Characters/Enemies/Giganto/Anim_BodyBlock.Anim_BodyBlock")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="DeathAnim",
    default_value="/Game/Characters/Enemies/Giganto/Anim_ZombieStandUp.Anim_ZombieStandUp")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="RunAnim",
    default_value="/Game/Characters/Enemies/Giganto/Anim_Running.Anim_Running")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="ScreamAnim",
    default_value="/Game/Characters/Enemies/Giganto/Anim_ZombieStandUp.Anim_ZombieStandUp")
```

#### Step 7: Compile

```
compile_blueprint(blueprint_name="/Game/Characters/Enemies/KingBot/BP_KingBot")
```

Must return `compiled: true`, `num_errors: 0`.

---

## Per-Enemy Tuning

| Enemy | AggroRange | AttackRange | LeashDistance | MoveSpeed | Cooldown | Damage | AttackRadius | Rationale |
|-------|-----------|-------------|--------------|-----------|----------|--------|--------------|-----------|
| **KingBot** | 1500 | 200 | 3000 | 400 | 2.0 | 10 | 150 | Standard fighter |
| **Giganto** | 1200 | 250 | 2500 | 250 | 3.0 | 25 | 200 | Slow tank, heavy hits |
| **Bell** | 2000 | 180 | 3500 | 500 | 1.5 | 8 | 130 | Fast scout, light hits |

These are BASE values. The personality system multiplies them at runtime:
- Berserker: MoveSpeed × 1.6, AttackCooldown × 0.4
- Stalker: MoveSpeed × 0.55, AggroRange × 2.0
- Brute: MoveSpeed × 0.8, AttackDamage × 1.8
- Crawler: MoveSpeed × 0.4

---

## Phase 3: Verification

### Compile Check
```
compile_blueprint(blueprint_name="/Game/Characters/Enemies/KingBot/BP_KingBot")
compile_blueprint(blueprint_name="/Game/Characters/Enemies/Giganto/BP_Giganto")
compile_blueprint(blueprint_name="/Game/Characters/Enemies/Bell/BP_Bell")
```
All must return `compiled: true`, `num_errors: 0`.

### Screenshot
```
take_screenshot()
```
Verify:
- Enemies standing on ground (not floating)
- Capsule wireframes envelop meshes
- AnimBP assigned (check SkeletalMeshComponent in Details panel)

### PIE Test (Alt+P)

1. **Idle behaviors**: Walk around enemies at >1500 units distance. They should:
   - Stand idle
   - Occasionally look around
   - Occasionally scream (KingBot only)
   - Wander slightly

2. **Chase**: Approach within AggroRange (~1500 units). Enemy should:
   - Turn toward player
   - Start walking toward player (AnimBP plays walk animation)
   - Move with slight lateral wobble (personality-based)

3. **Attack**: Let enemy reach AttackRange (~200 units). Enemy should:
   - Face player
   - Play attack animation (montage overlays on walk via Slot)
   - Deal damage to player (check player Health in Output Log)
   - Respect cooldown (2.0s default)

4. **Hit reaction**: Damage the enemy. Enemy should:
   - Play HitReactAnim (instantly stops current montage)
   - Face player
   - Resume previous state after anim finishes

5. **Death**: Reduce enemy Health to 0. Enemy should:
   - Play DeathAnim (SingleNode, freezes on last frame)
   - Destroy after animation finishes

6. **Return/Leash**: Run >3000 units away from enemy spawn. Enemy should:
   - Stop chasing
   - Walk back toward spawn point
   - Snap to spawn when close
   - Return to idle

7. **Multiple enemies**: Check that enemies don't stack (capsule collision pushes them apart).

### What to Check in Output Log

```
LogTemp: ApplyMeleeDamage: BP_RobotCharacter_C_0 took 10 damage, health now 90
LogTemp: Enemy assigned personality: Berserker (speed 640, aggro 750, cooldown 0.8)
LogTemp: Playing hit-react animation on BP_KingBot_C_0
LogTemp: Enemy BP_KingBot_C_0 died, playing death animation
```

---

## Troubleshooting

### "Enemy doesn't move"
- Check `CharacterMovementComponent` exists (it's default on ACharacter BPs)
- Check Event Tick is wired to UpdateEnemyAI
- Verify MoveSpeed > 0 in UpdateEnemyAI pin defaults
- Check Output Log for "Enemy assigned personality" — if missing, UpdateEnemyAI isn't being called

### "Enemy slides instead of walking animation"
- **Ensure AnimBP is assigned to skeletal mesh**: `set_character_properties(anim_blueprint=...)`
- AnimBP locomotion state machine reads `Speed` variable (from CharacterMovementComponent velocity)
- Use `setup_locomotion_state_machine` to create AnimBP (automatic Idle ↔ Walk state machine)
- If no AnimBP: enemy will T-pose slide (still functional AI, just no anim)

### "Enemy doesn't animate attacks"
- Check AttackAnim pin has correct asset path with `.Anim` suffix: `/Game/Path/Anim.Anim`
- Verify AnimBP has Slot(DefaultSlot) node in AnimGraph (setup_locomotion_state_machine adds this automatically)
- Check Output Log for "Playing animation montage" — if missing, PlayAnimationOneShot isn't being called
- Verify attack cooldown isn't too long (default 2.0s)

### "Hit reactions don't play"
- Ensure HitReactAnim pin is set in UpdateEnemyAI node
- Check that enemy has Health variable (created by `/unreal-combat` skill)
- Verify `bForceInterrupt=true` in PlayAnimationOneShot call for hit-react (allows interrupting current montage)
- Check Output Log for "Playing hit-react animation"

### "Enemy T-poses"
- AnimBP not assigned to skeletal mesh component
- Wrong skeleton in AnimBP (must match skeletal mesh skeleton)
- AnimBP not compiled — check for Blueprint compile errors

### "Enemy attacks too fast / too slow"
- Adjust `AttackCooldown` pin default (seconds between attacks)
- Default 2.0s is reasonable for melee combat
- Personality system multiplies this: Berserker × 0.4 (0.8s), Stalker × 1.5 (3.0s)

### "Enemy chases forever / never stops"
- Check `LeashDistance` is reasonable (3000 = ~30 meters)
- Leash check uses distance from **spawn point** (stored on first tick), not from player

### "Enemy doesn't return to spawn"
- Spawn point is stored on first tick (static TMap). If enemy was moved in editor after first PIE run, the cached spawn point may be stale.
- Stop and restart PIE to reset the static TMap.
- Check Output Log for "Returning to spawn" message

### "All enemies have the same personality"
- Personality is assigned randomly (per-actor, stored in static TMap)
- Probabilities: 30% Normal, 15% Berserker, 20% Stalker, 15% Brute, 20% Crawler
- Check Output Log for "Enemy assigned personality: ..." on first tick

### "Enemy doesn't die / death animation doesn't play"
- Ensure DeathAnim pin is set in UpdateEnemyAI node
- Verify enemy has Health variable (created by `/unreal-combat` skill)
- Check Output Log for "Enemy died, playing death animation"
- Death uses SingleNode mode (NOT montage) — freezes on last frame

### "Enemy animation freezes mid-attack"
- Old UpdateEnemyAI signature used OverrideAnimationData (breaks AnimBP) — DELETE old node, recreate with new signature
- Ensure AnimBP is assigned (not overridden by SingleNode mode during runtime)
- Check that you're NOT manually calling `MeshComp->PlayAnimation()` except for death

---

## Architecture

### Why NOT NavMesh / BehaviorTree / AIController

| Approach | Problem |
|----------|---------|
| NavMesh + MoveToActor | Requires NavMeshBoundsVolume in level + AIController. Silent failure without NavMesh. |
| BehaviorTree | BlackboardKey is protected in UE5.7 — can't wire via MCP. Manual editor work needed. |
| AIController | Adds complexity layer. `AddMovementInput()` works directly on ACharacter. |

### What We Use Instead

**Direct C++ tick function**: `UpdateEnemyAI()` in GameplayHelperLibrary.

```
Event Tick → UpdateEnemyAI(Enemy=Self, AggroRange, AttackRange, LeashDistance, MoveSpeed, ...)
```

One node per enemy BP. All logic in C++. State stored in static TMap (spawn location, attack cooldown, personality, current state, hit-react timer).

### Internal State Storage

```cpp
struct FEnemyAIState
{
    FVector SpawnLocation;
    double LastAttackTime = 0.0;
    bool bInitialized = false;
    EEnemyPersonality Personality = EEnemyPersonality::Normal;
    EEnemyAIState CurrentState = EEnemyAIState::Idle;
    EEnemyAIState PreHitState = EEnemyAIState::Idle;
    double HitReactEndTime = 0.0;
    int32 AttackIndex = 0; // Cycles 0→1→2 for Normal/Brute personalities
    double NextIdleBehaviorTime = 0.0;
    int32 IdleBehaviorIndex = 0;
};
static TMap<TWeakObjectPtr<AActor>, FEnemyAIState> EnemyAIStates;
```

No Blueprint variables needed — all state managed in C++.

---

## Future Enhancements (Not in MVP)

| Feature | Approach |
|---------|----------|
| Patrol waypoints | Add `FVector PatrolPoints[]` param, cycle between them in Idle state |
| NavMesh pathfinding | Add NavMeshBoundsVolume, switch to `AAIController::MoveToActor()` |
| Ranged attacks | Add bool `bRanged` + projectile spawn at AttackRange |
| Group coordination | Enemies share target info, flank from different angles |
| Alert system | One enemy spots player → nearby enemies also aggro |
| Dodge/block | Enemy reacts to player attacks with defensive animations |
| Grapple attacks | Enemy grabs player, player must break free with QTE |
