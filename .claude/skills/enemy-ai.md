# Skill: Enemy AI — Chase, Attack, Leash

Complete pipeline for adding enemy AI behavior via a single C++ tick function. Enemies chase the player, attack in melee range, and return to spawn when the player escapes.

## Trigger Phrases
- "enemy AI"
- "enemies chase"
- "make enemies attack"
- "enemy behavior"
- "AI movement"
- "leash distance"

## Prerequisites

| Asset | Path | Required |
|-------|------|----------|
| Enemy BPs | `/Game/Characters/Enemies/*/BP_*` | YES |
| Health variables | `Health` float on each enemy BP | YES (created by `/unreal-combat`) |
| GameplayHelpers plugin | `UpdateEnemyAI` function | YES (must be built + editor restarted) |
| Player BP | `/Game/Characters/Robot/BP_RobotCharacter` | YES |
| Enemy attack anims | `/Game/Characters/Enemies/*/SK_*_Anim` | YES |

## CRITICAL RULES

1. **ALL MCP calls STRICTLY SEQUENTIAL** — never parallel.
2. **Build C++ FIRST, restart editor, THEN wire BPs.**
3. **`UpdateEnemyAI` reuses `PlayAnimationOneShot` + `ApplyMeleeDamage`** — do NOT duplicate that logic in Blueprint.
4. **Static state storage** — no extra BP variables needed for AI state. The C++ function stores spawn location and attack cooldown in a static TMap.

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

One node per enemy BP. All logic in C++.

### State Machine

```
                ┌──────────┐
                │   IDLE   │ (player beyond AggroRange)
                └────┬─────┘
                     │ player enters AggroRange
                     ▼
                ┌──────────┐
                │  CHASE   │ (AddMovementInput toward player, face player)
                └────┬─────┘
                     │ player within AttackRange
                     ▼
                ┌──────────┐
                │  ATTACK  │ (PlayAnimOneShot + ApplyMeleeDamage, cooldown)
                └────┬─────┘
                     │ enemy beyond LeashDistance from spawn
                     ▼
                ┌──────────┐
                │  RETURN  │ (AddMovementInput toward spawn, ignore player)
                └──────────┘
                     │ reached spawn point
                     ▼
                   (IDLE)
```

---

## Phase 1: C++ Implementation (~60 lines)

### Step 1: Add to GameplayHelperLibrary.h

After `ApplyMeleeDamage` declaration, add:

```cpp
/**
 * Tick-based enemy AI: chase player, attack in range, return when leashed.
 * State stored internally (static TMap). Call from Event Tick on each enemy.
 */
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
    UAnimSequence* AttackAnim = nullptr
);
```

### Step 2: Add Implementation to GameplayHelperLibrary.cpp

