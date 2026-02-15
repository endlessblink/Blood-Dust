# Skill: Game Flow — "The Escape" Objective Loop

Set up the core game flow for Blood & Dust: a golden glowing portal beacon at the escape point, a dim warm start zone, breadcrumb path lights that explode when collected, and a golden victory screen when the player reaches the portal.

## Trigger Phrases
- "game flow"
- "escape objective"
- "demo flow"
- "portal beacon"
- "win condition"
- "reach the light"
- "checkpoint feedback"
- "victory screen"
- "end level"

## Prerequisites

Before running this skill, verify:
- UnrealMCP server connected (TCP 127.0.0.1:55557)
- Landscape exists in level
- BP_RobotCharacter exists at `/Game/Characters/Robot/BP_RobotCharacter`
- GameplayHelpers plugin compiled and loaded
- Player start location near X=0, Y=0

**Run `does_asset_exist` for critical assets before proceeding.**

## CRITICAL RULES

1. **ALL MCP calls strictly SEQUENTIAL** — never parallel. Each creates FTSTicker callback; parallel = crash.
2. **Breathing room** between heavy operations — call `get_actors_in_level` or `list_assets` as a lightweight pause.
3. **Max 3 spawn operations in rapid succession** — then pause with a lightweight MCP call.
4. **Prompt user to Ctrl+S** after each major phase. Unsaved packages accumulate in RAM.
5. **Take screenshot after visual phases** to verify placement.
6. **Actor names must be unique** — all names use `Portal_`, `Start_`, `Breadcrumb_` prefixes.
7. **connect_nodes APPENDS connections** — never call twice on the same pin. If wrong, DELETE the node and recreate.

---

## Architecture Overview

```
ManageGameFlow(Player)     <- called from EventTick on BP_RobotCharacter
|-- Init: TActorIterator<APointLight> discovers Breadcrumb_Light* and Portal_Light*
|-- Tick: Distance check -> collect checkpoints (intensity ramp -> burst -> destroy)
|-- Tick: Golden flash animation (0.5s ease-out)
|-- Tick: "SOUL RECOVERED (2/3)" text (1.5s fade-in/hold/fade-out)
+-- Tick: Victory at portal -> overlay + disable input + restart after 5s
```

**Why C++ over Blueprint Trigger + UMG Widgets:**
- Slate overlays created in C++ avoid MCP's widget wiring fragility
- Light intensity animation needs per-frame tick (FMath::InterpEaseOut)
- Single function handles collection, feedback, and victory — no Blueprint graph complexity
- Same proven pattern as ManagePlayerHUD and UpdateEnemyAI

---

## The Emotional Arc

```
START: Dark ruins, small warm light. "Where am I?"
       |
DISCOVER: First breadcrumb light. Walk toward it.
       |
COLLECT: Light ramps bright -> BURSTS -> golden flash! "SOUL RECOVERED (1/3)"
       |
CHASE: More lights ahead. Enemies in the way. "Fight or run?"
       |
ARRIVE: Portal's blinding golden glow. Walk into it.
       |
VICTORY: "THE ESCAPE -- LEVEL COMPLETE -- 3/3 Souls Recovered"
       |
RESTART: Level restarts after 5 seconds.
```

---

## Phase 0: Audit (~2 min, 3-5 MCP calls)

### Step 1: Capture Current State

```
get_actors_in_level()
```
Check for any existing portal lights, breadcrumb lights. If `Portal_Light_01` already exists, skip Phase 1. If `Breadcrumb_Light_01` exists, skip Phase 3.

### Step 2: Get Ground Heights

Query ground height at key locations for proper placement:

```
get_height_at_location(x=7500, y=0)
```
Save as `PORTAL_Z`.

```
get_height_at_location(x=0, y=0)
```
Save as `START_Z`.

### Step 3: Screenshot Before
```
take_screenshot()
```
Save as "before" reference.

---

## Phase 1: Portal Beacon — The Golden Light (~5 min, ~9 MCP calls)

### Goal
Create a bright golden glow at X=7500 visible from the player's start position. Two PointLights: one at ground level (warm pool) and one high up (beacon visible from afar).

