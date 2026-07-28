# Seed Map Cubiomes Status

Last updated: 2026-07-28

## Runtime Evidence

- The Seed Map panel captured raw seed `114514` (`0x000000000001BF52`).
- A visible in-game village was present at `(1243, 107, -4522)` while the panel reported `Candidates 0`, `Biome rejected 0`, and `Markers 0`.
- Its cubiomes village anchor is `(1256, -4536)`, inside tile `(1, -5)` with block bounds `[1024, 2048) x [-5120, -4096)`.

## Infinite Scan Fix

- Root cause: `QueryPlacementCandidates` used unbounded region loops whose exit checks were after the candidate processing. An out-of-bounds candidate used `continue`, skipping the inner-loop exit check and causing region X to advance indefinitely.
- The region X/Z loops are now explicitly bounded by the calculated minimum and maximum regions. Out-of-bounds candidates remain excluded from statistics, but cannot bypass iteration termination.
- Regression coverage queries seed `114514`, tile `(1, -5)`, and requires the user-visible anchor `(1256, -4536)`. The reference 26.20 finder returns four anchors in this full 1024-block tile, rejects two by biome, and emits two markers: `(1256, -4536)` and `(1896, -4680)`.

## Replacement Completed

- Reference: `FragrantResult186/cubiomes-viewer-bedrock`.
- Upstream review baseline: `4b631e6a5dd5457fcfa31a7353d849d0802ea0c3`.
- `Bedrock2620StructureFinder` is the only placement and viability adapter. A
  worker creates it with `setupGenerator(MC_26_20)`, applies its copied seed
  for the requested dimension, then delegates to `getStructureConfig`,
  `getStructurePos`, and `isViableStructurePos`.
- The adapter owns no game object and now rejects structure calls whose layer
  dimension differs from its generator dimension. This prevents an accidental
  overworld finder from returning a nether or end result.
- Village queries now use the reference API sequence: `getStructurePos(Village, MC_26_20, seed, regionX, regionZ, ...)`, followed by `isViableStructurePos(...)`.
- The former hand-written MT19937 placement path is no longer used by the Village query.
- `finders.c` and its `cave.c` dependency are included in both the mod and standalone worldgen test targets.

## Seed-Only Catalog

- `FixtureVerified` remains reserved for the observed village regression
  fixture. All other upstream-supported layers are now `CubiomesReference`:
  selectable, worker-scheduled, and calculated exclusively from the copied
  seed by the local 26.20 reference implementation.
- Supported overworld structures: outpost, desert pyramid, jungle temple,
  swamp hut, igloo, woodland mansion, ocean monument, ocean ruin, shipwreck,
  buried treasure, ruined portal, stronghold, mineshaft, ancient city, trail
  ruins, and trial chambers.
- Supported nether structures: ruined portal, fortress, and bastion remnant.
  The end catalog supports end city.
- Desert pyramids, jungle temples, and woodland mansions remain explicitly
  labelled seed predictions because the upstream project documents a surface
  height limitation for those structures. This implementation does not read
  terrain or loaded chunks to fill that gap.

- `Stronghold` uses the fork's `getStructurePos(Stronghold, ...)` static
  finder; it does not use a Java-style iterator.
- `Mineshaft` uses the fork's `getStructurePos(Mineshaft, ...)`, which
  delegates to local `getMineshafts` with a one-chunk region. It is bounded by
  the query tile and is therefore more expensive than region structures, but
  still deterministic and seed-only.
- `NetherFossil` remains `Unavailable`: this upstream revision has no
  deterministic 26.20 finder for it.
- Natural outer-End gateways are supported through the upstream
  `getEndGatewayPos(seed, EndNoise, SurfaceNoise, chunkX, chunkZ, ...)`
  finder. Fixed dragon-island gateways are event-driven, so the map never
  presents them as seed-generated results. The wrapper also bounds inputs
  before calling the upstream finder because its radius check squares signed
  `int` chunk coordinates.

## Rare Biomes

- Six independently selectable rare overworld biome layers use one
  batch `genBiomes` call per queried 1024-block tile: mushroom fields, cherry
  grove, badlands, ice spikes, mangrove swamp, and pale garden.
- Four selectable Nether layers use the same seed-only sampler at scale 4,
  Y=33: soul sand valley, crimson forest, warped forest, and basalt deltas.
  Four selectable End layers use the reference scale-16 End biome map: small
  End islands, End midlands, End highlands, and End barrens.
