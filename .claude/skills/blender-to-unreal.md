# Blender to Unreal Import Pipeline

## Overview
Automated pipeline for transferring Blender scenes to Unreal Engine 5.7, preserving geometry, materials, textures, and placement as closely as possible.

## Trigger Phrases
- "import from blender"
- "blender to unreal"
- "transfer scene"
- "import blender scene"

## Prerequisites
- BlenderMCP server running (Blender open with addon enabled)
- UnrealMCP server running (Unreal Editor open with plugin active)
- Both MCP servers connected

## CRITICAL: MCP Call Safety Rules

**These rules MUST be followed to prevent Unreal Editor crashes:**

1. **NEVER call MCP tools in parallel** — each call creates an FTSTicker callback in Unreal. Multiple callbacks can fire in the same frame, causing overlapping actor operations and crashes.
2. **Always call MCP tools strictly sequentially** — wait for each response before making the next call.
3. **NEVER batch heavy operations in a single MCP command** — e.g., spawning 9 actors in one call exceeds the frame budget and crashes.
4. **Separate delete and spawn operations** — `DestroyActor` marks actors as pending-kill but GC hasn't cleaned up yet. Wait between deletes and spawns.
5. **One import_mesh at a time** — FBX import is especially heavy (Nanite building uses TaskGraph internally).

## Pipeline Steps

### Step 1: Scene Inventory
Use BlenderMCP to catalog the current Blender scene:
1. Call `get_scene_info` to get all objects and material count
2. Call `get_object_info` for each mesh object to get position, rotation, scale, material, vertex count, bounding box
3. Call `get_viewport_screenshot` for visual reference

### Step 2: Check Existing FBX Exports
Look for pre-exported FBX files in known locations:
- `blender/models/*/fbx/` - Per-model FBX exports
- `blender/asset-library/` - Asset library exports
- `blender/exports/` - General export directory

Verify FBX files are valid (check file size > 1KB - corrupt files are often ~94 bytes).

### Step 3: Export Missing Meshes from Blender
For any mesh without an FBX export, use `execute_blender_code`:

```python
import bpy

bpy.ops.object.select_all(action='DESELECT')
obj = bpy.data.objects['object_name']
obj.select_set(True)
bpy.context.view_layer.objects.active = obj

bpy.ops.export_scene.fbx(
    filepath='/path/to/output.fbx',
    use_selection=True,
    apply_scale_options='FBX_SCALE_ALL',
    global_scale=1.0,
    apply_unit_scale=True,
    axis_forward='-Y',
    axis_up='Z',
    object_types={'MESH'},
    use_mesh_modifiers=True,
    mesh_smooth_type='FACE',
    use_tspace=True,
    bake_anim=False,
    embed_textures=False,
    batch_mode='OFF',
)
```

### Step 4: Import Textures to Unreal
For each material, import texture maps using `import_texture`.

**Naming convention**: `T_{AssetName}_{Channel}`
| Suffix | Channel |
|--------|---------|
| `_D` | Diffuse/Base Color |
| `_N` | Normal Map |
| `_R` | Roughness |
| `_M` | Metallic |
| `_AO` | Ambient Occlusion |
| `_ARM` | AO/Roughness/Metallic packed |
| `_Disp` | Displacement |

**Normal map handling**: Blender uses OpenGL normals (`*_nor_gl`). Unreal uses DirectX convention - flip the green channel. Options:
- Check "Flip Green Channel" on texture import in Unreal
- Create material that negates the G channel before Normal output

**ARM texture handling**: Poly Haven textures pack AO(R)/Roughness(G)/Metallic(B). Import as-is, use channel masks in material.

**Destination**: `/Game/Textures/BlenderImport/{AssetName}/`

### Step 5: Import Meshes to Unreal
For each FBX, use `import_mesh`:

```
import_mesh(
    source_path="/full/path/to/mesh.fbx",
    asset_name="SM_{MeshName}",
    destination_path="/Game/Meshes/BlenderImport/",
    import_materials=False,
    import_textures=False,
    generate_collision=True,
    enable_nanite=True,
    combine_meshes=True
)
```

**Naming**: `SM_{DescriptiveName}` for static meshes.

### Step 6: Create Materials in Unreal
For each Blender material, create an Unreal Material Instance.

1. **Master material** `M_BlenderPBR_Master` (create once if missing):
   - TextureSampleParameter2D nodes for BaseColor, Normal, Roughness, AO
   - ScalarParameter for RoughnessMultiplier, NormalIntensity
   - VectorParameter for ColorTint (for Rembrandt style)
   - Connect to material outputs

2. **Per-mesh Material Instances** `MI_{MaterialName}`:
   - `create_material_instance` with parent `M_BlenderPBR_Master`
   - `set_material_instance_parameter` to assign textures and scalars

