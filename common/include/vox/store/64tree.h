//
// Created by niooi on 10/6/2025.
//

#pragma once

// Implementation of a 4^3 tree (each node has 64 children)
// Unlike many implementations of SVTs where the 'depth' of the tree corresponds to how
// small subdivisions (the voxel size) gets, this implementation is based around the model
// of depth as the maximum volume of the tree. The voxel size is fixed to <1, 1, 1>, and
// depth will define how 'large' of a volume the tree occupies in world space. I've found
// this personally to be a nicer mental model. Too bad the rest of the engine will have to
// think so too.

// ALSO, air is implicitly stored. If a node doesn't exist, then it is air.
// A node that actaully exists will always encode at least one non-air voxel.

#include <defs.h>
#include <vmath.h>
#include <vox/aabb.h>
#include <vox/volume.h>

// TODO! change to implement VoxelVolume
namespace v {
    struct GS64Node {};

    using S64Node_UP = std::unique_ptr<struct S64Node>;
    using S64Node_P  = S64Node*;
    using VoxelType  = u8;

    struct S64Node {
        /// the existing children
        /// TODO!
        /// instead of allocating 64 on every use (also with the voxel vector)
        /// use POPCOUNT to tightly pack into a vector maybe?.
        std::vector<S64Node_UP> children;
        S64Node_P               parent = nullptr;
        /// for leaf node: represents which voxels in the brick exist
        /// for non leaf: represents which children in the children array exist
        u64 child_mask = 0;
        /// 4x4x4 voxel brick - allocated on demand YAY!
        std::vector<VoxelType> voxels;
        // TODO! i still want to use a single bit boolean on the GPU to
        // reduce mem per voxel, this is just more convenient for CPU stuff.
        // The gpu buffer will be a simple POD type anyways, this can be resolved when
        // flattening the tree
        enum class Type : u8 {
            /// No voxels exist / node is empty. In theory, this should never be used.
            /// Here for good measure.
            Empty,
            /// Just a regular non-leaf node
            Regular,
            /// Leaf node - contains 4x4x4 voxel brick.
            /// This does not mean that each 'voxel' in the brick is 1x1x1,
            /// as higher level nodes can be leaves as well, with each 'voxel' in the
            /// brick
            /// representing a completely filled region (of the same type) of nxnxn,
            /// depending on the depth
            /// of the leaf.
            Leaf,
            /// Single type leaf - all voxels within the node's region are the same type,
            /// and
            /// are therefore not stored in a 4x4x4 brick, but stored as a single u8 at
            /// voxels[0]. This means that the node is completely FILLED btw.
            SingleTypeLeaf
        };
        /// Node type
        Type type = Type::Empty;

        /// Returns the index of the child/voxel in the arrays present in the node.
        FORCEINLINE u32 get_idx(u32 x, u32 y, u32 z) noexcept
        {
            return x | (z << 2) | (y << 4);
        }

        struct ChildIterator {
            u64 mask = 0;
            u32 idx  = 0;

            using value_type        = u32;
            using difference_type   = std::ptrdiff_t;
            using iterator_category = std::forward_iterator_tag;

            FORCEINLINE value_type operator*() const { return idx; }

            FORCEINLINE ChildIterator& operator++()
            {
                mask &= (mask - 1); // clear lowest set bit
                if (mask)
                    idx = CTZ64(mask);
                return *this;
            }

            FORCEINLINE bool operator==(const ChildIterator& o) const
            {
                return mask == o.mask;
            }

            FORCEINLINE bool operator!=(const ChildIterator& o) const
            {
                return !(*this == o);
            }
        };

        struct ChildRange {
            u64                       mask;
            FORCEINLINE ChildIterator begin() const
            {
                ChildIterator it{ mask, 0 };
                if (mask)
                    it.idx = CTZ64(mask);
                return it;
            }
            FORCEINLINE ChildIterator end() const { return ChildIterator{ 0, 0 }; }
        };

        FORCEINLINE ChildRange child_indices() { return ChildRange{ child_mask }; };
    };

    class Sparse64Tree : public VoxelVolume<Sparse64Tree, u8> {
    public:
        using VoxelType = u8;

        explicit Sparse64Tree(u8 depth) :
            bounds_(glm::vec3(0), glm::vec3(v::pow(4.f, static_cast<f32>(depth)))),
            depth_(depth), g_nodes_(static_cast<u32>(v::pow(8.f, depth)))
        {}

