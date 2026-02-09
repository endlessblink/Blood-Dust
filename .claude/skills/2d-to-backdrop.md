# 2D to 3D Backdrop Skill

Create high-quality 3D backdrops from 2D images in Blender using AI depth estimation, camera projection, and displacement techniques.

## When to Use
- Converting painted backdrop images to 3D parallax environments
- Creating skydomes/skyboxes from 2D artwork
- Building matte painting setups for game environments
- Adding depth and parallax to flat backdrop images
- Creating Rembrandt/painterly atmospheric environments

## Quick Reference

| Method | Best For | Speed | Quality |
|--------|----------|-------|---------|
| Simple Emission Plane | Fast skybox, no parallax | Instant | Low |
| AI Depth + Displacement | Parallax 2.5D backdrops | 5-30s | High |
| Camera Projection | Matte painting workflows | Medium | High |
| Multi-Layer Parallax | Full environment depth | Medium | Highest |

## Method 1: Simple Emission Backdrop (No Parallax)

Quick backdrop plane with emission shader (no lighting needed).

```python
import bpy
import math

def create_simple_backdrop(
    image_path: str,
    name: str = "Backdrop",
    distance: float = 100.0,
    direction: str = "north"  # north, south, east, west, sky
):
    """Create simple emission-based backdrop plane"""

    # Load image to get dimensions
    img = bpy.data.images.load(image_path, check_existing=True)
    aspect = img.size[0] / img.size[1] if img.size[1] > 0 else 1.78

    # Calculate plane size (height based on distance for reasonable coverage)
    height = distance * 0.5  # 50% of distance = ~27 degree vertical FOV
    width = height * aspect

    # Direction configurations
    configs = {
        "north": {"loc": (0, distance, height/2), "rot": (math.pi/2, 0, 0)},
        "south": {"loc": (0, -distance, height/2), "rot": (math.pi/2, 0, math.pi)},
        "east":  {"loc": (distance, 0, height/2), "rot": (math.pi/2, 0, -math.pi/2)},
        "west":  {"loc": (-distance, 0, height/2), "rot": (math.pi/2, 0, math.pi/2)},
        "sky":   {"loc": (0, 0, distance), "rot": (0, 0, 0)}
    }

    cfg = configs.get(direction, configs["north"])

    # Create plane
    bpy.ops.mesh.primitive_plane_add(size=1, location=cfg["loc"], rotation=cfg["rot"])
    plane = bpy.context.active_object
    plane.name = f"{name}_{direction}"
    plane.scale = (width, height, 1)

    # Create emission material
    mat = bpy.data.materials.new(name=f"M_{name}_{direction}")
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()

    # Texture coordinate -> Image -> Emission -> Output
    tex_coord = nodes.new('ShaderNodeTexCoord')
    tex_coord.location = (-600, 0)

    tex_image = nodes.new('ShaderNodeTexImage')
    tex_image.image = img
    tex_image.extension = 'EXTEND'  # Prevent edge wrapping
    tex_image.location = (-300, 0)

    emission = nodes.new('ShaderNodeEmission')
    emission.inputs['Strength'].default_value = 1.0
    emission.location = (0, 0)

    output = nodes.new('ShaderNodeOutputMaterial')
    output.location = (300, 0)

    links.new(tex_coord.outputs['UV'], tex_image.inputs['Vector'])
    links.new(tex_image.outputs['Color'], emission.inputs['Color'])
    links.new(emission.outputs['Emission'], output.inputs['Surface'])

    plane.data.materials.append(mat)

    # Disable shadow casting
    plane.visible_shadow = False

    return plane
```

## Method 2: AI Depth + Displacement (Recommended)

Creates 2.5D parallax backdrop using AI-generated depth maps.

### Step 1: Generate Depth Map

**Option A: Marigold (Best for Painterly/Artistic Images)**
```bash
# Install dependencies
pip install diffusers transformers torch

# Python script
python -c "
from diffusers import MarigoldDepthPipeline
import torch
from PIL import Image

pipe = MarigoldDepthPipeline.from_pretrained(
    'prs-eth/marigold-depth-v1-0',
    torch_dtype=torch.float16
).to('cuda')

image = Image.open('/path/to/backdrop.png')
depth = pipe(image, num_inference_steps=50)
depth.save('/path/to/backdrop_depth.png')
"
```

