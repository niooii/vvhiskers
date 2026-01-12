//
// Created by niooi on 11/7/2025.
//

#pragma once

#include <prelude.h>
#include "particles.h"

namespace v {
    struct GridCell {
        glm::vec2 vel; // velocity
        f32       mass; // mass
    };
} // namespace v