```cpp
// Internal state storage
struct FEnemyAIState
{
    FVector SpawnLocation;
    double LastAttackTime = 0.0;
    bool bInitialized = false;
};
static TMap<TWeakObjectPtr<AActor>, FEnemyAIState> EnemyAIStates;

void UGameplayHelperLibrary::UpdateEnemyAI(ACharacter* Enemy, float AggroRange, float AttackRange,
    float LeashDistance, float MoveSpeed, float AttackCooldown,
    float AttackDamage, float AttackRadius, UAnimSequence* AttackAnim)
{
    if (!Enemy) return;
    UWorld* World = Enemy->GetWorld();
    if (!World) return;

    // Get/init state
    TWeakObjectPtr<AActor> Key(Enemy);
    FEnemyAIState& State = EnemyAIStates.FindOrAdd(Key);
    if (!State.bInitialized)
    {
        State.SpawnLocation = Enemy->GetActorLocation();
        State.bInitialized = true;
    }

    // Clean up dead entries periodically (every 100th call)
    static int32 CleanupCounter = 0;
    if (++CleanupCounter > 100)
    {
        CleanupCounter = 0;
        for (auto It = EnemyAIStates.CreateIterator(); It; ++It)
        {
            if (!It.Key().IsValid()) It.RemoveCurrent();
        }
    }

    // Find player
    ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
    if (!Player) return;

    float DistToPlayer = FVector::Dist(Enemy->GetActorLocation(), Player->GetActorLocation());
    float DistToSpawn = FVector::Dist(Enemy->GetActorLocation(), State.SpawnLocation);
    double CurrentTime = World->GetTimeSeconds();

    // Check if already dead (Health <= 0)
    FProperty* HealthProp = Enemy->GetClass()->FindPropertyByName(FName("Health"));
    if (HealthProp)
    {
        void* ValPtr = HealthProp->ContainerPtrToValuePtr<void>(Enemy);
        float HP = 0.f;
        if (FFloatProperty* FP = CastField<FFloatProperty>(HealthProp)) HP = FP->GetPropertyValue(ValPtr);
        else if (FDoubleProperty* DP = CastField<FDoubleProperty>(HealthProp)) HP = (float)DP->GetPropertyValue(ValPtr);
        if (HP <= 0.f) return; // Dead, do nothing
    }

    // Set movement speed
    UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement();
    if (MoveComp) MoveComp->MaxWalkSpeed = MoveSpeed;

    // --- STATE MACHINE ---

    // RETURN: Beyond leash distance from spawn → go home
    if (DistToSpawn > LeashDistance)
    {
        FVector DirToSpawn = (State.SpawnLocation - Enemy->GetActorLocation()).GetSafeNormal();
        Enemy->AddMovementInput(DirToSpawn, 1.0f);
        FRotator LookRot = DirToSpawn.Rotation();
        Enemy->SetActorRotation(FRotator(0.f, LookRot.Yaw, 0.f));

        // Snap when close
        if (DistToSpawn < 100.f)
        {
            Enemy->SetActorLocation(State.SpawnLocation);
        }
        return;
    }

    // ATTACK: Within attack range + cooldown expired
    if (DistToPlayer < AttackRange)
    {
        // Face player
        FVector DirToPlayer = (Player->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal();
        FRotator LookRot = DirToPlayer.Rotation();
        Enemy->SetActorRotation(FRotator(0.f, LookRot.Yaw, 0.f));

        // Attack with cooldown
        if ((CurrentTime - State.LastAttackTime) >= AttackCooldown)
        {
            State.LastAttackTime = CurrentTime;

            // Play attack animation (if provided)
            if (AttackAnim)
            {
                PlayAnimationOneShot(Enemy, AttackAnim, 1.0f, 0.15f, 0.15f, true);
            }

            // Deal damage to player
            ApplyMeleeDamage(Enemy, AttackDamage, AttackRadius, 30000.f);
        }
        return;
    }

    // CHASE: Within aggro range → move toward player
    if (DistToPlayer < AggroRange)
    {
        FVector DirToPlayer = (Player->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal();
        Enemy->AddMovementInput(DirToPlayer, 1.0f);
        FRotator LookRot = DirToPlayer.Rotation();
        Enemy->SetActorRotation(FRotator(0.f, LookRot.Yaw, 0.f));
        return;
    }

    // IDLE: Player not in range → do nothing (AnimBP handles idle animation)
}
```

### Step 3: Add include

Add `#include "Kismet/GameplayStatics.h"` to the .cpp if not already present.

### Step 4: Build Plugin

```bash
.../Engine/Build/BatchFiles/Linux/Build.sh bood_and_dustEditor Linux Development -Project=".../bood_and_dust.uproject"
```

Restart editor after build.

---

## Phase 2: Wire Enemy BPs (~10 MCP calls per enemy)

### For Each Enemy BP:

#### Step 1: Add Event Tick (check if exists first)
```
read_blueprint_content(blueprint_path="/Game/Characters/Enemies/KingBot/BP_KingBot", include_event_graph=true)
```
If no Event Tick node exists:
```
add_node(blueprint_name="...", node_type="Event", event_type="Tick", pos_x=0, pos_y=0)
```

#### Step 2: Add GetPlayerCharacter (pure, no exec)
Not needed — UpdateEnemyAI finds the player internally via `GetPlayerCharacter(World, 0)`.

#### Step 3: Add UpdateEnemyAI CallFunction
```
add_node(
    blueprint_name="/Game/Characters/Enemies/KingBot/BP_KingBot",
    node_type="CallFunction",
    target_function="UpdateEnemyAI",
    target_class="/Script/GameplayHelpers.GameplayHelperLibrary",
    pos_x=400, pos_y=0
)
```
Capture → `AI_NodeID`

#### Step 4: Wire Tick → UpdateEnemyAI
```
connect_nodes(source_node_id="<Tick_NodeID>", source_pin_name="then", target_node_id="<AI_NodeID>", target_pin_name="execute")
```