**Option B: Depth Anything V2 (10x Faster)**
```bash
pip install transformers torch

python -c "
from transformers import pipeline
pipe = pipeline('depth-estimation', model='depth-anything/Depth-Anything-V2-Large', device=0)
result = pipe('/path/to/backdrop.png')
result['depth'].save('/path/to/backdrop_depth.png')
"
```

### Step 2: Create Displaced Backdrop in Blender

```python
import bpy
import math

def create_depth_backdrop(
    image_path: str,
    depth_path: str,
    name: str = "Backdrop",
    distance: float = 100.0,
    displacement_strength: float = 10.0,
    subdivision_levels: int = 6,
    direction: str = "north"
):
    """Create 2.5D backdrop with AI depth displacement"""

    # Load images
    img_color = bpy.data.images.load(image_path, check_existing=True)
    img_depth = bpy.data.images.load(depth_path, check_existing=True)
    img_depth.colorspace_settings.name = 'Non-Color'  # Critical for depth

    aspect = img_color.size[0] / img_color.size[1] if img_color.size[1] > 0 else 1.78
    height = distance * 0.5
    width = height * aspect

    # Direction configurations
    configs = {
        "north": {"loc": (0, distance, height/2), "rot": (math.pi/2, 0, 0)},
        "south": {"loc": (0, -distance, height/2), "rot": (math.pi/2, 0, math.pi)},
        "east":  {"loc": (distance, 0, height/2), "rot": (math.pi/2, 0, -math.pi/2)},
        "west":  {"loc": (-distance, 0, height/2), "rot": (math.pi/2, 0, math.pi/2)},
    }

    cfg = configs.get(direction, configs["north"])

    # Create high-poly plane
    bpy.ops.mesh.primitive_plane_add(size=1, location=cfg["loc"], rotation=cfg["rot"])
    plane = bpy.context.active_object
    plane.name = f"{name}_{direction}"
    plane.scale = (width, height, 1)

    # Add subdivision for displacement detail
    subdiv = plane.modifiers.new(name="Subdivision", type='SUBSURF')
    subdiv.subdivision_type = 'SIMPLE'
    subdiv.levels = subdivision_levels
    subdiv.render_levels = subdivision_levels + 1

    # Create material with displacement
    mat = bpy.data.materials.new(name=f"M_{name}_{direction}")
    mat.use_nodes = True
    mat.cycles.displacement_method = 'BOTH'  # Enable true displacement
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()

    # === Color Path ===
    tex_coord = nodes.new('ShaderNodeTexCoord')
    tex_coord.location = (-800, 200)

    tex_color = nodes.new('ShaderNodeTexImage')
    tex_color.image = img_color
    tex_color.extension = 'EXTEND'
    tex_color.location = (-400, 200)

    emission = nodes.new('ShaderNodeEmission')
    emission.inputs['Strength'].default_value = 1.0
    emission.location = (0, 200)

    # === Depth/Displacement Path ===
    tex_depth = nodes.new('ShaderNodeTexImage')
    tex_depth.image = img_depth
    tex_depth.extension = 'EXTEND'
    tex_depth.location = (-400, -200)

    # Invert depth if needed (white = far in some depth maps)
    invert = nodes.new('ShaderNodeInvert')
    invert.location = (-100, -200)

    displacement = nodes.new('ShaderNodeDisplacement')
    displacement.inputs['Scale'].default_value = displacement_strength
    displacement.inputs['Midlevel'].default_value = 0.5
    displacement.location = (200, -200)

    # === Output ===
    output = nodes.new('ShaderNodeOutputMaterial')
    output.location = (500, 0)

    # Connect nodes
    links.new(tex_coord.outputs['UV'], tex_color.inputs['Vector'])
    links.new(tex_coord.outputs['UV'], tex_depth.inputs['Vector'])
    links.new(tex_color.outputs['Color'], emission.inputs['Color'])
    links.new(tex_depth.outputs['Color'], invert.inputs['Color'])
    links.new(invert.outputs['Color'], displacement.inputs['Height'])
    links.new(emission.outputs['Emission'], output.inputs['Surface'])
    links.new(displacement.outputs['Displacement'], output.inputs['Displacement'])

    plane.data.materials.append(mat)
    plane.visible_shadow = False

    return plane
```

