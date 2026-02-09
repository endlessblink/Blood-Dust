---
name: vegetation-scatter
description: Production-quality vegetation scattering in Blender 4.x using Geometry Nodes. Covers Poisson disk distribution, instance alignment, LOD systems, weight painting, and Rembrandt styling. Triggers on "scatter vegetation", "place foliage", "add grass", "vegetation system", "foliage scatter".
---

# High-Quality Vegetation Scattering (Blender 4.x)

Production-ready vegetation distribution using Geometry Nodes. This system offers 10x more flexibility than legacy particle systems and can reduce render times from 52 hours to under 3 minutes when properly optimized.

## When to Use

- Populating landscapes with grass, flowers, rocks, shrubs, trees
- Creating natural-looking vegetation distribution
- Optimizing foliage performance for large areas
- Styling vegetation to match the Blood & Dust Rembrandt aesthetic
- Keywords: "scatter", "vegetation", "foliage", "trees", "grass", "plants"

---

## Quick Reference: Node Type Identifiers

**CRITICAL for Python API - use these exact identifiers:**

| User-Facing Name | Python Identifier |
|------------------|-------------------|
| Distribute Points on Faces | `GeometryNodeDistributePointsOnFaces` |
| Instance on Points | `GeometryNodeInstanceOnPoints` |
| Realize Instances | `GeometryNodeRealizeInstances` |
| Join Geometry | `GeometryNodeJoinGeometry` |
| Collection Info | `GeometryNodeCollectionInfo` |
| Random Value | `FunctionNodeRandomValue` |
| Align Euler to Vector | `FunctionNodeAlignEulerToVector` |
| Combine XYZ | `ShaderNodeCombineXYZ` |
| Vector Math | `ShaderNodeVectorMath` |
| Math | `ShaderNodeMath` |
| Compare | `FunctionNodeCompare` |
| Named Attribute | `GeometryNodeInputNamedAttribute` |
| Position | `GeometryNodeInputPosition` |
| Normal | `GeometryNodeInputNormal` |
| Group Input | `NodeGroupInput` |
| Group Output | `NodeGroupOutput` |

---

## Phase 1: Distribution Method

### Poisson Disk vs Random

| Method | Performance | Control | Use Case |
|--------|-------------|---------|----------|
| **Random** | Fastest | Basic density only | Quick previews |
| **Poisson Disk** | Moderate | Min distance + density | Realistic vegetation |

**Always use Poisson Disk** - it prevents clumping by enforcing minimum spacing.

### Key Parameters

| Parameter | Recommended Value | Description |
|-----------|-------------------|-------------|
| **Distance Min** | 0.02-0.1m | Minimum spacing between instances |
| **Density Max** | 100-1000 pts/m² | Maximum point density |
| **Density** | 0.5-1.0 | Multiplier for Density Max |
| **Seed** | Any integer | Randomization seed |

**Critical:** If Density = 0, density is capped by available surface area based on Distance Min.

---

## Phase 2: Correct Node Setup

### The Golden Rule

**The `Join Geometry` node is ESSENTIAL** - without it, Distribute Points converts your mesh to points, losing the ground surface.

### Complete Node Tree Architecture

```
[Group Input: Geometry]
    ↓
[Distribute Points on Faces]
    • distribute_method = 'POISSON'
    • Distance Min: 0.05
    • Density Max: 500
    • Density: 1.0
    ↓ Points output
    ↓ Rotation output ──────────────────┐
                                        ↓
[Random Value (Float)]              [Combine XYZ]
    • Min: 0                            • X: 0
    • Max: 6.28319 (2π)                 • Y: 0
    ↓                                   • Z: Random output
    └───────────────────────────────────┘
                                        ↓
                                [Euler to Rotation] (Blender 4.x)
                                        ↓
[Collection Info]                       ↓
    • Separate Children: True           ↓
    • Reset Children: True              ↓
    ↓                                   ↓
[Instance on Points] ←──────────────────┘
    • Pick Instance: True
    • Instance: Collection geometry
    • Rotation: Combined rotation
    • Scale: Scale vector
    ↓
[Join Geometry]
    • Input 1: Original terrain geometry
    • Input 2: Instanced vegetation
    ↓
[Group Output]
```