#### Step 5: Set Per-Enemy Parameters
```
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="AggroRange", default_value="1500")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="AttackRange", default_value="200")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="LeashDistance", default_value="3000")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="MoveSpeed", default_value="400")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="AttackCooldown", default_value="2.0")
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="AttackDamage", default_value="10")
```

#### Step 6: Set Attack Animation
```
set_node_property(node_id="<AI_NodeID>", action="set_pin_default", pin_name="AttackAnim",
    default_value="/Game/Characters/Enemies/KingBot/SK_KingBot_Anim.SK_KingBot_Anim")
```
**NOTE**: Use the enemy's existing animation. Each enemy has `SK_{Type}_Anim`.

#### Step 7: Compile
```
compile_blueprint(blueprint_name="/Game/Characters/Enemies/KingBot/BP_KingBot")
```
Must return 0 errors.

### Per-Enemy Tuning

| Enemy | AggroRange | AttackRange | LeashDistance | MoveSpeed | Cooldown | Damage | Rationale |
|-------|-----------|-------------|--------------|-----------|----------|--------|-----------|
| **KingBot** | 1500 | 200 | 3000 | 400 | 2.0 | 10 | Standard fighter |
| **Giganto** | 1200 | 250 | 2500 | 250 | 3.0 | 25 | Slow tank, heavy hits |
| **Bell** | 2000 | 180 | 3500 | 500 | 1.5 | 8 | Fast scout, light hits |

---

## Phase 3: Verification

### Compile Check
```
compile_blueprint(blueprint_name="/Game/Characters/Enemies/KingBot/BP_KingBot")
compile_blueprint(blueprint_name="/Game/Characters/Enemies/Giganto/BP_Giganto")
compile_blueprint(blueprint_name="/Game/Characters/Enemies/Bell/BP_Bell")
```
All 0 errors.

### PIE Test (Alt+P)
1. Walk toward an enemy → they should start chasing when you're ~1500 units away
2. Let them reach you → they should attack (animation + damage)
3. Run away ~3000 units → they should stop chasing and walk back to spawn
4. Multiple enemies should spread out (capsule collision prevents stacking)

### What to Check in Output Log
```
LogTemp: ApplyMeleeDamage: BP_RobotCharacter_C_0 took 10 damage, health now 90
```
(If player has Health variable — optional TASK-026J)

---

## Troubleshooting

### "Enemy doesn't move"
- Check `CharacterMovementComponent` exists (it's default on ACharacter BPs)
- Check `MaxWalkSpeed` is > 0 (UpdateEnemyAI sets it each tick)
- Verify Event Tick is wired to UpdateEnemyAI

### "Enemy slides instead of walking animation"
- Ensure AnimBP is assigned to the enemy's skeletal mesh
- AnimBP locomotion state machine reads `Speed` variable → drives walk blend
- If no AnimBP: enemy will T-pose slide (still functional, just no anim)

### "Enemy attacks too fast / too slow"
- Adjust `AttackCooldown` pin default (seconds between attacks)
- Default 2.0s is reasonable for melee combat

### "Enemy chases forever / never stops"
- Check `LeashDistance` is reasonable (3000 = ~30 meters)
- Leash check uses distance from **spawn point**, not from player

### "Enemy doesn't return to spawn"
- Spawn point is stored on first tick (static TMap). If enemy was moved in editor after first PIE run, the cached spawn point may be stale.
- Stop and restart PIE to reset the static TMap.

### "All enemies attack at the same time"
- Each enemy has independent cooldown tracking (per-actor state in TMap)
- To stagger: give slightly different `AttackCooldown` values (1.8, 2.0, 2.2)

---

## Future Enhancements (Not in MVP)

| Feature | Approach |
|---------|----------|
| Patrol waypoints | Add `FVector PatrolPoints[]` param, cycle between them in Idle state |
| Hit reactions | Enemy plays getting-hit animation when taking damage |
| NavMesh pathfinding | Add NavMeshBoundsVolume, switch to `AAIController::MoveToActor()` |
| Ranged attacks | Add bool `bRanged` + projectile spawn at AttackRange |
| Group coordination | Enemies share target info, flank from different angles |
| Alert system | One enemy spots player → nearby enemies also aggro |
