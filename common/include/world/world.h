//
// Created by niooi on 9/24/2025.
//

#pragma once

#include <containers/ud_map.h>
#include <cstdint>
#include <defs.h>
#include <engine/domain.h>
#include <vox/store/svo.h>

namespace v {

    /// 3D integer chunk coordinate
    struct ChunkPos {
        i32 x;
        i32 y;
        i32 z;
    };

    struct ChunkPosHash {
        using is_avalanching = void;
        u64 operator()(const ChunkPos& p) const noexcept
        {
            // Lightweight 3D hash combine
            u64  h   = 1469598103934665603ull; // FNV offset basis
            auto mix = [&](u64 v)
            { h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2); };
            mix(static_cast<u64>(static_cast<u32>(p.x)));
            mix(static_cast<u64>(static_cast<u32>(p.y)));
            mix(static_cast<u64>(static_cast<u32>(p.z)));
            return h;
        }
    };

    struct ChunkPosEq {
        bool operator()(const ChunkPos& a, const ChunkPos& b) const noexcept
        {
            return a.x == b.x && a.y == b.y && a.z == b.z;
        }
    };

    /// Local voxel coordinate in a chunk [0, 127]
    struct VoxelPos {
        i32 x;
        i32 y;
        i32 z;
    };

    /// World grid coordinate (voxel-space)
    struct WorldPos {
        i32 x;
        i32 y;
        i32 z;
    };

    /// Chunk domain, queryable from the engine
    class ChunkDomain : public Domain<ChunkDomain> {
    public:
        static constexpr i32 k_size = SparseVoxelOctree128::size; // 128

        ChunkDomain(ChunkPos pos, const std::string& name = "Chunk") :
            Domain(name), pos_(pos)
        {}

        FORCEINLINE const ChunkPos&             pos() const { return pos_; }
        FORCEINLINE SparseVoxelOctree128&       svo() { return svo_; }
        FORCEINLINE const SparseVoxelOctree128& svo() const { return svo_; }

        u16  get(VoxelPos lp) const { return svo_.get(lp.x, lp.y, lp.z); }
        void set(VoxelPos lp, u16 v)
        {
            svo_.set(lp.x, lp.y, lp.z, v);
            dirty_ = true;
        }

        FORCEINLINE bool dirty() const { return dirty_; }
        FORCEINLINE void clear_dirty() { dirty_ = false; }

    private:
        ChunkPos             pos_{};
        SparseVoxelOctree128 svo_{};
        bool                 dirty_{ false };
    };

    /// World state shared by client and server (no server-only logic here)
    /// - Stores chunks in a sparse map keyed by ChunkPos
    /// - Provides get/set for world-space voxels
    /// - Contains conversion helpers between world and chunk coordinates
    class WorldDomain : public SDomain<WorldDomain> {
    public:
        static constexpr i32 k_chunk_size = ChunkDomain::k_size; // 128

        WorldDomain(const std::string& name = "World") : SDomain(name) {}
        ~WorldDomain() override = default;

        /// Convert world-space voxel coordinate to chunk position and local position
        static std::pair<ChunkPos, VoxelPos> world_to_chunk(WorldPos wp);

        /// Get a chunk if loaded, else nullptr
        ChunkDomain*       try_get_chunk(const ChunkPos& cp);
        const ChunkDomain* try_get_chunk(const ChunkPos& cp) const;

        /// Get or create a chunk at position
        ChunkDomain& get_or_create_chunk(const ChunkPos& cp);

        /// Remove a chunk if present; returns true if removed
        bool remove_chunk(const ChunkPos& cp);

        /// Returns true if a chunk is loaded
        bool has_chunk(const ChunkPos& cp) const;

        /// Get voxel at world coordinate (0 if not present)
        u16 get_voxel(WorldPos wp) const;

        /// Set voxel at world coordinate
        void set_voxel(WorldPos wp, u16 value);

        /// Iterate loaded chunks count
        size_t chunk_count() const { return chunks_.size(); }

    private:
        using ChunkMap = ud_map<ChunkPos, ChunkDomain*, ChunkPosHash, ChunkPosEq>;
        ChunkMap chunks_{};
    };
} // namespace v
