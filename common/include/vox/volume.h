//
// Created by niooi on 12/28/2025.
//

#pragma once

#include <defs.h>
#include <glm/glm.hpp>
#include <vox/aabb.h>

namespace v {
    using Coord = glm::ivec3;

    template <typename Derived, typename VoxelT>
    class VoxelVolume;

    // Check if T derives from VoxelVolume<T, SomeVoxelType>
    template <typename T>
    concept DerivedFromVV = requires { typename T::VoxelType; } &&
        std::is_base_of_v<VoxelVolume<T, typename T::VoxelType>, T>;

    // all volume conversions/operations should be overwritten here
    class VolumeOpImpl {
    public:
        // TODO! impl the other csg stuff and defaults for all

        // Implements a conversion function from one voxel type to another, if needed.
        // TODO! provide default specialization for Type a == b
        template <typename VoxelTypeA, typename VoxelTypeB>
        static VoxelTypeB voxel_to(const VoxelTypeA& a) {
            TODO()
        }

        template <DerivedFromVV From, DerivedFromVV To>
        static To to(const From& f) {
            To ret{};

            TODO()
        }
    };

    template <typename Derived, typename VoxelT>
    class VoxelVolume {
    public:
        using VoxelType = VoxelT;

        std::optional<VoxelType> get(Coord);
        u8                       set(Coord, VoxelType);

        void resize(AABB aabb);
        // TODO! template this i forgot the syntax lol
        void fill(VoxelType(Coord));

        struct Iterator {
            using value_type        = std::pair <Coord, VoxelType>;
            using difference_type   = std::ptrdiff_t;
            using iterator_category = std::forward_iterator_tag;

            // TODO! well actually each type should implement
        };

        // example of derivedfrom voxel volume usage w/ CSG igs
        // Ret is the first param so we can specificy it for the return type,
        // the Other type can/should be inferred from the passed in type
        // TODO! implement default SLOOW voxel by voxel csg
        template <DerivedFromVV Ret = Derived, DerivedFromVV Other = Derived>
        Ret subtract(const Other&);

        template <DerivedFromVV Ret = Derived, DerivedFromVV Other = Derived>
        Ret intersect(const Other&);

        /// This is actually a 'union' operation, however you can't name a member function
        /// a keyword.
        template <DerivedFromVV Ret = Derived, DerivedFromVV Other = Derived>
        Ret join(const Other&);

        /// Convert the volume to another volume type
        template <DerivedFromVV T>
        T to()
        {
            return VolumeOpImpl::to<Derived, T>(*this);
        }
    };
} // namespace v