3. **Apply** with `apply_material_to_actor`

### Step 7: Place Actors in Unreal Level

**Coordinate Conversion (Blender to Unreal)**:
```
Unreal_X =  Blender_Y * 100
Unreal_Y = -Blender_X * 100
```
Scale: Blender meters x 100 = Unreal centimeters
Rotation: Blender radians to Unreal degrees (rad * 180/pi)

**Z Height — DO NOT convert from Blender.** Instead, query the Unreal landscape:

1. Calculate target XY using coordinate conversion above
2. Call `get_height_at_location(x, y)` to get the landscape surface Z
3. Call `spawn_actor` with `location=[X, Y, terrain_Z]`
4. Or: spawn at any Z, then call `snap_actor_to_ground(actor_name)`
5. Apply materials with `apply_material_to_actor`

**If placing near landscape edge**, verify positions are within landscape bounds first using `get_landscape_info`.

**IMPORTANT: Place actors ONE AT A TIME.** Call spawn_actor, wait for response, then call the next. Never parallel.

### Step 8: Verify Import
1. `get_actors_in_level` to confirm all actors placed
2. `get_asset_info` on imported meshes to verify vertex counts
3. Visual comparison via Unreal viewport

## Special Handling

### Terrain
Options:
- **Static mesh import** (preserves exact Blender geometry)
- **Unreal Landscape** (better for gameplay, supports layer painting)

For Blood & Dust, the Unreal Landscape already exists - match elevation from Blender.

### High-Poly Meshes (>50K vertices)
Enable Nanite on import (`enable_nanite=True`). RTX 4070 Ti handles Nanite well.

### Rembrandt Visual Style
After import, ensure consistency with the Dutch Golden Age aesthetic:
- Warm ochre tint (ColorTint towards `#8C6D46`)
- High roughness (0.85+) to reduce specularity for oil painting feel
- Muted earth tone desaturation
- Existing PostProcessVolume handles color grading

## Troubleshooting

| Issue | Solution |
|-------|----------|
| **Unreal crashes on ANY MCP call** | **NEVER call MCP tools in parallel.** Each creates an FTSTicker callback — multiple fire in same frame → crash. Always sequential, one at a time. |
| **Unreal crashes on batch operation** | **NEVER do multiple spawns/deletes in a single C++ command.** Spread across separate MCP calls, one per tick. |
| **Unreal crashes on import** | **NEVER call import_mesh in parallel** — FBX import + Nanite is heavy, must be sequential. |
| **Rocks below terrain** | Don't use Blender Z. Use `get_height_at_location(x, y)` to query landscape surface Z, or `snap_actor_to_ground(name)` after placing. |
| **Rocks outside landscape** | Check landscape bounds with `get_landscape_info`. Use relative offsets from a center point on the landscape instead of absolute Blender coordinates. |
| FBX import fails | Verify file is valid FBX (not corrupt), check path |
| Wrong scale | Ensure FBX exported with `apply_unit_scale=True` |
| Normals look wrong | Flip green channel on normal map textures |
| Materials missing | Materials imported separately - check Step 6 |
| Corrupt FBX file (94 bytes) | Re-export from Blender with correct settings |

## Available MCP Tools

### BlenderMCP
- `get_scene_info` - Scene inventory
- `get_object_info` - Object details
- `get_viewport_screenshot` - Visual reference
- `execute_blender_code` - Run Blender Python for exports

### UnrealMCP
- `import_mesh` - Import FBX/OBJ as StaticMesh with Nanite
- `import_texture` - Import texture files
- `list_assets` - Browse content browser
- `does_asset_exist` - Check asset existence
- `get_asset_info` - Get asset details (verts, materials, bounds)
- `create_material` / `create_material_instance` - Material setup
- `set_material_instance_parameter` - Set texture/scalar params
- `apply_material_to_actor` - Assign material to actor
- `set_actor_transform` - Position/rotate/scale actors
- `spawn_actor` / `spawn_blueprint_actor` - Place actors
- `get_height_at_location` - Query landscape surface Z at any XY position
- `snap_actor_to_ground` - Snap existing actor to terrain below it
- `get_landscape_info` - Get landscape bounds, scale, materials, layers
- `delete_actor` - Remove actor by name

## Blood & Dust Project Specifics

**Texture sources**: `blender/textures/`, `blender/models/boulders-stones/*/textures/`
**FBX sources**: `blender/models/boulders-stones/fbx/`
**Unreal destination**: `/Game/Meshes/BlenderImport/`, `/Game/Textures/BlenderImport/`
**Color palette**: Dark `#2D261E`, Mid ochre `#8C6D46`, Golden `#B37233`
