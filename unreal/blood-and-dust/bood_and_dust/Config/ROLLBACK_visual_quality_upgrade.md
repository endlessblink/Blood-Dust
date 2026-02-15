# Visual Quality Upgrade - Rollback Instructions

**Date:** 2026-02-16
**Scope:** DefaultEngine.ini renderer settings + PostProcessVolume properties + Nanite on rock meshes

---

## 1. DefaultEngine.ini Rollback

Replace the `[/Script/Engine.RendererSettings]` section with this exact original content:

```ini
[/Script/Engine.RendererSettings]
r.ReflectionMethod=1
r.GenerateMeshDistanceFields=True
r.DynamicGlobalIlluminationMethod=1
r.Lumen.TraceMeshSDFs=0
r.Shadow.Virtual.Enable=1
r.Shadow.Virtual.MaxPhysicalPages=1024
r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange=True
r.AllowStaticLighting=False

r.SkinCache.CompileShaders=True
r.SkinCache.SceneMemoryLimitInMB=128

; --- DISABLED: Ray Tracing uses 1-2GB VRAM, not needed for Rembrandt style ---
r.RayTracing=False
r.RayTracing.RayTracingProxies.ProjectEnabled=False

r.Substrate=True
r.Substrate.ProjectGBufferFormat=0

; --- VRAM Budget Controls (RTX 4070 Ti 12GB) ---
r.Streaming.PoolSize=2048
r.Streaming.LimitPoolSizeToVRAM=1
r.RenderTargetPoolMin=300
r.Lumen.SurfaceCache.CardMaxResolution=128
r.Lumen.SurfaceCache.MeshCardsMinSize=25

; --- Performance Tuning ---
r.Lumen.Reflections.MaxRoughnessToTrace=0.4
r.Lumen.ScreenProbeGather.MaxRayIntensity=20
r.VolumetricCloud.ViewRaySampleMaxCount=64
r.VolumetricCloud.ReflectionRaySampleMaxCount=16
r.VolumetricCloud.ShadowViewRaySampleMaxCount=8

r.DefaultFeature.LocalExposure.HighlightContrastScale=0.8

r.DefaultFeature.LocalExposure.ShadowContrastScale=0.8
```

### Per-Setting Rollback Reference

| Setting | Original Value | Changed To | Tier |
|---------|---------------|------------|------|
| `r.AntiAliasingMethod` | not set (engine default) | 4 | 1A |
| `r.TemporalAA.Upscaler` | not set (engine default) | 1 | 1A |
| `r.Lumen.TraceMeshSDFs` | 0 | 1 | 1B |
| `r.RayTracing` | False | True | 2A |
| `r.RayTracing.RayTracingProxies.ProjectEnabled` | False | removed | 2A |
| `r.Lumen.HardwareRayTracing` | not set | 1 | 2A |
| `r.Lumen.HardwareRayTracing.LightingMode` | not set | 0 | 2A |
| `r.RayTracing.Shadows` | not set | 0 | 2A |
| `r.RayTracing.Reflections` | not set | 0 | 2A |
| `r.RayTracing.AmbientOcclusion` | not set | 0 | 2A |
| `r.Lumen.SurfaceCache.CardMaxResolution` | 128 | 256 | 2B |
| `r.Lumen.Reflections.MaxRoughnessToTrace` | 0.4 | 0.6 | 2B |
| `r.Lumen.ScreenProbeGather.MaxRayIntensity` | 20 | 40 | 2B |
| `r.Shadow.Virtual.MaxPhysicalPages` | 1024 | 2048 | 2C |
| `r.Streaming.PoolSize` | 2048 | 3072 | 3C |

---

## 2. PostProcessVolume Rollback (via MCP)

**Actor name:** `PostProcessVolume_Fresh_UAID_50EBF6B2A22934BE02_1102164291`

### Film Grain (Tier 1C) - Revert to defaults (all zero)