- The worker merges contiguous matching cells and emits the center of the
  first exact cubiomes sample in each region. It intentionally does not use a
  component centroid: preserving an observed sample gives the scale-16 End
  map an exact, round-trippable anchor. It never reads a loaded game chunk.
- Each rare biome has a separate pixel icon atlas entry and maps to its own
  layer, so its silhouette is independent of its color.

## 2026-07-28 Seed-Only Follow-Up

- The new Nether and End biome marker tests initially exposed a representative
  anchor error: averaging a connected component could yield a location not
  sampled as the target biome by cubiomes. Markers now keep a verified source
  cell instead, and the seed `114514` deterministic test rechecks each
  emitted marker through `getBiomeAt` for all eight layers.
- This change is entirely within `src/worldgen/`. The worker still receives
  only copied seed bits and tile bounds; no loaded client chunks, terrain, or
  game objects participate in placement or biome queries.

## Fixture Record Template

Each future `FixtureVerified` layer needs a checked-in test entry with: raw seed bits,
Bedrock 26.20 game version, dimension, `/locate` command output, structure
block coordinate, expected map anchor, tile bounds, and the upstream viability
result. Required coverage includes negative coordinates, a half-open tile
edge, a duplicate boundary query, viability rejection, and the stronghold and
mineshaft paths. Reference layers remain available as clearly labelled seed
predictions while this real-world fixture evidence is collected.

## Build And Deployment

- `worldgen_tests` and the complete `ChiyanMap` release build passed for both
  LeviLamina `26.10.14` and `26.20.4` after adding the reference status, all
  supported-structure scheduling, and seed-only rare-biome scans.
- With Minecraft closed, deployment copied only `ChiyanMap.dll`,
  `ChiyanMap.pdb`, and `manifest.json`; no `config.json`, cache, waypoint,
  death, or head data was modified.
- 2026-07-28 deployment SHA-256: `26.10.14`
  `E27D7D8B67AC646F6F9C29D2570BF8F26AB1C8F97CA9F7A989313D36D5B91303`;
  `26.20.4` `5A4E997A909FF24662C41FC4E95CBB30176D2CB4A989D591433CBD91ABCE3D0B`.
- Both complete `ChiyanMap` release builds also passed after the generic finder,
  dimension guard, compact icon atlas, and Eye-of-Ender stronghold icon update.
- The regression fixture verifies that the full tile query for seed `114514` terminates and includes the user-visible village anchor `(1256,-4536)`.
- `ChiyanMap` was force-rebuilt for both supported LeviLamina versions while the game was closed.
- Deployed only `ChiyanMap.dll`, `ChiyanMap.pdb`, and `manifest.json`; existing
  `config.json`, cache, waypoints, deaths, and heads were preserved.
- `26.20.04` deployment SHA-256: `6FE34B9F65DBC61045F7F6153A4A695C7CAAA65C8700C6B36F6B8A26D1AA23EA`.
- `26.10.04` deployment SHA-256: `DEE0A9D4573CD5F3840ACF924E94B8D6FF856AC5D48CF107C03243408C875794`.
- Icon rendering follow-up: removed the opaque black marker backing and made
  the source icon grid zoom-responsive (20px to 64px, 32px at default 3x).
- Latest `26.20.04` deployment SHA-256: `0071756CADCFD26759867D821467F0DAE84E531A23F1910BE0D40F943B61C026`.
- Latest `26.10.04` deployment SHA-256: `7E10F0735FD3FABD4043F002726618B38E16B7B5F23BA3F22A8E16C6D29C538A`.
- 2026-07-28 final seed-only biome follow-up deployment SHA-256: `26.20.04`
  `07E1F90E4E40ED48F3D944A5C50FBB4FC67AF466B3E7651B857B318AEECE5D75`;
  `26.10.04`
  `ED827DF1CC2F811C4E8D3FD50FED9655CC0226120FCC0DA604086D30AD17CC7A`.

## Runtime Verification Pending

1. In the seed-`114514` world at `(1243, 107, -4522)`, confirm the panel reports at least `Candidates 4` and `Markers 2`, including a marker near `(1256, -4536)`.
2. Confirm locations rejected by the reference viability check do not emit markers.
3. For each reference structure, capture the fixture record above before
   promoting it from `CubiomesReference` to `FixtureVerified`.
4. Confirm all enabled structure and rare-biome icons, click selection,
   centering, and saved waypoint naming in every supported dimension.