### Surface Normal Alignment

**Method 1: Direct Rotation (simplest)**
```
Distribute Points → Rotation output → Instance on Points → Rotation input
```

**Method 2: Manual Normal Processing (more control)**
```
Distribute Points → Normal output → Align Euler to Vector → Instance on Points
```

The Rotation output aligns geometry UP along surface normal, but provides NO control over spin around that axis - Z-rotation must be added separately.

---

## Phase 3: Random Z-Axis Rotation

**This is critical for natural appearance.**

### Z-Only Rotation (Recommended for Grass)

```python
# Node setup in Python
random_rot = nodes.new("FunctionNodeRandomValue")
random_rot.data_type = 'FLOAT'
random_rot.inputs[2].default_value = 0.0      # Min (index 2 for float min)
random_rot.inputs[3].default_value = 6.28319  # Max = 2π radians = 360°

combine_xyz = nodes.new("ShaderNodeCombineXYZ")
# Connect random to Z input only
links.new(random_rot.outputs[1], combine_xyz.inputs[2])  # outputs[1] = Value for float

# For Blender 4.x: Convert Euler to Rotation type
euler_to_rot = nodes.new("FunctionNodeEulerToRotation")
links.new(combine_xyz.outputs[0], euler_to_rot.inputs[0])
links.new(euler_to_rot.outputs[0], instance_node.inputs['Rotation'])
```

This preserves surface-aligned tilt (X/Y) while adding natural spin (Z).

---

## Phase 4: Uniform Scale Variation

Grass needs size variation but should maintain aspect ratio.

### Uniform XY, Variable Z (Height)

```python
# XY scale (uniform)
random_xy = nodes.new("FunctionNodeRandomValue")
random_xy.data_type = 'FLOAT'
random_xy.inputs[2].default_value = 0.8   # Min
random_xy.inputs[3].default_value = 1.2   # Max

# Z scale (height variation)
random_z = nodes.new("FunctionNodeRandomValue")
random_z.data_type = 'FLOAT'
random_z.inputs[2].default_value = 0.7    # Min
random_z.inputs[3].default_value = 1.5    # Max
random_z.inputs[8].default_value = 999    # Different seed!

combine_scale = nodes.new("ShaderNodeCombineXYZ")
links.new(random_xy.outputs[1], combine_scale.inputs[0])  # X
links.new(random_xy.outputs[1], combine_scale.inputs[1])  # Y (same as X)
links.new(random_z.outputs[1], combine_scale.inputs[2])   # Z (independent)

links.new(combine_scale.outputs[0], instance_node.inputs['Scale'])
```

---

## Phase 5: Grass Blade Geometry

### Vertex Count Performance

| Blade Type | Vertex Count | Use Case |
|------------|--------------|----------|
| Billboard (crossed quads) | 12-24 | Large distant fields |
| Simple 3D | 20-40 | Most vegetation (recommended) |
| High-poly 3D | 100+ | Extreme close-ups only |

**Sweet spot: 20-30 vertices per blade**

### Creating Grass Blades

**Critical: Origin point MUST be at blade base (ground contact)**

```python
import bpy
import random
from mathutils import Vector

def create_grass_blade(name, num_blades=5, height_range=(0.3, 0.6), width=0.03):
    """Create grass tuft with multiple blades. Origin at base."""
    verts = []
    faces = []

    for i in range(num_blades):
        offset_x = random.uniform(-0.05, 0.05)
        offset_y = random.uniform(-0.05, 0.05)
        height = random.uniform(height_range[0], height_range[1])

        base_idx = len(verts)
        # Blade quad - bottom to top
        verts.append(Vector((offset_x - width/2, offset_y, 0)))           # Bottom left
        verts.append(Vector((offset_x + width/2, offset_y, 0)))           # Bottom right
        verts.append(Vector((offset_x + width/2 * 0.3, offset_y, height))) # Top right (tapered)
        verts.append(Vector((offset_x - width/2 * 0.3, offset_y, height))) # Top left (tapered)

        faces.append((base_idx, base_idx+1, base_idx+2, base_idx+3))

    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()

    obj = bpy.data.objects.new(name, mesh)
    # Origin is already at (0,0,0) which is blade base
    return obj
```