```
set_actor_property(actor_name="PostProcessVolume_Fresh_UAID_50EBF6B2A22934BE02_1102164291", property_name="Settings.FilmGrainIntensity", property_value="0.0")
set_actor_property(actor_name="PostProcessVolume_Fresh_UAID_50EBF6B2A22934BE02_1102164291", property_name="Settings.FilmGrainIntensityShadows", property_value="0.0")
set_actor_property(actor_name="PostProcessVolume_Fresh_UAID_50EBF6B2A22934BE02_1102164291", property_name="Settings.FilmGrainIntensityHighlights", property_value="0.0")
```

### Bloom (Tier 2D) - Revert to defaults

```
set_actor_property(actor_name="PostProcessVolume_Fresh_UAID_50EBF6B2A22934BE02_1102164291", property_name="Settings.BloomSizeScale", property_value="1.0")
set_actor_property(actor_name="PostProcessVolume_Fresh_UAID_50EBF6B2A22934BE02_1102164291", property_name="Settings.Bloom1Tint", property_value="{\"R\":1.0, \"G\":1.0, \"B\":1.0, \"A\":1.0}")
set_actor_property(actor_name="PostProcessVolume_Fresh_UAID_50EBF6B2A22934BE02_1102164291", property_name="Settings.Bloom2Tint", property_value="{\"R\":1.0, \"G\":1.0, \"B\":1.0, \"A\":1.0}")
```

---

## 3. Nanite Rollback (via MCP)

Disable Nanite on all 9 rock meshes:

```
set_nanite_enabled(mesh_path="/Game/Meshes/Rocks/SM_Drone_Rock_02", enabled=false)
set_nanite_enabled(mesh_path="/Game/Meshes/Rocks/SM_Namaqualand_Boulder_05", enabled=false)
set_nanite_enabled(mesh_path="/Game/Meshes/Rocks/SM_Boulder_01", enabled=false)
set_nanite_enabled(mesh_path="/Game/Meshes/Rocks/rock_moss_set_01_rock06", enabled=false)
set_nanite_enabled(mesh_path="/Game/Meshes/Rocks/rock_moss_set_01_rock01", enabled=false)
set_nanite_enabled(mesh_path="/Game/Meshes/Rocks/rock_moss_set_01_rock05", enabled=false)
set_nanite_enabled(mesh_path="/Game/Meshes/Rocks/rock_moss_set_01_rock04", enabled=false)
set_nanite_enabled(mesh_path="/Game/Meshes/Rocks/rock_moss_set_01_rock03", enabled=false)
set_nanite_enabled(mesh_path="/Game/Meshes/Rocks/rock_moss_set_01_rock02", enabled=false)
```

---

## 4. Quick Full Rollback Procedure

1. **Stop Unreal Editor** (required for INI changes + RT pipeline toggle)
2. **Replace `Config/DefaultEngine.ini`** renderer section with the original block from Section 1 above
3. **Start Unreal Editor**
4. **Run MCP rollback commands** from Sections 2 and 3 above (sequentially, one at a time)
5. **Ctrl+S** to save all changes
6. **Verify:** Take screenshot, confirm original visual quality is restored

---

## 5. Partial Rollback by Tier

### Revert only Tier 1 (TSR + SDF + Film Grain)
- Remove `r.AntiAliasingMethod=4` and `r.TemporalAA.Upscaler=1` lines
- Change `r.Lumen.TraceMeshSDFs=1` back to `0`
- Run Film Grain MCP revert commands (Section 2)

### Revert only Tier 2 (Hardware Lumen + Quality + VSM + Bloom)
- Change `r.RayTracing=True` back to `r.RayTracing=False`
- Re-add `r.RayTracing.RayTracingProxies.ProjectEnabled=False`
- Remove all `r.Lumen.HardwareRayTracing*`, `r.RayTracing.Shadows/Reflections/AO` lines
- Change `r.Lumen.SurfaceCache.CardMaxResolution=256` back to `128`
- Change `r.Lumen.Reflections.MaxRoughnessToTrace=0.6` back to `0.4`
- Change `r.Lumen.ScreenProbeGather.MaxRayIntensity=40` back to `20`
- Change `r.Shadow.Virtual.MaxPhysicalPages=2048` back to `1024`
- Run Bloom MCP revert commands (Section 2)

### Revert only Tier 3 (Nanite + Streaming)
- Change `r.Streaming.PoolSize=3072` back to `2048`
- Run Nanite disable MCP commands (Section 3)