## Method 3: Camera Projection (Matte Painting)

Projects image from camera viewpoint onto geometry for matte painting workflows.

```python
import bpy
import math

def create_projected_backdrop(
    image_path: str,
    camera_name: str = "Camera",
    name: str = "MatteBackdrop"
):
    """Create camera-projected backdrop for matte painting"""

    camera = bpy.data.objects.get(camera_name)
    if not camera:
        raise ValueError(f"Camera '{camera_name}' not found")

    # Load image
    img = bpy.data.images.load(image_path, check_existing=True)
    aspect = img.size[0] / img.size[1] if img.size[1] > 0 else 1.78

    # Create geometry in camera's view
    # Position plane at a reasonable distance from camera
    distance = 50.0

    # Get camera direction
    cam_matrix = camera.matrix_world
    cam_loc = cam_matrix.translation
    cam_dir = cam_matrix.to_quaternion() @ mathutils.Vector((0, 0, -1))

    plane_loc = cam_loc + cam_dir * distance

    # Create subdivided plane
    bpy.ops.mesh.primitive_plane_add(size=distance * 0.8, location=plane_loc)
    plane = bpy.context.active_object
    plane.name = name

    # Orient to face camera
    direction = cam_loc - plane_loc
    plane.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()

    # Subdivide for projection detail
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.subdivide(number_cuts=30)
    bpy.ops.object.mode_set(mode='OBJECT')

    # Add UV Project modifier
    uv_proj = plane.modifiers.new(name="CameraProject", type='UV_PROJECT')
    uv_proj.projector_count = 1
    uv_proj.projectors[0].object = camera
    uv_proj.uv_layer = "UVMap"

    # Create material
    mat = bpy.data.materials.new(name=f"M_{name}")
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()

    tex_coord = nodes.new('ShaderNodeTexCoord')
    tex_coord.location = (-400, 0)

    tex_image = nodes.new('ShaderNodeTexImage')
    tex_image.image = img
    tex_image.extension = 'EXTEND'
    tex_image.location = (-100, 0)

    emission = nodes.new('ShaderNodeEmission')
    emission.location = (200, 0)

    output = nodes.new('ShaderNodeOutputMaterial')
    output.location = (500, 0)

    links.new(tex_coord.outputs['UV'], tex_image.inputs['Vector'])
    links.new(tex_image.outputs['Color'], emission.inputs['Color'])
    links.new(emission.outputs['Emission'], output.inputs['Surface'])

    plane.data.materials.append(mat)

    return plane
```

## Method 4: Multi-Layer Parallax Environment

Creates multiple backdrop layers at different distances for full parallax depth.

```python
import bpy
import math

def create_parallax_environment(
    layer_configs: list,
    name: str = "ParallaxEnv"
):
    """
    Create multi-layer parallax environment.

    layer_configs = [
        {"image": "/path/to/distant.png", "depth": "/path/to/distant_depth.png",
         "distance": 500, "displacement": 20},
        {"image": "/path/to/mid.png", "depth": "/path/to/mid_depth.png",
         "distance": 200, "displacement": 15},
        {"image": "/path/to/near.png", "depth": "/path/to/near_depth.png",
         "distance": 80, "displacement": 8},
    ]
    """

    collection = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(collection)

    layers = []
    for i, config in enumerate(layer_configs):
        layer = create_depth_backdrop(
            image_path=config["image"],
            depth_path=config["depth"],
            name=f"{name}_Layer{i}",
            distance=config["distance"],
            displacement_strength=config.get("displacement", 10),
            subdivision_levels=config.get("subdivisions", 5),
            direction=config.get("direction", "north")
        )

        # Move to collection
        for coll in layer.users_collection:
            coll.objects.unlink(layer)
        collection.objects.link(layer)

        layers.append(layer)

    return layers
```

## Method 5: Hemisphere Skydome

Creates an inverted hemisphere for panoramic sky backdrops.

