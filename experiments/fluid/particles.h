//
// Created by niooi on 11/7/2025.
//

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <prelude.h>
#include <vector>

namespace v {
    struct Particle {
        glm::vec2 pos; // position
        glm::vec2 vel; // velocity
        glm::mat2 C; // affine momentum matrix
        f32       mass; // particle mass
    };

    class ParticleDomain : Domain<ParticleDomain> {
    public:
        ParticleDomain()           = default;
        ~ParticleDomain() override = default;

        void init() override;

        std::vector<Particle>&       particles() { return particles_; }
        const std::vector<Particle>& particles() const { return particles_; }

    private:
        std::vector<Particle> particles_;
    };
} // namespace v
