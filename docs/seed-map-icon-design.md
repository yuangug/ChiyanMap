# Seed Map Icon Design Status

Last updated: 2026-07-28

## Requested Review Scope

- Create a Figma review board for distinct 20px pixel-badge markers.
- Structures: village, pillager outpost, desert pyramid, jungle temple,
  swamp hut, igloo, woodland mansion, ocean monument, shipwreck, ancient
  city, trail ruins, and trial chambers.
- Rare biomes: mushroom fields, cherry grove, badlands, ice spikes,
  mangrove swamp, and pale garden.
- The review board must show default, hover, and selected marker states,
  a legend, and a mixed dark-map preview.

## Figma Review Board

- File: [ChiyanMap - Seed Map Icon Review](https://www.figma.com/design/dj5AkSXMUWEK8QBr8Y4lnw)
- Review root: `Review / Minecraft Structure Icon Reference Draft` (`44:593`)
  on `01 Foundations`.
- The board is an isolated design review. It contains no game code and does
  not enable an unverified Seed Map layer.

## Current Review Draft

- The rejected simplified draft was deleted at the user's request. The current
  board is a complete redraw, not an edit of that earlier work.
- All 18 requested marks now start as larger 48px material miniatures before
  being reproduced in the 20px map test. The displayed forms intentionally
  retain actual structure massing: roofed village house, bannered outpost
  tower, dual-tower desert temple, stepped jungle temple, stilted swamp hut,
  igloo, mansion wing, monument spire, wrecked mast, ancient-city arch,
  trail-ruins fragments, and the copper trial-chamber frame.
- The map test includes village, outpost, desert temple, ocean monument,
  ancient city, mushroom fields, cherry grove, and ice spikes at 20px, plus
  hover and selected treatments. It is for visual legibility review only.
- Revision after visual feedback: the outpost now has its wide top canopy;
  ocean monument now uses a central prismarine hall with visible corner towers;
  trail ruins now use partially buried terracotta, broken pillars, and
  suspicious-gravel massing; trial chambers now use a tuff arch hall, copper
  trim, and a central trial-spawner motif. These four are redrawn rather than
  recolored.
- Follow-up revision: igloo is now a full stepped snow-brick dome with a
  shaded curve and low side entrance; the outpost canopy is explicitly a
  three-layer, overhanging dark-wood roof; cherry grove now uses white-pink
  highlights, mid-pink blossoms, deep-pink canopy shadow, and sparse petals
  instead of a single flat pink.
- Outpost completion pass: the marker now continues from the roof through the
  enclosed lookout, lower timber platform, four-leg support silhouette, and
  stone ground base. It is no longer a top-only tower fragment.
- Reference-corrected outpost pass: after reviewing a supplied in-game
  outpost image, the earlier platform silhouette was replaced with the actual
  tower composition: broad hipped dark-oak roof, pale upper interior, wrap
  balcony and fences, dark-oak lower tower, and mossed stone foundation.

## Reference Direction

- The visual direction was checked against publicly accessible Minecraft
  resource assets, including the 1.21.1 cherry-leaves texture from
  [InventivetalentDev/minecraft-assets](https://github.com/InventivetalentDev/minecraft-assets).
  Assets are reference material only: none are copied into the Figma file or
  the mod.
- Color supports block material recognition, but all final markers must remain
  distinguishable from their silhouette in monochrome.

## Implementation Boundary

## Full-Dimension Addition

- Added review section `Review / Additional Structures` (`46:1022`) under the
  same review root. It contains cards for ocean ruin, buried treasure,
  overworld ruined portal, nether ruined portal, stronghold, mineshaft, nether
  fortress, bastion remnant, end city, and end gateway.
- The card miniatures use the same material and silhouette decisions used by
  the in-game `src/state/SeedMapIconAtlas.h`: water/stone ruin columns,
  sand-buried chest, broken obsidian frames, stone-brick portal room, wooden
  mine supports, nether-brick bridge, irregular blackstone bastion, and a
  purpur end-city tower.
- End gateway is displayed as the natural outer-End gateway silhouette. Its
  in-game layer uses cubiomes' seed-only outer-gateway finder; fixed
  dragon-island gateways remain excluded because they are event-driven.
- Revision after review: the stronghold marker is now an **Eye of Ender**,
  rather than a stone-brick building. The ten added icons were reduced from
  card-sized architectural silhouettes to compact 12px-or-less bodies inside
  their 20px map slots, matching the lower visual weight requested for these
  additional layers.

## Game Atlas

- Added `Card / Slime Chunk` (`51:110`) as the visual authority for the green
  Minecraft slime-cube icon. Its 20px grid is transcribed into
  `SeedMapIconAtlas.h` for the control panel and right-bottom legend. On the
  map itself, a Slime Chunk uses the full translucent 16x16 cell rather than a
  dense center icon, while hover and selection use that real chunk footprint.

- The game now draws 20px data-only pixel icons instead of circular structure
  markers. The atlas contains one bounded, unique pattern for every enabled
  cubiomes structure, the six rare overworld layers, four Nether biome layers,
  and four End biome layers.
- The added structures use a deliberately smaller occupied area within their
  20px source grid. In-game, the source grid scales with map zoom from 20px to
  64px (32px at the default 3x zoom), so the compact silhouettes remain
  legible without becoming map-sized at close range.
- Markers have no opaque black backing. Hover and selection use the scaled icon
  boundary; selection retains an amber outline, while the detail panel keeps
  the name, coordinates, upstream risk note, centering action, and saved-
  waypoint action.
- `worldgen_tests` validates that every cubiomes-backed catalog layer has an
  icon, every painted rectangle lies inside the 20px box, and no two layers
  share an atlas entry.

## Figma Synchronization Contract

`Review / Minecraft Structure Icon Reference Draft` (`44:593`) is the visual
source of truth. `src/state/SeedMapIconAtlas.h` stores the matching review-card
ID beside every atlas entry. A data-only game icon must be transcribed from the
card's `20px map icon` rectangles: coordinates are in a 20 by 20 logical grid,
and Figma's 2x review pixels map directly to one logical atlas pixel. This
includes every rectangle's bounds and RGB material color. The atlas must not be
independently redrawn or replaced by an external texture.

| Review cards | Figma IDs |
| --- | --- |
| Original structures | `44:608` through `44:619` |
| Original rare overworld biomes | `44:620` through `44:625` |
| Additional 26.20 structures | `46:1024` through `46:1033` |
| Nether biomes | `50:41` through `50:44` |
| End biomes | `50:45` through `50:48` |

The newly completed cards are Soul Sand Valley (`50:41`), Crimson Forest
(`50:42`), Warped Forest (`50:43`), Basalt Deltas (`50:44`), Small End Islands
(`50:45`), End Midlands (`50:46`), End Highlands (`50:47`), and End Barrens
(`50:48`). End Gateway (`46:1033`) explicitly represents the seed-only natural
outer-End gateway; fixed dragon-island gateways remain excluded.

## Fixture Boundary

1. An icon does not establish real-world fixture accuracy. `FixtureVerified`
   and clearly labelled `CubiomesReference` layers can be toggled and queried.
2. Each non-village structure still needs a real Bedrock 26.20 `/locate`
   fixture before being promoted to `FixtureVerified`.
3. Keep `src/worldgen/` pure and thread-safe; the atlas belongs to the UI
   boundary and contains no game or generator object.