---

## Phase 6: Collection Setup for Instance Picking

### Proper Collection Structure

```python
# Create vegetation sources collection
if "Vegetation_Sources" not in bpy.data.collections:
    veg_col = bpy.data.collections.new("Vegetation_Sources")
    bpy.context.scene.collection.children.link(veg_col)
else:
    veg_col = bpy.data.collections["Vegetation_Sources"]

# Create multiple grass variations
random.seed(42)
for i in range(4):
    grass = create_grass_blade(
        f"grass_var_{i:02d}",
        num_blades=random.randint(4, 8),
        height_range=(0.2 + i*0.1, 0.4 + i*0.15)
    )
    veg_col.objects.link(grass)

# CRITICAL: Hide source collection but keep render enabled
veg_col.hide_viewport = True   # Hide from viewport clutter
veg_col.hide_render = False    # Must be False or instances won't render!
```

### Collection Info Node Setup

```python
collection_info = nodes.new("GeometryNodeCollectionInfo")
collection_info.inputs['Collection'].default_value = veg_col
collection_info.inputs['Separate Children'].default_value = True   # REQUIRED for pick instance
collection_info.inputs['Reset Children'].default_value = True
```

---

## Phase 7: Weight Painting for Density Control

### Setup Workflow

**Step 1: Create Vertex Group**
```python
terrain = bpy.data.objects['Terrain']
vg = terrain.vertex_groups.new(name="GrassDensity")
# Assign all verts with weight 1.0
vg.add(range(len(terrain.data.vertices)), 1.0, 'REPLACE')
```

**Step 2: Paint in Blender**
- Weight Paint mode (Ctrl+Tab)
- Weight 1.0 (red) = dense grass
- Weight 0.0 (blue) = no grass
- Brush Strength 0.3-0.5 for gradual transitions

**Step 3: Connect to Geometry Nodes**
```python
named_attr = nodes.new("GeometryNodeInputNamedAttribute")
named_attr.data_type = 'FLOAT'
named_attr.inputs['Name'].default_value = "GrassDensity"

# Connect to Density input
links.new(named_attr.outputs['Attribute'], distribute.inputs['Density'])
```

---

## Phase 8: Complete Production Script