        /// Constructs the smallest 64Tree that can contain the bounding box.
        /// Translation of the box does not matter.
        /// TODO! bunch of warnings and stuff
        explicit Sparse64Tree(const AABB& must_contain) :
            Sparse64Tree(
                static_cast<u8>(v::ceil_log(
                    v::max_component(must_contain.max - must_contain.min), 4.0)))
        {}

        /// Returns the bounding box that this tree occupies in it's local object space.
        /// The box will always be properly oriented.
        /// One vertex (min) will always be the origin, such that the other vertex (max)
        /// will always be in the positive octant.
        FORCEINLINE const AABB& bounding_box() const { return bounds_; };

        VoxelType voxel_at(const glm::vec3& pos) const;

        VoxelType get_voxel(u32 x, u32 y, u32 z) const;
        VoxelType get_voxel(const glm::ivec3& pos) const;

        void set_voxel(u32 x, u32 y, u32 z, VoxelType type);
        void set_voxel(const glm::ivec3& pos, VoxelType type);

        void fill_aabb(const AABB& region, VoxelType type);
        void fill_sphere(const glm::vec3& center, f32 radius, VoxelType type);
        void fill_cylinder(
            const glm::vec3& p0, const glm::vec3& p1, f32 radius, VoxelType type);

        /// Flattens the tree into an array of GPU friendly nodes.
        // TODO! should maybe move into different place? so 64tree only worries about cpu
        // side storage? idk
        void                         flatten() const;
        const std::vector<GS64Node>& gpu_nodes() { return g_nodes_; };


        /// Destroys the contents of the entire tree
        FORCEINLINE void clear()
        {
            clear_node(root_);
            dirty_ = true;
        }

    private:
        S64Node_UP root_{ nullptr };
        AABB       bounds_;
        u8         depth_;

        /// whether the flat gpu node buffer needs rebuilding
        bool                  dirty_{};
        std::vector<GS64Node> g_nodes_;

        // TODO! move utilities to struct S64Node? does this make sense even
        /// Recursively destroys nodes
        void clear_node(S64Node_UP& node);

        /// Fills an entire node with a single type. Very fast.
        void fill_node(S64Node_UP& node, VoxelType t);

        /// Returns the starting shift amount for tree traversal
        FORCEINLINE u8 init_shift_amt() const
        {
            return CTZ(static_cast<u32>(bounds_.max.x) >> 2);
        }

        /// Transforms position to local coordinates within a node
        FORCEINLINE void to_local_coords(glm::uvec3& pos, u8 shift_amt) const
        {
            u32 mask = (1u << shift_amt) - 1;
            pos.x &= mask;
            pos.y &= mask;
            pos.z &= mask;
        }

        /// Checks if a node contains any voxels
        bool is_node_empty(const S64Node& node) const;

        /// Attempts to convert a Leaf node to SingleTypeLeaf if all voxels are the same
        void try_collapse_to_single_type(S64Node_UP& node);

        /// Checks if a Regular node should be collapsed (all children empty)
        bool should_collapse_regular(S64Node_UP& node);

        /// Geometric tests
        static bool aabb_contains_aabb(const AABB& outer, const AABB& inner);
        static bool aabb_intersects_aabb(const AABB& a, const AABB& b);
        static bool
        aabb_inside_sphere(const AABB& box, const glm::vec3& center, f32 radius);
        static bool
        aabb_intersects_sphere(const AABB& box, const glm::vec3& center, f32 radius);
        static bool aabb_inside_cylinder(
            const AABB& box, const glm::vec3& p0, const glm::vec3& p1, f32 radius,
            const glm::vec3& axis, f32 length);
        static bool aabb_intersects_cylinder(
            const AABB& box, const glm::vec3& p0, const glm::vec3& p1, f32 radius,
            const glm::vec3& axis, f32 length);

        /// Hierarchical fill helpers
        void fill_aabb_recursive(
            S64Node_UP& node, const glm::uvec3& node_pos, u8 shift_amt,
            const AABB& region, VoxelType type);
        void fill_sphere_recursive(
            S64Node_UP& node, const glm::uvec3& node_pos, u8 shift_amt,
            const glm::vec3& center, f32 radius, VoxelType type);
        void fill_cylinder_recursive(
            S64Node_UP& node, const glm::uvec3& node_pos, u8 shift_amt,
            const glm::vec3& p0, const glm::vec3& p1, f32 radius, const glm::vec3& axis,
            f32 length, VoxelType type);
    };
} // namespace v