### Step 1: Spawn Ground-Level Portal Light

```
spawn_actor(
    name="Portal_Light_01",
    type="PointLight",
    location=[7500, 0, PORTAL_Z + 400],
    rotation=[0, 0, 0],
    scale=[1, 1, 1]
)
```

```
set_actor_property(
    actor_name="Portal_Light_01",
    property_name="LightColor",
    property_value={"R": 1.0, "G": 0.7, "B": 0.2, "A": 1.0}
)
```
Warm golden color matching the Rembrandt palette (#FFB333 range).

```
set_actor_property(
    actor_name="Portal_Light_01",
    property_name="Intensity",
    property_value=50000.0
)
```
High intensity — must be visible as a bright pool from 7500 units away.

```
set_actor_property(
    actor_name="Portal_Light_01",
    property_name="AttenuationRadius",
    property_value=5000.0
)
```
Large radius to illuminate surrounding terrain and structures.

### Step 2: Breathing Room
```
get_actors_in_level()
```

### Step 3: Spawn High Beacon Light

```
spawn_actor(
    name="Portal_Beacon_01",
    type="PointLight",
    location=[7500, 0, PORTAL_Z + 1200],
    rotation=[0, 0, 0],
    scale=[1, 1, 1]
)
```

```
set_actor_property(
    actor_name="Portal_Beacon_01",
    property_name="LightColor",
    property_value={"R": 1.0, "G": 0.85, "B": 0.4, "A": 1.0}
)
```
Slightly lighter gold for the beacon — more visible against dark sky.

```
set_actor_property(
    actor_name="Portal_Beacon_01",
    property_name="Intensity",
    property_value=100000.0
)
```
Double the ground light — this is the "lighthouse" effect.

```
set_actor_property(
    actor_name="Portal_Beacon_01",
    property_name="AttenuationRadius",
    property_value=15000.0
)
```
Massive radius — reaches all the way back to the start zone.

### Step 4: Optional — Portal Niagara VFX

**Only if a Niagara system exists.** Check first:
```
does_asset_exist(asset_path="/Game/FX/NS_Portal_Glow")
```

If it exists:
```
spawn_niagara_system(
    actor_name="Portal_FX_01",
    system_path="/Game/FX/NS_Portal_Glow",
    location=[7500, 0, PORTAL_Z + 200]
)
```

If no Niagara asset exists, **skip** — the twin lights are the primary visual beacon.

### Checkpoint
> "Phase 1 complete. Golden portal beacon placed at X=7500 with ground light (50K intensity) and high beacon (100K intensity). Press **Ctrl+S** to save. Ready for Phase 2?"

---

## Phase 2: Start Zone Atmosphere (~3 min, ~5 MCP calls)

### Goal
A small, warm, intimate light where the player spawns. Contrasts with the bright distant portal — pulling the player forward.

### Step 1: Spawn Start Zone Light

```
spawn_actor(
    name="Start_Light_01",
    type="PointLight",
    location=[0, 0, START_Z + 200],
    rotation=[0, 0, 0],
    scale=[1, 1, 1]
)
```

```
set_actor_property(
    actor_name="Start_Light_01",
    property_name="LightColor",
    property_value={"R": 1.0, "G": 0.6, "B": 0.3, "A": 1.0}
)
```
Warm campfire orange — intimate, safe feeling.

```
set_actor_property(
    actor_name="Start_Light_01",
    property_name="Intensity",
    property_value=5000.0
)
```
Intentionally dim — 1/10th of the portal light. Player should feel exposed.

```
set_actor_property(
    actor_name="Start_Light_01",
    property_name="AttenuationRadius",
    property_value=800.0
)
```
Small radius — a tight pool of warmth in the darkness.

### Checkpoint
> "Phase 2 complete. Dim warm light at start zone. The contrast with the bright portal beacon should create a strong visual pull. Press **Ctrl+S**. Ready for Phase 3?"

---

## Phase 3: Path Breadcrumbs (~5 min, ~14 MCP calls)

### Goal
3 smaller golden lights along the X axis from start to portal. They create a "trail of light" guiding the player. These lights will be COLLECTED by ManageGameFlow when the player walks near them.

### Breadcrumb Positions

| Light | X | Y | Intensity | Radius | Purpose |
|-------|---|---|-----------|--------|---------|
| Breadcrumb_Light_01 | 2000 | 200 | 3000 | 600 | First hint after leaving start |
| Breadcrumb_Light_02 | 4500 | -150 | 3000 | 600 | Midpoint reassurance |
| Breadcrumb_Light_03 | 6000 | 100 | 5000 | 800 | Getting close, brighter |

Y offsets are slightly varied — perfectly aligned would look artificial.

**IMPORTANT**: Names MUST contain "Breadcrumb_Light" — ManageGameFlow discovers lights by this name pattern via `TActorIterator<APointLight>`.

### For Each Breadcrumb Light:

1. Get ground height:
```
get_height_at_location(x=LIGHT_X, y=LIGHT_Y)
```

2. Spawn:
```
spawn_actor(
    name="Breadcrumb_Light_0N",
    type="PointLight",
    location=[LIGHT_X, LIGHT_Y, GROUND_Z + 150],
    rotation=[0, 0, 0],
    scale=[1, 1, 1]
)
```

3. Set properties:
```
set_actor_property(actor_name="Breadcrumb_Light_0N", property_name="LightColor", property_value={"R": 1.0, "G": 0.75, "B": 0.35, "A": 1.0})
set_actor_property(actor_name="Breadcrumb_Light_0N", property_name="Intensity", property_value=LIGHT_INTENSITY)
set_actor_property(actor_name="Breadcrumb_Light_0N", property_name="AttenuationRadius", property_value=LIGHT_RADIUS)
```

**IMPORTANT**: Max 3 spawns before breathing room. After spawning all 3 lights, call:
```
get_actors_in_level()
```

### Checkpoint
> "Phase 3 complete. 3 breadcrumb lights placed along path from start to portal. Press **Ctrl+S**. Ready for Phase 4?"

---

## Phase 4: C++ Implementation — ManageGameFlow (~20 min, requires plugin rebuild)

### Goal
Add `ManageGameFlow()` to GameplayHelperLibrary — a tick-based function that handles checkpoint collection, visual feedback, and victory screen. Same architecture as ManagePlayerHUD and UpdateEnemyAI.

### Architecture

```
ManageGameFlow(ACharacter* Player) -- static, called per frame
|
|-- STATE: FGameFlowState (static)
|   |-- TArray<FCheckpointData> Checkpoints -- discovered at init
|   |-- PortalLightActor/PortalLocation -- win trigger
|   |-- CheckpointsCollected / TotalCheckpoints -- counter
|   |-- GoldenFlashStartTime -- drives flash animation
|   |-- CheckpointTextStartTime -- drives text animation
|   +-- bVictory -- terminal state
|
|-- INIT (first call per level):
|   +-- TActorIterator<APointLight> discovers:
|       |-- "Breadcrumb_Light*" -> Checkpoints array (stores location + intensity)
|       +-- "Portal_Light*" -> PortalLightActor
|
|-- TICK -- Checkpoint Collection:
|   |-- For each Active: distance < 400 -> mark Collecting
|   |-- For each Collecting:
|   |   |-- 0-0.3s: InterpEaseOut intensity ramp (original -> original x 10)
|   |   |-- At 0.3s: Niagara burst (if loaded) -> Destroy light -> mark Collected
|   |   +-- Trigger golden flash + checkpoint text
|   +-- Niagara: StaticLoadObject "/Game/FX/NS_CheckpointBurst" (graceful null)
|
|-- TICK -- Golden Flash (0.5s):
|   |-- 0-0.1s: alpha 0 -> 0.35 (sharp in)
|   +-- 0.1-0.5s: alpha 0.35 -> 0 (slow fade)
|
|-- TICK -- Checkpoint Text (1.5s):
|   |-- 0-0.2s: fade in (alpha 0 -> 1)
|   |-- 0.2-0.8s: hold at alpha 1
|   +-- 0.8-1.5s: fade out (alpha 1 -> 0)
|
+-- TICK -- Victory Condition:
    |-- Distance to Portal < 500 AND !bDead -> bVictory = true
    |-- Show VictoryOverlay (Collapsed -> Visible)
    |-- Update "X/Y Souls Recovered" text
    |-- DisableInput on PlayerController
    +-- Timer 5s -> reset PlayerHUD + GameFlow + EnemyAIStates -> OpenLevel
```

### Research-Backed Design Decisions

**Light intensity curves (from VFX timing research):**
- Use `FMath::InterpEaseOut(Start, Peak, Alpha, 2.0f)` — NOT linear Lerp
- Rapid initial brightening that slows as it peaks feels professional
- Duration 0.3s is standard for collectible feedback (0.3-0.5s range)
- Peak intensity: 10x original (3000 -> 30000) — dramatic but brief

**Screen flash timing (from game UX research):**
- 0.5s total: 0.1s sharp ramp in, 0.4s slow ease-out fade
- Peak alpha 0.35 (not 0.8+ — that's jarring for a positive event)
- Golden color (0.55, 0.35, 0.10) — matches Rembrandt palette, warm reward
- Ease-out curve for fade: fast -> slow = satisfying

**Text notification timing (from UI/UX research):**
- 1.5s total: 0.2s fade-in, 0.6s hold, 0.7s fade-out
- Spaced letter text "S O U L   R E C O V E R E D" for gravitas
- Counter format "( 2 / 3 )" so player knows progress
- Same golden color as flash for visual cohesion

**Victory screen (from game design research):**
- 5s display before restart (3s too short for reading, 8s feels stuck)
- Disable input IMMEDIATELY on trigger (PlayerController::DisableInput)
- Show checkpoint counter ("X / Y Souls Recovered") for replayability motivation
- Use OpenLevel with current level name for clean restart
- Reset ALL static state: PlayerHUD + GameFlow + EnemyAIStates + BlockingActors

**Slate vs UMG (from Epic's UMG Best Practices):**
- Slate is correct here because ManagePlayerHUD already uses Slate
- Extending the existing SOverlay Root is cleaner than mixing Slate + UMG
- No editor-side UMG assets needed — everything built in C++
- Performance: Slate is faster than UMG for simple overlays

### Files to Modify

| File | Changes |
|------|---------|
| `GameplayHelperLibrary.h` | Add `ManageGameFlow` UFUNCTION declaration |
| `GameplayHelperLibrary.cpp` | Add includes, state structs, extend Slate HUD, implement ManageGameFlow |

**No Build.cs changes** — `Engine`, `Slate`, `Niagara`, `EngineUtils` already available.

### Step 1: Header Declaration

Add to `GameplayHelperLibrary.h` before closing `};`:

```cpp
/**
 * Manage game flow: checkpoint light collection + victory screen.
 * Call from EventTick on the player character, after ManagePlayerHUD.
 * Discovers PointLights named "Breadcrumb_Light*" and "Portal_Light*" on first call.
 */
UFUNCTION(BlueprintCallable, Category="Gameplay|GameFlow", meta=(DefaultToSelf="Player"))
static void ManageGameFlow(ACharacter* Player);
```

### Step 2: New Includes

Add after existing `#include "NiagaraSystem.h"`:

```cpp
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "EngineUtils.h"
```

### Step 3: State Structs

Add after `static FPlayerHUDState PlayerHUD;`:

```cpp
// --- Game Flow (Checkpoint + Victory) ---

enum class ECheckpointState : uint8
{
    Active,      // Waiting for player
    Collecting,  // Intensity ramp animation (0.3s)
    Collected    // Destroyed
};

struct FCheckpointData
{
    TWeakObjectPtr<AActor> LightActor;
    FVector Location;
    ECheckpointState State = ECheckpointState::Active;
    double CollectStartTime = 0.0;
    float OriginalIntensity = 3000.0f;
};

struct FGameFlowState
{
    TWeakObjectPtr<UWorld> OwnerWorld;
    bool bInitialized = false;
    bool bVictory = false;
    TArray<FCheckpointData> Checkpoints;
    int32 CheckpointsCollected = 0;
    int32 TotalCheckpoints = 0;
    TWeakObjectPtr<AActor> PortalLightActor;
    FVector PortalLocation = FVector::ZeroVector;
    double GoldenFlashStartTime = 0.0;
    double CheckpointTextStartTime = 0.0;
    double VictoryStartTime = 0.0;
    FString CheckpointDisplayText;
};
static FGameFlowState GameFlow;
```

### Step 4: Extend FPlayerHUDState

Add after `bool bDead = false;` inside the existing struct:

```cpp
// Game flow UI (built alongside HUD in ManagePlayerHUD)
TSharedPtr<SBorder> GoldenFlashBorder;
TSharedPtr<STextBlock> CheckpointText;
TSharedPtr<SWidget> VictoryOverlay;
TSharedPtr<STextBlock> VictoryCheckpointText;
```

### Step 5: Extend Slate Widget Tree

In the `!PlayerHUD.bCreated` block inside ManagePlayerHUD, after the DamageFlashBorder slot, add 3 new SOverlay slots:

**Slot: Golden Flash** — fullscreen golden overlay, starts transparent
```cpp
+ SOverlay::Slot()
.HAlign(HAlign_Fill)
.VAlign(VAlign_Fill)
[
    SAssignNew(PlayerHUD.GoldenFlashBorder, SBorder)
    .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
    .BorderBackgroundColor(FLinearColor(0.55f, 0.35f, 0.10f, 0.0f))
    .Padding(0)
    .Visibility(EVisibility::HitTestInvisible)
]
```

**Slot: Checkpoint Text** — centered, starts transparent
```cpp
+ SOverlay::Slot()
.HAlign(HAlign_Center)
.VAlign(VAlign_Center)
[
    SAssignNew(PlayerHUD.CheckpointText, STextBlock)
    .Text(FText::FromString(TEXT("")))
    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
    .ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.35f, 0.10f, 0.0f)))
    .ShadowOffset(FVector2D(2.0f, 2.0f))
    .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f))
    .Visibility(EVisibility::HitTestInvisible)
]
```

**Slot: Victory Overlay** — Dutch Golden Age ornate screen, starts collapsed
```cpp
+ SOverlay::Slot()
.HAlign(HAlign_Fill)
.VAlign(VAlign_Fill)
[
    SAssignNew(PlayerHUD.VictoryOverlay, SOverlay)
    .Visibility(EVisibility::Collapsed)

    // Deep warm umber vignette (NOT crimson -- golden/warm only)
    + SOverlay::Slot()
    [
        SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
        .BorderBackgroundColor(FLinearColor(0.08f, 0.05f, 0.02f, 0.90f))
        .Padding(0)
    ]

    // Centered victory content
    + SOverlay::Slot()
    .HAlign(HAlign_Center)
    .VAlign(VAlign_Center)
    [
        SNew(SVerticalBox)

        // Upper decorative double rule
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
          .Padding(0, 0, 0, 16)
        [ SNew(STextBlock)
          .Text(FText::FromString(TEXT("========================")))
          .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
          .ColorAndOpacity(FSlateColor(FLinearColor(0.40f, 0.28f, 0.10f, 0.80f)))
        ]

        // "THE ESCAPE" -- Bold 64pt
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
        [ SNew(STextBlock)
          .Text(FText::FromString(TEXT("THE ESCAPE")))
          .Font(FCoreStyle::GetDefaultFontStyle("Bold", 64))
          .ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.35f, 0.10f, 1.0f)))
          .ShadowOffset(FVector2D(3, 3))
          .ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.6f))
        ]

        // Lower decorative double rule
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
          .Padding(0, 16, 0, 24)
        [ SNew(STextBlock)
          .Text(FText::FromString(TEXT("========================")))
          .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
          .ColorAndOpacity(FSlateColor(FLinearColor(0.40f, 0.28f, 0.10f, 0.80f)))
        ]

        // "LEVEL COMPLETE" -- Regular 24pt
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
          .Padding(0, 0, 0, 16)
        [ SNew(STextBlock)
          .Text(FText::FromString(TEXT("LEVEL COMPLETE")))
          .Font(FCoreStyle::GetDefaultFontStyle("Regular", 24))
          .ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.40f, 0.22f, 1.0f)))
        ]

        // "X / Y Souls Recovered" -- dynamic text
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
          .Padding(0, 0, 0, 32)
        [ SAssignNew(PlayerHUD.VictoryCheckpointText, STextBlock)
          .Text(FText::FromString(TEXT("0 / 0 Souls Recovered")))
          .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
          .ColorAndOpacity(FSlateColor(FLinearColor(0.40f, 0.28f, 0.10f, 0.65f)))
        ]

        // "Restarting..." -- Regular 18pt, dim
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
        [ SNew(STextBlock)
          .Text(FText::FromString(TEXT("Restarting...")))
          .Font(FCoreStyle::GetDefaultFontStyle("Regular", 18))
          .ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.40f, 0.22f, 0.60f)))
        ]
    ]
]
```

**Palette Rules:**
- GoldHighlight: `(0.55, 0.35, 0.10)` — titles, flash, checkpoint text
- GildedEdge: `(0.40, 0.28, 0.10)` — decorative rules, dim text
- WarmParchment: `(0.55, 0.40, 0.22)` — subtitles
- **NO crimson/red** — victory screen is golden/warm only (death screen uses crimson)

### Step 6: Implement ManageGameFlow

Add after `IsPlayerBlocking()` function at end of file.

**Init block:**
- Reset if world changed (`OwnerWorld != current World`)
- `TActorIterator<APointLight>` discovers lights by name pattern
- Stores location + original intensity for each checkpoint
- Attempts `StaticLoadObject` for Niagara burst FX (graceful null)

**Checkpoint collection:**
- Active -> distance < 400 -> Collecting
- Collecting 0-0.3s: `FMath::InterpEaseOut(original, original*10, alpha, 2.0f)` on PointLightComponent
  - Research says ease-out feels more professional than linear lerp
  - 10x multiplier (e.g., 3000 -> 30000) is dramatic but brief
- At 0.3s: SpawnSystemAtLocation (Niagara burst, bAutoDestroy=true) -> Destroy light -> Collected
- Increment counter, trigger flash + text

**Golden flash (0.5s):**
- 0-0.1s: alpha ramp 0 -> 0.35 (sharp in)
- 0.1-0.5s: alpha fade 0.35 -> 0 (ease-out)
- Peak alpha 0.35 (research: 0.6-0.8 is for damage, 0.3-0.4 for positive events)

**Checkpoint text (1.5s):**
- 0-0.2s: fade in (0 -> 1)
- 0.2-0.8s: hold at 1
- 0.8-1.5s: fade out (1 -> 0)
- Text: "S O U L   R E C O V E R E D   ( 2 / 3 )"

**Victory condition:**
- Distance to Portal < 500 AND !bDead
- SetVisibility(EVisibility::Visible) on VictoryOverlay
- Update VictoryCheckpointText with "X / Y Souls Recovered"
- DisableInput on PlayerController (research: disable on PC, not Pawn)
- Timer 5s -> reset all static state -> OpenLevel for clean restart

**Reset on level restart:**
```cpp
PlayerHUD = FPlayerHUDState();
GameFlow = FGameFlowState();
EnemyAIStates.Empty();
BlockingActors.Empty();
```
All four static states cleared for clean restart.

### Step 7: Rebuild Plugin

```bash
/path/to/engine/Engine/Build/BatchFiles/Linux/Build.sh bood_and_dustEditor Linux Development -Project="/path/to/bood_and_dust.uproject"
```

Must complete with 0 errors. Common issues:
- Missing `#include "EngineUtils.h"` -> TActorIterator undefined
- Missing `#include "Components/PointLightComponent.h"` -> GetPointLightComponent undefined
- Mismatched brackets in Slate tree -> count `[` and `]` carefully

### Checkpoint
> "Phase 4 complete. ManageGameFlow implemented and plugin rebuilt. Restart the editor to load new code. Press **Ctrl+S** before closing. Ready for Phase 5?"

---

## Phase 5: Wire ManageGameFlow into BP_RobotCharacter (~5 min, ~4 MCP calls)

### Goal
Add ManageGameFlow to the EventTick chain in BP_RobotCharacter, after the existing ManagePlayerHUD call.

### Step 1: Read Current Blueprint

```
read_blueprint_content(blueprint_path="/Game/Characters/Robot/BP_RobotCharacter")
```

Find the LAST node in the EventTick exec chain. Should end with ManagePlayerHUD call. Note its node ID.

### Step 2: Add ManageGameFlow Node

```
add_node(
    blueprint_path="/Game/Characters/Robot/BP_RobotCharacter",
    node_type="CallFunction",
    function_name="ManageGameFlow",
    target_class="/Script/GameplayHelpers.GameplayHelperLibrary",
    pos_x=LAST_NODE_X + 300,
    pos_y=LAST_NODE_Y
)
```
Capture `flow_node_id`.

`meta=(DefaultToSelf="Player")` auto-resolves the Player pin to self in Character BPs.

### Step 3: Wire Exec Connection

```
connect_nodes(
    blueprint_path="/Game/Characters/Robot/BP_RobotCharacter",
    source_node_id="<last_tick_node_id>",
    source_pin_name="then",
    target_node_id="<flow_node_id>",
    target_pin_name="execute"
)
```

### Step 4: Compile

```
compile_blueprint(blueprint_name="/Game/Characters/Robot/BP_RobotCharacter")
```
Must return 0 errors. If "ManageGameFlow not found" -> editor hasn't loaded the rebuilt plugin yet. Restart editor.

### Checkpoint
> "Phase 5 complete. ManageGameFlow wired into EventTick chain. Press **Ctrl+S**. Ready for verification?"

---

## Phase 6: Verification & Screenshot (~3 min, 2-3 MCP calls)

### Step 1: Screenshot
```
take_screenshot()
```

### Step 2: Visual Verification Checklist

Compare with the "before" screenshot from Phase 0:

- [ ] **Portal beacon visible** — bright golden glow at X=7500, visible from start zone
- [ ] **Start zone light** — small warm pool near player spawn
- [ ] **Breadcrumb trail** — 3 intermediate lights along the path
- [ ] **No floating lights** — all lights sit above terrain (not underground)
- [ ] **Color consistency** — all lights match golden Rembrandt palette

### Step 3: PIE Test Checklist

Tell user to test in Play-In-Editor (Alt+P):
- [ ] Walk toward first breadcrumb light -> light ramps bright -> bursts -> disappears
- [ ] Golden screen flash appears briefly (0.5s, warm gold color)
- [ ] "SOUL RECOVERED (1/3)" text fades in, holds, fades out
- [ ] Repeat for all 3 breadcrumbs
- [ ] Walk to portal -> "THE ESCAPE / LEVEL COMPLETE" victory screen appears
- [ ] "3 / 3 Souls Recovered" counter shows correct count
- [ ] Player input disabled (can't move during victory)
- [ ] Level restarts after 5 seconds
- [ ] After restart: all lights are back, checkpoint counter reset

### Edge Cases to Test
- [ ] Die before reaching portal -> death screen shows, NOT victory
- [ ] Skip checkpoints -> counter shows "0/3" at portal (still wins)
- [ ] Lights already destroyed -> WeakObjectPtr handles gracefully
- [ ] Multiple restarts -> no memory leaks (static state fully reset)

### Final Message
> "Game flow complete. The Escape objective loop is in place: dark start -> collect soul lights -> golden portal -> victory screen -> restart. Press **Ctrl+S** to save all changes."

---

## Troubleshooting

| Problem | Cause | Solution |
|---------|-------|---------|
| Portal light not visible from start | AttenuationRadius too small | Increase Portal_Beacon_01 radius to 20000+ |
| Lights underground | Wrong Z height | Re-query `get_height_at_location` and re-set transform |
| "ManageGameFlow not found" in compile | Plugin not rebuilt or editor not restarted | Rebuild plugin, restart editor |
| Checkpoints not detected | Light names don't contain "Breadcrumb_Light" | Rename lights to include pattern |
| No golden flash on collection | GoldenFlashBorder not created | Verify ManagePlayerHUD creates Slate tree first |
| Victory screen never appears | Portal light name doesn't contain "Portal_Light" | Check actor naming |
| Game doesn't restart after victory | Timer didn't fire or OpenLevel failed | Check UE_LOG for ManageGameFlow messages |
| Niagara burst missing | No NS_CheckpointBurst asset | Expected — collection still works without VFX |
| Death screen overlaps victory | Both triggered same frame | bDead check in ManageGameFlow prevents this |

## What CANNOT Be Done via MCP

| Feature | Why | Workaround |
|---------|-----|-----------|
| Niagara burst creation | spawn_niagara_system only places existing systems | Create NS_CheckpointBurst in Niagara Editor, or skip (light ramp is primary feedback) |
| Sound on collection | No audio component wiring in ManageGameFlow | Add SoundCue playback in C++ (future enhancement) |
| Per-checkpoint unique VFX | Would need different Niagara assets | Use same burst for all (consistent visual language) |
| Smooth camera transitions on victory | Requires camera blend logic | Could add in future ManageGameFlow update |

## Professional Timing Reference (From Research)

| Effect | Duration | Curve | Peak Value | Source |
|--------|----------|-------|------------|--------|
| Light Intensity Ramp | 0.3s | InterpEaseOut(exp=2) | 10x original | VFX timing research |
| Niagara Burst VFX | 0.5-0.8s | Rapid start, slow fade | 20-50 particles | Game VFX best practices |
| Screen Flash | 0.5s (0.1s in + 0.4s out) | Ease-out | Alpha 0.35 | UI/UX research |
| Text Fade In | 0.2s | Linear | Alpha 1.0 | UE Slate animation patterns |
| Text Hold | 0.6s | - | Alpha 1.0 | - |
| Text Fade Out | 0.7s | Linear | Alpha 0.0 | - |
| Victory Display | 5.0s | - | - | Game design convention |
| Total Checkpoint Feedback | ~1.5s | Layered | All above combined | - |

## Pin Names Reference (MCP Blueprint Wiring)

| Node | Key Input Pins | Key Output Pins |
|------|---------------|-----------------|
| ManageGameFlow (GameplayHelperLibrary) | execute, Player (auto self) | then |

## Timeline Reference

| Phase | Time | MCP Calls | Cumulative |
|-------|------|-----------|-----------|
| 0: Audit | 2 min | 3-5 | 2 min |
| 1: Portal Beacon | 5 min | ~9 | 7 min |
| 2: Start Zone | 3 min | ~5 | 10 min |
| 3: Breadcrumbs | 5 min | ~14 | 15 min |
| 4: C++ Implementation | 20 min | 0 (code edit + rebuild) | 35 min |
| 5: MCP Wiring | 5 min | ~4 | 40 min |
| 6: Verification | 3 min | 2-3 | 43 min |
| **Total** | **~43 min** | **~37-40 calls** | |

## Actor Inventory (Created by This Skill)

| Actor/Asset | Type | Location |
|-------------|------|----------|
| Portal_Light_01 | PointLight | X=7500, Y=0, Z=PORTAL_Z+400 |
| Portal_Beacon_01 | PointLight | X=7500, Y=0, Z=PORTAL_Z+1200 |
| Portal_FX_01 | NiagaraActor | X=7500, Y=0, Z=PORTAL_Z+200 (optional) |
| Start_Light_01 | PointLight | X=0, Y=0, Z=START_Z+200 |
| Breadcrumb_Light_01 | PointLight | X=2000, Y=200, Z=GROUND+150 |
| Breadcrumb_Light_02 | PointLight | X=4500, Y=-150, Z=GROUND+150 |
| Breadcrumb_Light_03 | PointLight | X=6000, Y=100, Z=GROUND+150 |
