---
name: blender-dev
description: Blender 3D development via BlenderMCP for scene creation, materials, assets, and procedural workflows
model: sonnet
agent: designer
argument-hint: describe what to create or modify in Blender
---

# Blender Development Skill

Use this skill for all Blender 3D work including scene creation, materials, asset management, and procedural workflows via BlenderMCP.

## Prerequisites

1. **Blender must be running** with the BlenderMCP addon enabled
2. **BlenderMCP addon connected**: In Blender's 3D View sidebar (N key) → BlenderMCP tab → Click "Connect to Claude"
3. **MCP server configured** in Claude Desktop config

## Available MCP Tools

### Scene & Object Management
- `mcp__blenderMCP__get_scene_info` - Get complete scene hierarchy and object list
- `mcp__blenderMCP__get_object_info` - Get detailed info for specific object
- `mcp__blenderMCP__execute_blender_code` - Run arbitrary Python code in Blender

### Asset Integration
- `mcp__blenderMCP__get_polyhaven_status` - Check Poly Haven connection status
- `mcp__blenderMCP__get_polyhaven_categories` - List available asset categories
- `mcp__blenderMCP__search_polyhaven_assets` - Search for HDRIs, textures, models
- `mcp__blenderMCP__download_polyhaven_asset` - Download and apply assets (2K default)
- `mcp__blenderMCP__get_sketchfab_status` - Check Sketchfab connection

### Visualization
- `mcp__blenderMCP__get_viewport_screenshot` - Capture current viewport state

## Common Workflows

### Create Object with Material
```python
# Via execute_blender_code
import bpy
bpy.ops.mesh.primitive_cube_add(location=(0, 0, 1))
obj = bpy.context.active_object
mat = bpy.data.materials.new(name="MyMaterial")
mat.use_nodes = True
obj.data.materials.append(mat)
```

### Apply Poly Haven Texture
1. Check status: `get_polyhaven_status`
2. Search: `search_polyhaven_assets` with query like "rock" or "ground"
3. Download: `download_polyhaven_asset` with asset name and resolution (1k, 2k, 4k)

### Scene Lighting Setup
```python
import bpy
# Remove default light
for obj in bpy.data.objects:
    if obj.type == 'LIGHT':
        bpy.data.objects.remove(obj)

# Add sun light
bpy.ops.object.light_add(type='SUN', location=(5, 5, 10))
sun = bpy.context.active_object
sun.data.energy = 3.0
sun.rotation_euler = (0.8, 0.2, 0.5)
```

### Apply HDRI Environment
```python
import bpy
world = bpy.context.scene.world
world.use_nodes = True
nodes = world.node_tree.nodes
links = world.node_tree.links
nodes.clear()

# Add environment texture node
env_tex = nodes.new('ShaderNodeTexEnvironment')
env_tex.image = bpy.data.images.load('/path/to/hdri.exr')
bg = nodes.new('ShaderNodeBackground')
output = nodes.new('ShaderNodeOutputWorld')
links.new(env_tex.outputs['Color'], bg.inputs['Color'])
links.new(bg.outputs['Background'], output.inputs['Surface'])
```

## Blood & Dust Visual Style

For the Rembrandt Dutch Golden Age aesthetic:

### Color Palette
- Dark base: `#2D261E` (0.176, 0.149, 0.118)
- Mid ochre: `#8C6D46` (0.549, 0.427, 0.275)
- Golden highlight: `#B37233` (0.702, 0.447, 0.200)

### Lighting Guidelines
- Use warm directional light (golden/orange tint)
- Low angle sun for dramatic shadows
- Minimal ambient light (chiaroscuro effect)
- Add volumetric fog with warm ochre tint

### Recommended Poly Haven Assets
- Textures: rocky_terrain, aerial_rocks_02, brown_mud_leaves_01, grass_dry
- HDRIs: overcast sky, sunset variations
- Models: rocks, boulders, dead vegetation

## Project Files

- Scene files: `/media/endlessblink/data/my-projects/ai-development/game-dev/ai-game-production-lecture/blender/projects/`
- Downloaded textures: `/media/endlessblink/data/my-projects/ai-development/game-dev/ai-game-production-lecture/blender/textures/`
- Handoff doc: `/media/endlessblink/data/my-projects/ai-development/game-dev/ai-game-production-lecture/blender/docs/BLENDER_HANDOFF.md`

## Troubleshooting

1. **No tools visible**: Ensure blenderMCP is in Claude Desktop config and restart Claude
2. **Connection refused**: Click "Connect to Claude" in Blender's BlenderMCP panel
3. **Timeout errors**: Break complex operations into smaller steps
4. **Asset download fails**: Check Poly Haven checkbox is enabled in Blender addon panel