```python
import bpy
import math

def create_skydome(
    image_path: str,
    radius: float = 500.0,
    name: str = "Skydome"
):
    """Create hemisphere skydome with image texture"""

    # Create UV sphere
    bpy.ops.mesh.primitive_uv_sphere_add(
        radius=radius,
        segments=64,
        ring_count=32,
        location=(0, 0, 0)
    )
    dome = bpy.context.active_object
    dome.name = name

    # Delete bottom half
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='DESELECT')

    # Select vertices below z=0
    bpy.ops.object.mode_set(mode='OBJECT')
    mesh = dome.data
    for v in mesh.vertices:
        v.select = v.co.z < 0

    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.delete(type='VERT')

    # Flip normals inward
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.flip_normals()
    bpy.ops.object.mode_set(mode='OBJECT')

    # Load image
    img = bpy.data.images.load(image_path, check_existing=True)

    # Create material
    mat = bpy.data.materials.new(name=f"M_{name}")
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()

    tex_coord = nodes.new('ShaderNodeTexCoord')
    tex_coord.location = (-600, 0)

    # Use spherical projection
    mapping = nodes.new('ShaderNodeMapping')
    mapping.location = (-400, 0)

    tex_image = nodes.new('ShaderNodeTexImage')
    tex_image.image = img
    tex_image.projection = 'SPHERE'
    tex_image.location = (-100, 0)

    emission = nodes.new('ShaderNodeEmission')
    emission.inputs['Strength'].default_value = 1.0
    emission.location = (200, 0)

    output = nodes.new('ShaderNodeOutputMaterial')
    output.location = (500, 0)

    links.new(tex_coord.outputs['Generated'], mapping.inputs['Vector'])
    links.new(mapping.outputs['Vector'], tex_image.inputs['Vector'])
    links.new(tex_image.outputs['Color'], emission.inputs['Color'])
    links.new(emission.outputs['Emission'], output.inputs['Surface'])

    dome.data.materials.append(mat)
    dome.visible_shadow = False

    return dome
```

## Rembrandt/Dutch Golden Age Style Settings

For the Blood & Dust project aesthetic:

```python
# Color palette
REMBRANDT_PALETTE = {
    'dark': (0.176, 0.149, 0.118, 1.0),      # #2D261E
    'mid_ochre': (0.549, 0.427, 0.275, 1.0), # #8C6D46
    'golden': (0.702, 0.447, 0.200, 1.0)     # #B37233
}

def add_rembrandt_color_grade(material):
    """Add warm ochre color grading to backdrop material"""
    nodes = material.node_tree.nodes
    links = material.node_tree.links

    # Find emission node
    emission = None
    for node in nodes:
        if node.type == 'EMISSION':
            emission = node
            break

    if not emission:
        return

    # Insert color grade between texture and emission
    color_input = None
    for link in material.node_tree.links:
        if link.to_node == emission and link.to_socket.name == 'Color':
            color_input = link.from_socket
            links.remove(link)
            break

    if not color_input:
        return

    # Add HSV adjustment
    hsv = nodes.new('ShaderNodeHueSaturation')
    hsv.location = (emission.location.x - 200, emission.location.y - 100)
    hsv.inputs['Hue'].default_value = 0.08  # Slight warm shift
    hsv.inputs['Saturation'].default_value = 0.85  # Slightly muted
    hsv.inputs['Value'].default_value = 0.9  # Slight darken

    # Add warm overlay
    mix = nodes.new('ShaderNodeMixRGB')
    mix.blend_type = 'OVERLAY'
    mix.inputs['Fac'].default_value = 0.15
    mix.inputs['Color2'].default_value = REMBRANDT_PALETTE['mid_ochre']
    mix.location = (emission.location.x - 200, emission.location.y)

    links.new(color_input, hsv.inputs['Color'])
    links.new(hsv.outputs['Color'], mix.inputs['Color1'])
    links.new(mix.outputs['Color'], emission.inputs['Color'])
```

## Batch Processing All Cardinals

Process all 4 cardinal direction backdrops at once:

