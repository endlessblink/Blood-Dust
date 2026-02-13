# Skill: Game Flow — "The Escape" Objective Loop

Set up the core game flow for Blood & Dust: a golden glowing portal beacon at the escape point, a dim warm start zone, breadcrumb path lights, HUD objective text, and a win condition trigger that displays "DEMO COMPLETE" when the player reaches the portal.

## Trigger Phrases
- "game flow"
- "escape objective"
- "demo flow"
- "portal beacon"
- "win condition"
- "reach the light"

## Prerequisites

Before running this skill, verify:
- UnrealMCP server connected (TCP 127.0.0.1:55557)
- Landscape exists in level
- BP_RobotCharacter exists at `/Game/Characters/Robot/BP_RobotCharacter`
- Player start location near X=0, Y=0

**Run `does_asset_exist` for critical assets before proceeding.**

## CRITICAL RULES

1. **ALL MCP calls strictly SEQUENTIAL** — never parallel. Each creates FTSTicker callback; parallel = crash.
2. **Breathing room** between heavy operations — call `get_actors_in_level` or `list_assets` as a lightweight pause.
3. **Max 3 spawn operations in rapid succession** — then pause with a lightweight MCP call.
4. **Prompt user to Ctrl+S** after each major phase. Unsaved packages accumulate in RAM.
5. **Take screenshot after visual phases** to verify placement.
6. **Actor names must be unique** — all names use `Portal_`, `Start_`, `Breadcrumb_`, `WBP_` prefixes.
7. **connect_nodes APPENDS connections** — never call twice on the same pin. If wrong, DELETE the node and recreate.

---

## The Emotional Arc

```
START: Dark ruins, small warm light. "Where am I?"
       ↓
LOOK UP: Distant golden glow on horizon. "I need to reach that."
       ↓
MOVE: Enemies in the way, cover to use. "Fight or run?"
       ↓
ARRIVE: Blinding golden light. Relief. "I made it."
       ↓
END: "DEMO COMPLETE"
```

---

## Phase 0: Audit (~2 min, 3-5 MCP calls)

### Step 1: Capture Current State

```
get_actors_in_level()
```
Check for any existing portal lights, breadcrumb lights, or trigger volumes. If `Portal_Light_01` already exists, skip Phase 1. If `Start_Light_01` exists, skip Phase 3.

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

If no Niagara asset exists, **skip** — the twin lights are the primary visual beacon. Niagara is enhancement only.

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
3 smaller golden lights along the X axis from start to portal. They create a "trail of light" guiding the player without being obvious.

### Breadcrumb Positions

| Light | X | Y | Intensity | Radius | Purpose |
|-------|---|---|-----------|--------|---------|
| Breadcrumb_Light_01 | 2000 | 200 | 3000 | 600 | First hint after leaving start |
| Breadcrumb_Light_02 | 4500 | -150 | 3000 | 600 | Midpoint reassurance |
| Breadcrumb_Light_03 | 6000 | 100 | 5000 | 800 | Getting close, brighter |

Y offsets are slightly varied — perfectly aligned would look artificial.

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

## Phase 4: Objective HUD Widget (~3 min, ~4 MCP calls)

### Goal
Create a "REACH THE LIGHT" text that appears on-screen at game start.

### Step 1: Create Objective Widget

```
create_widget_blueprint(
    widget_name="WBP_Objective",
    widget_path="/Game/UI/",
    elements=[
        {
            "type": "TextBlock",
            "name": "ObjectiveText",
            "position": [0, 80],
            "size": [1920, 100],
            "properties": {
                "Text": "REACH THE LIGHT",
                "ColorAndOpacity": [0.95, 0.75, 0.3, 1.0],
                "FontSize": 36
            }
        }
    ]
)
```

Position [0, 80] places the text near the top of the screen. Width 1920 spans full HD width.

**NOTE**: Widget has NO anchoring via MCP — it will look correct at 1920x1080 but may misalign at other resolutions. For production, anchor it manually in UMG Editor after creation.

### Step 2: Verify Widget Created
```
does_asset_exist(asset_path="/Game/UI/WBP_Objective")
```