```python
import bpy
import random
from mathutils import Vector

def create_production_grass_system(terrain_name="Terrain"):
    """
    Complete production-ready grass scattering system.
    Blender 4.x compatible with proper node identifiers.
    """
    terrain = bpy.data.objects.get(terrain_name)
    if not terrain:
        print(f"ERROR: Object '{terrain_name}' not found")
        return None

    # === STEP 1: Create grass blade variations ===
    if "Vegetation_Sources" not in bpy.data.collections:
        veg_col = bpy.data.collections.new("Vegetation_Sources")
        bpy.context.scene.collection.children.link(veg_col)
    else:
        veg_col = bpy.data.collections["Vegetation_Sources"]
        # Clear existing
        for obj in list(veg_col.objects):
            bpy.data.objects.remove(obj)

    random.seed(42)
    for i in range(4):
        grass = create_grass_blade(
            f"grass_var_{i:02d}",
            num_blades=random.randint(4, 7),
            height_range=(0.25 + i*0.08, 0.45 + i*0.12),
            width=0.025
        )
        veg_col.objects.link(grass)

    veg_col.hide_viewport = True
    veg_col.hide_render = False

    # === STEP 2: Create Geometry Nodes modifier ===
    # Remove existing modifier if present
    if "GrassScatter" in terrain.modifiers:
        terrain.modifiers.remove(terrain.modifiers["GrassScatter"])

    gn_mod = terrain.modifiers.new(name="GrassScatter", type='NODES')

    # === STEP 3: Create node tree ===
    node_tree = bpy.data.node_groups.new(name="GN_GrassScatter", type='GeometryNodeTree')
    gn_mod.node_group = node_tree

    # Interface sockets (Blender 4.x API)
    interface = node_tree.interface
    interface.new_socket(name="Geometry", in_out='INPUT', socket_type='NodeSocketGeometry')
    interface.new_socket(name="Geometry", in_out='OUTPUT', socket_type='NodeSocketGeometry')

    density_socket = interface.new_socket(name="Density", in_out='INPUT', socket_type='NodeSocketFloat')
    density_socket.default_value = 0.5
    density_socket.min_value = 0.0
    density_socket.max_value = 5.0

    dist_socket = interface.new_socket(name="Min Distance", in_out='INPUT', socket_type='NodeSocketFloat')
    dist_socket.default_value = 0.08
    dist_socket.min_value = 0.01
    dist_socket.max_value = 1.0

    scale_min_socket = interface.new_socket(name="Scale Min", in_out='INPUT', socket_type='NodeSocketFloat')
    scale_min_socket.default_value = 0.8

    scale_max_socket = interface.new_socket(name="Scale Max", in_out='INPUT', socket_type='NodeSocketFloat')
    scale_max_socket.default_value = 1.4

    seed_socket = interface.new_socket(name="Seed", in_out='INPUT', socket_type='NodeSocketInt')
    seed_socket.default_value = 42

    nodes = node_tree.nodes
    links = node_tree.links

    # === STEP 4: Create nodes ===
    # Input/Output
    group_input = nodes.new('NodeGroupInput')
    group_input.location = (-800, 0)

    group_output = nodes.new('NodeGroupOutput')
    group_output.location = (800, 0)

    # Distribute Points (Poisson disk)
    distribute = nodes.new('GeometryNodeDistributePointsOnFaces')
    distribute.location = (-500, 0)
    distribute.distribute_method = 'POISSON'
    distribute.inputs['Density Max'].default_value = 200.0

    # Collection Info
    collection_info = nodes.new('GeometryNodeCollectionInfo')
    collection_info.location = (-200, 300)
    collection_info.inputs['Collection'].default_value = veg_col
    collection_info.inputs['Separate Children'].default_value = True
    collection_info.inputs['Reset Children'].default_value = True

    # Random scale
    random_scale = nodes.new('FunctionNodeRandomValue')
    random_scale.location = (-400, -200)
    random_scale.data_type = 'FLOAT'

    # Combine scale (uniform XYZ)
    combine_scale = nodes.new('ShaderNodeCombineXYZ')
    combine_scale.location = (-200, -200)

    # Random Z rotation
    random_rot = nodes.new('FunctionNodeRandomValue')
    random_rot.location = (-400, -400)
    random_rot.data_type = 'FLOAT'
    random_rot.inputs[2].default_value = 0.0
    random_rot.inputs[3].default_value = 6.28319

    # Combine rotation (Z only)
    combine_rot = nodes.new('ShaderNodeCombineXYZ')
    combine_rot.location = (-200, -400)

    # Euler to Rotation (Blender 4.x)
    euler_to_rot = nodes.new('FunctionNodeEulerToRotation')
    euler_to_rot.location = (0, -400)

    # Random instance index
    random_idx = nodes.new('FunctionNodeRandomValue')
    random_idx.location = (-200, 150)
    random_idx.data_type = 'INT'
    random_idx.inputs[2].default_value = 0
    random_idx.inputs[3].default_value = 3  # 4 grass variations (0-3)

    # Instance on Points
    instance = nodes.new('GeometryNodeInstanceOnPoints')
    instance.location = (200, 0)
    instance.inputs['Pick Instance'].default_value = True

    # Join Geometry (CRITICAL!)
    join_geo = nodes.new('GeometryNodeJoinGeometry')
    join_geo.location = (500, 0)

    # === STEP 5: Connect nodes ===
    # Group Input: [0]=Geo, [1]=Density, [2]=MinDist, [3]=ScaleMin, [4]=ScaleMax, [5]=Seed
    # Distribute: [0]=Mesh, [1]=Selection, [2]=DistMin, [3]=DensityMax, [4]=Density, [5]=DensityFactor, [6]=Seed

    # Input -> Distribute
    links.new(group_input.outputs[0], distribute.inputs[0])   # Geometry -> Mesh
    links.new(group_input.outputs[1], distribute.inputs[4])   # Density
    links.new(group_input.outputs[2], distribute.inputs[2])   # Min Distance
    links.new(group_input.outputs[5], distribute.inputs[6])   # Seed

    # Scale randomization
    links.new(group_input.outputs[3], random_scale.inputs[2])  # Scale Min
    links.new(group_input.outputs[4], random_scale.inputs[3])  # Scale Max
    links.new(random_scale.outputs[1], combine_scale.inputs[0])  # X
    links.new(random_scale.outputs[1], combine_scale.inputs[1])  # Y
    links.new(random_scale.outputs[1], combine_scale.inputs[2])  # Z

    # Rotation: Distribute rotation + random Z
    links.new(random_rot.outputs[1], combine_rot.inputs[2])  # Random -> Z
    links.new(combine_rot.outputs[0], euler_to_rot.inputs[0])

    # Instance on Points
    # Inputs: [0]=Points, [1]=Selection, [2]=Instance, [3]=PickInstance, [4]=InstanceIndex, [5]=Rotation, [6]=Scale
    links.new(distribute.outputs[0], instance.inputs[0])       # Points
    links.new(collection_info.outputs[0], instance.inputs[2])  # Instance geometry
    links.new(random_idx.outputs[2], instance.inputs[4])       # Instance Index (int output)
    links.new(euler_to_rot.outputs[0], instance.inputs[5])     # Rotation
    links.new(combine_scale.outputs[0], instance.inputs[6])    # Scale

    # Join original geometry with instances
    links.new(group_input.outputs[0], join_geo.inputs[0])
    links.new(instance.outputs[0], join_geo.inputs[0])

    # Output
    links.new(join_geo.outputs[0], group_output.inputs[0])

    print("=== GRASS SCATTER SYSTEM CREATED ===")
    print(f"Terrain: {terrain_name}")
    print(f"Grass variations: 4")
    print(f"Modifier: GrassScatter")
    print(f"Adjust parameters in Modifier Properties panel")

    return node_tree, gn_mod

# Helper function for grass blade creation
def create_grass_blade(name, num_blades=5, height_range=(0.3, 0.6), width=0.03):
    """Create grass tuft with multiple blades. Origin at base."""
    verts = []
    faces = []

    for i in range(num_blades):
        offset_x = random.uniform(-0.05, 0.05)
        offset_y = random.uniform(-0.05, 0.05)
        height = random.uniform(height_range[0], height_range[1])

        base_idx = len(verts)
        verts.append(Vector((offset_x - width/2, offset_y, 0)))
        verts.append(Vector((offset_x + width/2, offset_y, 0)))
        verts.append(Vector((offset_x + width/2 * 0.3, offset_y, height)))
        verts.append(Vector((offset_x - width/2 * 0.3, offset_y, height)))

        faces.append((base_idx, base_idx+1, base_idx+2, base_idx+3))

    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.update()

    obj = bpy.data.objects.new(name, mesh)
    return obj

# Execute
create_production_grass_system("Terrain")
```