```python
import bpy
import os

def create_cardinal_backdrops(
    backdrop_dir: str,
    depth_dir: str = None,
    distance: float = 100.0,
    displacement: float = 10.0
):
    """Create backdrops for all cardinal directions"""

    directions = {
        'north': 'backdrop_n.png',
        'south': 'backdrop_s.png',
        'east': 'backdrop_e.png',
        'west': 'backdrop_w.png'
    }

    backdrops = []

    for direction, filename in directions.items():
        image_path = os.path.join(backdrop_dir, filename)

        if not os.path.exists(image_path):
            print(f"Skipping {direction}: {image_path} not found")
            continue

        # Check for depth map
        depth_filename = filename.replace('.png', '_depth.png')
        depth_path = os.path.join(depth_dir or backdrop_dir, depth_filename)

        if depth_dir and os.path.exists(depth_path):
            backdrop = create_depth_backdrop(
                image_path=image_path,
                depth_path=depth_path,
                name="Backdrop",
                distance=distance,
                displacement_strength=displacement,
                direction=direction
            )
        else:
            backdrop = create_simple_backdrop(
                image_path=image_path,
                name="Backdrop",
                distance=distance,
                direction=direction
            )

        backdrops.append(backdrop)

    return backdrops

# Usage for Blood & Dust:
# backdrops = create_cardinal_backdrops(
#     backdrop_dir="/media/.../assets/Backdrop",
#     depth_dir="/media/.../assets/Backdrop/depth",
#     distance=200,
#     displacement=15
# )
```

## Integration with BlenderMCP

Execute via `execute_blender_code` tool:

```python
# 1. First, generate depth maps externally (Marigold recommended for painterly images)
# 2. Then execute backdrop creation in Blender:

code = '''
import bpy
import math

# [Paste any of the above functions]

# Create backdrop
backdrop = create_depth_backdrop(
    image_path="/media/endlessblink/data/my-projects/ai-development/game-dev/ai-game-production-lecture/assets/Backdrop/backdrop_n.png",
    depth_path="/media/endlessblink/data/my-projects/ai-development/game-dev/ai-game-production-lecture/assets/Backdrop/backdrop_n_depth.png",
    distance=200,
    displacement_strength=15,
    direction="north"
)
'''

# Use mcp__blenderMCP__execute_blender_code(code=code)
```

## Parameters Reference

| Parameter | Default | Description |
|-----------|---------|-------------|
| `distance` | 100.0 | Distance from origin in Blender units |
| `displacement_strength` | 10.0 | Depth extrusion amount |
| `subdivision_levels` | 6 | Geometry detail (higher = more polygons) |
| `direction` | "north" | Cardinal direction or "sky" |

## AI Depth Model Comparison

| Model | Best For | Speed | Install |
|-------|----------|-------|---------|
| **Marigold** | Painterly/artistic images | 5-10s | `pip install diffusers` |
| **Depth Anything V2** | Photos, batch processing | 0.3s | `pip install transformers` |
| **ZoeDepth** | Metric depth (real scale) | 1-2s | `pip install transformers` |
| **MiDaS** | General purpose fallback | 0.5s | `torch.hub.load()` |

**Recommendation for Rembrandt style:** Use **Marigold** - it preserves fine artistic details and handles non-photorealistic lighting (chiaroscuro) better than faster alternatives.

## Common Issues & Fixes

| Problem | Solution |
|---------|----------|
| Displacement not visible | Add Subdivision modifier with level 5+ |
| Depth appears inverted | Add Invert node between depth texture and displacement |
| Edge artifacts | Set texture extension to 'EXTEND' not 'REPEAT' |
| Too much displacement | Reduce displacement scale (try 5-15 range) |
| Lighting affects backdrop | Use Emission shader instead of Principled BSDF |
| Texture stretching | Ensure proper UV mapping or use Generated coordinates |

## Export for Unreal Engine

```python
def export_backdrop_fbx(backdrop, output_path):
    """Export backdrop with applied modifiers for Unreal"""

    # Select only the backdrop
    bpy.ops.object.select_all(action='DESELECT')
    backdrop.select_set(True)
    bpy.context.view_layer.objects.active = backdrop

    # Export with Unreal-compatible settings
    bpy.ops.export_scene.fbx(
        filepath=output_path,
        use_selection=True,
        apply_scale_options='FBX_SCALE_ALL',
        axis_forward='-Z',
        axis_up='Y',
        use_mesh_modifiers=True,
        mesh_smooth_type='FACE',
        use_mesh_edges=False,
        bake_anim=False
    )
```

## Sources

- Marigold Depth Estimation (CVPR 2024 Best Paper Candidate)
- Depth Anything V2 (NeurIPS 2024)
- Blender Python API Documentation
- Camera Projection Painter addon
- LeiaInc ImportDepthMap addon
- BlenderProc MaterialLoaderUtility patterns
