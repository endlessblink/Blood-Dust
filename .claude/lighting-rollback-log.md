# Rembrandt Lighting - Rollback Log

## Actor Names
- PostProcessVolume: `PostProcessVolume_Fresh_UAID_50EBF6B2A22934BE02_1102164291`
- ExponentialHeightFog: `ExponentialHeightFog_Fresh_UAID_50EBF6B2A22934BE02_1074237289`

## Changes Applied

| # | Property | Value | Status | Revert Value |
|---|----------|-------|--------|--------------|
| 1 | Settings.VignetteIntensity | 0.7 | APPROVED | 0.0 (default) |
| 2 | Settings.ColorSaturation | (0.9,0.9,0.9,1) | REVERTED | (1,1,1,1) |
| 3 | Settings.ColorContrast | (1.08,1.08,1.05,1) | APPROVED | (1,1,1,1) |
| 4 | FogDensity | 0.05 | APPROVED | (previous) |
| 5 | Settings.BloomIntensity | 0.5 | APPROVED | (default) |
| 6 | Settings.BloomThreshold | 1.0 | APPROVED | (default) |
| 7 | bEnableVolumetricFog | true | APPROVED | false |
