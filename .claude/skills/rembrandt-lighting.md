# Rembrandt Golden Hour Lighting via MCP

## Overview
Apply a Rembrandt Dutch Golden Age oil painting look to an Unreal Engine 5 level using MCP tools.
The approach relies on the **physically-based Sky Atmosphere** to produce natural golden tones from
a low sun angle, with **minimal manual color tinting**. Post-process adds subtle painterly feel
(desaturation, vignette, contrast) without tinting everything orange.

## Trigger Phrases
- "rembrandt lighting"
- "golden hour lighting"
- "dutch golden age"
- "oil painting lighting"
- "chiaroscuro lighting"
- "cinematic warm lighting"

## CRITICAL RULES

1. **NEVER tint every system warm** -- stacking warm tints on light + fog + atmosphere + post-process compounds into extreme red/orange. Let the physically-based sky handle warmth from the sun angle.
2. **NEVER batch property changes** -- set ONE property, then take a screenshot to verify before continuing. If something looks wrong, fix it immediately.
3. **All MCP calls strictly sequential** -- never parallel.
4. **ALWAYS take screenshots** -- after each major step (light, fog, sky, post-process), take a screenshot and evaluate before proceeding.
5. **Keep light color near-white** -- the atmosphere naturally warms low-angle light. Manual warm tint on top creates Mars-red look.
6. **Post-process should be SUBTLE** -- ColorSaturation, ColorGain, ColorGamma shifts should be no more than 0.05-0.1 from default (1.0). More than that looks fake.
7. **NEVER set AutoExposureBias above 2.0** -- high values fight with the lighting and create unpredictable results.
8. **Fog colors stay near-neutral** -- warm fog tint compounds with warm light. Use the default blueish fog inscattering or only very slightly warm it.
9. **Record starting values first** -- before changing ANY property, record its current value so you can restore if needed.

## UE5 Default Values Reference (from engine source code)

### DirectionalLight (DirectionalLightComponent.cpp)
| Property | Default |
|----------|---------|
| Intensity | 10 (lux) |
| LightColor | (255, 255, 255) white |
| bUseTemperature | false |
| Temperature | 6500 |
| IndirectLightingIntensity | 1.0 |
| VolumetricScatteringIntensity | 1.0 |
| AtmosphereSunDiskColorScale | (1, 1, 1, 1) |

### ExponentialHeightFog (ExponentialHeightFogComponent.cpp)
| Property | Default |
|----------|---------|
| FogDensity | 0.02 |
| FogHeightFalloff | 0.2 |
| FogInscatteringColor | (0.447, 0.638, 1.0) blueish |
| DirectionalInscatteringExponent | 4.0 |
| DirectionalInscatteringStartDistance | 10000.0 |
| DirectionalInscatteringColor | (0.25, 0.25, 0.125) |
| FogMaxOpacity | 1.0 |
| StartDistance | 0.0 |
| bEnableVolumetricFog | false |
| VolumetricFogScatteringDistribution | 0.2 |
| VolumetricFogAlbedo | (255, 255, 255) white |
| VolumetricFogExtinctionScale | 1.0 |
| VolumetricFogDistance | 6000.0 |

### SkyAtmosphere (SkyAtmosphereComponent.cpp)
| Property | Default |
|----------|---------|
| GroundAlbedo | (170, 170, 170) ~0.4 linear |
| RayleighScatteringScale | 0.0331 |
| MieScatteringScale | 0.003996 |
| MieAbsorptionScale | 0.000444 |
| MieAnisotropy | 0.8 |
| MultiScatteringFactor | 1.0 |

### PostProcessVolume Film Tonemapper
| Property | Default |
|----------|---------|
| FilmSlope | 0.88 |
| FilmToe | 0.55 |
| FilmShoulder | 0.26 |
| FilmBlackClip | 0.0 |
| FilmWhiteClip | 0.04 |

### PostProcessVolume Color Grading
| Property | Default |
|----------|---------|
| ColorSaturation | (1, 1, 1, 1) |
| ColorContrast | (1, 1, 1, 1) |
| ColorGamma | (1, 1, 1, 1) |
| ColorGain | (1, 1, 1, 1) |
| ColorOffset | (0, 0, 0, 0) |
| VignetteIntensity | 0.0 |
| BloomIntensity | 0.675 |
| AutoExposureBias | 0.0 |

## Execution Steps

### Phase 1: Set Sun Angle (creates natural golden hour)

The key to golden hour is **sun angle, not color tinting**. UE5's physically-based atmosphere
naturally creates warm golden light when the sun is low on the horizon.

**Directional Light rotation:**
- Pitch: **-20** (low angle, ~20 degrees above horizon = late afternoon golden hour)
- Yaw: **160** (side lighting for dramatic shadows)
- Roll: 0

