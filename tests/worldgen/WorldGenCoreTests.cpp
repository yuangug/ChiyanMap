#ifdef NDEBUG
#undef NDEBUG
#endif

#include "worldgen/BedrockBiomeValidator.h"
#include "worldgen/StructureCatalog.h"
#include "worldgen/TileQuery.h"
#include "state/SeedMapIconAtlas.h"
#include "state/SeedMapManager.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>

using namespace ChiyanMap::WorldGen;

namespace {

void TestCubiomesViewerVillageFinder() {
    const SeedSnapshot seed = SeedSnapshot::FromLevelSeed(114514);
    Bedrock2620StructureFinder finder{*seed.TrySeedBits()};
    const auto position = finder.FindVillagePosition(2, -9);
    assert(position);
    assert((*position == BlockPosition{1256, -4536}));
    assert(finder.IsVillageViable(*position));
    const LayerQuery query{
        seed,
        WorldGenProfile::Bedrock2620,
        Dimension::Overworld,
        LayerId::Village,
        BlockRect{position->x, position->z, position->x + 1, position->z + 1},
    };

    const LayerQueryResult result = QueryPlacementCandidates(query);
    assert(result.status == QueryStatus::Ok);
    assert(result.statistics.placementCandidates == 1);
    assert(result.statistics.biomeRejectedCandidates == 0);
    assert(result.statistics.emittedMarkers == 1);
    assert(result.statistics.emittedMarkers == result.markers.size());
    assert(result.markers.front().Block() == *position);
}

void TestVillageTileQueryTerminatesAtBounds() {
    // This is the tile containing the in-game village observed at
    // (1243, 107, -4522). Its first scanned region has an out-of-bounds
    // candidate, so it regresses the loop termination path directly.
    const LayerQuery query{
        SeedSnapshot::FromLevelSeed(114514),
        WorldGenProfile::Bedrock2620,
        Dimension::Overworld,
        LayerId::Village,
        BlockRect{1024, -5120, 2048, -4096},
    };

    const LayerQueryResult result = QueryPlacementCandidates(query);
    assert(result.status == QueryStatus::Ok);
    assert(result.statistics.placementCandidates == 4);
    assert(result.statistics.biomeRejectedCandidates == 2);
    assert(result.statistics.emittedMarkers == 2);
    assert(result.markers.size() == 2);
    assert((result.markers[0].Block() == BlockPosition{1256, -4536}));
    assert((result.markers[1].Block() == BlockPosition{1896, -4680}));
}

void TestBedrock2620VillageBiomeEligibility() {
    Bedrock2620StructureFinder finder{114514U};
    const auto position = finder.FindVillagePosition(2, -9);
    assert(position);
    assert(finder.IsVillageViable(*position));

    assert(!finder.IsVillageViable(BlockPosition{std::numeric_limits<std::int32_t>::max(), 0}));
}

void TestRemoteSeedSentinelIsRejected() {
    assert(!IsUsableCapturedBedrockSeed(kUnavailableRemoteBedrockSeedBits));
    assert(IsUsableCapturedBedrockSeed(0U));
    assert(IsUsableCapturedBedrockSeed(0xFFFFFFFFFFFFFFFFULL));
}

void TestCubiomesFinderContracts() {
    Bedrock2620StructureFinder overworld{Dimension::Overworld, 114514U};
    constexpr std::array overworldLayers{
        LayerId::Village, LayerId::PillagerOutpost, LayerId::DesertPyramid, LayerId::JungleTemple,
        LayerId::SwampHut, LayerId::Igloo, LayerId::WoodlandMansion, LayerId::OceanMonument,
        LayerId::OceanRuin, LayerId::Shipwreck, LayerId::BuriedTreasure, LayerId::RuinedPortal,
        LayerId::Stronghold, LayerId::Mineshaft, LayerId::AncientCity, LayerId::TrailRuins,
        LayerId::TrialChambers,
    };
    for (const LayerId layer : overworldLayers) assert(overworld.RegionSizeChunks(layer));
    assert(overworld.RegionSizeChunks(LayerId::Village) == 34);
    assert(!overworld.RegionSizeChunks(LayerId::NetherFortress));
    assert(!overworld.FindStructurePosition(LayerId::NetherFortress, 0, 0));
    assert(!overworld.RegionSizeChunks(LayerId::EndGateway));

    Bedrock2620StructureFinder nether{Dimension::Nether, 114514U};
    constexpr std::array netherLayers{
        LayerId::RuinedPortalNether, LayerId::NetherFortress, LayerId::BastionRemnant,
    };
    for (const LayerId layer : netherLayers) assert(nether.RegionSizeChunks(layer));
    assert(!nether.RegionSizeChunks(LayerId::Village));

    Bedrock2620StructureFinder end{Dimension::End, 114514U};
    assert(end.RegionSizeChunks(LayerId::EndCity));
    assert(!end.RegionSizeChunks(LayerId::EndGateway));
}

void TestSeedMapIconAtlas() {
    using namespace ChiyanMap::SeedMapIcons;
    assert(ValidateAtlas());
    for (const LayerDescriptor& descriptor : LayerCatalog()) {
        if (!IsMarkerEmissionAllowed(descriptor.support)) continue;
        if (descriptor.algorithm != PlacementAlgorithm::CubiomesFinder
            && descriptor.algorithm != PlacementAlgorithm::BiomeNoise
            && descriptor.algorithm != PlacementAlgorithm::ChunkPopulation) continue;
        const IconDefinition* icon = FindIcon(descriptor.id);
        assert(icon != nullptr);
        assert(!icon->pixels.empty());
        assert(!icon->figmaCardId.empty());
    }
}

void TestBedrock2620SlimeChunks() {
    constexpr ChunkPosition slimeChunk{-586, 434};
    constexpr ChunkPosition adjacentChunk{-585, 434};
    Bedrock2620StructureFinder finder{114514U};
    assert(finder.IsSlimeChunk(slimeChunk));
    assert(!finder.IsSlimeChunk(adjacentChunk));

    const LayerQuery exactChunk{
        SeedSnapshot::FromLevelSeed(114514),
        WorldGenProfile::Bedrock2620,
        Dimension::Overworld,
        LayerId::SlimeChunks,
        BlockRect{-9376, 6944, -9360, 6960},
    };
    assert(exactChunk.Bounds().Contains(BlockPosition{-9371, 6953}));
    const LayerQueryResult result = QueryPlacementCandidates(exactChunk);
    assert(result.status == QueryStatus::Ok);
    assert(result.support == LayerSupportState::FixtureVerified);
    assert(result.statistics.placementCandidates == 1);
    assert(result.statistics.biomeRejectedCandidates == 0);
    assert(result.statistics.emittedMarkers == 1);
    assert(result.markers.size() == 1);
    assert(result.markers.front().Chunk() == slimeChunk);
    assert((result.markers.front().Block() == BlockPosition{-9368, 6952}));
    assert(result.markers.front().Kind() == MarkerKind::ChunkRegion);

    const LayerQuery excludesNeighbor{
        SeedSnapshot::FromLevelSeed(114514),
        WorldGenProfile::Bedrock2620,
        Dimension::Overworld,
        LayerId::SlimeChunks,
        BlockRect{-9360, 6944, -9344, 6960},
    };
    assert(QueryPlacementCandidates(excludesNeighbor).markers.empty());

    const auto tile = QueryPlacementCandidatesForTile(
        SeedSnapshot::FromLevelSeed(114514), WorldGenProfile::Bedrock2620, Dimension::Overworld,
        LayerId::SlimeChunks, TileCoordinate{-10, 6}
    );
    const auto neighborTile = QueryPlacementCandidatesForTile(
        SeedSnapshot::FromLevelSeed(114514), WorldGenProfile::Bedrock2620, Dimension::Overworld,
        LayerId::SlimeChunks, TileCoordinate{-9, 6}
    );
    std::vector<SeedMapMarker> joined = tile.markers;
    joined.insert(joined.end(), neighborTile.markers.begin(), neighborTile.markers.end());
    assert(DeduplicateMarkers(joined).size() == joined.size());
}

void TestCubiomesReferenceLayersCanQuery() {
    const SeedSnapshot seed = SeedSnapshot::FromLevelSeed(8675309);
    const LayerQuery query{
        seed,
        WorldGenProfile::Bedrock2620,
        Dimension::Overworld,
        LayerId::TrialChambers,
        BlockRect{0, 0, 1024, 1024},
    };

    const LayerQueryResult result = QueryPlacementCandidates(query);
    assert(result.status == QueryStatus::Ok);
    assert(result.support == LayerSupportState::CubiomesReference);
    assert(result.statistics.emittedMarkers == result.markers.size());
}

void TestRareBiomeQueriesUseSeedOnlyBiomeNoise() {
    Bedrock2620StructureFinder finder{Dimension::Overworld, 114514U};
    constexpr std::array layers{
        LayerId::MushroomFields,
        LayerId::CherryGrove,
        LayerId::Badlands,
        LayerId::IceSpikes,
        LayerId::MangroveSwamp,
        LayerId::PaleGarden,
    };
    constexpr std::array expectedBiomes{
        14,  // mushroom_fields
        185, // cherry_grove
        37,  // badlands
        140, // ice_spikes
        184, // mangrove_swamp
        186, // pale_garden
    };

    for (std::size_t index = 0; index < layers.size(); ++index) {
        const LayerQuery query{
            SeedSnapshot::FromLevelSeed(114514),
            WorldGenProfile::Bedrock2620,
            Dimension::Overworld,
            layers[index],
            BlockRect{0, 0, 1024, 1024},
        };
        const LayerQueryResult result = QueryPlacementCandidates(query);
        assert(result.status == QueryStatus::Ok);
        assert(result.support == LayerSupportState::CubiomesReference);
        assert(result.statistics.emittedMarkers == result.markers.size());
        for (const SeedMapMarker& marker : result.markers) {
            assert(marker.Kind() == MarkerKind::BiomeRegion);
            const auto biome = finder.GetBiomeIdAt(marker.Block());
            assert(biome && *biome == expectedBiomes[index]);
        }
    }
}

void TestNetherAndEndBiomeQueriesUseSeedOnlyBiomeNoise() {
    struct BiomeLayerFixture final {
        Dimension dimension;
        LayerId layer;
        int expectedBiome;
    };
    constexpr std::array fixtures{
        BiomeLayerFixture{Dimension::Nether, LayerId::SoulSandValley, 170},
        BiomeLayerFixture{Dimension::Nether, LayerId::CrimsonForest, 171},
        BiomeLayerFixture{Dimension::Nether, LayerId::WarpedForest, 172},
        BiomeLayerFixture{Dimension::Nether, LayerId::BasaltDeltas, 173},
        BiomeLayerFixture{Dimension::End, LayerId::EndSmallIslands, 40},
        BiomeLayerFixture{Dimension::End, LayerId::EndMidlands, 41},
        BiomeLayerFixture{Dimension::End, LayerId::EndHighlands, 42},
        BiomeLayerFixture{Dimension::End, LayerId::EndBarrens, 43},
    };

    for (const BiomeLayerFixture& fixture : fixtures) {
        Bedrock2620StructureFinder finder{fixture.dimension, 114514U};
        const LayerQuery query{
            SeedSnapshot::FromLevelSeed(114514),
            WorldGenProfile::Bedrock2620,
            fixture.dimension,
            fixture.layer,
            BlockRect{0, 0, 1024, 1024},
        };
        const LayerQueryResult result = QueryPlacementCandidates(query);
        assert(result.status == QueryStatus::Ok);
        assert(result.support == LayerSupportState::CubiomesReference);
        assert(result.statistics.emittedMarkers == result.markers.size());
        for (const SeedMapMarker& marker : result.markers) {
            assert(marker.Kind() == MarkerKind::BiomeRegion);
            const auto biome = finder.GetBiomeIdAt(marker.Block());
            assert(biome && *biome == fixture.expectedBiome);
        }
    }
}

void TestOuterEndGatewayQueryUsesSeedOnlyFinder() {
    const LayerQuery query{
        SeedSnapshot::FromLevelSeed(114514),
        WorldGenProfile::Bedrock2620,
        Dimension::End,
        LayerId::EndGateway,
        BlockRect{1024, 1024, 2048, 2048},
    };
    const LayerQueryResult result = QueryPlacementCandidates(query);
    assert(result.status == QueryStatus::Ok);
    assert(result.support == LayerSupportState::CubiomesReference);
    assert(result.statistics.biomeRejectedCandidates == 0);
    assert(result.statistics.emittedMarkers == result.markers.size());
}

void TestUnsupportedLayersNeverEmitMarkers() {
    const LayerQuery query{
        SeedSnapshot::FromLevelSeed(8675309),
        WorldGenProfile::Bedrock2620,
        Dimension::Nether,
        LayerId::NetherFossil,
        BlockRect{0, 0, 1024, 1024},
    };
    const LayerQueryResult result = QueryPlacementCandidates(query);
    assert(result.status == QueryStatus::LayerUnavailable);
    assert(result.support == LayerSupportState::Unavailable);
    assert(result.markers.empty());
}

void TestSeedZeroAndTileBoundaries() {
    const SeedSnapshot zero = SeedSnapshot::FromLevelSeed(0);
    assert(zero.HasSeed());
    assert(zero.TryLegacySeed32() && *zero.TryLegacySeed32() == 0U);
    assert(!SeedSnapshot::Unavailable().HasSeed());

    const TileCoordinate expectedNegativeTile{-1, -2};
    assert(TileForBlock(BlockPosition{-1, -1025}) == expectedNegativeTile);
    const auto tile = TryBoundsForTile(TileCoordinate{-1, 0});
    assert(tile && tile->minX == -1024 && tile->maxXExclusive == 0);

    const auto left = QueryPlacementCandidatesForTile(
        SeedSnapshot::FromLevelSeed(8675309),
        WorldGenProfile::Bedrock2620,
        Dimension::Overworld,
        LayerId::Village,
        TileCoordinate{0, 0}
    );
    const auto right = QueryPlacementCandidatesForTile(
        SeedSnapshot::FromLevelSeed(8675309),
        WorldGenProfile::Bedrock2620,
        Dimension::Overworld,
        LayerId::Village,
        TileCoordinate{1, 0}
    );

    std::vector<SeedMapMarker> joined = left.markers;
    joined.insert(joined.end(), right.markers.begin(), right.markers.end());
    assert(DeduplicateMarkers(joined).size() == joined.size());
}

void TestSeedMapManagerIntegration() {
    using namespace SeedMapManager;

    const Settings defaults = DefaultSettings();
    for (const bool enabled : defaults.enabledLayers) assert(!enabled);

    Init(1);
    SetSettings(defaults);
    NotifyWorldChanged("worldgen-integration", SeedSnapshot::FromLevelSeed(8675309), 8675309U, 0);

    const Status captured = GetStatus();
    assert(captured.initialized);
    assert(captured.seedAvailable);
    assert(captured.capturedSeedBits && *captured.capturedSeedBits == 8675309U);
    assert(captured.currentGameDimension && *captured.currentGameDimension == Dimension::Overworld);

    // Worker scheduling is covered by the pure tile-query tests. Keep this
    // lifecycle test free of a background biome scan.
    assert(GetStatus().seedAvailable);

    assert(SetLayerEnabled(LayerId::Village, true));
    assert(SetLayerEnabled(LayerId::EndCity, true));
    assert(SetLayerEnabled(LayerId::SlimeChunks, true));
    assert(IsLayerEnabled(LayerId::Village));
    assert(IsLayerEnabled(LayerId::EndCity));
    assert(IsLayerEnabled(LayerId::SlimeChunks));
    assert(ClearAllLayers());
    assert(!IsLayerEnabled(LayerId::Village));
    assert(!IsLayerEnabled(LayerId::EndCity));
    assert(!IsLayerEnabled(LayerId::SlimeChunks));
    assert(!ClearAllLayers());

    ClearWorldContext();
    const Status cleared = GetStatus();
    assert(!cleared.seedAvailable);
    assert(!cleared.capturedSeedBits);
    assert(!cleared.currentGameDimension);
    Shutdown();
}

} // namespace

// Compile this file with -DCHIYANMAP_WORLDGEN_TEST_MAIN for a standalone pure
// C++ test binary. It is intentionally not part of the production mod target.
#ifdef CHIYANMAP_WORLDGEN_TEST_MAIN
int main() {
    TestBedrock2620VillageBiomeEligibility();
    TestCubiomesViewerVillageFinder();
    TestVillageTileQueryTerminatesAtBounds();
    TestRemoteSeedSentinelIsRejected();
    TestCubiomesFinderContracts();
    TestSeedMapIconAtlas();
    TestCubiomesReferenceLayersCanQuery();
    TestRareBiomeQueriesUseSeedOnlyBiomeNoise();
    TestNetherAndEndBiomeQueriesUseSeedOnlyBiomeNoise();
    TestOuterEndGatewayQueryUsesSeedOnlyFinder();
    TestBedrock2620SlimeChunks();
    TestUnsupportedLayersNeverEmitMarkers();
    TestSeedMapManagerIntegration();
}
#endif