---

## Phase 9: LOD System for Large Terrains

### Distance-Based LOD

```python
def add_lod_system(node_tree, camera_name="Camera"):
    """Add LOD culling to existing grass system."""
    nodes = node_tree.nodes
    links = node_tree.links

    # Camera position
    camera_info = nodes.new("GeometryNodeObjectInfo")
    camera_info.inputs['Object'].default_value = bpy.data.objects.get(camera_name)

    # Point position
    position = nodes.new("GeometryNodeInputPosition")

    # Calculate distance
    distance = nodes.new("ShaderNodeVectorMath")
    distance.operation = 'DISTANCE'
    links.new(camera_info.outputs['Location'], distance.inputs[0])
    links.new(position.outputs['Position'], distance.inputs[1])

    # LOD threshold comparison
    compare = nodes.new("FunctionNodeCompare")
    compare.data_type = 'FLOAT'
    compare.operation = 'LESS_THAN'
    compare.inputs[1].default_value = 50.0  # Cull distance in meters

    links.new(distance.outputs['Value'], compare.inputs[0])

    # Connect to Instance on Points Selection
    instance_node = nodes.get('Instance on Points')
    if instance_node:
        links.new(compare.outputs['Result'], instance_node.inputs['Selection'])
```

### LOD Vertex Recommendations

