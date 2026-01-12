//
// Created by niooi on 12/28/2025.
//

#pragma once

#include <defs.h>
#include <glm/glm.hpp>
#include <vox/aabb.h>

namespace v {
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

        template <DerivedFromVV From, DerivedFromVV To>
        static To to(const From&);
    };

    template <typename Derived, typename VoxelT>
    class VoxelVolume {
    public:
        using VoxelType = VoxelT;

        std::optional<VoxelType> get(glm::ivec3);
        u8                       set(glm::ivec3, VoxelType);

        void resize(AABB aabb);
        // TODO! template this i forgot the syntax lol
        void fill(VoxelType(glm::ivec3));

        struct Iterator {
            using value_type        = VoxelType;
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

        /// Convert the volume to another volume type
        template <DerivedFromVV T>
        T to()
        {
            return VolumeOpImpl::to<Derived, T>(*this);
        }
    };
} // namespace v