**Directional Light properties:**
- Intensity: **10** (default -- don't increase)
- LightColor: **(255, 255, 255)** white -- atmosphere handles the warmth
- bUseTemperature: **false**
- AtmosphereSunDiskColorScale: **(1, 1, 1, 1)** default

**TAKE SCREENSHOT** -- verify terrain is lit with warm-ish light from the side. If too dark, raise pitch to -30. If no warmth, the sun is too high -- lower pitch toward -15.

### Phase 2: Sky Atmosphere (slight mood adjustment)

Only change from defaults if the sky looks too bright/blue. Small adjustments:

- MieScatteringScale: **0.01** (slightly more atmospheric haze than default 0.004)
- MieAnisotropy: **0.8** (keep default -- forward scattering for halo around sun)
- MultiScatteringFactor: **1.0** (keep default)
- RayleighScatteringScale: **0.0331** (keep default)
- GroundAlbedo: **(170, 170, 170)** (keep default)

**TAKE SCREENSHOT** -- sky should look naturally warm near horizon, blue above. If too blue, try MieScatteringScale 0.02. Never go above 0.1.

### Phase 3: Fog (subtle atmospheric depth)

Light fog for depth, NOT for color tinting. Keep colors near-neutral.

- FogDensity: **0.005** (lighter than default 0.02 -- just enough for depth)
- FogHeightFalloff: **0.2** (default)
- FogInscatteringColor: **(0.45, 0.55, 0.65, 1)** (slightly less blue than default, near-neutral)
- FogMaxOpacity: **0.75** (don't fully obscure distant objects)
- bEnableVolumetricFog: **true**
- VolumetricFogScatteringDistribution: **0.3** (slight forward scatter)
- VolumetricFogAlbedo: **(240, 235, 225, 255)** (near-white, barely warm)
- VolumetricFogExtinctionScale: **0.5** (subtle)
- VolumetricFogDistance: **15000** (extended to avoid visible cutoff)

**TAKE SCREENSHOT** -- should see subtle depth haze, NOT an orange wall. If too orange, make FogInscatteringColor and VolumetricFogAlbedo more neutral/white.

### Phase 4: Post-Process (painterly feel)

This is where the "oil painting" feel comes from. Key: **desaturation + vignette + slight contrast**.
Changes from default are TINY -- 0.05-0.10 max per channel.

**Color Grading (Global):**
- ColorSaturation: **(0.9, 0.9, 0.9, 1)** -- slight desaturation for painterly feel
- ColorContrast: **(1.08, 1.08, 1.05, 1)** -- slight contrast boost, less on blue
- ColorGamma: **(1.0, 0.98, 0.96, 1)** -- barely perceptible warm midtone shift
- ColorGain: **(1.0, 1.0, 1.0, 1)** -- KEEP DEFAULT. Don't tint highlights.
- ColorOffset: **(0, 0, 0, 0)** -- KEEP DEFAULT. Don't tint shadows.

**Shadows:**
- ColorSaturationShadows: **(0.9, 0.9, 0.85, 1)** -- slightly desaturated cool shadows
- ColorGainShadows: **(1.0, 1.0, 1.0, 1)** -- KEEP DEFAULT

**Highlights:**
- ColorSaturationHighlights: **(0.95, 0.95, 0.9, 1)** -- barely warm highlights
- ColorGainHighlights: **(1.0, 1.0, 1.0, 1)** -- KEEP DEFAULT

**Film Tonemapper:**
- FilmSlope: **0.88** (default)
- FilmToe: **0.55** (default)
- FilmShoulder: **0.26** (default)
- FilmBlackClip: **0.0** (default)
- FilmWhiteClip: **0.04** (default)

**Lens Effects:**
- VignetteIntensity: **0.4** -- dark edges like a painting frame
- BloomIntensity: **0.5** -- subtle glow on bright areas
- BloomThreshold: **1.0** -- only bright spots bloom

**Ambient Occlusion:**
- AmbientOcclusionIntensity: **0.6** -- darken crevices for depth
- AmbientOcclusionRadius: **200** -- wide AO for dramatic shadow feel

**Exposure:**
- AutoExposureBias: **0.0** (default -- let auto-exposure do its job)

**TAKE SCREENSHOT** -- should see slightly desaturated scene with dark vignette edges, subtle warm tone from the sun angle (NOT from tinting), and good visibility of terrain/objects.

## Incremental Verification Protocol

After EACH phase:
1. Take screenshot with `mcp__unrealMCP__take_screenshot`
2. Evaluate: Is terrain visible? Is it too dark? Too orange? Too blue?
3. If wrong, identify which single property caused it and revert that ONE property
4. Take another screenshot to confirm the fix
5. Only proceed to next phase when current phase looks right

## What Success Looks Like
- Terrain, rocks, vegetation clearly visible (not silhouetted)
- Natural golden-warm light from low sun angle (NOT orange tint filter)
- Blue sky above transitioning to warm near horizon
- Subtle atmospheric haze adding depth
- Slightly desaturated look with dark vignette edges
- Overall impression: warm late afternoon, dramatic but not extreme

## What Failure Looks Like (AVOID)
- Mars-red terrain = too much warm tinting across multiple systems
- Pure silhouettes = sun too low or exposure too low
- Orange wall sky = fog too warm or too dense
- Abrupt fog cutoff = VolumetricFogDistance too short
- Flat/washed out = too much desaturation or too much fog
- No visible difference from default = sun angle not low enough

## Recovery: Reset to Defaults

If things go wrong, reset ALL actors to the UE5 defaults listed above.
The fastest reset approach: delete the fog/sky/post-process actors and spawn fresh ones,
then only change the directional light rotation.

## Required Actor Names

These must exist in the level (exact names from Blood & Dust):
- DirectionalLight: `DirectionalLight_UAID_F4A475FF15A3736A02_1961932697`
- ExponentialHeightFog: `ExponentialHeightFog_UAID_F4A475FF15A3736A02_1961936700`
- SkyAtmosphere: `SkyAtmosphere_UAID_F4A475FF15A3736A02_1961927691`
- PostProcessVolume: `PostProcessVolume_UAID_F4A475FF15A3736A02_1961895679`

If actor names differ, use `mcp__unrealMCP__find_actors_by_name` to locate them first.