| LOD Level | Distance | Vertex Count |
|-----------|----------|--------------|
| LOD0 | 0-10m | 30-40 (full 3D) |
| LOD1 | 10-50m | 15-20 (simplified) |
| LOD2 | 50-200m | 8-12 (billboard) |
| Beyond 200m | Culled | 0 |

---

## Phase 10: Rembrandt Styling

### Color Palette

| Element | Hex | RGB (0-1) |
|---------|-----|-----------|
| Dark foliage | `#2D261E` | (0.176, 0.149, 0.118) |
| Mid ochre | `#8C6D46` | (0.549, 0.427, 0.275) |
| Golden highlight | `#B37233` | (0.702, 0.447, 0.200) |
| Burnt sienna | `#8B4513` | (0.545, 0.271, 0.075) |

### Material Setup

```python
def create_grass_material():
    mat = bpy.data.materials.new("M_DriedGrass")
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links

    nodes.clear()

    # Output
    output = nodes.new('ShaderNodeOutputMaterial')
    output.location = (400, 0)

    # Principled BSDF
    principled = nodes.new('ShaderNodeBsdfPrincipled')
    principled.location = (100, 0)
    principled.inputs['Base Color'].default_value = (0.549, 0.427, 0.275, 1.0)  # Mid ochre
    principled.inputs['Roughness'].default_value = 0.9
    principled.inputs['Specular IOR Level'].default_value = 0.1

    links.new(principled.outputs['BSDF'], output.inputs['Surface'])

    return mat
```

---

## Common Mistakes Checklist

| Issue | Symptom | Solution |
|-------|---------|----------|
| No grass visible | Empty viewport | Check Collection visibility (camera icon enabled) |
| All grass identical | No variation | Enable "Pick Instance" + "Separate Children" |
| Grass floating/buried | Wrong placement | Set object origin to blade base |
| Scale too small | Can't see instances | Apply scale (Ctrl+A) before using in GeoNodes |
| Grass perpendicular | Wrong alignment | Connect Rotation output, not Normal directly |
| Missing ground | Terrain disappeared | Add Join Geometry node before output |
| Render missing grass | Viewport OK, render empty | Check collection render visibility |

---

## Success Criteria

- [ ] Grass source objects have origin at base (ground contact point)
- [ ] Collection has "Separate Children" enabled
- [ ] "Pick Instance" enabled on Instance on Points node
- [ ] Distribution method set to POISSON (not RANDOM)
- [ ] Join Geometry combines terrain + instances
- [ ] Random Z rotation added (0 to 2π radians)
- [ ] Scale variation is uniform (same X/Y)
- [ ] Rembrandt color palette applied (desaturated ochre tones)
- [ ] LOD/culling for terrains > 100m²

---

## Quick MCP Commands

**Create complete system:**
```
"Create a production grass scatter system on Terrain with Poisson disk distribution,
0.08m minimum distance, density 0.5, and 4 grass blade variations"
```

**Add weight painting control:**
```
"Add vertex group 'GrassDensity' to control grass distribution,
connect to Named Attribute node in the scatter system"
```

**Optimize for performance:**
```
"Add LOD system to grass scatter: cull grass beyond 50 meters from camera"
```