### Limitations & Manual Follow-Up
- **Cannot add fade-in animation via MCP** — must add manually in UMG if desired
- **Cannot center-align text via MCP** — default is left-aligned
- **Cannot add to viewport automatically** — `add_widget_to_viewport` only works during PIE
- **For runtime display**: The widget must be added to viewport via Blueprint logic (see Phase 6 for an approach, or add it to BP_RobotCharacter's BeginPlay manually)

### Checkpoint
> "Phase 4 complete. WBP_Objective widget created with 'REACH THE LIGHT' text. Press **Ctrl+S**. Ready for Phase 5?"

---

## Phase 5: Demo Complete Widget (~3 min, ~4 MCP calls)

### Goal
Create the "DEMO COMPLETE" end screen widget.

### Step 1: Create Demo Complete Widget

```
create_widget_blueprint(
    widget_name="WBP_DemoComplete",
    widget_path="/Game/UI/",
    elements=[
        {
            "type": "TextBlock",
            "name": "TitleText",
            "position": [0, 300],
            "size": [1920, 120],
            "properties": {
                "Text": "THE ESCAPE",
                "ColorAndOpacity": [0.95, 0.80, 0.35, 1.0],
                "FontSize": 64
            }
        },
        {
            "type": "TextBlock",
            "name": "SubtitleText",
            "position": [0, 450],
            "size": [1920, 80],
            "properties": {
                "Text": "DEMO COMPLETE",
                "ColorAndOpacity": [0.8, 0.65, 0.3, 0.85],
                "FontSize": 42
            }
        }
    ]
)
```

Two text blocks: a large title and a smaller subtitle, centered vertically.

### Step 2: Verify
```
does_asset_exist(asset_path="/Game/UI/WBP_DemoComplete")
```

### Checkpoint
> "Phase 5 complete. WBP_DemoComplete widget created. Press **Ctrl+S**. Ready for Phase 6?"

---

## Phase 6: Portal Trigger Blueprint (~8 min, ~20 MCP calls)

### Goal
Create a Blueprint actor with a BoxCollision trigger volume. When the player overlaps it, display WBP_DemoComplete and pause the game.

**WHY A BLUEPRINT**: `spawn_actor` does NOT support TriggerBox. We create a custom Blueprint with a BoxCollision component instead, using ReceiveActorBeginOverlap (Actor-level overlap).

### Step 1: Create BP_PortalTrigger

```
create_blueprint(
    blueprint_name="BP_PortalTrigger",
    blueprint_path="/Game/Gameplay/",
    parent_class="Actor"
)
```

### Step 2: Add BoxCollision Component

```
add_component_to_blueprint(
    blueprint_path="/Game/Gameplay/BP_PortalTrigger",
    component_type="BoxCollision",
    component_name="TriggerZone"
)
```

This adds a UBoxComponent that generates overlap events.

### Step 3: Compile Initial Blueprint

```
compile_blueprint(blueprint_name="/Game/Gameplay/BP_PortalTrigger")
```
Must return 0 errors before adding nodes.

### Step 4: Add ReceiveActorBeginOverlap Event

```
add_event_node(
    blueprint_path="/Game/Gameplay/BP_PortalTrigger",
    event_name="ReceiveActorBeginOverlap",
    pos_x=0,
    pos_y=0
)
```
Capture `overlap_node_id` from response.

### Step 5: Add CreateWidget Node

```
add_node(
    blueprint_path="/Game/Gameplay/BP_PortalTrigger",
    node_type="CallFunction",
    function_name="CreateWidget",
    target_class="/Script/UMG.WidgetBlueprintLibrary",
    pos_x=300,
    pos_y=0
)
```
Capture `widget_node_id`.

### Step 6: Wire Overlap → CreateWidget (exec chain)

```
connect_nodes(
    blueprint_path="/Game/Gameplay/BP_PortalTrigger",
    source_node_id="<overlap_node_id>",
    source_pin_name="then",
    target_node_id="<widget_node_id>",
    target_pin_name="execute"
)
```

### Step 7: Set WidgetType Pin Default

The CreateWidget node needs its `WidgetType` pin set to WBP_DemoComplete:
```
set_node_property(
    blueprint_name="/Game/Gameplay/BP_PortalTrigger",
    node_id="<widget_node_id>",
    action="set_pin_default",
    pin_name="WidgetType",
    default_value="/Game/UI/WBP_DemoComplete.WBP_DemoComplete_C"
)
```

**NOTE**: The class path uses `_C` suffix (Blueprint Generated Class).

### Step 8: Add AddToViewport Node

```
add_node(
    blueprint_path="/Game/Gameplay/BP_PortalTrigger",
    node_type="CallFunction",
    function_name="AddToViewport",
    pos_x=600,
    pos_y=0
)
```
Capture `viewport_node_id`.

### Step 9: Wire CreateWidget → AddToViewport

Exec chain:
```
connect_nodes(
    blueprint_path="/Game/Gameplay/BP_PortalTrigger",
    source_node_id="<widget_node_id>",
    source_pin_name="then",
    target_node_id="<viewport_node_id>",
    target_pin_name="execute"
)
```

Widget instance connection:
```
connect_nodes(
    blueprint_path="/Game/Gameplay/BP_PortalTrigger",
    source_node_id="<widget_node_id>",
    source_pin_name="ReturnValue",
    target_node_id="<viewport_node_id>",
    target_pin_name="self"
)
```

### Step 10: Add SetGamePaused Node

```
add_node(
    blueprint_path="/Game/Gameplay/BP_PortalTrigger",
    node_type="CallFunction",
    function_name="SetGamePaused",
    target_class="/Script/Engine.GameplayStatics",
    pos_x=900,
    pos_y=0
)
```
Capture `pause_node_id`.

### Step 11: Wire AddToViewport → SetGamePaused

```
connect_nodes(
    blueprint_path="/Game/Gameplay/BP_PortalTrigger",
    source_node_id="<viewport_node_id>",
    source_pin_name="then",
    target_node_id="<pause_node_id>",
    target_pin_name="execute"
)
```

### Step 12: Set bPaused Pin to True

```
set_node_property(
    blueprint_name="/Game/Gameplay/BP_PortalTrigger",
    node_id="<pause_node_id>",
    action="set_pin_default",
    pin_name="bPaused",
    default_value="true"
)
```

### Step 13: Final Compile

```
compile_blueprint(blueprint_name="/Game/Gameplay/BP_PortalTrigger")
```
**MUST return**: `compiled: true`, `num_errors: 0`

If errors: run `analyze_blueprint_graph` to inspect node connections. Common issues:
- Wrong pin names → check with `read_blueprint_content`
- Missing WidgetType default → set_node_property may need full class path with `_C` suffix

### Step 14: Spawn BP_PortalTrigger in Level

```
spawn_blueprint_actor_in_level(
    blueprint_path="/Game/Gameplay/BP_PortalTrigger",
    actor_name="PortalTrigger_01",
    location=[7500, 0, PORTAL_Z + 200],
    rotation=[0, 0, 0],
    scale=[5, 5, 4]
)
```
Scale [5,5,4] creates a ~500x500x400 unit trigger zone centered at the portal.

### Fallback: If Blueprint Wiring Fails

Blueprint graph wiring via MCP can be fragile. If compile fails or nodes don't connect properly:

1. **Skip the wiring steps** (Steps 4-12)
2. **Just create the Blueprint with the BoxCollision component** (Steps 1-3)
3. **Spawn it in the level** (Step 14)
4. **Tell the user**: "Open BP_PortalTrigger in the Blueprint Editor. Add an ActorBeginOverlap event → CreateWidget (class: WBP_DemoComplete) → AddToViewport → Set Game Paused (true). This takes ~30 seconds manually."

This hybrid approach guarantees the visual elements work even if the programmatic wiring fails.

### Checkpoint
> "Phase 6 complete. Portal trigger Blueprint created and placed at X=7500. Press **Ctrl+S**. Ready for Phase 7?"

---

## Phase 7: HUD Display Setup (~3 min, ~6 MCP calls)

### Goal
Wire the objective HUD ("REACH THE LIGHT") to display when the game starts. This requires adding widget creation to BP_RobotCharacter's BeginPlay chain.

### Approach A: Add to Character BeginPlay (via MCP)

Read existing BeginPlay chain:
```
read_blueprint_content(blueprint_path="/Game/Characters/Robot/BP_RobotCharacter")
```

Find the LAST node in the BeginPlay exec chain. Then:

```
add_node(
    blueprint_path="/Game/Characters/Robot/BP_RobotCharacter",
    node_type="CallFunction",
    function_name="CreateWidget",
    target_class="/Script/UMG.WidgetBlueprintLibrary",
    pos_x=1200,
    pos_y=0
)
```
Capture `hud_widget_id`.

```
set_node_property(
    blueprint_name="/Game/Characters/Robot/BP_RobotCharacter",
    node_id="<hud_widget_id>",
    action="set_pin_default",
    pin_name="WidgetType",
    default_value="/Game/UI/WBP_Objective.WBP_Objective_C"
)
```

```
add_node(
    blueprint_path="/Game/Characters/Robot/BP_RobotCharacter",
    node_type="CallFunction",
    function_name="AddToViewport",
    pos_x=1500,
    pos_y=0
)
```
Capture `hud_viewport_id`.

Wire exec chain from last BeginPlay node → CreateWidget → AddToViewport, plus ReturnValue → self.

```
compile_blueprint(blueprint_name="/Game/Characters/Robot/BP_RobotCharacter")
```

### Approach B: Manual (Fallback)

Tell user: "In BP_RobotCharacter → BeginPlay, after the IMC setup nodes, add: CreateWidget (class: WBP_Objective) → AddToViewport. This shows 'REACH THE LIGHT' when the game starts."

### Checkpoint
> "Phase 7 complete. Objective HUD wired to display on game start. Press **Ctrl+S**. Ready for final verification?"

---

## Phase 8: Verification & Screenshot (~2 min, 2-3 MCP calls)

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
- [ ] "REACH THE LIGHT" text appears on screen at start
- [ ] Golden portal glow visible in the distance
- [ ] Breadcrumb lights visible along the path
- [ ] Walking into portal trigger zone shows "DEMO COMPLETE"
- [ ] Game pauses after "DEMO COMPLETE" appears

### Final Message
> "Game flow complete. The Escape objective loop is in place: dark start → trail of light → golden portal → DEMO COMPLETE. Press **Ctrl+S** to save all changes."

---

## Troubleshooting

| Problem | Cause | Solution |
|---------|-------|---------|
| Portal light not visible from start | AttenuationRadius too small | Increase Portal_Beacon_01 radius to 20000+ |
| Lights underground | Wrong Z height | Re-query `get_height_at_location` and re-set transform |
| Widget doesn't appear in PIE | CreateWidget not wired in BeginPlay | Check BP_RobotCharacter graph for widget nodes |
| "DEMO COMPLETE" never shows | Trigger volume too small or wrong location | Increase scale to [8,8,6], verify location matches portal |
| Game doesn't pause | SetGamePaused not wired or bPaused=false | Check BP_PortalTrigger graph, verify bPaused default |
| Blueprint compile errors | Wrong pin names or missing connections | Run `analyze_blueprint_graph`, use fallback manual approach |
| Niagara FX missing | No NS_Portal_Glow asset | Expected — skip Niagara, lights are primary beacon |

## What CANNOT Be Done via MCP

| Feature | Why | Workaround |
|---------|-----|-----------|
| Widget anchoring/alignment | MCP widget tools don't expose anchor settings | Edit in UMG Designer after creation |
| Text fade-in animation | No animation support in widget tools | Add UMG animation manually |
| Component-specific overlap | Only ReceiveActorBeginOverlap, no OnComponentBeginOverlap | Actor-level overlap is sufficient for demo |
| Niagara creation | spawn_niagara_system only places existing systems | Create in Niagara Editor, then place via MCP |
| Widget runtime updates | No widget instance reference after AddToViewport | Add Blueprint logic for dynamic text changes |

## Pin Names Reference

| Node | Key Input Pins | Key Output Pins |
|------|---------------|-----------------|
| ReceiveActorBeginOverlap | (event, no input exec) | then, OtherActor |
| CreateWidget (WidgetBlueprintLibrary) | execute, OwningPlayer, WidgetType | then, ReturnValue (UUserWidget) |
| AddToViewport | execute, self (UUserWidget), ZOrder | then |
| SetGamePaused (GameplayStatics) | execute, bPaused | then, ReturnValue (bool) |

## Timeline Reference

| Phase | Time | MCP Calls | Cumulative |
|-------|------|-----------|-----------|
| 0: Audit | 2 min | 3-5 | 2 min |
| 1: Portal Beacon | 5 min | ~9 | 7 min |
| 2: Start Zone | 3 min | ~5 | 10 min |
| 3: Breadcrumbs | 5 min | ~14 | 15 min |
| 4: Objective Widget | 3 min | ~4 | 18 min |
| 5: Demo Complete Widget | 3 min | ~4 | 21 min |
| 6: Portal Trigger BP | 8 min | ~20 | 29 min |
| 7: HUD Display | 3 min | ~6 | 32 min |
| 8: Verification | 2 min | 2-3 | 34 min |
| **Total** | **~34 min** | **~65-70 calls** | |

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
| WBP_Objective | Widget Blueprint | /Game/UI/ |
| WBP_DemoComplete | Widget Blueprint | /Game/UI/ |
| BP_PortalTrigger | Blueprint Actor | /Game/Gameplay/ |
| PortalTrigger_01 | BP_PortalTrigger instance | X=7500, Y=0, Z=PORTAL_Z+200 |
